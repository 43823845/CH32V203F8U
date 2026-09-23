#include "usb_cdc.h"
#include "debug.h"
#include <string.h>

/* DMA 端点数据缓冲区 (必须 4 字节对齐) */
__attribute__((aligned(4))) static uint8_t s_ep0_buf[64];
__attribute__((aligned(4))) static uint8_t s_ep1_buf[USB_CDC_CMD_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t s_ep2_buf[USB_CDC_DATA_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t s_ep3_buf[USB_CDC_DATA_PACKET_SIZE];

/* CDC Line Coding (波特率 115200, 1 停止位, 无校验, 8 数据位) */
static uint8_t s_line_coding[7] = {
    0x00, 0xC2, 0x01, 0x00, // 115200 bps
    0x00,                   // 1 Stop bit
    0x00,                   // None parity
    0x08                    // 8 Data bits
};

/* 设备状态与控制变量 */
static volatile uint8_t s_dev_addr = 0;
static volatile uint8_t s_dev_config = 0;
static volatile uint8_t s_dev_addr_pending = 0;
static volatile uint8_t s_ep2_tx_busy = 0;

static const uint8_t *s_setup_tx_ptr = NULL;
static volatile uint16_t s_setup_tx_len = 0;

/* 接收环形缓冲区 */
static uint8_t s_rx_buf[USB_CDC_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

/* 发送环形缓冲区 */
static uint8_t s_tx_buf[USB_CDC_TX_BUF_SIZE];
static volatile uint16_t s_tx_head = 0;
static volatile uint16_t s_tx_tail = 0;

static void USB_CDC_TriggerNextTx(void);

void USB_CDC_Init(void)
{
    // 1. 配置 48MHz USB 时钟 (系统 96MHz 时 2 分频 = 48MHz)
    RCC_USBCLKConfig(RCC_USBPLL_Div2);

    // 2. 使能 USBFS 外设时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, ENABLE);

    // 3. 复位 USB 串行接口引擎 (SIE)
    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    Delay_Us(10);
    USBFSD->BASE_CTRL = 0x00;

    // 4. 配置端点 DMA 物理地址
    USBFSD->UEP0_DMA = (uint32_t)s_ep0_buf;
    USBFSD->UEP1_DMA = (uint32_t)s_ep1_buf;
    USBFSD->UEP2_DMA = (uint32_t)s_ep2_buf;
    USBFSD->UEP3_DMA = (uint32_t)s_ep3_buf;

    // 5. 配置端点模式 (EP1: IN 中断, EP2: IN 批量, EP3: OUT 批量)
    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_TX_EN | USBFS_UEP3_RX_EN;

    // 6. 配置端点响应初始状态
    USBFSD->UEP0_TX_LEN  = 0;
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;

    USBFSD->UEP1_TX_LEN  = 0;
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_T_AUTO_TOG;

    USBFSD->UEP2_TX_LEN  = 0;
    USBFSD->UEP2_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_T_AUTO_TOG;

    USBFSD->UEP3_RX_CTRL = USBFS_UEP_R_RES_ACK | USBFS_UEP_R_AUTO_TOG;

    // 7. 配置设备地址为 0
    USBFSD->DEV_ADDR = 0x00;

    // 8. 使能 USB 端口与 DP 内部 1.5k 上拉电阻
    USBFSD->UDEV_CTRL = USBFS_UD_PORT_EN | USBFS_UD_PD_DIS;
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_DMA_EN | USBFS_UC_INT_BUSY;

    // 9. 清除所有中断挂起并开启中断
    USBFSD->INT_FG = 0xFF;
    USBFSD->INT_EN = USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER | USBFS_UIE_SUSPEND;

    // 10. 注册并使能 PFIC/NVIC 中断
    NVIC_SetPriority(USBFS_IRQn, 0);
    NVIC_EnableIRQ(USBFS_IRQn);

    s_dev_addr = 0;
    s_dev_config = 0;
    s_dev_addr_pending = 0;
    s_ep2_tx_busy = 0;
    s_rx_head = 0;
    s_rx_tail = 0;
    s_tx_head = 0;
    s_tx_tail = 0;
}

void USB_CDC_DeInit(void)
{
    NVIC_DisableIRQ(USBFS_IRQn);
    USBFSD->INT_EN = 0;
    USBFSD->BASE_CTRL = 0;
    USBFSD->UDEV_CTRL = 0;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, DISABLE);

    s_dev_config = 0;
    s_ep2_tx_busy = 0;
}

uint8_t USB_CDC_IsConfigured(void)
{
    return (s_dev_config != 0);
}

uint32_t USB_CDC_Available(void)
{
    uint16_t head = s_rx_head;
    uint16_t tail = s_rx_tail;
    if (head >= tail)
    {
        return head - tail;
    }
    return (USB_CDC_RX_BUF_SIZE - tail) + head;
}

int16_t USB_CDC_ReadByte(void)
{
    if (s_rx_head == s_rx_tail)
    {
        return -1;
    }
    uint8_t ch = s_rx_buf[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) % USB_CDC_RX_BUF_SIZE;
    return (int16_t)ch;
}

uint32_t USB_CDC_ReadBytes(uint8_t *buf, uint32_t max_len)
{
    uint32_t count = 0;
    while (count < max_len)
    {
        int16_t ch = USB_CDC_ReadByte();
        if (ch < 0) break;
        buf[count++] = (uint8_t)ch;
    }
    return count;
}

static void USB_CDC_TriggerNextTx(void)
{
    if (s_ep2_tx_busy) return;
    if (s_tx_head == s_tx_tail) return;
    if (!USB_CDC_IsConfigured()) return;

    uint16_t len = 0;
    while (len < USB_CDC_DATA_PACKET_SIZE && s_tx_tail != s_tx_head)
    {
        s_ep2_buf[len++] = s_tx_buf[s_tx_tail];
        s_tx_tail = (s_tx_tail + 1) % USB_CDC_TX_BUF_SIZE;
    }

    if (len > 0)
    {
        s_ep2_tx_busy = 1;
        USBFSD->UEP2_TX_LEN = len;
        USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
    }
}

uint32_t USB_CDC_SendBytes(const uint8_t *data, uint32_t len)
{
    if (!USB_CDC_IsConfigured() || len == 0) return 0;

    uint32_t sent = 0;
    for (uint32_t i = 0; i < len; i++)
    {
        uint16_t next_head = (s_tx_head + 1) % USB_CDC_TX_BUF_SIZE;
        if (next_head == s_tx_tail)
        {
            // 缓冲区满，尝试立即触发一次发送以腾出空间
            __disable_irq();
            USB_CDC_TriggerNextTx();
            __enable_irq();
            break;
        }
        s_tx_buf[s_tx_head] = data[i];
        s_tx_head = next_head;
        sent++;
    }

    __disable_irq();
    USB_CDC_TriggerNextTx();
    __enable_irq();

    return sent;
}

/* 控制端点 0 SETUP 请求解析 */
static void USB_CDC_HandleEP0_Setup(void)
{
    USB_SETUP_REQ *req = (USB_SETUP_REQ *)s_ep0_buf;
    uint8_t req_type = req->bRequestType & USB_REQ_TYP_MASK;
    uint16_t len = 0;
    const uint8_t *p_des = NULL;

    s_setup_tx_ptr = NULL;
    s_setup_tx_len = 0;

    if (req_type == USB_REQ_TYP_STANDARD)
    {
        switch (req->bRequest)
        {
            case USB_GET_DESCRIPTOR:
            {
                uint8_t desc_type = (uint8_t)(req->wValue >> 8);
                uint8_t desc_idx  = (uint8_t)(req->wValue & 0xFF);

                switch (desc_type)
                {
                    case USB_DEVICE_DESCRIPTOR_TYPE:
                        p_des = USB_DeviceDescriptor;
                        len = sizeof(USB_DeviceDescriptor);
                        break;

                    case USB_CONFIGURATION_DESCRIPTOR_TYPE:
                        p_des = USB_ConfigDescriptor;
                        len = sizeof(USB_ConfigDescriptor);
                        break;

                    case USB_STRING_DESCRIPTOR_TYPE:
                        if (desc_idx == 0)
                        {
                            p_des = USB_StringLangID;
                            len = sizeof(USB_StringLangID);
                        }
                        else if (desc_idx == 1)
                        {
                            p_des = USB_StringVendor;
                            len = sizeof(USB_StringVendor);
                        }
                        else if (desc_idx == 2)
                        {
                            p_des = USB_StringProduct;
                            len = sizeof(USB_StringProduct);
                        }
                        else if (desc_idx == 3)
                        {
                            p_des = USB_StringSerial;
                            len = sizeof(USB_StringSerial);
                        }
                        break;

                    default:
                        break;
                }
                break;
            }

            case USB_SET_ADDRESS:
                s_dev_addr = (uint8_t)(req->wValue & 0x7F);
                s_dev_addr_pending = 1;
                len = 0; // 0 字节 Status 阶段
                break;

            case USB_GET_CONFIGURATION:
                s_ep0_buf[0] = s_dev_config;
                p_des = s_ep0_buf;
                len = 1;
                break;

            case USB_SET_CONFIGURATION:
                s_dev_config = (uint8_t)(req->wValue & 0xFF);
                len = 0;
                break;

            case USB_CLEAR_FEATURE:
            case USB_SET_FEATURE:
                len = 0;
                break;

            case USB_GET_STATUS:
                s_ep0_buf[0] = 0x00;
                s_ep0_buf[1] = 0x00;
                p_des = s_ep0_buf;
                len = 2;
                break;

            default:
                break;
        }
    }
    else if (req_type == USB_REQ_TYP_CLASS)
    {
        // CDC 类特定请求
        switch (req->bRequest)
        {
            case 0x20: // SET_LINE_CODING
                len = 0; // 在 OUT 阶段接收 7 字节
                break;

            case 0x21: // GET_LINE_CODING
                p_des = s_line_coding;
                len = sizeof(s_line_coding);
                break;

            case 0x22: // SET_CONTROL_LINE_STATE
                len = 0; // 无数据阶段，直接 ACK
                break;

            default:
                break;
        }
    }

    if (p_des != NULL || len == 0)
    {
        if (len > req->wLength)
        {
            len = req->wLength;
        }

        uint16_t send_len = len;
        if (send_len > 64) send_len = 64;

        if (p_des && p_des != s_ep0_buf)
        {
            memcpy(s_ep0_buf, p_des, send_len);
            s_setup_tx_ptr = p_des + send_len;
            s_setup_tx_len = len - send_len;
        }
        else
        {
            s_setup_tx_ptr = NULL;
            s_setup_tx_len = 0;
        }

        USBFSD->UEP0_TX_LEN = send_len;
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
    }
    else
    {
        // 无法识别请求 -> STALL
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_STALL;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_STALL;
    }
}

/* USBFS 中断服务函数 */
void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBFS_IRQHandler(void)
{
    uint8_t int_fg = USBFSD->INT_FG;

    if (int_fg & USBFS_UIF_TRANSFER)
    {
        uint8_t int_st = USBFSD->INT_ST;
        uint8_t endp   = int_st & USBFS_UIS_ENDP_MASK;
        uint8_t token  = int_st & USBFS_UIS_TOKEN_MASK;

        switch (endp)
        {
            case 0: // 控制端点 0
                if (token == USBFS_UIS_TOKEN_SETUP)
                {
                    USB_CDC_HandleEP0_Setup();
                }
                else if (token == USBFS_UIS_TOKEN_IN)
                {
                    if (s_dev_addr_pending)
                    {
                        USBFSD->DEV_ADDR = s_dev_addr;
                        s_dev_addr_pending = 0;
                    }

                    if (s_setup_tx_len > 0)
                    {
                        uint16_t send_len = s_setup_tx_len;
                        if (send_len > 64) send_len = 64;
                        memcpy(s_ep0_buf, s_setup_tx_ptr, send_len);
                        s_setup_tx_ptr += send_len;
                        s_setup_tx_len -= send_len;
                        USBFSD->UEP0_TX_LEN = send_len;
                        USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                        USBFSD->UEP0_TX_CTRL |= USBFS_UEP_T_RES_ACK;
                    }
                    else
                    {
                        USBFSD->UEP0_TX_LEN = 0;
                        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
                    }
                }
                else if (token == USBFS_UIS_TOKEN_OUT)
                {
                    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
                }
                break;

            case 1: // EP1 (CDC Notification IN)
                USBFSD->UEP1_TX_CTRL = (USBFSD->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                break;

            case 2: // EP2 (CDC Data IN, 发送至主机)
                s_ep2_tx_busy = 0;
                USB_CDC_TriggerNextTx();
                if (!s_ep2_tx_busy)
                {
                    USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                }
                break;

            case 3: // EP3 (CDC Data OUT, 主机发至设备)
            {
                uint16_t rx_len = USBFSD->RX_LEN;
                for (uint16_t i = 0; i < rx_len; i++)
                {
                    uint16_t next_head = (s_rx_head + 1) % USB_CDC_RX_BUF_SIZE;
                    if (next_head != s_rx_tail)
                    {
                        s_rx_buf[s_rx_head] = s_ep3_buf[i];
                        s_rx_head = next_head;
                    }
                }
                USBFSD->UEP3_RX_CTRL = (USBFSD->UEP3_RX_CTRL & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                break;
            }

            default:
                break;
        }

        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }

    if (int_fg & USBFS_UIF_BUS_RST)
    {
        USBFSD->DEV_ADDR = 0x00;
        s_dev_addr = 0;
        s_dev_config = 0;
        s_dev_addr_pending = 0;
        s_ep2_tx_busy = 0;

        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_T_AUTO_TOG;
        USBFSD->UEP2_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_T_AUTO_TOG;
        USBFSD->UEP3_RX_CTRL = USBFS_UEP_R_RES_ACK | USBFS_UEP_R_AUTO_TOG;

        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    }

    if (int_fg & USBFS_UIF_SUSPEND)
    {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
    }
}
