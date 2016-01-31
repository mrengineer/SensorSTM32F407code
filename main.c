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
uint32_t TimingDelay;           //Переменная для организации задержек через прерывание SysTick

//Переменные для отсчета времени без прерывания исполнения кода
uint32_t RTC_upd_period = 10*1000;      //Проверка что пора синхронизировать время
uint32_t PING_period;                   //Пора связаться с сервером
uint32_t WIFI_period;                   //Через сколько проверить снова подключение к WIFI точке доступа

bool sflag;

extern WIFI_STATE WIFI;                 //Подключен ли WIFI к точке доступа?

unsigned short irqs;
unsigned long mains;

extern short battery;
extern short X, Y, Z;
extern unsigned int milliseconds;
extern unsigned int cmd_timout;

extern unsigned long long cnt_timer;    //Счетчик для организации различных таймаутов в коде
extern bool direct;                     //Прямое соединение WIFI<->DEBUG USART
extern unsigned short marker;           //Для поиска причины Hard_Fault

int main(void){
  long long moment;
  RCC_ClocksTypeDef RCC_Clocks;
  

  //Сдвигаем указатель на таблицу прерываний из-за наличия бутлоадера
  //Без этого программа не заработает при прошивке и отладки напрямую
  //NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0xC000);    //не забыть установить в настройках линковщика intvec start 0x0800C000  
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

//Инициализация USART под WIFI
 USARTInit();
      
//  init_settings();              //Инициализация настроек прибора. Нужно сделать ранее инициализации прерываний таймера
 // rtc_init();                    //Инициализация и установка RTC
  

  //TICK иницализировать только после RTC, иначе IRQ тиков не будет
  // SysTick end of count event each 1ms
  RCC_GetClocksFreq(&RCC_Clocks);
  //SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);


  //Срочно проводим инициализации пока акселерометр не подвесил выполнение кода
//  battery_init();       // ацп
  //acc_init();           // Акселерометр
  
  
  //TM_NRF24L01_Init(2, 32);   // Initialize NRF24L01+ on channel 5 and 32bytes of payload
  
  //save(LOG, "Start %i\r\n", FW_REVISON);
 // DEBUG("(C) D. Bulkin, www.kajacloud.com %i\r\n", FW_REVISON);
  
  
  disk_initialize(0);  //Всякий раз перед записью инициализируем карту,
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
    //т.к. ее отключаем через транзистор для энергосбережения
    //НАДО УЛУЧШИТЬ. Mount надо постоянный, иначе падает скорость
  
  //WIFI_set_settings();        //Настраиваем модуль WIFI для работы
  
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
  
  //ОЧЕНЬ МЕДЛЕННАЯ ЗАПИСЬ НА SD - около 10 кб/сек. Надо искать причину
  //25 кб/с: 252 байта строка, FAT, 2gb Kingston
  //16 кб/c: 252 байта строка, Fat32 4gb transcend
  //25 кб/c: 252 байта строка, Fat 4gb transcend!
  //55 кб/с на чистом diskwrite
  
  //  while(1){
  //  1+1;
  //  }
  //TODO: ВНИМАНИЕ! КОЛИЧЕСТВО ВЫПОЛНЕНИЙ MAIN в СЕКУНДУ надо ограничить ~ до 2000, остальное время спать!
  LED_on(LED_RED);
while (1){
  FLUSH();                              //Передача между DEBUG и WIFI. Обмен через буфферы.

 /* if (TM_NRF24L01_DataReady()) {
      LED_on(LED_RED);
      for(int i = 0; i < 1; ++i) __no_operation();
      cnt_timer = moment;
      TM_NRF24L01_GetData(dataIn);        // Get data from NRF2L01+
      
      //Перенесем данные в буффер обработки DEBUG
      for(int j = 0; j < 32; ++j){
        if (dataIn[j] == 0) break;      //На нуль-терминаторе прекращаем перенос
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
  
  ESP_is_connected();                   //Проверка подключения к сети. Для модуля ESP
  if (WIFI==WIFI_ONLINE) ESP_server_ping();
//  update_fw();                          //Проверка обновлений ПО и скачивание
  
  WIFI_data_process("main()");          //Обрабатываем данные, поступившие от WI-FI. Специально обработка сделана не из прерывания.
//  DEBUG_processing(0);                  //Обработка данных. 0 - обработать. Другое - поместить символ
  
 
  if (RTC_upd_period==0 AND WIFI==WIFI_ONLINE){   //Пора обновить часы и мы соединены с точкой доступа
    //WIFI_request_datetime();            //Спрашивает время у сервера. ST
    //ESP_get_datetime();
    RTC_upd_period=10*1000;
    //RTC_upd_period=10*60*1000;          //Пока ставим период запроса 10*60 секунд. Если получим время - установим больше
  }
 
  //TO-DO !!! НИКАКИХ ПИНГОВ ВО ВРЕМЯ ОТПРАВКИ ФАЙЛА
  //TO-DO никакой проверки готовности WIFI во время отправки файла
  
  if (PING_period==0 AND WIFI==WIFI_ONLINE){      //Пора сообщить серверу что мы online и какое-то время готовы обмениваться информацией
    //WIFI_ping();                        //Спрашивает статус у модуля и выставляет соответсвующий флаг. Для ST
   // ESP_server_ping();                    // ESP модуль отправит PING
    PING_period=20*1000;                //Пока ставим период пинга 60 секунд.
  }
  
 // ESP_get_commands(); 
  //WIFI_request_commands();              //При необходимости (если есть флаг) запрашиваем комманды с сервера. ST
  //command_exec();                       //Проверка выполнения и таймаута выполнения текущей команды
  
//  flush_to_sd();                        //перенос полученных по результатам измерений данных из буффера на SD. Если буффер близок к переполнению
  /* Request to enter SLEEP mode */
  //__WFI();

  //WIFI_send_files();                    //Отправка файлов на сервер

  mains++;
} // main loop
}

/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in us.
  * @retval None
  */
void Delay(uint32_t nTime){
  //ВНИМАНИЕ! SYS_TICK прикращает декремент DELAY если его вызвать из прерывания
  TimingDelay = nTime;

  while(TimingDelay != 0)    {
    //Это должно выполняться вне зависомости от того ушли мы из main и остались что-то ждать в коде или нет
    FLUSH();                              //Передача между DEBUG и WIFI. Обмен через буфферы.
    //Внимание! Нельзя делать этот вызов. Возникает переполнение стека. Видно Delay где-то еще в прерывниях
    //WIFI_data_process("Delay");                  //Обрабатываем данные, поступившие от WI-FI. Специально обработка сделана не из прерывания.
    flush_to_sd(); 
  }
}

/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void){
  //Обслуживание таймера, прерывающего код
  if (TimingDelay != 0x00)
  {
    TimingDelay--;
  }
  
  //Обслуживание таймеров, не прерывающих код
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

