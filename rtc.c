/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "rtc.h"
#include "usart.h"
#include "main.h"
#include "logic.h"
#include <stdio.h>

unsigned int milliseconds; //Миллисекунды. RTC обнуляет их каждую секунду, а SysTick насчитывает
unsigned char seconds;
unsigned char minutes;
unsigned char hours;
unsigned long long cnt_timer;     //Счетчик миллисекунд. Применяется для организации таймаутов

//extern char newkey;
//extern char key;
extern unsigned short marker;
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Uncomment the corresponding line to select the RTC Clock source */
#define RTC_CLOCK_SOURCE_LSE   /* LSE used as RTC source clock */
//#define RTC_CLOCK_SOURCE_LSI  /* LSI used as RTC source clock. The RTC Clock
//                                      may varies due to LSI frequency dispersion. */ 

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

RTC_InitTypeDef   RTC_InitStructure;
RTC_AlarmTypeDef  RTC_AlarmStructure;


void rtc_init(void){
  
RCC->APB1ENR    |=      (1<<28);	 // POWER enable

/* RTC configuration set */
PWR->CR         |=      (1<<8);	        // Access to RTC and RTC backup registers and backup SRAM enabled
RCC->CSR        &=      (1<<0);	        // LSI Off
RCC->BDCR       =       0x00000000;	 // Reset BDCR register
RCC->BDCR       |=      (1<<15);	 // RTC clock enable
RCC->BDCR       |=      (1<<0);	        // LSE On
RCC->BDCR       &=      ~(1<<2);	 // LSE not bypassed quartz On
RCC->BDCR       &=      ~(1<<16);	 // Backup domain software reset not activated
RCC->BDCR       |=      (0x1<<8);	 // LSE used as the RTC clock
RTC->WPR        =       0x000000CA;	 // Key protect 1
RTC->WPR        =       0x00000053;	 // Key protect 2
RTC->ISR        |=      (1<<7);	        // Initialization mode On

for(;((RTC->ISR & 0x40) == 0x00);)	// delay while initialization flag will be set
{
}

RTC->PRER       =       0x00000000;	 // RESET PRER register
RTC->PRER       |=      (0xFF<<0);	 // 255 + 1 Synchronous prescaler factor set
RTC->PRER       |=      (0x7F<<16);	 // 127 + 1 Asynchronous prescaler factor set
RTC->CR         &=      ~(1<<6);	 // Hour format 24 hour day format

int val;
sscanf("010615", "%X", &val); 
RTC->DR         = val;                          //Set data

sscanf("000001", "%X", &val); 
RTC->TR         =       val;	 //Set time

RTC->ISR        &=      ~(1<<7);	 // Initialization mode Off
for(;((RTC->ISR & 0x40) == 0x40);)	// delay while initialization flag will be set
{
}


}


/**
  * @brief  Display the current time on the Hyperterminal.
  * @param  None
  * @retval None
  */
void RTC_DateTimePrint(void){
  // Get the current Time
  RTC_TimeTypeDef   RTC_TimeStructure;
  RTC_DateTypeDef   RTC_DateStructure;
    
  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
  RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
  
  printf("RTC %0.2d/%0.2d/%0.2d %0.2d:%0.2d:%0.2d.%0.4d\r\n", 
         RTC_DateStructure.RTC_Date,    RTC_DateStructure.RTC_Month, 
         RTC_DateStructure.RTC_Year,    RTC_TimeStructure.RTC_Hours,   
         RTC_TimeStructure.RTC_Minutes, RTC_TimeStructure.RTC_Seconds, 
         milliseconds);
}


void timestamp (char * res) {
  RTC_TimeTypeDef   RTC_TimeStructure;
  RTC_DateTypeDef   RTC_DateStructure;

  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
  RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
  
  //Формирует timestamp в формате MY SQL с замененными пробелами на +, т.к. HTTP GET пробелы не принимает
  //Думает, что это новый аргумент
  unsigned short slen = sprintf(res, "%0.4d-%0.2d-%0.2d+%0.2d:%0.2d:%0.2d", 
                                RTC_DateStructure.RTC_Year+2000,        RTC_DateStructure.RTC_Month, 
                                RTC_DateStructure.RTC_Date,             RTC_TimeStructure.RTC_Hours,   
                                RTC_TimeStructure.RTC_Minutes,          RTC_TimeStructure.RTC_Seconds);
}


//Заполняет  удобню мне структуру данными от часов реального времени
/*void RTC_PutInto(rtc_data* mrtc){
  // Get the current Time
    
  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
  RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
  
  mrtc->ms      = milliseconds;
  mrtc->second  = RTC_TimeStructure.RTC_Seconds;
  mrtc->minute  = RTC_TimeStructure.RTC_Minutes;
  mrtc->hour    = RTC_TimeStructure.RTC_Hours;  
  
  if (RTC_TimeStructure.RTC_H12 == RTC_H12_PM)   mrtc->hour += 12;

  mrtc->date    = RTC_DateStructure.RTC_Date;
  mrtc->month   = RTC_DateStructure.RTC_Month;
  mrtc->year    = RTC_DateStructure.RTC_Year;
}*/

void RTC_SetDateTime(uint8_t Day, uint8_t Month, uint8_t Year, uint8_t Hours, uint8_t Minutes, uint8_t Seconds){

  RTC->WPR        =       0x000000CA;	 // Key protect 1
  RTC->WPR        =       0x00000053;	 // Key protect 2
  RTC->ISR        |=      (1<<7);	        // Initialization mode On

  for(;((RTC->ISR & 0x40) == 0x00);)	// delay while initialization flag will be set
  {
  }

  char str[6];
  int val;
  
       marker = 1;
  
  sprintf(&str[0], "%02i%02i%02i", Year, Month, Day);   //Преврощаем в строку
  sscanf(&str[0], "%X", &val);                          //Делаем BSD 12:00 -> 1200 -> 0x1200
  RTC->DR         = val;                          //Set data

       marker = 2;
  sprintf(&str[0], "%02i%02i%02i", Hours, Minutes, Seconds);
  sscanf(&str[0], "%X", &val); 
  RTC->TR         =       val;	 //Set time

  RTC->ISR        &=      ~(1<<7);	 // Initialization mode Off
  for(;((RTC->ISR & 0x40) == 0x40);)	// delay while initialization flag will be set
  {
  }
  
       marker = 3;
  
  //Нужно несколько раз прочитать время Иначе потом первые разы оно прочтется с ошибкой
  RTC_TimeTypeDef   RTC_TimeStructure;
  RTC_DateTypeDef   RTC_DateStructure;
  
  Delay(5);
    marker = 30;
  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
    marker = 31;
  RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
    marker = 32;
  Delay(1);
  
       marker = 4;
  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
  RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
  Delay(1);
  
       marker = 5;
  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
  RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
  
       marker = 6;
}
   

