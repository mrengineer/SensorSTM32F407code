
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F4_DISCOVERY_H
#define __STM32F4_DISCOVERY_H

#ifdef __cplusplus
 extern "C" {
#endif
                                              
/* Includes ------------------------------------------------------------------*/
 #include "stm32f4xx.h"
   
typedef enum 
{
  LED_GREEN = 0,
  LED_ORANGE = 1,
  LED_RED = 2,
  LED_BLUE = 3
} Led_TypeDef;

typedef enum 
{  
  BUTTON_USER = 0,
} Button_TypeDef;

typedef enum 
{  
  BUTTON_MODE_GPIO = 0,
  BUTTON_MODE_EXTI = 1
} ButtonMode_TypeDef;     
/**
  * @}
  */ 

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_Exported_Constants
  * @{
  */ 

/** @addtogroup STM32F4_DISCOVERY_LOW_LEVEL_LED
  * @{
  */
#define LEDn                             4

#define LED4_PIN                         GPIO_Pin_12
#define LED4_GPIO_PORT                   GPIOD
#define LED4_GPIO_CLK                    RCC_AHB1Periph_GPIOD  
  
#define LED3_PIN                         GPIO_Pin_13
#define LED3_GPIO_PORT                   GPIOD
#define LED3_GPIO_CLK                    RCC_AHB1Periph_GPIOD  
  
#define LED5_PIN                         GPIO_Pin_14
#define LED5_GPIO_PORT                   GPIOD
#define LED5_GPIO_CLK                    RCC_AHB1Periph_GPIOD  
  
#define LED6_PIN                        GPIO_Pin_15
#define LED6_GPIO_PORT                  GPIOD
#define LED6_GPIO_CLK                   RCC_AHB1Periph_GPIOD


//Пин подачи питания на карту SD
#define SD_POWER_PIN                    GPIO_Pin_6
#define SD_POWER_GPIO_PORT              GPIOD
#define SD_POWER_GPIO_CLK               RCC_AHB1Periph_GPIOD

//Пин подачи питания на WI-FI
#define WIFI_POWER_PIN                    GPIO_Pin_7
#define WIFI_POWER_GPIO_PORT              GPIOD
#define WIFI_POWER_GPIO_CLK               RCC_AHB1Periph_GPIOD


#define BUTTONn                          1  

/**
 * @brief Wakeup push-button
 */
#define USER_BUTTON_PIN                GPIO_Pin_0
#define USER_BUTTON_GPIO_PORT          GPIOA
#define USER_BUTTON_GPIO_CLK           RCC_AHB1Periph_GPIOA
#define USER_BUTTON_EXTI_LINE          EXTI_Line0
#define USER_BUTTON_EXTI_PORT_SOURCE   EXTI_PortSourceGPIOA
#define USER_BUTTON_EXTI_PIN_SOURCE    EXTI_PinSource0
#define USER_BUTTON_EXTI_IRQn          EXTI0_IRQn 



/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_Exported_Functions
  * @{
  */
void SD_POWER_Init(void);
void SD_POWER_ON(void);
void SD_POWER_OFF(void);

void WIFI_POWER_Init(void);
void WIFI_POWER_OFF(void);
void WIFI_POWER_ON(void);


void PE7_Init(void);
void PE7_OFF(void);
void PE7_ON(void);
void PE7_toggle(void);

void LED_init   (Led_TypeDef Led);
void LED_on     (Led_TypeDef Led);
void LED_off    (Led_TypeDef Led);
void LED_toggle (Led_TypeDef Led);
void STM_EVAL_PBInit(Button_TypeDef Button, ButtonMode_TypeDef Button_Mode);
uint32_t STM_EVAL_PBGetState(Button_TypeDef Button);
/**
  * @}
  */
  
#ifdef __cplusplus
}
#endif

#endif /* __STM32F4_DISCOVERY_H */


 

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
