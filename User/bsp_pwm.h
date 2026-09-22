#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "ch32v20x.h"

/*
 * PWM 引脚映射：
 * PWM1: PA2 (TIM2_CH3) -> 主 WLED 驱动调光端
 * PWM2: PA3 (TIM2_CH4) -> 副 WLED 驱动调光端
 * 频率: ~20kHz (无啸叫、无频闪)
 * 占空比分辨率: 0 ~ 1000 (对应 0% ~ 100.0%)
 */

#define PWM_DUTY_MAX    1000

void BSP_PWM_Init(void);
void BSP_PWM_SetDuty_PWM1(uint16_t duty); // 0 ~ 1000
void BSP_PWM_SetDuty_PWM2(uint16_t duty); // 0 ~ 1000
void BSP_PWM_AllOff(void);

#endif /* __BSP_PWM_H */
