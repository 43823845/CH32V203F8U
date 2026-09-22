#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "ch32v20x.h"

/*
 * PWM 引脚映射 (直驱 TI TPS92201DRVR 同步降压恒流 IC 的 EN/PWM 调光端):
 * PWM1: PA2 (TIM2_CH3) -> 主 WLED 驱动调光端
 * PWM2: PA3 (TIM2_CH4) -> 副 WLED 驱动调光端
 * 频率: 精确 20.00kHz 硬件高频 PWM (超声频段，无任何啸叫与人眼频闪)
 * 占空比分辨率: 0 ~ 1000 (对应 0.0% ~ 100.0%)
 */

#define PWM_DUTY_MAX    1000

void BSP_PWM_Init(void);
void BSP_PWM_SetDuty_PWM1(uint16_t duty); // 0 ~ 1000
void BSP_PWM_SetDuty_PWM2(uint16_t duty); // 0 ~ 1000
void BSP_PWM_AllOff(void);

#endif /* __BSP_PWM_H */
