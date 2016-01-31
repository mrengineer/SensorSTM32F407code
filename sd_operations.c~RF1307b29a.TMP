#include "main.h"
#include "stm32f4xx.h"
#include "sdio_sd.h"
#include "usart.h"
#include "stm32f4_discovery.h"
#include "sd_operations.h"
#include "logic.h"

//Software
#include "integer.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// chan-lib "stuff"
#include "ff.h"
#include "ffconf.h"
#include "diskio.h"

extern char fileRaw[13];                       //Имя файла для записи

extern unsigned int milliseconds;       //Миллисекунды RTC
extern unsigned char seconds;           //rtc seconds
extern unsigned char minutes;
extern unsigned char hours;             //rtc hours

extern unsigned long long cnt_timer;    //Счетчик для организации различных таймаутов в коде

extern Settings SETS;                   //настройки датчика

extern bool sd_in_use;


//Буфер для записи на карточку и уменьшения числа включений карточки
char SaveBuff1[SD_WRITE_BUFFER];
char SaveBuff2[SD_WRITE_BUFFER];

char *mem = &SaveBuff1[0];
char *sd  = &SaveBuff2[0];

//Для работы с файловой системой
DWORD acc_size;				// Work register for fs command 
WORD acc_files, acc_dirs;
FILINFO finfo;
FILINFO Finfo;

FATFS Fatfs;				// File system object for each logical drive

#define LOG_LEN_MAX 32


/* Private typedef -----------------------------------------------------------*/
typedef enum {FAILED = 0, PASSED = !FAILED} TestStatus;

SD_Error Status = SD_OK;

/* Private functions ---------------------------------------------------------*/

GPIO_InitTypeDef GPIO_InitStructure;


  // Setup SysTick Timer for 1 msec interrupts.
  //   ------------------------------------------
  //  1. The SysTick_Config() function is a CMSIS function which configure:
  //     - The SysTick Reload register with value passed as function parameter.
  //     - Configure the SysTick IRQ priority to the lowest value (0x0F).
  //     - Reset the SysTick Counter register.
  //     - Configure the SysTick Counter clock source to be Core Clock Source (HCLK).
  //     - Enable the SysTick Interrupt.
  //     - Start the SysTick Counter.
  //  
  //  2. You can change the SysTick Clock source to be HCLK_Div8 by calling the
  //     SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8) just after the
  //     SysTick_Config() function call. The SysTick_CLKSourceConfig() is defined
  //     inside the misc.c file.

  //  3. You can change the SysTick IRQ priority by calling the
  //     NVIC_SetPriority(SysTick_IRQn,...) just after the SysTick_Config() function 
  //     call. The NVIC_SetPriority() is defined inside the core_cm4.h file.

  //  4. To adjust the SysTick time base, use the following formula:
                            
  //       Reload Value = SysTick Counter Clock (Hz) x  Desired Time base (s)
    
  //     - Reload Value is the parameter to be passed for SysTick_Config() function
  //     - Reload Value should not exceed 0xFFFFFF
 

void NVIC_Configuration(void){
 /* NVIC_InitTypeDef NVIC_InitStructure;

  //Configure the NVIC Preemption Priority Bits
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

  NVIC_InitStructure.NVIC_IRQChannel                    = SDIO_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  
  NVIC_InitStructure.NVIC_IRQChannel                    = SD_SDIO_DMA_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 1;
  NVIC_Init(&NVIC_InitStructure);  */
  
  NVIC_EnableIRQ(SDIO_IRQn);
  NVIC_SetPriority (SDIO_IRQn, 0);
  NVIC_EnableIRQ(SD_SDIO_DMA_IRQn);
  NVIC_SetPriority (SD_SDIO_DMA_IRQn, 0);
}

void settings_to_sd(void) {
   //Удалить старые настройки
  char * filename = "settings.ini";
  
  DEBUG("WRITE SETTINGS... ");
   
   if (f_mount(&Fatfs, "0:", 1) == FR_OK){                        //если все открылось нормально
         f_unlink(filename);                                    //Удалим файл
         save(SETTINGS, "<SensorID>%i</SensorID>\r\n",                          SETS.SensorID);
         save(SETTINGS, "<XOperationMode>%i</XOperationMode>\r\n",              SETS.XOperationMode);
         save(SETTINGS, "<YOperationMode>%i</YOperationMode>\r\n",              SETS.YOperationMode);
         save(SETTINGS, "<ZOperationMode>%i</ZOperationMode>\r\n",              SETS.ZOperationMode);
         save(SETTINGS, "<GravityInfluenceAxis>%c</GravityInfluenceAxis>\r\n",  SETS.GravityInfluenceAxis);
         save(SETTINGS, "<Window>%i</Window>\r\n",                              SETS.Window);
         save(SETTINGS, "<Sensivity>%i</Sensivity>\r\n",                        SETS.Sensivity);
         save(SETTINGS, "<Filter>%i</Filter>\r\n",                              SETS.Filter);
         save(SETTINGS, "<ZeroPoints>%i</ZeroPoints>\r\n",                      SETS.ZeroPoints);
         save(SETTINGS, "<SSID>%s</SSID>\r\n",                                  SETS.SSID);
         save(SETTINGS, "<pass>%s</pass>\r\n",                                  SETS.pass);
         f_mount(NULL, "0:", 1);      //Unmount
    }
    
    DEBUG("DONE\r\n");
}

void read_all(char * filename){
  FIL file;
  
  char buff[64];       //Буффер для чтения в него
  if (f_mount(&Fatfs, "0:", 1) == FR_OK){;                         //Монтируемся
     char r=f_open(&file, filename, FA_READ);                  //Открываем
      while (f_gets(buff, sizeof(buff), &file)) 1+1;
            // DEBUG("LOG: %s\r\n", buff);
      f_close(&file);                                        //Закрываем
      f_mount(NULL, "0:", 1); //Unmount
   }
}  
  
int save_buffer (FileType FT, char *buff){
  FIL file;
  char* filename = "log.txt";
    
  switch (FT) {    
  case DATA:
     filename = "actual.dat";
      break;
  case LOG:
      filename = "log.txt";
      break;
  case SETTINGS:
      filename = "settings.ini";
      break;   
  }
  
  unsigned int bw;    //Кол-во записанных байтов 
  
  if (f_mount(&Fatfs, "0:", 1) == FR_OK){                        //Монтируемся
    f_open (&file, filename, FA_WRITE | FA_OPEN_ALWAYS);        //Открываем или создаем
    f_lseek(&file, f_size(&file));                              //Идем в конец файла
    f_write(&file, buff, strlen(buff), &bw);	                //Write data to the file
    f_close(&file);                                             //Закрываем и тем самым производим запись на диск
    f_mount(NULL, "0:", 1); //Unmount
  }
  
  return 0;
}

//Запись свежих данных в память с форматированием
int save (FileType FT, char const* fmt, ... ) {     
  FIL file;

    char* filename = "log.txt";
    
  switch (FT) {    
  case DATA:
     filename = "actual.dat";
      break;
  case LOG:
      filename = "log.txt";
      break;
  case SETTINGS:
      filename = "settings.ini";
      break;
  case DOWNLOAD:
      filename = "download.tmp";
      break;
  }
                              //В какой файл писать

  unsigned int bw;    //Кол-во записанных байтов
  char buff[1024+1];

  va_list   uk_arg;
  va_start (uk_arg, fmt);

  int slen = vsnprintf(buff, 1024, fmt, uk_arg);       //Форматирование строки
      if (slen > 1024) slen = 1024;
    
 
  int a, b;

  //  LED_on(LED5);


//LED_on(LED_GREEN);
  f_open(&file, filename, FA_WRITE | FA_OPEN_ALWAYS);    //Открываем или создаем
  f_lseek(&file, f_size(&file));                         //Идем в конец файла
    a = (int)cnt_timer;
    f_write(&file, buff, slen, &bw);	                // Write data to the file
    f_sync(&file);
    b = (int)cnt_timer;
  f_close(&file);                                        //Закрываем
//LED_off(LED_GREEN);

        
  return (b-a);
  }

void unmount (){
  f_mount(NULL, "0:", 1); //Unmount
}

void mount (){
    if (f_mount(&Fatfs, "0:", 1) == FR_OK){                         //Монтируемся
    //OK
    } else {
    //Ошибка
    }
}

//Открывает файл download.tmp для записи
/*
void open_download_file (){
   disk_initialize(0);  //Всякий раз перед записью инициализируем карту, 
   //т.к. ее отключаем через транзистор для энергосбережения
   //НАДО УЛУЧШИТЬ

  
   if (f_mount(&Fatfs, "0:", 1) == FR_OK){                     //Монтируемся
         f_open(&download_file, "download.tmp", FA_CREATE_ALWAYS);      //Открываем файл. Существующий, если он сеть, очищаем
         f_lseek(&download_file, f_size(&download_file));                        //Идем в конец файла
  }
}

void write_download_file (char *buff) {
    unsigned int bw;    //Кол-во записанных байтов
    f_write(&download_file, buff, strlen(buff), &bw);	                // Write data to the file
    f_sync(&download_file);                                             //flush data
}

void close_download_file () {
         f_close(&download_file);                                       //Закрываем
         f_mount(NULL, "0:", 1);                                        //Unmount
}
*/

//Сохраняет новые данные. Сначала в буфер. Потом на картчку.
void save_value(char Axis, float Filtered, float a, float b){
  RTC_TimeTypeDef   RTC_Time;
  RTC_DateTypeDef   RTC_Date;
  
  //Получаем текущие время и дату
  RTC_GetTime(RTC_Format_BIN, &RTC_Time);
  RTC_GetDate(RTC_Format_BIN, &RTC_Date);
  //milliseconds //Здесь миллисекунды
  
  char buff[256];                                //Куда сохраним сторку + \0
  static long rec_num;
  
  rec_num++;
    
   //Формируем строку с записью.
  int slen = sprintf(buff, "%0.6d %0.2d-%0.2d-%0.2d %0.2d:%0.2d:%0.2d.%0.3d %c %0.3f %0.3f %0.3f\r\n", rec_num, 
                     RTC_Date.RTC_Year, RTC_Date.RTC_Month,   RTC_Date.RTC_Date, 
                     RTC_Time.RTC_Hours,     RTC_Time.RTC_Minutes, RTC_Time.RTC_Seconds,
                     milliseconds,
                     Axis, Filtered, a, b);       //Форматирование строки
  
  //Если буффер уже не вместит новые данные, подменяем буффер.
  //Буффер в памяти сделан для того, чтобы можно было отключать SD-карточу и не тратить энергию.
  if (strlen(mem) + slen+1 >= SD_WRITE_BUFFER) {
    if (mem != &SaveBuff1[0]){
      mem = &SaveBuff1[0];
      sd  = &SaveBuff2[0];
    } else {
      mem = &SaveBuff2[0];
      sd  = &SaveBuff1[0];   
    }
    
     memset (mem, (char)'\0', SD_WRITE_BUFFER);
    
  }
  
  //Добавим во временный буфер
  strcat(mem, buff);
 

  //Проверка необходимости переноса данных на карточку во избежание переполнения 
  //ФИФО вызывается из main чтобы не задерживать прерывание
  //Но при отправке файлов он функция записи на SD вызывается слишком редко!
}

//Сохраняет новые данные. Сначала в буфер. Потом на картчку.
void save_raw_values(int X, int Y, int Z){
  char buff[256];                                //Куда сохраним сторку + \0
  static long rec_num;
  
  rec_num++;
    
   //Формируем строку с записью.
  int slen = sprintf(buff, "%0.8d %0.2d %0.2d %0.2d %0.3d %0.3d %0.3d %0.3d\r\n", rec_num, hours, minutes, seconds, milliseconds, X, Y, Z);       //Форматирование строки
  
  //Если буффер уже не вместит новые данные, подменяем буффер.
  //Буффер в памяти сделан для того, чтобы можно было отключать SD-карточу и не тратить энергию.
  if (strlen(mem) + slen+1 >= SD_WRITE_BUFFER) {
    if (mem != &SaveBuff1[0]){
      mem = &SaveBuff1[0];
      sd  = &SaveBuff2[0];
    } else {
      mem = &SaveBuff2[0];
      sd  = &SaveBuff1[0];   
    }
    
     memset (mem, (char)'\0', SD_WRITE_BUFFER);
    
  }
  
  //Добавим во временный буфер
  strcat(mem, buff);
 

  //Проверка необходимости переноса данных на карточку во избежание переполнения 
  //ФИФО вызывается из main чтобы не задерживать прерывание
  //Но при отправке файлов он функция записи на SD вызывается слишком редко!
}

void flush_to_sd (void){
    FIL file;
    UINT bytes_written;
    unsigned int filelen;
    char sendfilename[13];      //для формирования имени нового файла
    unsigned short rest;        //Остаток буффера после деления на блоки WRITE_SAFE_BLOCK = 256 байт
    unsigned short t;           //Число блоков WRITE_SAFE_BLOCK = 256 байт
    char *wr;                   //Для ссылки на массив sd чтобы не инкрементировать адрес символа в самом sd
    unsigned short len;         //Объем буффера
   // unsigned short mss;
    

    //В буффер mem пишутся данные. Как только их там становится много буфферы mem и sd взаимно меняются
    //сделано чтобы не было проблем с одновременной работой измерений и записи
    len = strlen(sd);
    
  if (len){  //Не пора ли переносить данные на крточку? А то переполнится буффер mem
    //Не произвожу запись минуя буфер при помощи f_putc, т.к. я опасаюсь длительных задержек в выполнении прерывания
    //В то же время буффер должен пополняться по прерыванию вне зависимости от записи
    //Вообще все это затеяно чтобы резже включать питание карточки
    //DEBUG("TRY FLUSH TO SD %i bytes\r\n", len);
    //RTC_DateTimePrint();
  
      if (f_mount(&Fatfs, "0:", 1) == FR_OK){                           //если все открылось нормально
        //LED_on(LED_GREEN);
        
          //Пишем кусочками по 256 байт, иначе возникают ошибки. Где-то в FATFS баг. Могут пропускаться или повторяться символы при записи файла если писать сразу много. Происходит случайным образом.
          wr = sd;                              //Копируем указатель в новый указатель чтобы его можно было без проблем инкрементировать
          t = len/WRITE_SAFE_BLOCK;             //Целое число блоков
          rest =  len % WRITE_SAFE_BLOCK;       //Остаток буффера не кратный 256
          

         // f_open(&file, "actual.dat", FA_WRITE | FA_OPEN_ALWAYS);        //Открываем или создаем
           f_open(&file, fileRaw, FA_WRITE | FA_OPEN_ALWAYS);        //Открываем или создаем
          f_lseek(&file, f_size(&file));                                 //Идем в конец файла
          
          for (unsigned short k = t; k != 0; k--){
            //Пишем WRITE_SAFE_BLOCK байт из wr в файл. если писать больше - будут пропадать символы
            f_write(&file, wr, WRITE_SAFE_BLOCK , &bytes_written);                      
            wr += WRITE_SAFE_BLOCK;         //Сдвигаем буффер
          }
          
          f_close(&file);      
          //И остался еще кусочек буффера не кратный WRITE_SAFE_BLOCK байт
          //пишем rest байт
          
        
/*          f_open(&file, "actual.dat", FA_WRITE | FA_OPEN_ALWAYS);        //Открываем или создаем
          f_lseek(&file, f_size(&file));                                 //Идем в конец файла

          f_write(&file, wr, rest, &bytes_written);
//        DEBUG("BYTES WRITTEN %i\r\n", bytes_written);
          
          filelen = file.fsize;
         
          f_close(&file);*/
         
          //DEBUG("DONE\r\n");
          //RTC_DateTimePrint();
          
         //Если файл уже слишком большой, пометим его на отправку, а писать будем в новый.
         //Переименование файла не требует его блокирования, так как из прерывания сейчас идет запись в буффер
         //И записи в файл сейчас происходить просто не может
         
 /*        if (filelen > MAX_FILE_SIZE){
            //Определяем новое имя для файла. Генерим пока оно не станет уникальным
            generate_name:
            sprintf(sendfilename, "%0.2d%0.2d%0.4d.TS", minutes, seconds, milliseconds);
            if (f_stat(sendfilename, NULL) == FR_OK) goto generate_name;
                
            //Переименовали actual.dat
            f_rename("actual.dat", sendfilename);          //Переименовываем файл
            DEBUG("MARK file as '%s' for post to server\r\n", sendfilename);
         }*/
         
         f_mount(NULL, 0, 1);
         //LED_off(LED_GREEN);
      } else {
        DEBUG("SD mount FAIL!\r\n");
      }
    
    sd[0] = 0; //Иначе снова будем писать на карточку то же самое
  } //if
  
}

void test_write_sd (void){
      unsigned short rest;        //Остаток буффера после деления на блоки WRITE_SAFE_BLOCK = 256 байт
    unsigned short t;           //Число блоков WRITE_SAFE_BLOCK = 256 байт
    char *wr;               //Для ссылки на массив sd чтобы не инкрементировать адрес символа в самом sd
    
    FIL file;
    UINT bytes_written;
    char bfr[512*2];
    
    short rt=0;
    
int slen = sprintf(bfr, "QWERTYUIOPASDF1234567890-=- --QWERTYUIOPASDF1234567890-=-906QWERTYUIOPASDF1234567890-=- PASDF1234567890-=-906QWERTYUIOPASDF1234567890-=PASDF1234567890-=-906QWERTYUIOPASDF1234567890-=PASDF1234567890-=-906QWERTYUIOPASDF1234567890-= --QWERTYUIOPASDF1234567890-=-90699202321QWERTYUIOPASDF1234567890-=- --QWERTYUIOPASDF1234567890-=-906QWERTYUIOPASDF1234567890-QWERTYUIOPASDF1234567890-=- --QWERTYUIOPASDF1234567890-=-906QWERTYUIOPASDF1234567890-=- PASDF1234567890-=-906QWERTYUIOPASDF1234567890-=PASDF1234567890-=-906Q78687435\r\n");       //Форматирование строки
    
 printf("TRY write SD %i Kbytes\r\n", slen*1000/1024);
  RTC_DateTimePrint();

 
      if (f_mount(&Fatfs, "0:", 1) == FR_OK){                           //если все открылось нормально

       // LED_on(LED_GREEN);
        
          //Пишем кусочками по 256 байт, иначе возникают ошибки. Где-то в FATFS баг. Могут пропускаться или повторяться символы при записи файла если писать сразу много. Происходит случайным образом.
          wr = bfr;                              //Копируем указатель в новый указатель чтобы его можно было без проблем инкрементировать
          t = slen/WRITE_SAFE_BLOCK;             //Целое число блоков
          rest =  slen % WRITE_SAFE_BLOCK;       //Остаток буффера не кратный 256
          


          f_unlink("test.dat");
                  while (rt<100) {
                    f_open(&file, "test.dat", FA_WRITE | FA_OPEN_ALWAYS);        //Открываем или создаем
                    f_lseek(&file, f_size(&file));                                 //Идем в конец файла
          
                    for (unsigned short k = t; k != 0; k--){
                      //Пишем WRITE_SAFE_BLOCK байт из wr в файл. если писать больше - будут пропадать символы
                      f_write(&file, wr, WRITE_SAFE_BLOCK , &bytes_written);                      
                      wr += WRITE_SAFE_BLOCK;         //Сдвигаем буффер
                    }

                    
                    f_close(&file);      
                    
                    //И остался еще кусочек буффера не кратный WRITE_SAFE_BLOCK байт
                    //пишем rest байт
                    
                  
                  //  f_open(&file, "test.dat", FA_WRITE | FA_OPEN_ALWAYS);        //Открываем или создаем
                  //  f_lseek(&file, f_size(&file));                                 //Идем в конец файла

                  //  f_write(&file, wr, rest, &bytes_written);
                  //  f_close(&file);
                   rt++;
                  }
         

          printf("DONE\r\n");
          RTC_DateTimePrint();
         
         f_mount(NULL, 0, 1);
        // LED_off(LED_GREEN);
        
      } else {
        printf("SD mount FAIL!\r\n");
      }
      
      while(1){
      }
 
}


