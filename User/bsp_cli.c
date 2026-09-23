#include "bsp_cli.h"
#include "usb_cdc.h"
#include "gunlight.h"
#include "bsp_pwm.h"
#include "bsp_isp.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define CLI_LINE_MAX_LEN    80

static char s_cli_line[CLI_LINE_MAX_LEN];
static uint8_t s_cli_len = 0;

void BSP_CLI_Init(void)
{
    s_cli_len = 0;
    memset(s_cli_line, 0, sizeof(s_cli_line));
}

static void CLI_ExecuteCommand(char *cmd)
{
    // 转为大写以便兼容大小写
    char cmd_upper[CLI_LINE_MAX_LEN];
    uint8_t i = 0;
    while (cmd[i] && i < CLI_LINE_MAX_LEN - 1)
    {
        cmd_upper[i] = (char)toupper((unsigned char)cmd[i]);
        i++;
    }
    cmd_upper[i] = '\0';

    if (strcmp(cmd_upper, "HELP") == 0)
    {
        printf("\r\n=== CH32V203 Tactical WML Debug CLI ===\r\n");
        printf("STATUS            - Query system status in JSON\r\n");
        printf("SET PWM1 <0-1000> - Set 3535 WLED brightness\r\n");
        printf("SET PWM2 <0-1000> - Set 650nm Laser brightness\r\n");
        printf("SET MODE <0-5>    - 0:OFF 1:100%% 2:25%% 3:DUAL 4:STROBE 5:SOS\r\n");
        printf("SET TIMEOUT <sec> - Set auto-standby timeout\r\n");
        printf("ISP               - Jump to factory USB Bootloader\r\n");
        printf("REBOOT            - Soft system reset\r\n");
        printf("========================================\r\n");
    }
    else if (strcmp(cmd_upper, "STATUS") == 0 || strcmp(cmd_upper, "GET STATUS") == 0)
    {
        uint16_t bat_mv = Gunlight_GetBatteryVoltage_mV();
        uint8_t tier = Gunlight_GetBatteryTier();
        Gunlight_State_e state = Gunlight_GetState();
        uint16_t timeout = Gunlight_GetStandbyTimeoutSec();
        uint16_t pwm1 = Gunlight_GetPwm1Duty();
        uint16_t pwm2 = Gunlight_GetPwm2Duty();

        printf("{\"bat_mv\":%d,\"tier\":%d,\"state\":%d,\"pwm1\":%d,\"pwm2\":%d,\"timeout\":%d,\"sysclk\":%d}\r\n",
               (int)bat_mv, (int)tier, (int)state, (int)pwm1, (int)pwm2, (int)timeout, (int)SystemCoreClock);
    }
    else if (strncmp(cmd_upper, "SET PWM1 ", 9) == 0)
    {
        int val = atoi(&cmd_upper[9]);
        if (val < 0) val = 0;
        if (val > 1000) val = 1000;
        BSP_PWM_SetDuty_PWM1((uint16_t)val);
        printf("OK: PWM1 set to %d\r\n", val);
    }
    else if (strncmp(cmd_upper, "SET PWM2 ", 9) == 0)
    {
        int val = atoi(&cmd_upper[9]);
        if (val < 0) val = 0;
        if (val > 1000) val = 1000;
        BSP_PWM_SetDuty_PWM2((uint16_t)val);
        printf("OK: PWM2 set to %d\r\n", val);
    }
    else if (strncmp(cmd_upper, "SET MODE ", 9) == 0)
    {
        int mode = atoi(&cmd_upper[9]);
        if (mode >= 0 && mode <= 5)
        {
            Gunlight_SetState((Gunlight_State_e)mode);
            printf("OK: MODE set to %d\r\n", mode);
        }
        else
        {
            printf("ERR: Invalid mode (0~5)\r\n");
        }
    }
    else if (strncmp(cmd_upper, "SET TIMEOUT ", 12) == 0)
    {
        int sec = atoi(&cmd_upper[12]);
        if (sec >= 1 && sec <= 3600)
        {
            Gunlight_SetStandbyTimeoutSec((uint16_t)sec);
            printf("OK: TIMEOUT set to %d seconds\r\n", sec);
        }
        else
        {
            printf("ERR: Timeout must be 1~3600 seconds\r\n");
        }
    }
    else if (strcmp(cmd_upper, "ISP") == 0 || strcmp(cmd_upper, "BOOTLOADER") == 0)
    {
        printf("OK: Jumping to USB ISP Bootloader in 50ms...\r\n");
        Delay_Ms(50);
        BSP_ISP_JumpToBootloader();
    }
    else if (strcmp(cmd_upper, "REBOOT") == 0 || strcmp(cmd_upper, "RESET") == 0)
    {
        printf("OK: Rebooting...\r\n");
        Delay_Ms(50);
        NVIC_SystemReset();
    }
    else
    {
        printf("ERR: Unknown command '%s'. Type 'HELP' for commands.\r\n", cmd);
    }
}

void BSP_CLI_Process(void)
{
    while (USB_CDC_Available() > 0)
    {
        int16_t ch = USB_CDC_ReadByte();
        if (ch < 0) break;

        if (ch == '\r' || ch == '\n')
        {
            if (s_cli_len > 0)
            {
                s_cli_line[s_cli_len] = '\0';
                CLI_ExecuteCommand(s_cli_line);
                s_cli_len = 0;
            }
        }
        else if (ch == '\b' || ch == 0x7F) // 退格
        {
            if (s_cli_len > 0)
            {
                s_cli_len--;
            }
        }
        else
        {
            if (s_cli_len < CLI_LINE_MAX_LEN - 1)
            {
                s_cli_line[s_cli_len++] = (char)ch;
            }
        }
    }
}
