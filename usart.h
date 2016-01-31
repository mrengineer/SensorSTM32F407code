#include "stm32f4xx_usart.h"

#define USART_CLK                    RCC_APB1Periph_USART3
#define USART_TX_PIN                 GPIO_Pin_8
#define USART_TX_GPIO_PORT           GPIOD
#define USART_TX_GPIO_CLK            RCC_AHB1Periph_GPIOD
#define USART_TX_SOURCE              GPIO_PinSource8
#define USART_TX_AF                  GPIO_AF_USART3
#define USART_RX_PIN                 GPIO_Pin_9
#define USART_RX_GPIO_PORT           GPIOD
#define USART_RX_GPIO_CLK            RCC_AHB1Periph_GPIOD
#define USART_RX_SOURCE              GPIO_PinSource9
#define USART_RX_AF                  GPIO_AF_USART3
#define USART_IRQn                   USART3_IRQn


void USARTInit(void);
//int putchar(int c);
void DEBUG              (char const* fmt, ... );
void SOCK               (char const* fmt, ... );

//void DBG_STR(char * s);
void DEBUG_CHAR         (unsigned char c);
void USART2_IRQHandler  (void);
void USART3_IRQHandler  (void);
void USART6_IRQHandler  (void);
void FLUSH(void);

//Учитывает, что нужно подождать пока символ отправится
void USART_Send(USART_TypeDef* USARTx, uint16_t Data);