/********************************** (C) COPYRIGHT *******************************
 * File Name          : bsp.c
 * Author             : Weapon Systems Lab
 * Version            : V3.0.0 (Unified All-in-One BSP)
 * Description        : 战术枪灯一体化板级驱动实现 (硬件外设高内聚实现)
 *******************************************************************************/
#include "bsp.h"
#include "debug.h"
#include "usb_cdc.h"

/* -------------------------------------------------------------------------- */
/* 1. 双路 20kHz 硬件高频 PWM 驱动 (TIM2 CH3:PA2, CH4:PA3)                   */
/* -------------------------------------------------------------------------- */
#define PWM_ARR_PERIOD_VAL  4799 // 96MHz / (0+1) / (4799+1) = 20,000.0 Hz

static void BSP_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // PA2 (TIM2_CH3, 主白光), PA3 (TIM2_CH4, 副激光)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period = PWM_ARR_PERIOD_VAL;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0; // 不分频，96MHz 计数时钟
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC3Init(TIM2, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_OC4Init(TIM2, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void BSP_PWM_SetDuty_PWM1(uint16_t duty)
{
    if (duty > 1000) duty = 1000;
    uint32_t compare = ((uint32_t)duty * (PWM_ARR_PERIOD_VAL + 1)) / 1000;
    TIM_SetCompare3(TIM2, (uint16_t)compare);
}

void BSP_PWM_SetDuty_PWM2(uint16_t duty)
{
    if (duty > 1000) duty = 1000;
    uint32_t compare = ((uint32_t)duty * (PWM_ARR_PERIOD_VAL + 1)) / 1000;
    TIM_SetCompare4(TIM2, (uint16_t)compare);
}

void BSP_PWM_AllOff(void)
{
    TIM_SetCompare3(TIM2, 0);
    TIM_SetCompare4(TIM2, 0);
}

/* -------------------------------------------------------------------------- */
/* 2. 单线全彩智能 RGB 指示灯 (XL-1615RGBC-RF, PA4, 800kHz 归零码)          */
/* -------------------------------------------------------------------------- */
static Status_LED_Mode_e s_led_mode = STATUS_LED_OFF;
static uint16_t s_led_anim_ticks = 0;

static void BSP_RGB_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_4);
}

static inline void Send_Bit0(void)
{
    GPIOA->BSHR = GPIO_Pin_4; // 高电平 ~300ns
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    GPIOA->BCR  = GPIO_Pin_4; // 低电平 ~900ns
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
}

static inline void Send_Bit1(void)
{
    GPIOA->BSHR = GPIO_Pin_4; // 高电平 ~900ns
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    GPIOA->BCR  = GPIO_Pin_4; // 低电平 ~300ns
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
}

void BSP_StatusLED_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    uint32_t mask = 0x800000;

    __disable_irq();
    while (mask)
    {
        if (grb & mask)
        {
            Send_Bit1();
        }
        else
        {
            Send_Bit0();
        }
        mask >>= 1;
    }
    GPIOA->BCR = GPIO_Pin_4;
    __enable_irq();

    Delay_Us(60); // Reset 码 (保持 >50us 低电平)
}

void BSP_StatusLED_SetMode(Status_LED_Mode_e mode)
{
    s_led_mode = mode;
    s_led_anim_ticks = 0;
}

void BSP_StatusLED_AllOff(void)
{
    s_led_mode = STATUS_LED_OFF;
    BSP_StatusLED_SetColor(0, 0, 0);
}

void BSP_StatusLED_Process_10ms(void)
{
    s_led_anim_ticks++;

    switch (s_led_mode)
    {
        case STATUS_LED_BAT_HIGH:
            BSP_StatusLED_SetColor(0, 180, 0); // 绿灯常亮
            break;

        case STATUS_LED_BAT_MED:
            BSP_StatusLED_SetColor(0, 80, 220); // 蓝灯常亮
            break;

        case STATUS_LED_BAT_LOW:
            // 黄灯慢闪 (500ms 周期: 250ms 亮 / 250ms 灭)
            if ((s_led_anim_ticks % 50) < 25)
            {
                BSP_StatusLED_SetColor(180, 100, 0);
            }
            else
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            break;

        case STATUS_LED_BAT_CRITICAL:
            // 红灯快闪 (200ms 周期: 100ms 亮 / 100ms 灭)
            if ((s_led_anim_ticks % 20) < 10)
            {
                BSP_StatusLED_SetColor(220, 0, 0);
            }
            else
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            break;

        case STATUS_LED_STROBE:
            // 爆闪青蓝同步
            if ((s_led_anim_ticks % 10) < 5)
            {
                BSP_StatusLED_SetColor(0, 200, 200);
            }
            else
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            break;

        case STATUS_LED_SOS:
            // 紫光同步
            if ((s_led_anim_ticks % 30) < 15)
            {
                BSP_StatusLED_SetColor(180, 0, 200);
            }
            else
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            break;

        case STATUS_LED_OFF:
        default:
            BSP_StatusLED_SetColor(0, 0, 0);
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* 3. 单通道 ADC 电池电压检测 (PA1: ADC_Channel_1, 470k:470k 分压)             */
/* -------------------------------------------------------------------------- */
static void BSP_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    ADC_InitTypeDef  ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8); // ADC 采样时钟

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    // ADC 校准
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

uint16_t BSP_ADC_GetBatteryVoltage_mV(void)
{
    uint32_t sum = 0;
    // 连续采样 8 次取均值滤波
    for (int i = 0; i < 8; i++)
    {
        ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
        sum += ADC_GetConversionValue(ADC1);
    }
    uint16_t raw_adc = (uint16_t)(sum / 8);

    // Vbat = raw * 3000mV / 4095 * 2 (1:1 分压电路) = raw * 6000 / 4095
    uint32_t vbat_mv = ((uint32_t)raw_adc * 6000) / 4095;
    return (uint16_t)vbat_mv;
}

/* -------------------------------------------------------------------------- */
/* 4. 按键扫描与状态机 (PA0: 主键/WKUP; PA6: 备用战术键)                      */
/* -------------------------------------------------------------------------- */
#define KEY_SHORT_CLICK_MAX_TICKS   40   // 400ms 内松开为短按
#define KEY_MULTI_CLICK_GAP_TICKS   25   // 250ms 内为连击间隔
#define KEY_LONG_PRESS_TICKS        120  // 1.2s 判定为长按

static uint16_t s_press_ticks = 0;
static uint16_t s_gap_ticks = 0;
static uint8_t  s_click_count = 0;
static uint8_t  s_long_triggered = 0;
static uint8_t  s_aux_last_state = 0;

static void BSP_Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA0 (主功能按键, 接 3.0V, 外接 100k 下拉至 GND, 默认低电平, 按下高电平)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA6 (备用战术点动按键, 开启内部上拉, 按下一端接 GND, 按下低电平)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

uint8_t BSP_MainKey_IsPressed(void)
{
    return (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_SET);
}

uint8_t BSP_AuxKey_IsPressed(void)
{
    return (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == Bit_RESET);
}

Key_Event_e BSP_Key_Scan_10ms(void)
{
    Key_Event_e event = KEY_EVT_NONE;
    uint8_t is_down = BSP_MainKey_IsPressed();

    if (is_down)
    {
        s_press_ticks++;
        s_gap_ticks = 0;

        if (s_press_ticks >= KEY_LONG_PRESS_TICKS && !s_long_triggered)
        {
            s_long_triggered = 1;
            s_click_count = 0;
            return KEY_EVT_LONG_PRESS;
        }
    }
    else
    {
        if (s_press_ticks > 2 && s_press_ticks < KEY_SHORT_CLICK_MAX_TICKS && !s_long_triggered)
        {
            s_click_count++;
            s_gap_ticks = 0;
        }

        s_press_ticks = 0;
        s_long_triggered = 0;

        if (s_click_count > 0)
        {
            s_gap_ticks++;
            if (s_gap_ticks >= KEY_MULTI_CLICK_GAP_TICKS)
            {
                if (s_click_count == 1)      event = KEY_EVT_SINGLE_CLICK;
                else if (s_click_count == 2) event = KEY_EVT_DOUBLE_CLICK;
                else if (s_click_count >= 3) event = KEY_EVT_TRIPLE_CLICK;

                s_click_count = 0;
                s_gap_ticks = 0;
            }
        }
    }

    return event;
}

AuxKey_Event_e BSP_AuxKey_Scan_10ms(void)
{
    AuxKey_Event_e evt = AUX_KEY_EVT_NONE;
    uint8_t cur_down = BSP_AuxKey_IsPressed();

    if (cur_down && !s_aux_last_state)
    {
        evt = AUX_KEY_EVT_PRESS;
    }
    else if (!cur_down && s_aux_last_state)
    {
        evt = AUX_KEY_EVT_RELEASE;
    }
    s_aux_last_state = cur_down;
    return evt;
}

/* -------------------------------------------------------------------------- */
/* 5. 电源待机管理与 ISP 固件跳转                                             */
/* -------------------------------------------------------------------------- */
void BSP_EnterStandby(void)
{
    PRINT("[PWR] Entering Ultra-Low Power Standby...\r\n");

    // 1. 关闭 USB 虚拟串口控制器并拔掉 DP 内部上拉电阻 (彻底消除毫安级漏电)
    USB_CDC_DeInit();

    // 2. 关闭 PWM 输出
    BSP_PWM_AllOff();

    // 3. 关闭 RGB 指示灯
    BSP_StatusLED_AllOff();

    // 4. 关闭 ADC 转换与外设时钟
    ADC_Cmd(ADC1, DISABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, DISABLE);

    // 5. 等待主键松手防误唤醒
    while (BSP_MainKey_IsPressed())
    {
        Delay_Ms(10);
    }
    Delay_Ms(50); // 防抖消除

    // 6. 开启电源管理时钟，使能 WKUP (PA0) 硬件高电平唤醒
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_WakeUpPinCmd(ENABLE);

    // 7. 清除唤醒标志，进入 Standby 深度休眠模式 (~2uA)
    PWR_ClearFlag(PWR_FLAG_WU);
    PWR_EnterSTANDBYMode();
}

#define BOOTLOADER_MAGIC_ADDR   ((volatile uint32_t *)0x20000FF0)
#define BOOTLOADER_MAGIC_KEY    0x57434821 // "WCH!"

void BSP_JumpToBootloader(void)
{
    PRINT("[ISP] Software jump to factory Bootloader!\r\n");

    // 1. 安全卸载 USB
    USB_CDC_DeInit();

    // 2. 写入跳转魔数
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    *BOOTLOADER_MAGIC_ADDR = BOOTLOADER_MAGIC_KEY;

    // 3. 产生软件系统复位，芯片 Bootloader 将拦截并进入 USB 烧录模式
    Delay_Ms(20);
    NVIC_SystemReset();
}

/* -------------------------------------------------------------------------- */
/* 6. 板级初始化入口                                                          */
/* -------------------------------------------------------------------------- */
void BSP_Init(void)
{
    BSP_Key_Init();
    BSP_PWM_Init();
    BSP_RGB_Init();
    BSP_ADC_Init();

    BSP_PWM_AllOff();
    BSP_StatusLED_AllOff();
    PRINT("[BSP] Unified Hardware Init Done.\r\n");
}
