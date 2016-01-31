/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "diskio.h"
#include "usart.h"
#include "rtc.h"
#include "acc.h"
#include "battery.h"
#include "stm32f4xx.h"
#include "logic.h"
#include <stdbool.h>
#include "stm32f4_discovery.h"
#include "string.h"
#include "sd_operations.h"
#include "sdio_sd.h"

#include "tm_stm32f4_nrf24l01.h"
#include "tm_stm32f4_spi.h"


uint8_t dataIn[32];
uint32_t TimingDelay;           //���������� ��� ����������� �������� ����� ���������� SysTick

//���������� ��� ������� ������� ��� ���������� ���������� ����
uint32_t RTC_upd_period = 10*1000;      //�������� ��� ���� ���������������� �����
uint32_t PING_period;                   //���� ��������� � ��������
uint32_t WIFI_period;                   //����� ������� ��������� ����� ����������� � WIFI ����� �������

bool sflag;

extern WIFI_STATE WIFI;                 //��������� �� WIFI � ����� �������?

unsigned short irqs;
unsigned long mains;

extern short battery;
extern short X, Y, Z;
extern unsigned int milliseconds;
extern unsigned int cmd_timout;

extern unsigned long long cnt_timer;    //������� ��� ����������� ��������� ��������� � ����
extern bool direct;                     //������ ���������� WIFI<->DEBUG USART
extern unsigned short marker;           //��� ������ ������� Hard_Fault

int main(void){
  long long moment;
  RCC_ClocksTypeDef RCC_Clocks;
  

  //�������� ��������� �� ������� ���������� ��-�� ������� ����������
  //��� ����� ��������� �� ���������� ��� �������� � ������� ��������
  //NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0xC000);    //�� ������ ���������� � ���������� ���������� intvec start 0x0800C000  
  //STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);

  LED_init(LED_GREEN);          //GREEN
  LED_init(LED_ORANGE);         //Orange
  LED_init(LED_RED);            //Red
  LED_init(LED_BLUE);           //Blue

  
  SD_POWER_Init();              //INIT SD power enable
  SD_POWER_ON();
  SD_POWER_OFF();
  
  PE7_Init();

 // disk_initialize(0);
  WIFI_POWER_Init();               //INIT WIFI power enable
  WIFI_POWER_ON();
  WIFI_POWER_OFF();
  
  NVIC_Configuration();            //Interrupt Config of DMA for SDIO

//������������� USART ��� WIFI
 USARTInit();
      
//  init_settings();              //������������� �������� �������. ����� ������� ����� ������������� ���������� �������
 // rtc_init();                    //������������� � ��������� RTC
  

  //TICK ��������������� ������ ����� RTC, ����� IRQ ����� �� �����
  // SysTick end of count event each 1ms
  RCC_GetClocksFreq(&RCC_Clocks);
  //SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);


  //������ �������� ������������� ���� ������������ �� �������� ���������� ����
//  battery_init();       // ���
  //acc_init();           // ������������
  
  
  //TM_NRF24L01_Init(2, 32);   // Initialize NRF24L01+ on channel 5 and 32bytes of payload
  
  //save(LOG, "Start %i\r\n", FW_REVISON);
 // DEBUG("(C) D. Bulkin, www.kajacloud.com %i\r\n", FW_REVISON);
  
  
  disk_initialize(0);  //������ ��� ����� ������� �������������� �����,
  mount();
  
 // while(1){
  //  save(LOG, "RUN REC QWERTYUIOPASDFGHJKLZXCVBNMMM1234567890QWERTYUIOPASDFGHJKLZXCVBNMMM1234567890*1235699123456\r\n");
 // }
  
  while(1){
  //  read_all("LOG.TXT");
    1+1;
  }
  //test_write_sd();
    
//  mount();
    //�.�. �� ��������� ����� ���������� ��� ����������������
    //���� ��������. Mount ���� ����������, ����� ������ ��������
  
  //WIFI_set_settings();        //����������� ������ WIFI ��� ������
  
 /* printf("++++");
  Delay(30);
  printf("AT+RST\r\n");
  Delay(500);*/
  
  //a = (int)cnt_timer;
  //for(int j = 0; j < 10; ++j){
   // c = SD_WriteMultiBlocks("123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789", 312832, 512,1);
    //disk_write_single(0,"123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789", 1805+j);
//    c= c + save(LOG, "123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789\r\n");
  //}
  //b = (int)cnt_timer;

 // printf("TMR: %i WRITE: %i\r\n", b-a, c);*/
  
  //����� ��������� ������ �� SD - ����� 10 ��/���. ���� ������ �������
  //25 ��/�: 252 ����� ������, FAT, 2gb Kingston
  //16 ��/c: 252 ����� ������, Fat32 4gb transcend
  //25 ��/c: 252 ����� ������, Fat 4gb transcend!
  //55 ��/� �� ������ diskwrite
  
  //  while(1){
  //  1+1;
  //  }
  //TODO: ��������! ���������� ���������� MAIN � ������� ���� ���������� ~ �� 2000, ��������� ����� �����!
  LED_on(LED_RED);
while (1){
  FLUSH();                              //�������� ����� DEBUG � WIFI. ����� ����� �������.

 /* if (TM_NRF24L01_DataReady()) {
      LED_on(LED_RED);
      for(int i = 0; i < 1; ++i) __no_operation();
      cnt_timer = moment;
      TM_NRF24L01_GetData(dataIn);        // Get data from NRF2L01+
      
      //��������� ������ � ������ ��������� DEBUG
      for(int j = 0; j < 32; ++j){
        if (dataIn[j] == 0) break;      //�� ����-����������� ���������� �������
        DEBUG_processing(dataIn[j]);
      }  
  }  */             
  if (cnt_timer-moment < 2) {
    LED_on(LED_RED);
  } else LED_off(LED_RED);
  
  if (cnt_timer-moment >= 998) cnt_timer = moment;

  if (cnt_timer-moment < 2) {
    if (WIFI == WIFI_ONLINE) LED_on(LED_BLUE);
  } else LED_off(LED_BLUE);
  
  if (cnt_timer-moment >= 998) cnt_timer = moment;
  
  ESP_is_connected();                   //�������� ����������� � ����. ��� ������ ESP
  if (WIFI==WIFI_ONLINE) ESP_server_ping();
//  update_fw();                          //�������� ���������� �� � ����������
  
  WIFI_data_process("main()");          //������������ ������, ����������� �� WI-FI. ���������� ��������� ������� �� �� ����������.
//  DEBUG_processing(0);                  //��������� ������. 0 - ����������. ������ - ��������� ������
  
 
  if (RTC_upd_period==0 AND WIFI==WIFI_ONLINE){   //���� �������� ���� � �� ��������� � ������ �������
    //WIFI_request_datetime();            //���������� ����� � �������. ST
    //ESP_get_datetime();
    RTC_upd_period=10*1000;
    //RTC_upd_period=10*60*1000;          //���� ������ ������ ������� 10*60 ������. ���� ������� ����� - ��������� ������
  }
 
  //TO-DO !!! ������� ������ �� ����� �������� �����
  //TO-DO ������� �������� ���������� WIFI �� ����� �������� �����
  
  if (PING_period==0 AND WIFI==WIFI_ONLINE){      //���� �������� ������� ��� �� online � �����-�� ����� ������ ������������ �����������
    //WIFI_ping();                        //���������� ������ � ������ � ���������� �������������� ����. ��� ST
   // ESP_server_ping();                    // ESP ������ �������� PING
    PING_period=20*1000;                //���� ������ ������ ����� 60 ������.
  }
  
 // ESP_get_commands(); 
  //WIFI_request_commands();              //��� ������������� (���� ���� ����) ����������� �������� � �������. ST
  //command_exec();                       //�������� ���������� � �������� ���������� ������� �������
  
//  flush_to_sd();                        //������� ���������� �� ����������� ��������� ������ �� ������� �� SD. ���� ������ ������ � ������������
  /* Request to enter SLEEP mode */
  //__WFI();

  //WIFI_send_files();                    //�������� ������ �� ������

  mains++;
} // main loop
}

/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in us.
  * @retval None
  */
void Delay(uint32_t nTime){
  //��������! SYS_TICK ���������� ��������� DELAY ���� ��� ������� �� ����������
  TimingDelay = nTime;

  while(TimingDelay != 0)    {
    //��� ������ ����������� ��� ����������� �� ���� ���� �� �� main � �������� ���-�� ����� � ���� ��� ���
    FLUSH();                              //�������� ����� DEBUG � WIFI. ����� ����� �������.
    //��������! ������ ������ ���� �����. ��������� ������������ �����. ����� Delay ���-�� ��� � ����������
    //WIFI_data_process("Delay");                  //������������ ������, ����������� �� WI-FI. ���������� ��������� ������� �� �� ����������.
    flush_to_sd(); 
  }
}

/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void){
  //������������ �������, ������������ ���
  if (TimingDelay != 0x00)
  {
    TimingDelay--;
  }
  
  //������������ ��������, �� ����������� ���
  if (RTC_upd_period    != 0)      RTC_upd_period--;
  if (PING_period       != 0)      PING_period--;
  if (WIFI_period       != 0)      WIFI_period--;
  if (cmd_timout        != 0)      cmd_timout--; 
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line){
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

