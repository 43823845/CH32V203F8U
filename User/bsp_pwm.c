#include "bsp_pwm.h"

void BSP_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    // 使能 GPIOA 和 TIM2 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 配置 PA2 (TIM2_CH3) 和 PA3 (TIM2_CH4) 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*
     * 设置定时器频率约 20kHz
     * Period = 1000 - 1
     * Prescaler = SystemCoreClock / (20000 * 1000) - 1
     */
    uint32_t prescaler = (SystemCoreClock / 20000000);
    if (prescaler > 0) prescaler -= 1;
    else prescaler = 0;

    TIM_TimeBaseStructure.TIM_Period = PWM_DUTY_MAX - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 配置通道 3 和 4 为 PWM 模式 1
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

void BSP_PWM_SetDuty_PWM1(uint16_t duty)
{
    if (duty > PWM_DUTY_MAX) duty = PWM_DUTY_MAX;
    TIM_SetCompare3(TIM2, duty);
}

void BSP_PWM_SetDuty_PWM2(uint16_t duty)
{
    if (duty > PWM_DUTY_MAX) duty = PWM_DUTY_MAX;
    TIM_SetCompare4(TIM2, duty);
}

void BSP_PWM_AllOff(void)
{
    TIM_SetCompare3(TIM2, 0);
    TIM_SetCompare4(TIM2, 0);
}
