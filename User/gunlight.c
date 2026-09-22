#include "gunlight.h"
#include "bsp_key.h"
#include "bsp_pwm.h"
#include "bsp_adc.h"
#include "bsp_led.h"
#include "bsp_isp.h"
#include "debug.h"

#define STANDBY_TIMEOUT_TICKS   1000    // 10秒无操作超时 (1000 * 10ms = 10s) 进入 Standby
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
    uint8_t on;         // 亮(1) 或 灭(0)
    uint16_t duration;  // 持续 10ms ticks (20=200ms, 60=600ms, 140=1400ms)
} c_sos_seq[] = {
    // S: · · ·
    {1, 20}, {0, 20}, {1, 20}, {0, 20}, {1, 20}, {0, 60},
    // O: — — —
    {1, 60}, {0, 20}, {1, 60}, {0, 20}, {1, 60}, {0, 60},
    // S: · · ·
    {1, 20}, {0, 20}, {1, 20}, {0, 20}, {1, 20}, {0, 140}
};
#define SOS_SEQ_LEN (sizeof(c_sos_seq) / sizeof(c_sos_seq[0]))

static Gunlight_State_e s_gl_state = GL_STATE_OFF;
static Battery_Tier_e   s_bat_tier = BAT_TIER_NORMAL;
static uint16_t s_standby_timer = 0;
static uint16_t s_strobe_timer = 0;
static uint8_t  s_strobe_flag = 0;
static uint8_t  s_sos_step = 0;
static uint16_t s_sos_timer = 0;
static uint16_t s_adc_timer = 0;
static uint8_t  s_tactical_override = 0; // 备用按键战术点亮覆盖标志
static uint8_t  s_cutoff_confirm = 0;    // 截止保护防误判连续计数

static void Gunlight_Apply_PWM(void);
static void Gunlight_Update_Battery_Status(void);

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
            BSP_PWM_SetDuty_PWM1(250);
        }
        else
        {
            BSP_PWM_SetDuty_PWM1(1000);
        }
        BSP_PWM_SetDuty_PWM2(0);
        return;
    }

    // 关灯状态
    if (s_gl_state == GL_STATE_OFF)
    {
        BSP_PWM_AllOff();
        return;
    }

    // 【保命微光档强制接管】当电池极度亏电 (2.95V ~ 3.1V) 时，强制 5% 月光微光，副灯关闭
    if (s_bat_tier == BAT_TIER_CRITICAL)
    {
        BSP_PWM_SetDuty_PWM1(50); // 5% 超低功耗月光照路
        BSP_PWM_SetDuty_PWM2(0);
        return;
    }

    switch (s_gl_state)
    {
        case GL_STATE_MODE1_100:
            // 模式 1: 低电节能限流钳位为 25%，正常电量输出 100%
            if (s_bat_tier == BAT_TIER_LOW)
            {
                BSP_PWM_SetDuty_PWM1(250); // 25% 节能限流防跳水
            }
            else
            {
                BSP_PWM_SetDuty_PWM1(1000); // 100% 满功率
            }
            BSP_PWM_SetDuty_PWM2(0);
            break;

        case GL_STATE_MODE2_25:
            // 模式 2: 主灯 25%
            BSP_PWM_SetDuty_PWM1(250);
            BSP_PWM_SetDuty_PWM2(0);
            break;

        case GL_STATE_MODE3_DUAL:
            // 模式 3: 主副同亮；低电节能模式适度降额
            if (s_bat_tier == BAT_TIER_LOW)
            {
                BSP_PWM_SetDuty_PWM1(250);
                BSP_PWM_SetDuty_PWM2(150);
            }
            else
            {
                BSP_PWM_SetDuty_PWM1(1000);
                BSP_PWM_SetDuty_PWM2(500);
            }
            break;

        case GL_STATE_STROBE:
            // 爆闪由周期定时器动态翻转
            if (s_strobe_flag)
            {
                uint16_t duty = (s_bat_tier == BAT_TIER_LOW) ? 350 : 1000;
                BSP_PWM_SetDuty_PWM1(duty);
            }
            else
            {
                BSP_PWM_SetDuty_PWM1(0);
            }
            BSP_PWM_SetDuty_PWM2(0);
            break;

        case GL_STATE_SOS:
            // SOS 由莫尔斯序列步进控制
            if (c_sos_seq[s_sos_step].on)
            {
                uint16_t duty = (s_bat_tier == BAT_TIER_LOW) ? 350 : 1000;
                BSP_PWM_SetDuty_PWM1(duty);
                BSP_PWM_SetDuty_PWM2(0);
            }
            else
            {
                BSP_PWM_AllOff();
            }
            break;

        default:
            BSP_PWM_AllOff();
            break;
    }
}

/**
 * @brief 周期性检测电池电压并执行智能阶梯降档与指示灯管理
 */
static void Gunlight_Update_Battery_Status(void)
{
    uint16_t vbat = BSP_ADC_GetBatteryVoltage_mV();

    // 1. 低电量截止保护 (< 2950mV，连续确认2次防瞬态浪涌干扰)
    if (vbat < 2950)
    {
        s_cutoff_confirm++;
        if (s_cutoff_confirm >= 2)
        {
            PRINT("[PWR] Battery Critical (< 2.95V)! Cutoff protection triggered.\r\n");
            BSP_PWM_AllOff();
            BSP_StatusLED_SetColor(255, 0, 0); // 红色警示
            Delay_Ms(300);
            BSP_StatusLED_AllOff();
            Gunlight_Enter_LowPower_Standby(); // 强制进入 Standby 保护电池不被过放
            return;
        }
    }
    else
    {
        s_cutoff_confirm = 0;
    }

    // 2. 阶梯降档判定 (含迟滞回差 Hysteresis 防抖)
    Battery_Tier_e old_tier = s_bat_tier;

    if (vbat < 3100)
    {
        // 极低电量：进入 5% 保命微光档
        s_bat_tier = BAT_TIER_CRITICAL;
    }
    else if (vbat < 3400)
    {
        // 若之前在极低微光档，需升至 3150mV 才退出微光档
        if (s_bat_tier != BAT_TIER_CRITICAL || vbat >= 3150)
        {
            s_bat_tier = BAT_TIER_LOW; // 节能降额限流
        }
    }
    else if (vbat >= 3450)
    {
        // 满血放电档 (带 50mV 迟滞回差)
        s_bat_tier = BAT_TIER_NORMAL;
    }

    // 若放电阶梯变更，立即重新刷新功率限制
    if (old_tier != s_bat_tier)
    {
        PRINT("[PWR] Battery Tier Switch: %d -> %d (Vbat=%d mV)\r\n", old_tier, s_bat_tier, (int)vbat);
        Gunlight_Apply_PWM();
    }

    // 3. RGB 指示灯更新 (在常规照明档位下实时展示)
    if (vbat >= 3600)
    {
        BSP_StatusLED_SetMode(STATUS_LED_BAT_HIGH); // 绿灯常亮 (>= 3.6V)
    }
    else if (vbat >= 3400)
    {
        BSP_StatusLED_SetMode(STATUS_LED_BAT_MED);  // 蓝灯常亮 (3.4V ~ 3.6V)
    }
    else if (vbat >= 3100)
    {
        BSP_StatusLED_SetMode(STATUS_LED_BAT_LOW);  // 黄灯慢闪 (3.1V ~ 3.4V)
    }
    else
    {
        BSP_StatusLED_SetMode(STATUS_LED_BAT_CRITICAL); // 红灯快闪 (2.95V ~ 3.1V)
    }
}

/**
 * @brief 进入真正的待机低功耗模式 (Standby 模式，功耗 ~10uA)
 */
void Gunlight_Enter_LowPower_Standby(void)
{
    PRINT("[PWR] Entering Low-Power Standby Mode...\r\n");

    // 1. 关闭所有 PWM 输出
    BSP_PWM_AllOff();

    // 2. 关闭 RGB 指示灯
    BSP_StatusLED_AllOff();

    // 3. 关闭 ADC 转换与内部参考电压
    BSP_ADC_DeInit();

    // 4. 等待用户按键松手，防止立即二次触发唤醒
    while (BSP_MainKey_IsPressed())
    {
        Delay_Ms(10);
    }
    Delay_Ms(50); // 防抖消除

    // 5. 开启电源管理时钟，使能 WKUP (PA0) 引脚唤醒
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_WakeUpPinCmd(ENABLE);

    // 6. 清除唤醒标志，进入 Standby 模式
    PWR_ClearFlag(PWR_FLAG_WU);
    PWR_EnterSTANDBYMode();

    // 唤醒后系统会自动产生系统复位重新执行 main()
}

/**
 * @brief 枪灯系统初始化
 */
void Gunlight_Init(void)
{
    BSP_Key_Init();
    BSP_PWM_Init();
    BSP_StatusLED_Init();

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
        BSP_StatusLED_SetColor(0, 255, 0); // 绿光点动提示
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
        // 只要有任何按键操作，重置 10S 超时计时器
        s_standby_timer = 0;

        switch (key_evt)
        {
            case KEY_EVT_SINGLE_CLICK:
                // 单击模式循环: 主灯100% -> 25% -> 主+副同时亮 -> 同时关闭 -> 100%
                if (s_gl_state == GL_STATE_OFF)
                {
                    // 1. 开机 -> 模式 1
                    s_gl_state = GL_STATE_MODE1_100;
                    BSP_ADC_Init();
                    Gunlight_Update_Battery_Status();
                    Gunlight_Apply_PWM();
                    PRINT("[GUNLIGHT] Click: Mode 1 Turned On\r\n");
                }
                else if (s_gl_state == GL_STATE_MODE1_100)
                {
                    // 2. 模式 2: 主灯 25%
                    s_gl_state = GL_STATE_MODE2_25;
                    Gunlight_Apply_PWM();
                    PRINT("[GUNLIGHT] Click: Mode 2 (Main 25%)\r\n");
                }
                else if (s_gl_state == GL_STATE_MODE2_25)
                {
                    // 3. 模式 3: 主灯+副灯 同时亮
                    s_gl_state = GL_STATE_MODE3_DUAL;
                    Gunlight_Apply_PWM();
                    PRINT("[GUNLIGHT] Click: Mode 3 (Main + Aux)\r\n");
                }
                else if (s_gl_state == GL_STATE_MODE3_DUAL)
                {
                    // 4. 同时关闭！
                    s_gl_state = GL_STATE_OFF;
                    s_standby_timer = 0;
                    BSP_PWM_AllOff();
                    BSP_StatusLED_AllOff();
                    BSP_ADC_DeInit();
                    PRINT("[GUNLIGHT] Click: All Off, 10s countdown to Standby...\r\n");
                }
                else if (s_gl_state == GL_STATE_STROBE || s_gl_state == GL_STATE_SOS)
                {
                    // 爆闪或SOS特殊模式下单按退出，返回关灯
                    s_gl_state = GL_STATE_OFF;
                    s_standby_timer = 0;
                    BSP_PWM_AllOff();
                    BSP_StatusLED_AllOff();
                    BSP_ADC_DeInit();
                    PRINT("[GUNLIGHT] Click: Exit Special Mode -> All Off\r\n");
                }
                break;

            case KEY_EVT_DOUBLE_CLICK:
                // 双击：启动 10Hz 战术爆闪模式 (青色/冰蓝指示)
                s_gl_state = GL_STATE_STROBE;
                s_strobe_timer = 0;
                s_strobe_flag = 1;
                BSP_ADC_Init();
                BSP_StatusLED_SetMode(STATUS_LED_STROBE);
                Gunlight_Apply_PWM();
                PRINT("[GUNLIGHT] Double Click: 10Hz Strobe Mode\r\n");
                break;

            case KEY_EVT_TRIPLE_CLICK:
                // 三连击：启动国际标准 SOS 救援模式 (紫色莫尔斯同步指示)
                s_gl_state = GL_STATE_SOS;
                s_sos_step = 0;
                s_sos_timer = 0;
                BSP_ADC_Init();
                BSP_StatusLED_SetMode(STATUS_LED_SOS);
                Gunlight_Apply_PWM();
                PRINT("[GUNLIGHT] Triple Click: SOS Distress Mode\r\n");
                break;

            case KEY_EVT_LONG_PRESS:
                // 任何模式下长按：全部关闭
                s_gl_state = GL_STATE_OFF;
                s_standby_timer = 0;
                BSP_PWM_AllOff();
                BSP_StatusLED_AllOff(); // 关闭指示灯
                BSP_ADC_DeInit();       // 停止电压采样
                PRINT("[GUNLIGHT] Long-press: All Off! Checking for 8s ISP hold...\r\n");

                // 关机灭灯后，若继续保持按住达 8 秒进入 ISP 烧录模式
                uint16_t extra_cnt = 0;
                while (BSP_MainKey_IsPressed())
                {
                    Delay_Ms(10);
                    extra_cnt++;
                    if (extra_cnt >= 680) // 1.2s + 6.8s = 8 秒
                    {
                        PRINT("[GUNLIGHT] 8s hold reached -> Enter ISP Bootloader!\r\n");
                        BSP_ISP_JumpToBootloader();
                    }
                }
                PRINT("[GUNLIGHT] All Off confirmed, 10s countdown to Standby...\r\n");
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

    // 5. 电压检测与阶梯降档管理：常规照明模式下周期采样
    if (s_gl_state == GL_STATE_MODE1_100 || s_gl_state == GL_STATE_MODE2_25 ||
        s_gl_state == GL_STATE_MODE3_DUAL || s_tactical_override)
    {
        s_adc_timer++;
        if (s_adc_timer >= ADC_SAMPLE_PERIOD_TICKS)
        {
            s_adc_timer = 0;
            Gunlight_Update_Battery_Status();
        }
    }

    // 6. 灭灯关闭状态下的 10 秒无操作关机休眠倒计时
    if (s_gl_state == GL_STATE_OFF && !s_tactical_override)
    {
        s_standby_timer++;
        if (s_standby_timer >= STANDBY_TIMEOUT_TICKS)
        {
            Gunlight_Enter_LowPower_Standby();
        }
    }

    // 7. 处理 RGB 指示灯动画时钟
    BSP_StatusLED_Process_10ms();
}

/**
 * @brief 正常开机点亮模式 1
 */
void Gunlight_TurnOn_Mode1(void)
{
    s_gl_state = GL_STATE_MODE1_100;
    s_standby_timer = 0;
    BSP_ADC_Init();
    Gunlight_Update_Battery_Status();
    Gunlight_Apply_PWM();
    PRINT("[GUNLIGHT] Turned On -> Mode 1\r\n");
}
