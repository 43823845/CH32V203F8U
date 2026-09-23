/********************************** (C) COPYRIGHT *******************************
 * File Name          : gunlight.h
 * Author             : Weapon Systems Lab
 * Version            : V3.0.0 (Unified Architecture)
 * Description        : 战术枪灯业务逻辑状态机与调参管理
 *******************************************************************************/
#ifndef __GUNLIGHT_H
#define __GUNLIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 枪灯工作模式枚举 */
typedef enum {
    GL_STATE_OFF = 0,       // 模式 0: 全熄关灯 (进入 10s 休眠倒计时)
    GL_STATE_MODE1_100,     // 模式 1: 主灯 100% 满功率照明 (500~1000LX)
    GL_STATE_MODE2_25,      // 模式 2: 主灯 25% 节能微光档
    GL_STATE_MODE3_DUAL,    // 模式 3: 主灯照明 + 650nm 红光瞄准激光双开
    GL_STATE_STROBE,        // 模式 4: 10Hz 战术爆闪压制
    GL_STATE_SOS            // 模式 5: 国际标准 SOS 莫尔斯求救
} Gunlight_State_e;

/* 枪灯业务 API */
void                Gunlight_Init(void);
void                Gunlight_Process_10ms(void);
void                Gunlight_TurnOn_Mode1(void);

// 状态与调参 Getter / Setter
Gunlight_State_e    Gunlight_GetState(void);
void                Gunlight_SetState(Gunlight_State_e state);
uint16_t            Gunlight_GetBatteryVoltage_mV(void);
uint8_t             Gunlight_GetBatteryTier(void);
uint16_t            Gunlight_GetStandbyTimeoutSec(void);
void                Gunlight_SetStandbyTimeoutSec(uint16_t sec);
uint16_t            Gunlight_GetPwm1Duty(void);
uint16_t            Gunlight_GetPwm2Duty(void);
void                Gunlight_SetPwm1Duty(uint16_t duty);
void                Gunlight_SetPwm2Duty(uint16_t duty);

#ifdef __cplusplus
}
#endif

#endif /* __GUNLIGHT_H */
