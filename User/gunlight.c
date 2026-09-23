/********************************** (C) COPYRIGHT *******************************
 * File Name          : gunlight.c
 * Author             : Weapon Systems Lab
 * Version            : V3.0.0 (Unified Architecture)
 * Description        : 战术枪灯核心业务状态机与交互式 CLI 调参引擎
 *******************************************************************************/
#include "gunlight.h"
#include "bsp.h"
#include "usb_cdc.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define STROBE_PERIOD_TICKS     5       // 50ms 翻转一次 (10Hz 爆闪)
#define ADC_SAMPLE_PERIOD_TICKS 20      // 200ms 采样一次电压

/* 电池放电阶梯等级 (LVP 自适应低压降额管理) */
typedef enum {
    BAT_TIER_NORMAL = 0,    // >= 3400mV: 正常满血状态 (允许 100% 满功率输出)
    BAT_TIER_LOW,           // 3100mV ~ 3400mV: 节能限流 (限制最大 25% 功率，防电池内阻跳水)
    BAT_TIER_CRITICAL,      // 2950mV ~ 3100mV: 保命微光 (强制 5% 月光档，延长续航 1~2 小时)
    BAT_TIER_CUTOFF         // < 2950mV: 截止保护休眠
} Battery_Tier_e;

/* 国际标准 SOS 莫尔斯电码序列 (· · · — — — · · ·) */
static const struct {
    uint8_t on;
    uint16_t duration;
} c_sos_seq[] = {
    {1, 20}, {0, 20}, {1, 20}, {0, 20}, {1, 20}, {0, 60},
    {1, 60}, {0, 20}, {1, 60}, {0, 20}, {1, 60}, {0, 60},
    {1, 20}, {0, 20}, {1, 20}, {0, 20}, {1, 20}, {0, 140}
};
#define SOS_SEQ_LEN (sizeof(c_sos_seq) / sizeof(c_sos_seq[0]))

static Gunlight_State_e s_gl_state = GL_STATE_OFF;
static Battery_Tier_e   s_bat_tier = BAT_TIER_NORMAL;
static uint16_t s_cur_vbat = 3800;
static uint16_t s_cur_pwm1 = 0;
static uint16_t s_cur_pwm2 = 0;
static uint16_t s_standby_timeout_ticks = 1000; // 默认 10s (1000 * 10ms)

static uint16_t s_standby_timer = 0;
static uint16_t s_strobe_timer = 0;
static uint8_t  s_strobe_flag = 0;
static uint8_t  s_sos_step = 0;
static uint16_t s_sos_timer = 0;
static uint16_t s_adc_timer = 0;
static uint8_t  s_tactical_override = 0;
static uint8_t  s_cutoff_confirm = 0;

static void Gunlight_Apply_PWM(void);
static void Gunlight_Update_Battery_Status(void);
static void Gunlight_Process_CLI(void);

static void Gunlight_SetPWM_Internal(uint16_t p1, uint16_t p2)
{
    s_cur_pwm1 = p1;
    s_cur_pwm2 = p2;
    BSP_PWM_SetDuty_PWM1(p1);
    BSP_PWM_SetDuty_PWM2(p2);
}

/**
 * @brief 根据当前枪灯状态与电池放电阶梯等级综合更新 PWM 输出
 */
static void Gunlight_Apply_PWM(void)
{
    if (s_tactical_override)
    {
        // 备用战术按键按住：若在极低电量下限制为 25%，正常情况下 100% 满功率
        if (s_bat_tier == BAT_TIER_CRITICAL)
        {
            Gunlight_SetPWM_Internal(250, 0);
        }
        else
        {
            Gunlight_SetPWM_Internal(1000, 0);
        }
        return;
    }

    if (s_gl_state == GL_STATE_OFF)
    {
        Gunlight_SetPWM_Internal(0, 0);
        return;
    }

    // 保命微光档强制接管：强制 5% 月光微光，副灯关闭
    if (s_bat_tier == BAT_TIER_CRITICAL)
    {
        Gunlight_SetPWM_Internal(50, 0);
        return;
    }

    switch (s_gl_state)
    {
        case GL_STATE_MODE1_100:
            if (s_bat_tier == BAT_TIER_LOW)
            {
                Gunlight_SetPWM_Internal(250, 0); // 节能降额 25%
            }
            else
            {
                Gunlight_SetPWM_Internal(1000, 0); // 100% 满功率
            }
            break;

        case GL_STATE_MODE2_25:
            Gunlight_SetPWM_Internal(250, 0);
            break;

        case GL_STATE_MODE3_DUAL:
            if (s_bat_tier == BAT_TIER_LOW)
            {
                Gunlight_SetPWM_Internal(250, 1000); // 主灯降额，激光器维持 100%
            }
            else
            {
                Gunlight_SetPWM_Internal(1000, 1000); // 双灯 100% 满额
            }
            break;

        case GL_STATE_STROBE:
            if (s_strobe_flag)
            {
                uint16_t duty = (s_bat_tier == BAT_TIER_LOW) ? 350 : 1000;
                Gunlight_SetPWM_Internal(duty, 0);
            }
            else
            {
                Gunlight_SetPWM_Internal(0, 0);
            }
            break;

        case GL_STATE_SOS:
            if (c_sos_seq[s_sos_step].on)
            {
                uint16_t duty = (s_bat_tier == BAT_TIER_LOW) ? 350 : 1000;
                Gunlight_SetPWM_Internal(duty, 0);
            }
            else
            {
                Gunlight_SetPWM_Internal(0, 0);
            }
            break;

        default:
            Gunlight_SetPWM_Internal(0, 0);
            break;
    }
}

/**
 * @brief 周期性检测电池电压并执行智能阶梯降档与指示灯管理
 */
static void Gunlight_Update_Battery_Status(void)
{
    uint16_t vbat = BSP_ADC_GetBatteryVoltage_mV();
    s_cur_vbat = vbat;

    // 1. 低电量截止保护 (< 2950mV，连续确认2次防瞬态浪涌误判)
    if (vbat < 2950)
    {
        s_cutoff_confirm++;
        if (s_cutoff_confirm >= 2)
        {
            PRINT("[PWR] Battery Critical (< 2.95V)! Cutoff protection triggered.\r\n");
            BSP_PWM_AllOff();
            BSP_StatusLED_SetColor(255, 0, 0);
            Delay_Ms(300);
            BSP_StatusLED_AllOff();
            BSP_EnterStandby();
            return;
        }
    }
    else
    {
        s_cutoff_confirm = 0;
    }

    // 2. 阶梯降档判定 (含 50mV 迟滞回差防抖)
    Battery_Tier_e old_tier = s_bat_tier;

    if (vbat < 3100)
    {
        s_bat_tier = BAT_TIER_CRITICAL; // 5% 保命微光档
    }
    else if (vbat < 3400)
    {
        if (s_bat_tier != BAT_TIER_CRITICAL || vbat >= 3150)
        {
            s_bat_tier = BAT_TIER_LOW; // 25% 节能限流
        }
    }
    else if (vbat >= 3450)
    {
        s_bat_tier = BAT_TIER_NORMAL; // 满血放电档
    }

    if (old_tier != s_bat_tier)
    {
        PRINT("[PWR] Battery Tier Switch: %d -> %d (Vbat=%d mV)\r\n", old_tier, s_bat_tier, (int)vbat);
        Gunlight_Apply_PWM();
    }

    // 3. RGB 指示灯更新
    if (vbat >= 3600)      BSP_StatusLED_SetMode(STATUS_LED_BAT_HIGH);
    else if (vbat >= 3400) BSP_StatusLED_SetMode(STATUS_LED_BAT_MED);
    else if (vbat >= 3100) BSP_StatusLED_SetMode(STATUS_LED_BAT_LOW);
    else                   BSP_StatusLED_SetMode(STATUS_LED_BAT_CRITICAL);
}

/**
 * @brief 枪灯系统初始化
 */
void Gunlight_Init(void)
{
    s_gl_state = GL_STATE_OFF;
    s_bat_tier = BAT_TIER_NORMAL;
    s_standby_timer = 0;
    s_strobe_timer = 0;
    s_strobe_flag = 0;
    s_adc_timer = 0;
    s_tactical_override = 0;
    s_cutoff_confirm = 0;

    BSP_PWM_AllOff();
    BSP_StatusLED_AllOff();
    PRINT("[GUNLIGHT] Init Completed.\r\n");
}

/**
 * @brief 正常开机点亮模式 1
 */
void Gunlight_TurnOn_Mode1(void)
{
    s_gl_state = GL_STATE_MODE1_100;
    s_standby_timer = 0;
    Gunlight_Update_Battery_Status();
    Gunlight_Apply_PWM();
    PRINT("[GUNLIGHT] Turned On -> Mode 1 (100%%)\r\n");
}

/**
 * @brief 10ms 周期主轮询任务
 */
void Gunlight_Process_10ms(void)
{
    // 1. 扫描按键事件
    Key_Event_e key_evt = BSP_Key_Scan_10ms();
    AuxKey_Event_e aux_evt = BSP_AuxKey_Scan_10ms();

    // 2. 备用按键战术点动处理
    if (aux_evt == AUX_KEY_EVT_PRESS)
    {
        s_tactical_override = 1;
        Gunlight_Apply_PWM();
        BSP_StatusLED_SetColor(0, 255, 0); // 战术绿光点动提示
    }
    else if (aux_evt == AUX_KEY_EVT_RELEASE)
    {
        s_tactical_override = 0;
        Gunlight_Apply_PWM();
        if (s_gl_state == GL_STATE_OFF)
        {
            BSP_StatusLED_AllOff();
        }
        else
        {
            Gunlight_Update_Battery_Status();
        }
    }

    // 3. 主按键事件响应
    if (key_evt != KEY_EVT_NONE)
    {
        s_standby_timer = 0; // 重置无操作超时

        switch (key_evt)
        {
            case KEY_EVT_SINGLE_CLICK:
                // 单击轮循: 主灯100% -> 25% -> 主+激光双开 -> 全关 -> 100%
                if (s_gl_state == GL_STATE_OFF)
                {
                    s_gl_state = GL_STATE_MODE1_100;
                    Gunlight_Update_Battery_Status();
                    Gunlight_Apply_PWM();
                    PRINT("[GUNLIGHT] Click: Mode 1\r\n");
                }
                else if (s_gl_state == GL_STATE_MODE1_100)
                {
                    s_gl_state = GL_STATE_MODE2_25;
                    Gunlight_Apply_PWM();
                    PRINT("[GUNLIGHT] Click: Mode 2 (25%%)\r\n");
                }
                else if (s_gl_state == GL_STATE_MODE2_25)
                {
                    s_gl_state = GL_STATE_MODE3_DUAL;
                    Gunlight_Apply_PWM();
                    PRINT("[GUNLIGHT] Click: Mode 3 (Main + Laser)\r\n");
                }
                else if (s_gl_state == GL_STATE_MODE3_DUAL || s_gl_state == GL_STATE_STROBE || s_gl_state == GL_STATE_SOS)
                {
                    s_gl_state = GL_STATE_OFF;
                    s_standby_timer = 0;
                    Gunlight_SetPWM_Internal(0, 0);
                    BSP_StatusLED_AllOff();
                    PRINT("[GUNLIGHT] Click: All Off\r\n");
                }
                break;

            case KEY_EVT_DOUBLE_CLICK:
                // 双击：10Hz 战术爆闪
                s_gl_state = GL_STATE_STROBE;
                s_strobe_timer = 0;
                s_strobe_flag = 1;
                BSP_StatusLED_SetMode(STATUS_LED_STROBE);
                Gunlight_Apply_PWM();
                PRINT("[GUNLIGHT] Double Click: 10Hz Strobe Mode\r\n");
                break;

            case KEY_EVT_TRIPLE_CLICK:
                // 三击：国际标准 SOS 救援
                s_gl_state = GL_STATE_SOS;
                s_sos_step = 0;
                s_sos_timer = 0;
                BSP_StatusLED_SetMode(STATUS_LED_SOS);
                Gunlight_Apply_PWM();
                PRINT("[GUNLIGHT] Triple Click: SOS Distress Mode\r\n");
                break;

            case KEY_EVT_LONG_PRESS:
                // 长按：全部关闭并检测是否保持按住 8 秒跳转 ISP
                s_gl_state = GL_STATE_OFF;
                s_standby_timer = 0;
                Gunlight_SetPWM_Internal(0, 0);
                BSP_StatusLED_AllOff();
                PRINT("[GUNLIGHT] Long Press: All Off. Checking 8s ISP hold...\r\n");

                uint16_t extra_cnt = 0;
                while (BSP_MainKey_IsPressed())
                {
                    Delay_Ms(10);
                    extra_cnt++;
                    if (extra_cnt >= 680) // 1.2s + 6.8s = 8 秒！
                    {
                        PRINT("[GUNLIGHT] 8s reached -> Jump to ISP Bootloader!\r\n");
                        BSP_JumpToBootloader();
                    }
                }
                break;

            default:
                break;
        }
    }

    // 4. 特殊模式动态翻转
    if (s_gl_state == GL_STATE_STROBE)
    {
        s_strobe_timer++;
        if (s_strobe_timer >= STROBE_PERIOD_TICKS)
        {
            s_strobe_timer = 0;
            s_strobe_flag = !s_strobe_flag;
            Gunlight_Apply_PWM();
        }
    }
    else if (s_gl_state == GL_STATE_SOS)
    {
        s_sos_timer++;
        if (s_sos_timer >= c_sos_seq[s_sos_step].duration)
        {
            s_sos_timer = 0;
            s_sos_step = (s_sos_step + 1) % SOS_SEQ_LEN;
            Gunlight_Apply_PWM();
        }
    }

    // 5. 电压采样与阶梯降档
    if (s_gl_state != GL_STATE_OFF || s_tactical_override)
    {
        s_adc_timer++;
        if (s_adc_timer >= ADC_SAMPLE_PERIOD_TICKS)
        {
            s_adc_timer = 0;
            Gunlight_Update_Battery_Status();
        }
    }

    // 6. 灭灯无操作自动休眠倒计时
    if (s_gl_state == GL_STATE_OFF && !s_tactical_override)
    {
        s_standby_timer++;
        if (s_standby_timer >= s_standby_timeout_ticks)
        {
            BSP_EnterStandby();
        }
    }

    // 7. 处理 RGB 动画与 USB CLI 解析
    BSP_StatusLED_Process_10ms();
    Gunlight_Process_CLI();
}

/* -------------------------------------------------------------------------- */
/* 内置 USB CLI 调参命令行解析引擎 (无额外依赖，极速响应)                      */
/* -------------------------------------------------------------------------- */
static char s_cli_line[64];
static uint8_t s_cli_idx = 0;

static void Gunlight_Process_CLI(void)
{
    while (USB_CDC_Available())
    {
        int16_t ch = USB_CDC_ReadByte();
        if (ch < 0) break;

        if (ch == '\r' || ch == '\n')
        {
            if (s_cli_idx > 0)
            {
                s_cli_line[s_cli_idx] = '\0';
                s_cli_idx = 0;

                // 指令分发
                if (strcasecmp(s_cli_line, "STATUS") == 0)
                {
                    printf("[STATUS] State: %d, Vbat: %d mV, BatTier: %d, PWM1: %d/1000, PWM2: %d/1000, Timeout: %ds\r\n",
                           s_gl_state, s_cur_vbat, s_bat_tier, s_cur_pwm1, s_cur_pwm2, s_standby_timeout_ticks / 100);
                }
                else if (strncasecmp(s_cli_line, "SET PWM1 ", 9) == 0)
                {
                    int duty = atoi(&s_cli_line[9]);
                    if (duty < 0) duty = 0;
                    if (duty > 1000) duty = 1000;
                    s_cur_pwm1 = (uint16_t)duty;
                    BSP_PWM_SetDuty_PWM1(s_cur_pwm1);
                    s_standby_timer = 0;
                    printf("[OK] Set PWM1 = %d/1000 (%.1f%%)\r\n", s_cur_pwm1, (float)s_cur_pwm1 / 10.0f);
                }
                else if (strncasecmp(s_cli_line, "SET PWM2 ", 9) == 0)
                {
                    int duty = atoi(&s_cli_line[9]);
                    if (duty < 0) duty = 0;
                    if (duty > 1000) duty = 1000;
                    s_cur_pwm2 = (uint16_t)duty;
                    BSP_PWM_SetDuty_PWM2(s_cur_pwm2);
                    s_standby_timer = 0;
                    printf("[OK] Set PWM2 = %d/1000 (%.1f%%)\r\n", s_cur_pwm2, (float)s_cur_pwm2 / 10.0f);
                }
                else if (strncasecmp(s_cli_line, "SET MODE ", 9) == 0)
                {
                    int mode = atoi(&s_cli_line[9]);
                    if (mode >= 0 && mode <= 5)
                    {
                        Gunlight_SetState((Gunlight_State_e)mode);
                        printf("[OK] Mode switched to %d\r\n", mode);
                    }
                    else
                    {
                        printf("[ERR] Invalid mode 0-5\r\n");
                    }
                }
                else if (strncasecmp(s_cli_line, "SET TIMEOUT ", 12) == 0)
                {
                    int sec = atoi(&s_cli_line[12]);
                    if (sec >= 1 && sec <= 120)
                    {
                        s_standby_timeout_ticks = (uint16_t)(sec * 100);
                        printf("[OK] Set Standby Timeout = %ds\r\n", sec);
                    }
                    else
                    {
                        printf("[ERR] Timeout range: 1~120s\r\n");
                    }
                }
                else if (strcasecmp(s_cli_line, "ISP") == 0)
                {
                    printf("[SYS] Jump to Bootloader requested via USB...\r\n");
                    BSP_JumpToBootloader();
                }
                else if (strcasecmp(s_cli_line, "REBOOT") == 0)
                {
                    printf("[SYS] Rebooting...\r\n");
                    Delay_Ms(20);
                    NVIC_SystemReset();
                }
                else if (strcasecmp(s_cli_line, "HELP") == 0)
                {
                    printf("\r\n=== Tactical Gunlight CLI Commands ===\r\n");
                    printf("  STATUS              : Get all telemetry\r\n");
                    printf("  SET PWM1 <0-1000>   : Set Main WLED duty\r\n");
                    printf("  SET PWM2 <0-1000>   : Set Laser duty\r\n");
                    printf("  SET MODE <0-5>      : Set state (0:Off, 1:100%%, 2:25%%, 3:Dual, 4:Strobe, 5:SOS)\r\n");
                    printf("  SET TIMEOUT <1-120> : Set auto-sleep seconds\r\n");
                    printf("  ISP                 : Jump to factory Bootloader\r\n");
                    printf("  REBOOT              : Soft reset system\r\n\r\n");
                }
                else
                {
                    printf("[ERR] Unknown command: %s. Type HELP.\r\n", s_cli_line);
                }
            }
        }
        else if (ch >= 32 && ch <= 126 && s_cli_idx < sizeof(s_cli_line) - 1)
        {
            s_cli_line[s_cli_idx++] = (char)ch;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Getter / Setter 实现                                                       */
/* -------------------------------------------------------------------------- */
Gunlight_State_e Gunlight_GetState(void) { return s_gl_state; }

void Gunlight_SetState(Gunlight_State_e state)
{
    s_gl_state = state;
    s_standby_timer = 0;
    if (state == GL_STATE_OFF)
    {
        Gunlight_SetPWM_Internal(0, 0);
        BSP_StatusLED_AllOff();
    }
    else
    {
        Gunlight_Update_Battery_Status();
        Gunlight_Apply_PWM();
    }
}

uint16_t Gunlight_GetBatteryVoltage_mV(void) { return s_cur_vbat; }
uint8_t  Gunlight_GetBatteryTier(void) { return (uint8_t)s_bat_tier; }
uint16_t Gunlight_GetStandbyTimeoutSec(void) { return s_standby_timeout_ticks / 100; }
void     Gunlight_SetStandbyTimeoutSec(uint16_t sec) { s_standby_timeout_ticks = sec * 100; }
uint16_t Gunlight_GetPwm1Duty(void) { return s_cur_pwm1; }
uint16_t Gunlight_GetPwm2Duty(void) { return s_cur_pwm2; }
void     Gunlight_SetPwm1Duty(uint16_t duty) { s_cur_pwm1 = (duty > 1000) ? 1000 : duty; BSP_PWM_SetDuty_PWM1(s_cur_pwm1); }
void     Gunlight_SetPwm2Duty(uint16_t duty) { s_cur_pwm2 = (duty > 1000) ? 1000 : duty; BSP_PWM_SetDuty_PWM2(s_cur_pwm2); }
