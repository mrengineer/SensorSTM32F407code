// �������������� USART3 �� ����� PD8(TX) � 9 ��� ������ � WIFI (up to 2.6 Mbit/s)
// � USART2 �� ����� PA3 (RX) � PA2 (TX) ��� DEBUG (up to 2.6 Mbit/s)

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

extern bool fESP_send_ready; //������� ����������� ���������� ������ (ESP)
extern unsigned short marker;

bool direct = false;    //�������������� WIFI <-> DEBUG ��� ��������� ������

#define BODS    115200 //921600

//������� ��� ������ ����� DEBUG <-> WIFI.
//��� �������� ����� ��������� �������, ���� �� ����� �� ���������� � ���� USART
//�������: �������� �������� ����� � ����������
FIFO(128) WIFI_tx_fifo; //��� >128 ��� ����� ����, �� fifo.h
FIFO(128) DBG_tx_fifo;   //FIFO ������ DEBUG �� �������� �� �����. ����� ���� ����� USART

extern unsigned short WIFIdataIx;

void FLUSH(void){
unsigned char data;
uint8_t TXdata[32];            //������ payload ��� �������� ����� NRF
uint8_t cnt;
 
  //��������� ���������� ������� DEBUG �� �����

  while (!FIFO_IS_EMPTY(DBG_tx_fifo)) {   //���� ������ �� �����������, ��������� ������ � ������ �� ��������
    data = FIFO_FRONT(DBG_tx_fifo);       //����� ������ ������� �� fifo
    FIFO_POP(DBG_tx_fifo);                //��������� ���������� ��������� � �������
    TXdata[cnt] = data;
    cnt++;
    
    if (cnt==31){                         //���� ����� �������� = payload
//      TM_NRF24L01_SendData(TXdata);       //����������
      for (unsigned int j = 0; j < 32; j++) TXdata[j] = 0;    //������ ������ NRF
      cnt = 0;
    }
  }
  
//  if (cnt) TM_NRF24L01_SendData(TXdata);       //���������� ������� �� ������� payload
  
  while (!FIFO_IS_EMPTY(WIFI_tx_fifo)) {
    //����� �������� ��������� ����
    data = FIFO_FRONT(WIFI_tx_fifo);       //����� ������ ������� �� fifo
    FIFO_POP(WIFI_tx_fifo);                //��������� ���������� ��������� � �������
    USART_Send(USART3, data);
  }
}

void USARTInit(void){

  USART_InitTypeDef     USART_InitStructure;                    //��������� ���������� ��������� �����
  GPIO_InitTypeDef      GPIO_InitStructure;                     //��������� ���������� ��������� USART

  RCC_AHB1PeriphClockCmd(USART_TX_GPIO_CLK | USART_RX_GPIO_CLK, ENABLE);        //�������� ������������ ����� A
  RCC_APB1PeriphClockCmd(USART_CLK, ENABLE);                                    //�������� ������������ ����� USART3

  GPIO_PinAFConfig(USART_TX_GPIO_PORT, USART_TX_SOURCE, USART_TX_AF);           // TX USART3
  GPIO_PinAFConfig(USART_RX_GPIO_PORT, USART_RX_SOURCE, USART_RX_AF);           //���������� RX USART3

  USART_StructInit(&USART_InitStructure); //�������������� UART � ���������� �����������: �������� 9600, 8 ��� ������, 1 ���� ���

  USART_InitStructure.USART_BaudRate    = BODS;                 //�������� ������
  USART_InitStructure.USART_WordLength  = USART_WordLength_8b;  //����� ����� 8 ���
  USART_InitStructure.USART_StopBits    = USART_StopBits_1;     //1 ����-���
  USART_InitStructure.USART_Parity      = USART_Parity_No ;     //��� �������� ��������
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //��� ����������� ��������
  USART_InitStructure.USART_Mode        = USART_Mode_Rx | USART_Mode_Tx; //������� ���������� � �������� USART2

  //������������� PA2 ��� �������������� ������� -> TX UART. ��������� �� ������������ ����� �������� �� ������ �����.
  GPIO_InitStructure.GPIO_OType                 = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd                  = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode                  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin                   = USART_TX_PIN;
  GPIO_InitStructure.GPIO_Speed                 = GPIO_Speed_50MHz;
  GPIO_Init(USART_TX_GPIO_PORT, &GPIO_InitStructure);

  //������������� PA2 ��� �������������� ������� -> RX UART. ��������� �� ������������ ����� �������� �� ������ �����.
  GPIO_InitStructure.GPIO_Mode                  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin                   = USART_RX_PIN;
  GPIO_Init(USART_RX_GPIO_PORT, &GPIO_InitStructure);

  USART_Init(USART3, &USART_InitStructure);
  USART_Cmd(USART3, ENABLE);            //�������� UART

  //����������� ���������� �� ������
  NVIC_EnableIRQ(USART3_IRQn);          //�������� ���������� �� UART
  NVIC_SetPriority(USART3_IRQn, 1);     //���������� �� UART, ��������� 1
  USART3->CR1 |= USART_CR1_RXNEIE;      //���������� �� �����
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); //IRQ �� ������ �����
  //USART_ITConfig(USART3, USART_IT_TC, ENABLE); //IRQ �� ��������� �������� �����
}

//������ ������ DEBUG � ������ ��� ����������� ��������
void DEBUG(char const* fmt, ... ) {  
  char buff[256+1];

  va_list   uk_arg;
  va_start (uk_arg, fmt);

  int slen = vsnprintf(buff, 256, fmt, uk_arg);       //�������������� ������

  for(int i = 0; i < slen-1; ++i) { //���� � ������ �� �������� � DBG ���� �����, �� ��������� ���� ����
   
    if (!FIFO_IS_FULL(DBG_tx_fifo)) FIFO_PUSH(DBG_tx_fifo, buff[i]);
    if (buff[i] == 0) break;    //����������� ��� ����������� ����� ������
  }
}

//void DBG_STR(char * s) {
//  while (*s++) USART_Send(USART2, (uint16_t)s);
//}

void DEBUG_CHAR(unsigned char c){
  
    if (!FIFO_IS_FULL(DBG_tx_fifo)) FIFO_PUSH(DBG_tx_fifo, c);
}

void USART2_IRQHandler (void){                  //USART2 DEBUG PA2 to tx & PA3 to rx (�� �����)
   char data;

   //Receive Data register not empty interrupt
   if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET){
      USART_ClearITPendingBit(USART2, USART_IT_RXNE);
      data = (char)(USART_ReceiveData(USART2) & 0x7F);

      //LED_toggle(LED6); //BLUE
      
      //USART_Send(USART3, data);
        if(!FIFO_IS_FULL(WIFI_tx_fifo) AND direct) {    //���� � ������ �� �������� � DBG ���� �����, �� ��������� ���� ����
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

void USART3_IRQHandler(void){                   // �� WI FI MODULE
  char data;

   if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET){
      USART_ClearITPendingBit(USART3, USART_IT_RXNE);
      data = (char)(USART_ReceiveData(USART3) & 0x7F);

    //  LED_toggle(LED4); //GREEN
        if(!FIFO_IS_FULL(DBG_tx_fifo) AND direct) {    //���� � ������ �� �������� � DBG ���� �����, �� ��������� ���� ����
            FIFO_PUSH(DBG_tx_fifo, data);
        }
     
  if (data == (char)'>') fESP_send_ready = true; //������ ����������� �������� ������. ����� ��� ������ ESP �� HTTPGET � logic.c
        WIFIdataAdd(data); //��������� ������ � FIFO ������ ��� ���������
        //DEBUG_CHAR(data);
  }
        //Transmission complete interrupt
   if(USART_GetITStatus(USART3, USART_IT_TC) != RESET){
      // USART_ClearITPendingBit(USART3, USART_IT_TC);
       LED_toggle(LED_GREEN); //BLUE
   }
}

void USART6_IRQHandler(void){                   // �� ������ WI-FI

}

//���������, ��� ����� ��������� ���� ������ ����������
void USART_Send(USART_TypeDef* USARTx, uint16_t Data) {
  while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET)
  {}
  USART_SendData(USARTx, Data);
}
//����� �������� � ����. �����������
int putchar(int c){
  // Place your implementation of fputc here  e.g. write a character to the USART
  USART_SendData(USART3, (uint8_t)c);

  // Loop until the end of transmission
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET)
  {}

  return c;
}