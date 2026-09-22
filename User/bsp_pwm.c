#include "bsp_pwm.h"

/* 动态保存自动匹配 20kHz 频率的重装载值 ARR */
static uint32_t s_pwm_arr = 999;

void BSP_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    // 使能 GPIOA 和 TIM2 挂载的总线时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 配置 PA2 (TIM2_CH3, 主灯 3535 WLED) 和 PA3 (TIM2_CH4, 红光激光二极管 650nm) 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*
     * 精确锁定 20.00kHz 硬件高频 PWM (直驱 TPS92201DRVR 的 EN/PWM 调光端):
     * f_PWM = SystemCoreClock / ((Prescaler + 1) * (ARR + 1)) = 20000 Hz
     * 72MHz 时: Prescaler = 0, ARR = 3599 -> f = 20.000kHz (误差 0.00%)
     * 96MHz 时: Prescaler = 0, ARR = 4799 -> f = 20.000kHz (误差 0.00%)
     */
    uint32_t total_div = SystemCoreClock / 20000UL;
    uint32_t prescaler = 0;
    uint32_t arr = 0;

    if (total_div <= 65536)
    {
        prescaler = 0;
        arr = total_div - 1;
    }
    else
    {
        prescaler = (total_div / 65536);
        arr = (total_div / (prescaler + 1)) - 1;
    }

    s_pwm_arr = arr;

    TIM_TimeBaseStructure.TIM_Period = (uint16_t)arr;
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 配置通道 3 (PA2) 和通道 4 (PA3) 为 PWM1 模式 (高电平有效)
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

    BSP_PWM_AllOff();
}

/**
 * @brief 设置主灯 3535 WLED (PA2 / TIM2_CH3) PWM 占空比
 * @param duty: 0 ~ 1000 (对应 0.0% ~ 100.0%)
 */
void BSP_PWM_SetDuty_PWM1(uint16_t duty)
{
    if (duty > PWM_DUTY_MAX) duty = PWM_DUTY_MAX;
    uint32_t pulse = ((uint32_t)duty * s_pwm_arr) / PWM_DUTY_MAX;
    TIM_SetCompare3(TIM2, (uint16_t)pulse);
}

/**
 * @brief 设置红光瞄准激光二极管 650nm ~3mW (PA3 / TIM2_CH4) PWM 占空比
 * @param duty: 0 ~ 1000 (对应 0.0% ~ 100.0%)
 */
void BSP_PWM_SetDuty_PWM2(uint16_t duty)
{
    if (duty > PWM_DUTY_MAX) duty = PWM_DUTY_MAX;
    uint32_t pulse = ((uint32_t)duty * s_pwm_arr) / PWM_DUTY_MAX;
    TIM_SetCompare4(TIM2, (uint16_t)pulse);
}

/**
 * @brief 立即关闭所有 PWM 输出 (直拉低电平，关闭驱动 IC)
 */
void BSP_PWM_AllOff(void)
{
    TIM_SetCompare3(TIM2, 0);
    TIM_SetCompare4(TIM2, 0);
}
