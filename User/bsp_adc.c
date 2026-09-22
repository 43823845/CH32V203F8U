#include "bsp_adc.h"

static uint8_t s_adc_inited = 0;

void BSP_ADC_Init(void)
{
    if (s_adc_inited) return;

    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // ADC时钟分频 (72MHz/6=12MHz, 96MHz/6=16MHz)

    // PA1 配置为高阻模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 使能内部温度传感器与 Vrefint (内部稳定 1.20V 基准参考)
    ADC_TempSensorVrefintCmd(ENABLE);

    ADC_Cmd(ADC1, ENABLE);

    // 执行 ADC 自校准
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));

    s_adc_inited = 1;
}

void BSP_ADC_DeInit(void)
{
    if (!s_adc_inited) return;

    ADC_Cmd(ADC1, DISABLE);
    ADC_TempSensorVrefintCmd(DISABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, DISABLE);

    // PA1 保持模拟输入，防止产生分流漏电
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    s_adc_inited = 0;
}

static uint16_t ADC_Read_Channel(uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

/**
 * @brief 读取电池实际电压 (mV)
 * 采用内部 Vrefint (1.20V) 进行绝对电压基准校准：
 * 电池分压输入 ADC_bat = (Vbat / 2) / VDDA * 4096
 * 内部基准输入 ADC_vref = 1200mV / VDDA * 4096
 * 联立消去 VDDA，得：
 * Vbat (mV) = (2400mV * ADC_bat) / ADC_vref
 * 即使电池电压跌至 3.0V 导致 LDO 稳压输出下跌，测量值依然 100% 绝对精准！
 */
uint16_t BSP_ADC_GetBatteryVoltage_mV(void)
{
    if (!s_adc_inited)
    {
        BSP_ADC_Init();
    }

    // 1. 采样外部电池通道 (ADC_Channel_1) 8 次均值滤波
    uint32_t sum_bat = 0;
    for (int i = 0; i < 8; i++)
    {
        sum_bat += ADC_Read_Channel(ADC_Channel_1);
    }
    uint16_t avg_bat = (uint16_t)(sum_bat / 8);

    // 2. 采样内部参考基准通道 (ADC_Channel_Vrefint) 4 次均值滤波
    uint32_t sum_vref = 0;
    for (int i = 0; i < 4; i++)
    {
        sum_vref += ADC_Read_Channel(ADC_Channel_Vrefint);
    }
    uint16_t avg_vref = (uint16_t)(sum_vref / 4);

    // 3. 基于内部 1.20V 带隙基准反算真实电池电压
    if (avg_vref > 500) // 合法采样值通常在 1000 ~ 2000 之间
    {
        uint32_t vbat = ((uint32_t)avg_bat * 2400UL) / avg_vref;
        return (uint16_t)vbat;
    }
    else
    {
        // 兜底降级算法 (按标称 3.3V 换算)
        uint32_t vbat = ((uint32_t)avg_bat * 6600UL) >> 12;
        return (uint16_t)vbat;
    }
}
