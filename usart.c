// Инициализирует USART3 на пинах PD8(TX) и 9 для работы с WIFI (up to 2.6 Mbit/s)
// и USART2 на пинах PA3 (RX) и PA2 (TX) для DEBUG (up to 2.6 Mbit/s)

#include "stm32f4xx_usart.h"
#include "tm_stm32f4_nrf24l01.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "stm32f4xx_it.h"
#include "main.h"
#include "logic.h"
#include "stm32f4_discovery.h"
#include <stdint.h>
#include <stdbool.h>
#include "fifo.h"

extern bool fESP_send_ready; //Признак приглацения отправлять данные (ESP)
extern unsigned short marker;

bool direct = false;    //Перенаправлять WIFI <-> DEBUG для настройки модуля

#define BODS    115200 //921600

//Буфферы для прямой связи DEBUG <-> WIFI.
//Без буфферов будут пропадать символы, если их сразу же передавать в порт USART
//Причина: ожидание отправки прямо в прерывании
FIFO(128) WIFI_tx_fifo; //При >128 уже будут баги, см fifo.h
FIFO(128) DBG_tx_fifo;   //FIFO буффер DEBUG на отправку по радио. Ранее было через USART

extern unsigned short WIFIdataIx;

void FLUSH(void){
unsigned char data;
uint8_t TXdata[32];            //Буффер payload для отправки через NRF
uint8_t cnt;
 
  //Отправлем содержимое буффера DEBUG по радио

  while (!FIFO_IS_EMPTY(DBG_tx_fifo)) {   //Пока буффер не опустошится, переносим данные в буффер на отправку
    data = FIFO_FRONT(DBG_tx_fifo);       //взять первый элемент из fifo
    FIFO_POP(DBG_tx_fifo);                //уменьшить количество элементов в очереди
    TXdata[cnt] = data;
    cnt++;
    
    if (cnt==31){                         //Если число символов = payload
//      TM_NRF24L01_SendData(TXdata);       //Отправляем
      for (unsigned int j = 0; j < 32; j++) TXdata[j] = 0;    //Чистим буффер NRF
      cnt = 0;
    }
  }
  
//  if (cnt) TM_NRF24L01_SendData(TXdata);       //Отправляем остатки не кратные payload
  
  while (!FIFO_IS_EMPTY(WIFI_tx_fifo)) {
    //иначе передаем следующий байт
    data = FIFO_FRONT(WIFI_tx_fifo);       //взять первый элемент из fifo
    FIFO_POP(WIFI_tx_fifo);                //уменьшить количество элементов в очереди
    USART_Send(USART3, data);
  }
}

void USARTInit(void){

  USART_InitTypeDef     USART_InitStructure;                    //Структура содержащая настройки порта
  GPIO_InitTypeDef      GPIO_InitStructure;                     //Структура содержащая настройки USART

  RCC_AHB1PeriphClockCmd(USART_TX_GPIO_CLK | USART_RX_GPIO_CLK, ENABLE);        //Включаем тактирование порта A
  RCC_APB1PeriphClockCmd(USART_CLK, ENABLE);                                    //Включаем тактирование порта USART3

  GPIO_PinAFConfig(USART_TX_GPIO_PORT, USART_TX_SOURCE, USART_TX_AF);           // TX USART3
  GPIO_PinAFConfig(USART_RX_GPIO_PORT, USART_RX_SOURCE, USART_RX_AF);           //Подключаем RX USART3

  USART_StructInit(&USART_InitStructure); //Инициализируем UART с дефолтными настройками: скорость 9600, 8 бит данных, 1 стоп бит

  USART_InitStructure.USART_BaudRate    = BODS;                 //Скорость обмена
  USART_InitStructure.USART_WordLength  = USART_WordLength_8b;  //Длина слова 8 бит
  USART_InitStructure.USART_StopBits    = USART_StopBits_1;     //1 стоп-бит
  USART_InitStructure.USART_Parity      = USART_Parity_No ;     //Без проверки четности
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //Без аппаратного контроля
  USART_InitStructure.USART_Mode        = USART_Mode_Rx | USART_Mode_Tx; //Включен передатчик и приемник USART2

  //Конфигурируем PA2 как альтернативную функцию -> TX UART. Подробнее об конфигурации можно почитать во втором уроке.
  GPIO_InitStructure.GPIO_OType                 = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd                  = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode                  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin                   = USART_TX_PIN;
  GPIO_InitStructure.GPIO_Speed                 = GPIO_Speed_50MHz;
  GPIO_Init(USART_TX_GPIO_PORT, &GPIO_InitStructure);

  //Конфигурируем PA2 как альтернативную функцию -> RX UART. Подробнее об конфигурации можно почитать во втором уроке.
  GPIO_InitStructure.GPIO_Mode                  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin                   = USART_RX_PIN;
  GPIO_Init(USART_RX_GPIO_PORT, &GPIO_InitStructure);

  USART_Init(USART3, &USART_InitStructure);
  USART_Cmd(USART3, ENABLE);            //Включаем UART

  //Настраиваем прерывания по приему
  NVIC_EnableIRQ(USART3_IRQn);          //Включаем прерывания от UART
  NVIC_SetPriority(USART3_IRQn, 1);     //Прерывание от UART, приоритет 1
  USART3->CR1 |= USART_CR1_RXNEIE;      //Прерывание по прием
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); //IRQ на приход байта
  //USART_ITConfig(USART3, USART_IT_TC, ENABLE); //IRQ на окончание отправки байта
}

//Выдача данных DEBUG в массив для последующей отправки
void DEBUG(char const* fmt, ... ) {  
  char buff[256+1];

  va_list   uk_arg;
  va_start (uk_arg, fmt);

  int slen = vsnprintf(buff, 256, fmt, uk_arg);       //Форматирование строки

  for(int i = 0; i < slen-1; ++i) { //если в буфере на отправку в DBG есть место, то добавляем туда байт
   
    if (!FIFO_IS_FULL(DBG_tx_fifo)) FIFO_PUSH(DBG_tx_fifo, buff[i]);
    if (buff[i] == 0) break;    //Прерываемся при обнаружении конца строки
  }
}

//void DBG_STR(char * s) {
//  while (*s++) USART_Send(USART2, (uint16_t)s);
//}

void DEBUG_CHAR(unsigned char c){
  
    if (!FIFO_IS_FULL(DBG_tx_fifo)) FIFO_PUSH(DBG_tx_fifo, c);
}

void USART2_IRQHandler (void){                  //USART2 DEBUG PA2 to tx & PA3 to rx (ОТ КОМПА)
   char data;

   //Receive Data register not empty interrupt
   if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET){
      USART_ClearITPendingBit(USART2, USART_IT_RXNE);
      data = (char)(USART_ReceiveData(USART2) & 0x7F);

      //LED_toggle(LED6); //BLUE
      
      //USART_Send(USART3, data);
        if(!FIFO_IS_FULL(WIFI_tx_fifo) AND direct) {    //если в буфере на отправку в DBG есть место, то добавляем туда байт
            FIFO_PUSH(WIFI_tx_fifo, data);
        }
  
        DEBUG_processing(data);
   }
        //Transmission complete interrupt
   if(USART_GetITStatus(USART2, USART_IT_TC) != RESET){
     //  USART_ClearITPendingBit(USART2, USART_IT_TC);
       //LED_toggle(LED4); //GREEN
   }
}

void USART3_IRQHandler(void){                   // от WI FI MODULE
  char data;

   if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET){
      USART_ClearITPendingBit(USART3, USART_IT_RXNE);
      data = (char)(USART_ReceiveData(USART3) & 0x7F);

    //  LED_toggle(LED4); //GREEN
        if(!FIFO_IS_FULL(DBG_tx_fifo) AND direct) {    //если в буфере на отправку в DBG есть место, то добавляем туда байт
            FIFO_PUSH(DBG_tx_fifo, data);
        }
     
  if (data == (char)'>') fESP_send_ready = true; //Символ приглацения отправки данных. Нужно для модуля ESP См HTTPGET в logic.c
        WIFIdataAdd(data); //Сохраняем символ в FIFO буффер для обработки
        //DEBUG_CHAR(data);
  }
        //Transmission complete interrupt
   if(USART_GetITStatus(USART3, USART_IT_TC) != RESET){
      // USART_ClearITPendingBit(USART3, USART_IT_TC);
       LED_toggle(LED_GREEN); //BLUE
   }
}

void USART6_IRQHandler(void){                   // от сокета WI-FI

}

//Учитывает, что нужно подождать пока символ отправится
void USART_Send(USART_TypeDef* USARTx, uint16_t Data) {
  while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET)
  {}
  USART_SendData(USARTx, Data);
}
//вывод символов в порт. Перекрывает
int putchar(int c){
  // Place your implementation of fputc here  e.g. write a character to the USART
  USART_SendData(USART3, (uint8_t)c);

  // Loop until the end of transmission
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET)
  {}

  return c;
}