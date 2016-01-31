#ifndef _rtc_h
#define _rtc_h
 /* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"


typedef struct {
	unsigned short  ms;
	unsigned char   second;
	unsigned char   minute;
	unsigned char   hour;
	unsigned char   date;
	unsigned char   month;
	unsigned short  year;
} rtc_data;

void rtc_init(void);
void RTC_Config(void);
void RTC_TimeRegulate(void);
void RTC_PutInto(rtc_data* mrtc);
void RTC_SetDateTime(uint8_t Day, uint8_t Month, uint8_t Year, uint8_t Hours, uint8_t Minutes, uint8_t Seconds);
void RTC_DateTimePrint(void);
void timestamp (char * res);


#endif