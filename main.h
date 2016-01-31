
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __main_h
#define __main_h

#define FW_REVISON 150629

/* Includes ------------------------------------------------------------------*/
#include "stm32f4_discovery.h"
#include <stdio.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
#define DOUBLECLICK_Z                    ((uint8_t)0x60)
#define SINGLECLICK_Z                    ((uint8_t)0x50)

/* Exported constants --------------------------------------------------------*/
/* TIM2 Autoreload and Capture Compare register values */
#define TIM_ARR                          (uint16_t)1999
#define TIM_CCR                          (uint16_t)1000


/* Exported macro ------------------------------------------------------------*/
#define ABS(x)         (x < 0) ? (-x) : x
#define MAX(a,b)       (a < b) ? (b) : a

#define AND	&&
#define OR	||
#define EQ	==
#define NOT	!=

/* Exported functions ------------------------------------------------------- */
void TimingDelay_Decrement(void);
void Delay(uint32_t nTime);

void Fail_Handler(void);
#endif /* __main_h */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
