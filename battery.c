#include "stm32f4xx_adc.h"
#include "main.h"

unsigned short battery;


void battery_init() {
       ADC_InitTypeDef ADC_InitStructure;
       ADC_CommonInitTypeDef adc_init;
       /* ��������� ������������ A��1 */
       RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
       /* ���������� ��������� ��� */
       ADC_DeInit();

       /* ���1 � ���2 �������� ���������� */
       adc_init.ADC_Mode = ADC_Mode_Independent;
       adc_init.ADC_Prescaler = ADC_Prescaler_Div2;
       adc_init.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;

       /* ��������� scan conversion */
       ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
       ADC_InitStructure.ADC_ScanConvMode = DISABLE;
       /* �� ������ ���������� �������������� */
       ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
       /* �������� �������������� ���������, � �� �� ������������ �������� */
       ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConvEdge_None;
       /* 12 ������ ��������������. ��������� � 12 ������� �������� ���������� */
       ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
       
       ADC_InitStructure.ADC_NbrOfConversion = 1;

       /* ������������� */
       ADC_CommonInit(&adc_init);
       ADC_Init(ADC1, &ADC_InitStructure);
       
       /* �������� ���1 */
       ADC_Cmd(ADC1, ENABLE);
       
      //   ADC_TempSensorVrefintCmd(ENABLE);          // ��������� ��������������  
      ADC_VBATCmd(ENABLE);
}

u16 readADC1(u8 channel){
   ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_15Cycles);
   ADC_VBATCmd(ENABLE);
   // �������� ������
   ADC_SoftwareStartConv(ADC1);
   // ��� ���� ������������� ���������� � ���
   while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
   // ���������� ���������

   //ADC_VBATCmd(DISABLE);
      return ADC_GetConversionValue(ADC1);
}

void battery_read (void){
   unsigned int bat = readADC1(ADC_Channel_Vbat);   // ������ ������ � ������� ���1
   battery = ((bat*2)*2932/0xFFF)/10;
   
   //printf("%i\r", ((bat*2)*2932/0xFFF)/10);

}

void battery_print(void){
    printf ("%0.3d", battery);
}
//2.932=2059