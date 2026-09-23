/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_endp.c
 * Author             : WCH & Antigravity
 * Version            : V1.1.0
 * Description        : Endpoint callbacks connected to USB CDC ring buffers
 *******************************************************************************/
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_mem.h"
#include "hw_config.h"
#include "usb_istr.h"
#include "usb_pwr.h"
#include "usb_prop.h"
#include "usb_cdc.h"

uint8_t USBD_Endp3_Busy = 0;

void EP1_IN_Callback(void)
{
}

void EP2_OUT_Callback(void)
{
    uint32_t len = GetEPRxCount(EP2_OUT & 0x7F);
    uint8_t temp[DEF_USBD_MAX_PACK_SIZE];
    if (len > 0)
    {
        PMAToUserBufferCopy(temp, GetEPRxAddr(EP2_OUT & 0x7F), len);
        USB_CDC_OnRxChunk(temp, len);
    }
    SetEPRxValid(ENDP2);
}

void EP3_IN_Callback(void)
{
    USBD_Endp3_Busy = 0;
    USB_CDC_OnTxComplete();
}

uint8_t USBD_ENDPx_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len)
{
    if (endp == ENDP3)
    {
        if (USBD_Endp3_Busy)
        {
            return USB_ERROR;
        }
        USB_SIL_Write(EP3_IN, pbuf, len);
        USBD_Endp3_Busy = 1;
        SetEPTxStatus(ENDP3, EP_TX_VALID);
    }
    else
    {
        return USB_ERROR;
    }
    return USB_SUCCESS;
}
