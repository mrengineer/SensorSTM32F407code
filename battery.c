#include "stm32f4xx_adc.h"
#include "main.h"

unsigned short battery;


void battery_init() {
       ADC_InitTypeDef ADC_InitStructure;
       ADC_CommonInitTypeDef adc_init;
       /* разрешаем тактирование AЦП1 */
       RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
       /* сбрасываем настройки АЦП */
       ADC_DeInit();

       /* АЦП1 и АЦП2 работают независимо */
       adc_init.ADC_Mode = ADC_Mode_Independent;
       adc_init.ADC_Prescaler = ADC_Prescaler_Div2;
       adc_init.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;

       /* выключаем scan conversion */
       ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
       ADC_InitStructure.ADC_ScanConvMode = DISABLE;
       /* Не делать длительные преобразования */
       ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
       /* Начинать преобразование програмно, а не по срабатыванию триггера */
       ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConvEdge_None;
       /* 12 битное преобразование. результат в 12 младших разрядах результата */
       ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
       
       ADC_InitStructure.ADC_NbrOfConversion = 1;

       /* инициализация */
       ADC_CommonInit(&adc_init);
       ADC_Init(ADC1, &ADC_InitStructure);
       
       /* Включаем АЦП1 */
       ADC_Cmd(ADC1, ENABLE);
       
      //   ADC_TempSensorVrefintCmd(ENABLE);          // разрешаем преобразование  
      ADC_VBATCmd(ENABLE);
}

u16 readADC1(u8 channel){
   ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_15Cycles);
   ADC_VBATCmd(ENABLE);
   // начинаем работу
   ADC_SoftwareStartConv(ADC1);
   // ждём пока преобразуется напряжение в код
   while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
   // возвращаем результат

   //ADC_VBATCmd(DISABLE);
      return ADC_GetConversionValue(ADC1);
}

void battery_read (void){
   unsigned int bat = readADC1(ADC_Channel_Vbat);   // читаем данные с датчика АЦП1
   battery = ((bat*2)*2932/0xFFF)/10;
   
   //printf("%i\r", ((bat*2)*2932/0xFFF)/10);

}

void battery_print(void){
    printf ("%0.3d", battery);
}
//2.932=2059