/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : Weapon Systems Lab
 * Version            : V3.0.0 (Unified Architecture)
 * Description        : 战术枪灯主程序入口 (CH32V203F8U6)
 *******************************************************************************/
#include "debug.h"
#include "bsp.h"
#include "usb_cdc.h"
#include "gunlight.h"

int main(void)
{
    // 1. 系统核心时钟与基础外设初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);

    PRINT("\r\n========================================\r\n");
    PRINT("  CH32V203 Tactical Gunlight (WML) V3.0 \r\n");
    PRINT("  System Clock : %d Hz\r\n", (int)SystemCoreClock);
    PRINT("  USBD CDC VCP : Ready (PA11:DM, PA12:DP)\r\n");
    PRINT("========================================\r\n");

    // 2. 检查 Standby 待机唤醒标志
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    if (PWR_GetFlagStatus(PWR_FLAG_WU) != RESET)
    {
        PWR_ClearFlag(PWR_FLAG_WU);
        PRINT("[PWR] Wakeup from Standby Mode!\r\n");
    }

    // 3. 一体化板级硬件与 USB 虚拟串口初始化
    BSP_Init();
    USB_CDC_Init();
    Gunlight_Init();

    /*
     * 开机与唤醒逻辑:
     * 1. 若按键被按下 (无论冷上电按住还是从 Standby 唤醒):
     *    检测按压时长: 持续按住达 8 秒 -> 跳转 ISP 烧录模式！8秒内松手 -> 点亮模式 1。
     * 2. 若冷上电 (换电池或插 USB) 且未按按键:
     *    保持在 GL_STATE_OFF 待命状态，若 10 秒内无操作无 USB 通信，则自动休眠进 Standby。
     */
    if (BSP_MainKey_IsPressed())
    {
        PRINT("[PWR] Key Pressed: Checking 8s hold for ISP...\r\n");
        uint16_t hold_cnt = 0;
        while (BSP_MainKey_IsPressed())
        {
            Delay_Ms(10);
            hold_cnt++;
            if (hold_cnt >= 800) // 800 * 10ms = 8000ms = 8 秒
            {
                PRINT("[PWR] 8 seconds reached -> Jump to ISP Bootloader!\r\n");
                BSP_JumpToBootloader();
            }
        }
        // 8秒内松开，正常开机点亮主灯
        Gunlight_TurnOn_Mode1();
    }
    else
    {
        PRINT("[PWR] Standby Standby State (Auto sleep after timeout or ready for host)\r\n");
    }

    // 4. 10ms 主调度循环
    while (1)
    {
        Gunlight_Process_10ms();
        Delay_Ms(10);
    }
}
