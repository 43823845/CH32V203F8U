#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "ch32v20x.h"

/*
 * 单线 RGB 指示灯 (XL-1615RGBC-RF / 内置 WS2812B 驱动协议):
 * 仅占用一个 GPIO (PA4) 作为单线高速归零码通信 (DIN)
 * 原 PA5 (Pin 6) 释放为普通备用 GPIO
 */
#define LED_RGB_PORT        GPIOA
#define LED_RGB_PIN         GPIO_Pin_4

typedef enum {
    STATUS_LED_OFF = 0,
    STATUS_LED_BAT_HIGH,        // 绿灯常亮 (电量充足 >= 3.6V)
    STATUS_LED_BAT_MED,         // 蓝灯常亮 (电量良好 3.4V ~ 3.6V)
    STATUS_LED_BAT_LOW,         // 黄/橙灯慢闪 (1Hz，低电提醒 3.1V ~ 3.4V)
    STATUS_LED_BAT_CRITICAL,    // 红灯快闪 (4Hz，严重缺电警告 2.95V ~ 3.1V)
    STATUS_LED_STROBE,          // 冰蓝/青色频闪 (战术爆闪模式专属指示)
    STATUS_LED_SOS,             // 紫色莫尔斯同步闪烁 (SOS求救模式专属指示)
    STATUS_LED_ISP,             // 金黄色常亮 (进入 ISP 烧录模式指示)

    // 向下兼容旧枚举名
    STATUS_LED_GREEN_ON        = STATUS_LED_BAT_HIGH,
    STATUS_LED_RED_SLOW_BLINK  = STATUS_LED_BAT_LOW,
    STATUS_LED_RED_FAST_BLINK  = STATUS_LED_BAT_CRITICAL,
    STATUS_LED_ISP_INDICATE    = STATUS_LED_ISP
} StatusLed_Mode_e;

void BSP_StatusLED_Init(void);
void BSP_StatusLED_SetColor(uint8_t r, uint8_t g, uint8_t b);
void BSP_StatusLED_SetMode(StatusLed_Mode_e mode);
void BSP_StatusLED_Process_10ms(void);
void BSP_StatusLED_AllOff(void);

#endif /* __BSP_LED_H */
