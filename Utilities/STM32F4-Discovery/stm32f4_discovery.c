
  
/* Includes ------------------------------------------------------------------*/
#include "stm32f4_discovery.h"


GPIO_TypeDef* GPIO_PORT[LEDn] = {LED4_GPIO_PORT, LED3_GPIO_PORT, LED5_GPIO_PORT,
                                 LED6_GPIO_PORT};
const uint16_t GPIO_PIN[LEDn] = {LED4_PIN, LED3_PIN, LED5_PIN,
                                 LED6_PIN};
const uint32_t GPIO_CLK[LEDn] = {LED4_GPIO_CLK, LED3_GPIO_CLK, LED5_GPIO_CLK,
                                 LED6_GPIO_CLK};

GPIO_TypeDef* BUTTON_PORT[BUTTONn] = {USER_BUTTON_GPIO_PORT }; 

const uint16_t BUTTON_PIN[BUTTONn] = {USER_BUTTON_PIN }; 

const uint32_t BUTTON_CLK[BUTTONn] = {USER_BUTTON_GPIO_CLK };

const uint16_t BUTTON_EXTI_LINE[BUTTONn] = {USER_BUTTON_EXTI_LINE };

const uint8_t BUTTON_PORT_SOURCE[BUTTONn] = {USER_BUTTON_EXTI_PORT_SOURCE};
								 
const uint8_t BUTTON_PIN_SOURCE[BUTTONn] = {USER_BUTTON_EXTI_PIN_SOURCE }; 
const uint8_t BUTTON_IRQn[BUTTONn] = {USER_BUTTON_EXTI_IRQn };

NVIC_InitTypeDef   NVIC_InitStructure;



void LED_init(Led_TypeDef Led){
  GPIO_InitTypeDef  GPIO_InitStructure;
  
  /* Enable the GPIO_LED Clock */
  RCC_AHB1PeriphClockCmd(GPIO_CLK[Led], ENABLE);

  /* Configure the GPIO_LED pin */
  GPIO_InitStructure.GPIO_Pin   = GPIO_PIN[Led];
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIO_PORT[Led], &GPIO_InitStructure);
}

void PE7_Init(){
  GPIO_InitTypeDef  GPIO_InitStructure;
  
  /* Enable the Clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

  /* Configure the pin */
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(SD_POWER_GPIO_PORT, &GPIO_InitStructure);
}

void PE7_OFF(){
  GPIOE->BSRRL = GPIO_Pin_7;
}

void PE7_ON(){
  GPIOE->BSRRH = GPIO_Pin_7;  
}


void PE7_toggle(void){
  GPIOE->ODR ^= GPIO_Pin_7;
}

void SD_POWER_Init(){
  GPIO_InitTypeDef  GPIO_InitStructure;
  
  /* Enable the Clock */
  RCC_AHB1PeriphClockCmd(SD_POWER_GPIO_CLK, ENABLE);

  /* Configure the pin */
  GPIO_InitStructure.GPIO_Pin   = SD_POWER_PIN;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(SD_POWER_GPIO_PORT, &GPIO_InitStructure);
}

void SD_POWER_OFF(){
  SD_POWER_GPIO_PORT->BSRRL = SD_POWER_PIN;
}

void SD_POWER_ON(){
  SD_POWER_GPIO_PORT->BSRRH = SD_POWER_PIN;  
}


void WIFI_POWER_Init(){
  GPIO_InitTypeDef  GPIO_InitStructure;
  
  /* Enable the Clock */
  RCC_AHB1PeriphClockCmd(SD_POWER_GPIO_CLK, ENABLE);

  /* Configure the pin */
  GPIO_InitStructure.GPIO_Pin   = WIFI_POWER_PIN;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(WIFI_POWER_GPIO_PORT, &GPIO_InitStructure);
}

void WIFI_POWER_OFF(){
  WIFI_POWER_GPIO_PORT->BSRRL = WIFI_POWER_PIN;
}

void WIFI_POWER_ON(){
  WIFI_POWER_GPIO_PORT->BSRRH = WIFI_POWER_PIN;  
}


void LED_on(Led_TypeDef Led){
  GPIO_PORT[Led]->BSRRL = GPIO_PIN[Led];
}

void LED_off(Led_TypeDef Led){
  GPIO_PORT[Led]->BSRRH = GPIO_PIN[Led];  
}

void LED_toggle(Led_TypeDef Led){
  GPIO_PORT[Led]->ODR ^= GPIO_PIN[Led];
}

void STM_EVAL_PBInit(Button_TypeDef Button, ButtonMode_TypeDef Button_Mode)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  EXTI_InitTypeDef EXTI_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  /* Enable the BUTTON Clock */
  RCC_AHB1PeriphClockCmd(BUTTON_CLK[Button], ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

  /* Configure Button pin as input */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Pin = BUTTON_PIN[Button];
  GPIO_Init(BUTTON_PORT[Button], &GPIO_InitStructure);

  if (Button_Mode == BUTTON_MODE_EXTI)
  {
    /* Connect Button EXTI Line to Button GPIO Pin */
    SYSCFG_EXTILineConfig(BUTTON_PORT_SOURCE[Button], BUTTON_PIN_SOURCE[Button]);

    /* Configure Button EXTI line */
    EXTI_InitStructure.EXTI_Line    = BUTTON_EXTI_LINE[Button];
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;  
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Enable and set Button EXTI Interrupt to the lowest priority */
    NVIC_InitStructure.NVIC_IRQChannel = BUTTON_IRQn[Button];
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure); 
  }
}

/**
  * @brief  Returns the selected Button state.
  * @param  Button: Specifies the Button to be checked.
  *   This parameter should be: BUTTON_USER  
  * @retval The Button GPIO pin value.
  */
uint32_t STM_EVAL_PBGetState(Button_TypeDef Button)
{
  return GPIO_ReadInputDataBit(BUTTON_PORT[Button], BUTTON_PIN[Button]);
}

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */   

/**
  * @}
  */ 
    
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
