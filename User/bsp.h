/********************************** (C) COPYRIGHT *******************************
 * File Name          : bsp.h
 * Author             : Weapon Systems Lab
 * Version            : V3.0.0 (Unified All-in-One BSP)
 * Description        : 战术枪灯一体化板级支持包 (按键/PWM/ADC/RGB/电源/ISP)
 *******************************************************************************/
#ifndef __BSP_H
#define __BSP_H

#include "ch32v20x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 按键事件枚举 */
typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_SINGLE_CLICK,   // 单击
    KEY_EVT_DOUBLE_CLICK,   // 双击
    KEY_EVT_TRIPLE_CLICK,   // 三击
    KEY_EVT_LONG_PRESS,     // 长按 (1.2s)
} Key_Event_e;

/* 备用按键点动事件 */
typedef enum {
    AUX_KEY_EVT_NONE = 0,
    AUX_KEY_EVT_PRESS,      // 按下 (战术常亮开始)
    AUX_KEY_EVT_RELEASE     // 松开 (战术点亮结束)
} AuxKey_Event_e;

/* RGB 指示灯动画模式 */
typedef enum {
    STATUS_LED_OFF = 0,
    STATUS_LED_BAT_HIGH,    // 绿灯常亮 (>= 3.6V)
    STATUS_LED_BAT_MED,     // 蓝灯常亮 (3.4V ~ 3.6V)
    STATUS_LED_BAT_LOW,     // 黄灯慢闪 (3.1V ~ 3.4V)
    STATUS_LED_BAT_CRITICAL,// 红灯快闪 (2.95V ~ 3.1V)
    STATUS_LED_STROBE,      // 青蓝爆闪提示
    STATUS_LED_SOS          // 紫色 SOS 莫尔斯同步
} Status_LED_Mode_e;

/* BSP 一体化接口 */
void            BSP_Init(void);

// 1. 按键与战术操作
Key_Event_e     BSP_Key_Scan_10ms(void);
AuxKey_Event_e  BSP_AuxKey_Scan_10ms(void);
uint8_t         BSP_MainKey_IsPressed(void);
uint8_t         BSP_AuxKey_IsPressed(void);

// 2. 双路 20kHz 硬件高频 PWM
void            BSP_PWM_SetDuty_PWM1(uint16_t duty); // 0~1000 ‰
void            BSP_PWM_SetDuty_PWM2(uint16_t duty); // 0~1000 ‰
void            BSP_PWM_AllOff(void);

// 3. 单通道 ADC 电池电压
uint16_t        BSP_ADC_GetBatteryVoltage_mV(void);

// 4. 单线全彩 RGB 指示灯 (XL-1615RGBC-RF)
void            BSP_StatusLED_SetMode(Status_LED_Mode_e mode);
void            BSP_StatusLED_SetColor(uint8_t r, uint8_t g, uint8_t b);
void            BSP_StatusLED_AllOff(void);
void            BSP_StatusLED_Process_10ms(void);

// 5. 电源管理与系统待机
void            BSP_EnterStandby(void);
void            BSP_JumpToBootloader(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_H */
