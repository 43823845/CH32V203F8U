#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "ch32v20x.h"

/*
 * ADC 引脚与通道定义：
 * PA1 -> ADC1 通道 1 (ADC_Channel_1)
 * 硬件分压：1:1 分压电路 (例如 470kΩ + 470kΩ)
 * 满电 4.2V -> ADC输入 2.1V
 * 标称 3.7V -> ADC输入 1.85V
 * 截止 3.0V -> ADC输入 1.5V
 */

void BSP_ADC_Init(void);
void BSP_ADC_DeInit(void);
uint16_t BSP_ADC_GetBatteryVoltage_mV(void);

#endif /* __BSP_ADC_H */
