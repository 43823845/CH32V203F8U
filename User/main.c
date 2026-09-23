/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : 枪灯下挂主控固件 (CH32V203F8U6)
 *******************************************************************************/

#include "debug.h"
#include "gunlight.h"
#include "bsp_key.h"
#include "bsp_isp.h"
#include "usb_cdc.h"
#include "bsp_cli.h"

int main(void)
{
    // 1. 系统核心时钟与中断初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);

    // 2. 初始化 USB 虚拟串口 (CDC ACM) 与 CLI 调试命令行
    USB_CDC_Init();
    BSP_CLI_Init();

    PRINT("\r\n========================================\r\n");
    PRINT("  CH32V203F8U6 Tactical Gunlight (WML)  \r\n");
    PRINT("  System Clock : %d Hz\r\n", (int)SystemCoreClock);
    PRINT("  USB CDC VCP  : Ready (PA11:DM, PA12:DP)\r\n");
    PRINT("========================================\r\n");

    // 3. 检查唤醒标志
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    if (PWR_GetFlagStatus(PWR_FLAG_WU) != RESET)
    {
        PWR_ClearFlag(PWR_FLAG_WU);
        PRINT("[PWR] Wakeup from Low-Power Standby!\r\n");
    }

    // 4. 枪灯外设与状态机初始化
    Gunlight_Init();

    /*
     * 关机上电与待机唤醒逻辑:
     * 1. 若按键被按下 (无论冷上电按住还是从 Standby 待机模式按键唤醒):
     *    检测按键持续按压时长:
     *    - 持续按住达到 8 秒 (800 * 10ms = 8000ms): 判定为进入 ISP 烧录模式！
     *    - 8 秒内松手 (正常按键唤醒/开机): 立即点亮模式 1 (100% 满功率)，进入正常按钮控制！
     * 2. 若冷上电 (初次装入电池或插 Type-C) 且未按按键:
     *    保持在 GL_STATE_OFF 关灯待命状态，若 10 秒内无操作无 USB 通信，则自动休眠进 Standby。
     */
    if (BSP_MainKey_IsPressed())
    {
        PRINT("[PWR] Key Pressed on Wakeup/Boot: Checking 8s hold for ISP...\r\n");
        uint16_t hold_cnt = 0;
        while (BSP_MainKey_IsPressed())
        {
            Delay_Ms(10);
            hold_cnt++;
            if (hold_cnt >= 800) // 800 * 10ms = 8000ms = 8 秒！
            {
                PRINT("[PWR] 8 seconds hold reached -> Entering ISP Bootloader!\r\n");
                BSP_ISP_JumpToBootloader();
            }
        }
        // 8秒内松手，确认为正常开机，点亮主灯！
        PRINT("[PWR] Normal Turn On -> Mode 1 (100%%).\r\n");
        Gunlight_TurnOn_Mode1();
    }
    else
    {
        PRINT("[PWR] Boot in Standby Wait State (Auto sleep after timeout or ready for USB host)\r\n");
    }

    // 4. 10ms 主调度循环
    while(1)
    {
        Gunlight_Process_10ms();
        Delay_Ms(10);
    }
}
