#include "main.h"
#include "logic.h"
#include "rtc.h"
#include "stm32f4xx.h"
#include "acc.h"
#include "math.h"
#include "usart.h"
#include <stdint.h>
#include <float.h>
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "sd_operations.h"
#include "fifo.h"
#include "stm32f4_discovery_lis302dl.h"


// chan-lib "stuff"
#include "ff.h"
#include "ffconf.h"
#include "diskio.h"

#define RTC_UPD         6               //Запрашивать время пока оно не установлено каждые RTC_UPD секунд.
#define ADJ             0.8             //Порог отклонения по которому считаются новые a и b при фильтрации лишних данных
#define MAX_BUFFER      1000            //Максимальный размер буффера с данными для сглаживания
#define READS           10000           //На протяжении скольки считываний  не должно быть сохранений при подстройке окна фильтрации

extern unsigned short marker;           //Для поиска причины Hard_Fault

extern unsigned short acc_reads;        //Число считываний акселерометра. Сбрасывается каждую секунду для счета по-новой.
extern unsigned short battery;          //Переменная с зарядом батареи
extern uint32_t PING_period;            //Период пинга
extern uint32_t RTC_upd_period;         //Период запроса времени
extern int32_t WIFI_period;             //Период проверки подключения к WIFI

extern bool sflag;

extern FATFS Fatfs;

enum device_modes {                     //Состояния устройства
  IDLE                  = 0,            //Ничего не делаю
  FILTER_ADJUST         = 1,            //Подстройка фильтрации
  NORMAL_OPERATION      = 2,            //Штатное состояние. Сбор данных, отправка на сервер по мере надобности. Пинги.
  SLEEP_OPERATION       = 3,            //Работа в состоянии SLEEP
  FW_DOWNLOAD           = 4             //Состояние загрузки нового ПО
};


unsigned char MODE = IDLE;                      //Режим работы устройства.
unsigned char MODE_BEFORE_COMMAND = IDLE;       //Режим работы устройства до начала выполнения команды

WIFI_STATE WIFI=WIFI_OFFLINE;                   //Подключен ли WIFI к точке доступа? Есть ли доступ к серверу?
char WIFI_searches;                             //Сколько раз пытались найти сеть. Для ST

//Для модуля на ESP
bool fESP_sock_is_open;                           //Модуль WIFI открыл сокет. Используется только для модуля ESP
bool fESP_send_ready;                             //Получен символ '>' как приглашение передавать данные в сокет модуля на ESP
bool fDataEnd;                                    //Устанавливаеся true если пришел конец передачи данных </DATA> чтобы не ожидать timeout

extern bool direct;

extern unsigned int milliseconds;       //Текущие миллисекунды rtc. Сбрасываются в ноль каждую секунду
extern unsigned char seconds;           //rtc seconds
extern unsigned char minutes;           //rtc minutes
extern unsigned char hours;             //rtc hours
extern unsigned long long cnt_timer;    //Счетчик для организации различных таймаутов в коде


//Для испытаний датчика и получения RAW данных
bool fRawRecord = false;                //Для испытаний Писать ли в файл
char fileRaw[13];                       //Имя файла для записи

Settings SETS;                          //настройки датчика

float corrX, corrY, corrZ;              //Корректоры X Y Z для учета смещения 0. Например, из-за гравитации. Периодически обновляются.

//Число данных должно быть кратно 2 одинаково с define в usart.c!
char WIFIdata[WIFI_DATA_BUFFER];        //Буффер для накопления данных от WIFI для последующей обработки вызовом из main +1 на /0
unsigned short WIFIStartIx = 0;          //Индекс НАЧАЛЬНОГО символа
unsigned short WIFICount = 0;            //Индекс число хранимых символов

//FATFS нещадно насилует SD, поэтому буффер комманд в памяти а не в файле
bool request_commands = false;          //Флаг необходимости запросить команды с сервера
//Для обработки команд
unsigned int CmdIdExec;                 //ID выполняемой команды. 0 = никакая
unsigned int cmd_timout;                //Таймаут на выполнение команды

//Для автоподстроки фильтра
unsigned int new_reads;                 //число значений считываний аккселерометра начиная с момента последней установки окна сглаживания
bool new_saves = false;                 //признак, что после установки нового окна сглаживания были отклонения от прямой. Т.е. шумы слишком большие

//Отправка файла
bool fWIFI_file_sent = false;           //Файл принят сервером. См функции отправки файла через post

//Идентификатор сокета для отправки POST запросов
signed char SocketID            = -1;   //Приходит по открытию сокета, например ID: 00
signed short SocketDataLen      = 0;    //Сколько сейчас в сокете байтов (AT.S+SOCKQ...)

bool fWIFI_OK           = false;        //Флаг - признак что мы получили OK от модуля в ответ на команду
bool WIFI_need_connect  = false;        //Признак необходимости соединиться с wifi

unsigned long long last_data_moment;    //Когда были получены последние данные через сокет. Нужно для таймаута в WIFI_TimeOut_Sock
unsigned int download_lines_count;      //кол-во строк в скачиваемом файле. Нужно чтобы запрашивать построчно файл с сревера, иначе запиь на SD не поспевает за приходящими данными
unsigned int last_downloaded_line;      //Номер последней скачаной (и записанной ф файл) строки

void WIFIdataDel (void) {               //Удаляет из буффера 1й байт
    WIFICount--;
    WIFIStartIx++;
    if (WIFIStartIx >= WIFI_DATA_BUFFER) WIFIStartIx = WIFIStartIx - WIFI_DATA_BUFFER;
}

void WIFIdataAdd (char data){           //Добавляет в буффер
   
  unsigned short ix;
  if (WIFICount !=  WIFI_DATA_BUFFER){                  //если Буффер еще не полон
                                                        //Добавляем символ
    if (WIFIStartIx + WIFICount < WIFI_DATA_BUFFER) {   //Определим индекс этого самого символа
        ix = WIFIStartIx + WIFICount;
    } else {
        ix = WIFIStartIx + WIFICount - WIFI_DATA_BUFFER;
    }
    
    WIFIdata[ix] = data;
    WIFICount++;
  } 
}

char WIFIdataGet (unsigned short ix){
  unsigned short i;
  
  if (ix <= WIFICount){                                 //Не превышаем объем буффера
        if (WIFIStartIx + ix < WIFI_DATA_BUFFER) {      //Определим индекс этого искомого символа
          i = WIFIStartIx + ix;
        } else i =  WIFIStartIx+ix - WIFI_DATA_BUFFER;   //Если за пределы длинны масива
        
        return WIFIdata[i];
  } else {      //Превышает объем буффера
    return NULL;
  }
}

//Сначала уточняем нет ли смещения нуля Если есть, рассматриваем его обновление
void adjust_zero (signed short X, signed short Y, signed short Z){
  static float prevX, prevY, prevZ;      //Предыдущие сглаженные значения по осям
  float cX, cY, cZ;                      //Новые сглаженные значения по осям. Только вычисленные
  float dcX, dcY, dcZ;                   //Производные сглаженных кривых
  static unsigned int unchanged;         //Сколько измерений не притерпело изменений, те. dc(?) = 0. Важно, чтобы площадка на ускорении ускорения была заведомо меньше этого времени.


   cX = (1.0/(float)SETS.Window)*(float)X;
   cX += (((float)SETS.Window-1.0)/(float)SETS.Window)*prevX;        //X/w + (prevX*(w-1)/w)

   dcX = cX - prevX;                                                    //Вычисляем производную
   prevX = cX;                                                          //Сохраняем текущее значение для следующего расчета


   cY = (1.0/(float)SETS.Window)*(float)Y;
   cY += (((float)SETS.Window-1.0)/(float)SETS.Window)*prevY;

   dcY = cY - prevY;
   prevY = cY;


   cZ = (1.0/(float)SETS.Window)*(float)Z;
   cZ += (((float)SETS.Window-1.0)/(float)SETS.Window)*prevZ;

   dcZ = cZ - prevZ;
   prevZ = cZ;

   unchanged++;                         //Инкремент счетчика времени неизменности базового уровня (нуля)
   if (fabs(dcX) > 0.03 OR fabs(dcY) > 0.03 OR fabs(dcZ) > 0.03) unchanged = 0;  //Если амплитуда производной ускорения совсем большая, обнуляем счетчик

   if (unchanged > SETS.ZeroPoints ) {  //Если изменений не было заданное число точек, сохраняем новый базовый уровень. // Это время должно сильно превышать переходные процессы, но быть меньше пауз между ними
     if (fabs(cX-corrX) > 1.0) { corrX = cX; DEBUG("New X corrector = %0.2f\r\n", corrX);}  //Если отклонение базового уровня существенное
     if (fabs(cY-corrY) > 1.0) { corrY = cY; DEBUG("New Y corrector = %0.2f\r\n", corrY);}  //Если отклонение базового уровня существенное
     if (fabs(cZ-corrZ) > 1.0) { corrZ = cZ; DEBUG("New Z corrector = %0.2f\r\n", corrZ);}  //Если отклонение базового уровня существенное
     unchanged = 0;
   }
}

void adjust_filter(){
          //NB! При увеличении количества точек буффера (командой из вне) под окно скользящего среднего надо, чтобы сначала по новой заполнился буффер.
          // Иначе даже на сглаженной кривой будет провал, т.к. ранее в добавленных ячейках буффера были 0
          new_reads ++;                             //Инкремент счетчика считываний

          if (new_reads <= SETS.Filter+1) new_saves = false;  //флаг признака что были отклонения от прямой очишаем.
                                                              //Он устанавливается в true вместо сохранения точки

          if (new_reads >  SETS.Filter+10) {                   //Убедимся, что буффер фильтрации полностью заполнен свежими данными.
                                                              //Без нулей в результате его расширения.
             if (new_saves == true) {                         //Увеличиваем окно фильтрации, если по одной из осей в состоянии покоя было сохранение точки

                  SETS.Filter ++;                             //Увеличим окно
                  printf("Set filter=%i on reads %i\r", SETS.Filter, new_reads);
                  new_reads = 0;                              //Сбросим число считываний чтобы не получить нули в буффере фильтра
                  new_saves = false;
              } else {
                if (new_reads == READS + SETS.Filter+10){       //Если READS отсчетов отклонений не было
                  SETS.Filter += 2;                             //Для надежности добавлеям еще две точки
                  command_done("SUCCESS");
                  MODE = NORMAL_OPERATION;                                   //Переходим к режиму нормального функционирования
                  DEBUG("FILTER AJUSTMENT IS FINISHED.\r\nFilter=%i, on step %i\r\n", SETS.Filter, new_reads);
                  settings_to_sd();
                }
              }
           }
}

//Обрабатывает новые данные от сенсора
void process_data(signed short X, signed short Y, signed short Z){
if (fRawRecord) save_raw_values(X, Y, Z);
  //Просто пишем данные в буффер откуда они пойдут на SD
 /*   float fX,  fY,  fZ;                           //Вычисленные осредненные значения на текущий момент
    static Datum buffer[MAX_BUFFER];              //буффер для сглаживания поступающих данных скользящим средним
    static unsigned short cix=0;                  //индекс в буффере

    float difX, difY, difZ;                       //Разница между расчетным и реальным значениями точки
    static float fX0, fY0, fZ0;                   //Сглаженное значение в предыдущей СОХРАНЕННОЙ точке. Т.е. между ней и текущей может быть масса точек

    static float cX, cY, cZ;                      //Расчетное значение точки на линии
    static float aX, aY, aZ;                      //Коэффициенты для X=a*t+b. Уравнение применяется для уменьшения числа сохраняемых точек
    static float bX, bY, bZ;                      //Когда точка укладывается на прямую, она не сохраняется. Когда НЕ укладвается, a и b пересчитываются, а точка сохраняется
    static unsigned long dTx, dTy, dTz;           //Счетчики времени (числа измерений) с момента последнего сохранения точки для вычисления X=a*t+b.
                                                  //Больше UINT32_MAX быть не могут
    static unsigned long Tx0, Ty0, Tz0;           //Нулевая координата по времени для расечта a и b в уравнении. Точек то три X0, X1 и вычисляемая. dT отсчитывается от X1


//1. УЧЕТ СМЕЩЕНИЯ НУЛЯ ИЗ-ЗА НАКЛОНА ПРИБОРА, ДРЕЙФА ТЕМПЕРАТУРЫ
    //Эта функция периодически подправляет базовое значение 0 в ходе работы датчика
    if (MODE != FILTER_ADJUST) adjust_zero(X, Y, Z);          //Сначала уточняем нет ли смещения нуля. Если есть, рассматриваем его обновление
                                                  //Передаются не сглаженные точки потому что там применено другого рода сглаживание с очень большим окном
                                                  //Исключением является автоподстройка фильтра. А то могут быть резких скачков и по
                                                  //производной увеличится окно фильтра

//2. СГЛАЖИВАНИЕ ДАННЫХ (ФИЛЬТРАЦИЯ СКОЛЬЗЯЩИМ СРЕДНИМ)
    //Добавляем в буффер откорректированнное на смещение нуля значение
    buffer[cix].X = (float)X - corrX;
    buffer[cix].Y = (float)Y - corrY;
    buffer[cix].Z = (float)Z - corrZ;

    //Вычисляем сумму и среднее
    for(int j = 0; j < SETS.Filter; ++j){
      fX = fX + ((float)buffer[j].X /(float)SETS.Filter);
      fY = fY + ((float)buffer[j].Y /(float)SETS.Filter);
      fZ = fZ + ((float)buffer[j].Z /(float)SETS.Filter);
    }

    //Помещаем в буффер
    buffer[cix].fX = fX;
    buffer[cix].fY = fY;
    buffer[cix].fZ = fZ;

    cix++;                         //Инкремент индекса в буффере
    if (cix>=SETS.Filter) cix = 0; //Именно больше или равно. Потому что при уменьшении SETS.Filter будут проблемы,
                                   //т.к. cix >> SETS.Filter и == никогда не настанет.

//3. ПОДСТРОЙКА ОКНА СГЛАЖИВАНИЯ, ЕСЛИ ВЫБРАН ЭТОТ РЕЖИМ
    if (MODE == FILTER_ADJUST) {             //Если мы в режиме автоподстройки фильтра
      adjust_filter();
    }

//4. ФИЛЬТРАЦИЯ ИЗБЫТОЧНЫХ ДАННЫХ при помощи линейной интерполяции
//   ВАЖНО ЧТОБЫ ПОНЯТЬ

//    Всякий раз по двум точкам считаются коэффициенты a и b прямой. Пусть это точки X0(T0) и X1(T1)
//    Потом считается прогноз Xc для времени dT относительно времени T1.
//    А T1 на расстоянии от начала координат (T1-T0).
//    При каждом пересчете коэффициентов dT сбрасывается в 0.
//    Вот поэтому в алгоритме фигурирует Xc = aX*(Tx0+dTx) + bX, где Tx0 и есть это T1-T0
//    В свое время не учтя этого я поломал голову.
//    При этом dTx и Tx0 могут при простое быть ооочень большими. Месяцы. А инкремент каждую миллисекунду.
//    365*24*60*60*1000 мс. А unsigned long это не более 49 дней.  Поэтому тип unsigned long, тогда "ненужное" сохранение точки не чаще чем раз 49 дней
    

        //Получили данные. Теперь проводим над ними логическую фильтрацию чтобы понять, нужно ли их сохранять в памяти, или они избыточные
        //Вычисляем разницу между предыдущим сглаженным СОХРАНЕННЫМ значением и текущим измеренным и сглаженным

        //Инкрементируем счетчики количества измерений с момента последнего сохранения точки
        if (dTx < UINT32_MAX) dTx ++;                     //Чтобы не случилось переполнения ограничеваем размером UINT32_MAX
        if (dTy < UINT32_MAX) dTy ++;                     //что соответствует unsigned long
        if (dTz < UINT32_MAX) dTz ++;


        //Считаем какое значение на прямой должно быть в этой точке
        cX = aX * (float)dTx + aX * (float)Tx0 + bX;    //Так записано во избежание переполнения. Не хотелось разбираться с max(float)
        cY = aY * (float)dTy + aY * (float)Ty0 + bY;
        cZ = aZ * (float)dTz + aZ * (float)Tz0 + bZ;

        difX = fX - cX;                                 //Разница между расчетом и измерением
        difY = fY - cY;
        difZ = fZ - cZ;

        
        if (fabs(difX) > (float)ADJ AND fabs(fX0 - fX)>(float)ADJ) {         //Если разница по оси X больше заданного значения, т.е (X - aT+b) > U
        LED_on(LED_RED);                                           //А еще отклонение от предыдущей СОХРАНЕННОЙ точки достаточно слишком большое
        //ВЫЧИСЛЯЕМ КОЭФФИЦИЕНТЫ НОВОЙ ПРЯМОЙ, А ТОЧКУ СОХРАНЯЕМ В ПАМЯТЬ
          aX = (fX - fX0)/(float)dTx;
          bX =  fX - aX*(float)dTx;

          if (MODE != FILTER_ADJUST) {             //Если НЕ в режиме автоподстройки фильтра
            //Мы прошли все проверки и точка действительно ценная. Сохраним ее в файле для последующей передачи на сервер
            if (MODE != IDLE) save_value((char)'X', fX, aX, bX);
          } else {
            new_saves = true;          //В режиме автоподстройки ставим флаг что было отклонение, нужно увеличить окно
          }

          fX0 = fX;
          Tx0 = dTx;    //Сохраняем чтобы знать промежуток времени между T0 и T1 по кторым строится прямая на которой должно лежать или не лежать текущее значение.
                        //Сейчас T1 это и есть тот момент времени когда мы получили вторую точку прямой
          dTx = 0;      //Обнуляем чтобы считать вновь время от последней сохраненной точки
         LED_off(LED_RED);
        }

        if (fabs(difY) > (float)ADJ AND fabs(fY0 - fY)>(float)ADJ) {         //Аналогично для Y
          aY = (fY - fY0)/(float)dTy;
          bY =  fY - aY*(float)dTy;

          if (MODE != FILTER_ADJUST) {             //Если НЕ в режиме автоподстройки фильтра
            if (MODE != IDLE) save_value((char)'Y', fY, aY, bY);
          } else {
            new_saves = true;          //В режиме автоподстройки ставим флаг что было отклонение, нужно увеличить окно
          }

          fY0 = fY;
          Ty0 = dTy;

          dTy = 0;
        }

        if (fabs(difZ) > (float)ADJ AND fabs(fZ0 - fZ)>(float)ADJ) {         //Аналогично для Z
          aZ = (fZ - fZ0)/(float)dTz;
          bZ =  fZ - aZ*(float)dTz;

          if (MODE != FILTER_ADJUST) {             //Если НЕ в режиме автоподстройки фильтра
            if (MODE != IDLE) save_value((char)'Y', fY, aY, bY);
          } else {
            new_saves = true;          //В режиме автоподстройки ставим флаг что было отклонение, нужно увеличить окно
          }

          fZ0 = fZ;
          Tz0 = dTz;

          dTz = 0;
        }*/
}

//Выполнение команды
void command_exec(void){
  if (CmdIdExec > 0){                      //Выполняется команда
    if (cmd_timout == 0) {                 //Получился таймаут команды
      DEBUG("Command %i is timedout\r\n", CmdIdExec);
      MODE = MODE_BEFORE_COMMAND;       //Вернем предыдущий режим работы
      command_done("ERR_TIMEOUT");      //Сообщаем серверу результат выполнения
    }
  }
}

void command_done(char *res){
  DEBUG("i COMMAND %i is DONE\r\n", CmdIdExec);
  printf("AT+S.HTTPGET=www.kajacloud.com,/device/command.php?SensorID=%i&CmdID=%i&done=1&responce=%s\r\n", SETS.SensorID, CmdIdExec, res);
  CmdIdExec     = 0;                    //Никакая команда не выполняется
  cmd_timout    = 0;
  request_commands = true;            //Команда обработана. Можно запрашивать следущую
}

//Обработка строки с командой от сервера
void process_command(char * block){
    char tmp[50];
    char cmd[11];       //Команда +\0 
    
    request_commands = false;   //Гарантия что мы не запросим сейчасже новую новую команду

    get_value(block, "<CMDID>", "</CMDID>", tmp);
    CmdIdExec = atoi(tmp);

    get_value(block, "<TOUT>", "</TOUT>", tmp);
    cmd_timout = (unsigned short)atoi(tmp)*1000;  

    get_value(block, "<COMMAND>", "</COMMAND>", cmd);
    DEBUG("> COMMAND %i %s, timout %i\r\n", CmdIdExec, cmd, cmd_timout);

    get_value(block, "<A1>", "</A1>", tmp);
    DEBUG("Arg1=%s ", tmp);

    get_value(block, "<A2>", "</A2>", tmp);
    DEBUG("Arg2=%s\r\n", tmp); 
    
    //Сообщаем серверу, что команда выполняется
    printf("AT+S.HTTPGET=www.kajacloud.com,/device/command.php?SensorID=%i&CmdID=%i&execution=1\r\n", SETS.SensorID, CmdIdExec);
    Delay(50);
    
    if (strcmp(cmd, "ADJ")==0){           //Пришла команда подстройки
      MODE_BEFORE_COMMAND = MODE;         //Сохраняем предыдущий режим
      MODE = FILTER_ADJUST;
    } else if (strcmp(cmd, "IDLE")==0) {  //Холостая работа
      MODE = IDLE;
      command_done("SUCCESS");
    } else if (strcmp(cmd, "RUN")==0) {   //Запуск работы
      MODE = NORMAL_OPERATION;
      command_done("SUCCESS");
    } else {  
        DEBUG("Unknown command!\r\n");
         //Сообщаем серверу, что команда "выполнена" и не распознана
        command_done("ERR_UNKNOWN_CMD");
        cmd_timout      = 0;
    }
    WIFI_ping();    //Пингуемся, сообщаем свой MODE вновь и просим сколько осталось команд. Получаем новые
}

//Получает содержимое между тегами <...> и </...> в строке
void get_value(char *input_str, char *left_str, char *right_str, char *output_str) {
  //Ищем где начинается ^ в <...>^ 
    char * cmdstr = strstr(input_str, left_str) + strlen(left_str);
    //Сколько символов между <...> и </...>
    int  length = strstr(input_str, right_str) - cmdstr;
    
    //Копируем все, что между тегами в возвращаемую строку
    strncpy(output_str, cmdstr, length); //Узнаем где закрывающий тег и копируем
    
    if (length > 0)  output_str[length] = 0;
    else output_str[0] = 0; //Иначе глюки
}

void init_settings(void){
	SETS.SensorID	          = 1;
	SETS.XOperationMode	  = 0;   //Отключено
        SETS.YOperationMode	  = 0;   //Отключено
        SETS.ZOperationMode	  = 0;   //Отключено
        SETS.GravityInfluenceAxis = 0;   //Отключено
        SETS.AwakeSampleRate      = 500; //500 точек с акселерометра в режиме активности
        SETS.HitDetector          = 0;   //Отключено
        SETS.HitTrishold          = 100; //Порог срабатывания на удар. БЕЗ ФИЛЬТРАЦИИ. NB: Все верно! Удара не будет пока не начнется движение

	SETS.Filter	          = 110;  //Вес каждой новой точки. Для фильтрации. НЕ МЕНЕЕ 10!
        SETS.Sensivity            = 2;   //Чувствительность
	SETS.RadioAwakePeriod     = 5;   //Период через которое просыпается радио и отправляет PING в эфир, сек
        SETS.SleepAfterIdle       = 80;  //Спустя какое время после регистрации события датчик переходит в сотсотяние сна по акселеромуетру
	SETS.SleepSampleRate      = 50;  //50 точек снимается с акселерометра
        SETS.Window               = 200; //Окно сглаживания для определения 0
        SETS.ZeroPoints           = 5000; //Через сколько после начала покоя устанавливать базовый ноль

        strcpy(SETS.SSID, "WirelessNet");
        strcpy(SETS.pass, "AwseDr00");
}

//Если 0, то обрабатываем строчку
void DEBUG_processing  (char data) {
  static char buff[31]; //Буффер для обработки
  char i = strlen(buff);
  char tmp[12];
  static uint8_t y, m, d, h, mi, s;
  static bool tobe_proccessed = false;
  
  if (data != 0){       //Если не ноль просто пишем в строчку символ
    if (i<(31-1-1)) {   //Если индекс буффера не превышает его длинну - 1 символ (один под \0)
      buff[i]  = data;  //Подписываем данные
      buff[i+1]= 0;     //Чтоб число определялось всегда корректно несмотря на старые записи в буффере
    }
    if (data == (char)('\r') OR data == (char)('\n')){  //Если закончилось переводом каретки
      tobe_proccessed = true;
    }
  } else {      //Обработка строки
    if (tobe_proccessed) {
     tobe_proccessed = false;
          //<YR>14</YR>
          //<MO>06</MO>
          //<DY>01</DY>
          //<HR>13</HR>
          //<MN>52</MN>
          //<SE>23</SE>
      if (strstr(buff, "<YR>") AND strstr(buff, "</YR>")) {    
          //Год
          get_value(buff, "<YR>", "</YR>", tmp);
          y = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<MO>") AND strstr(buff, "</MO>")) {
          //Месяц
          get_value(buff, "<MO>", "</MO>", tmp);
          m = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }

      if (strstr(buff, "<DY>") AND strstr(buff, "</DY>")) {
          //День
          get_value(buff, "<DY>", "</DY>", tmp);
          d = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<HR>") AND strstr(buff, "</HR>")) {
          //Час
          get_value(buff, "<HR>", "</HR>", tmp);
          h = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }

      if (strstr(buff, "<MN>") AND strstr(buff, "</MN>")) {
          get_value(buff, "<MN>", "</MN>", tmp);
          mi = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<SE>") AND strstr(buff, "</SE>")) {    
          //Секунды
          get_value(buff, "<SE>", "</SE>", tmp);
          s = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<SETTIME>")) {  
        RTC_SetDateTime(d, m, y, h, mi, s); //Пишем в RTC. Внимание! Он не ставит milliseconds 
        RTC_DateTimePrint();
      }
      
      if (strstr(buff, "<TIME>")) RTC_DateTimePrint();
      
      if (strstr(buff, "<VALUES>")) acc_print();
 
      if (strstr(buff, "<SCALE>") AND strstr(buff, "</SCALE>")) {
        DEBUG("BEFORE: "); acc_print();
        if (strstr(buff, "2.3")){
          DEBUG("Scale is +/-2.3g\r\n");
          LIS302DL_FullScaleCmd(LIS302DL_FULLSCALE_2_3);
        } else { 
          if (strstr(buff, "9.2")){
            LIS302DL_FullScaleCmd(LIS302DL_FULLSCALE_9_2);
            DEBUG("Scale is +/-9.2g\r\n");
          } else {
            DEBUG("ERR. USE 2.3 or 9.2 scale of g\r\n");
          }
        }
      }
      
      if (strstr(buff, "<FILE>") AND strstr(buff, "</FILE>")) {
         get_value(buff, "<FILE>", "</FILE>", fileRaw);
      } else {
        if (strstr(buff, "<FILE>")) DEBUG("RAW FILENAME '%s', len=%i\r\n", fileRaw, strlen(fileRaw));
      }
      
      if (strstr(buff, "<REC>")){
        fRawRecord = true;
        DEBUG("REC to '%s'\r\n", fileRaw);
      }
      
      if (strstr(buff, "<STOP>")){
        fRawRecord = false;
        DEBUG("STOP to '%s'\r\n", fileRaw);
      }
      
     buff[0] = 0; //Очистка строки
    } //tobe_proccessed
  }
}

//Аргумент fromwhere для отслеживания откуда был вызов
// fromwhere - откуда (main, irq) вызов был сделан. Иногда это мне важно для отладки
char WIFI_data_process (char * fromwhere) {  //Обработка всего, что идет от WIFI (не от DEBUG!)
  //В буффер символы попадают по прерыванию. Обрабатываются здесь.
  //Концом блока для обработки является строка, заканчивающаяся \r или \n
  //Данные в буффер поступают даже во время их обработки.
  //Поэтому забираем каждый блок из FIFO и после этого его обрабатываем.
  //+ строку удобнее обрабатывать стандартными функциями в отличие от FIFO
  //Отбираем блок для обработки, удаляем его из FIFO
  
  
  static char block[STR_BLOCK_SIZE+1];     //+1 под \0  //Блок для анализа. Сюда перекачиваются данные из FIFO 
  char * p;     //Указатель для обработки строк
   
  short j = strlen(block);      //Сколько сейчас в буффере символов (индекс на добавление). Далее используется как индекс для работы с массивами
  char c;                       //полученный char
  char SSID_NAME[33];          //Название WIFI AP, полученное при поиске
  
  while (WIFICount) {           //Обработаем все символы в FIFO
    
    //Появились новые символы от WIFI. Обновляем момент когда они были получены. Нужно для таймаутов ожидания данных от сервера
    last_data_moment = cnt_timer;
    
    c = WIFIdataGet(0);         //Получим первый символ из FIFO
    
    WIFIdataDel();              //Удаляем символ из начала FIFO

    //Переносим символ в блок для обработки, который затем и обработаем
    if (j<=STR_BLOCK_SIZE) {
      block[j]    = c;
      block[j+1]  = (char)'\0';
      j++;
    }
    
    if (c == (char)'\r' OR c == (char)'\n') break; //При обнаружении конца строки останавливаемся и обрабатываем блок
  }


  //Обработка полученной строчки-блока --------------------------------------------------------
  if (strlen(block)>1 AND (c == (char)'\r' OR c == (char)'\n')) {

        if (strstr(block, "SSID:")){          //Найдена точка доступа. Парсим ее название, тип подключенияю Работает для модуля ST
          //Приходит строка вида SSID: 'Ruspolymer' CAPS: 0431 WPA2
                    
           p = strchr (block, (int)'\'') + 1;                 //ищем "'", ставим на следующий символ после '
           
           //Затем ищем последний ', которым обрамлено название
           j = 0; //Вторично используем переменную как индекс
           
           //Копируем название WIFI точки доступа в буффер
           while (p[0] != (char)'\''){
              SSID_NAME[j] = p[0]; 
              j++;
              *p++;
           }
           SSID_NAME[j]  = 0; //Конец строки
           
           DEBUG("FOUND '%s' ", SSID_NAME);
           
           //Определяем тип ключа. Специально после названия чтобы не попастся на наличие букв WPAв названии точки доступа
           //Важно: если есть открытая, то список поддерживаемых протоколов пуст. Поэтому построение if-ов играет роль
           if (strstr(p, " WPA2")) {
             DEBUG("WPA 2\r\n");
           } else {
             if (strstr(p, "WPA")) {
               DEBUG(" WPA\r\n");
             } else {
               DEBUG(" OPEN NET\r\n");
             }
           }
        }

        if (fWIFI_OK AND strstr(block, "Linked")){       //Было сообщение ОК и открыт сокет. Сообщение от модуля WIFI ESP. Прошивка v9.2.2
          DEBUG("DBG: LINKED!\r\n");
          fESP_sock_is_open = true;
        }

        if (fWIFI_OK AND strstr(block, "Unlink")){       //Было сообщение ОК  и закрыт сокет. Сообщение от модуля WIFI ESP. Прошивка v9.2.2
          DEBUG("DBG: UNLINKED!\r\n");
          fESP_sock_is_open = true;
        }
                
        if (strstr(block, "<RESULT>") AND strstr(block, "POSTED") AND strstr(block, "</RESULT>")){
          fWIFI_file_sent = true;       //Файл принят сервером. См. функции отправки файлов на сервер
          goto EndOfProccessing;
        }
    
        if (strstr(block, "ID:") AND !strstr(block, "SSID:")) {            //Получен ID сокета для полнофункциональной отправки POST с данными (см пересылку файлов на сервер)
          p = strchr (block, (int)':');                 //ищем двоеточие
          p[strlen(p)-1] = (char)'\0';                  //Вместо \r или \n ставим \0, иначе цифру не получим
          
          if (p != NULL) SocketID = atoi(p+1);          //Определяем SOCKET ID
        }
    
        if (strstr(block, "OK\n\r") OR strstr(block, "OK\r\n") OR strstr(block, "\nOK\r") OR strstr(block, "\rOK\n") OR strstr(block, "SEND OK")){               //Получен ответ OK на какую-то команду. Для ESP и ST
          fWIFI_OK = true;
          DEBUG("DBG: OK detected\r\n");
          goto EndOfProccessing;
        }
        
        if (strstr(block, "ERROR\n\r")){               //Получен ответ ERROR на какую-то команду. Для ESP
          fWIFI_OK = false;
          DEBUG("DBG: ERROR detected\r\n");
          goto EndOfProccessing;
        }

        //Ответ от сервера на PING -----------------------------------------------------------
        if (strstr(block, "<PING>") AND strstr(block, "</PING>")) { 
         PING_period = 5*60*1000;                      //Пинг каждую 5ю минуту 
          //Определяем, сколько команд нас ожидает
          char cmds[6];
          short dcmds;                                  //Число команд
          get_value(block, "<CMDS>", "</CMDS>", cmds);
            
          dcmds = atoi(cmds);                                   //Преобразуем следующее за равно число в Int 
          //Парсим строчку, ищем сколько команд нас ожидает`
          DEBUG("> PING ANS, CMDS=%i ", dcmds);
          RTC_DateTimePrint();
        
          if (dcmds != 0 && CmdIdExec == 0){            //Если есть команды и сейчас никакая не выполняется                              
              request_commands = true;                   //из main запросм у сервера команды. А пока завершим обработку пришедших данных
          }
        goto EndOfProccessing;
        } //Ответ от сервера на PING
        
        if (strstr(block, "<PINGBACK>")){               //Сервер что-то выполнил и просит сразуже его пропинговать
          PING_period = 0;                              //Делаем вид, что пора пинговаться
          goto EndOfProccessing;
        }
        
        if (strstr(block, "<") AND strstr(block, ">"))  {                   //Пришел некий тег от сервера
          fDataEnd = false;                             //Закончился приход данных от сервера
          WIFI_period = WIFI_CONNECTION_CHECK_PERIOD;   //Сбрасываем отсчет до проверки соединения wifi чтобы избежать отправки левой информации
        }
        
        if (strstr(block, "<DATA>"))  {
        }
        
        if (strstr(block, "</DATA>")) {
          fDataEnd = true;          //Закончился приход данных от сервера
        }
        
        //Строка файла firmware
        if (strstr(block, "<FW>") AND strstr(block, "</FW>"))  {
          char dta[70];
          get_value(block, "<FW>", "</FW>", dta);
          save(DOWNLOAD, "%s\r\n", dta);
          //write_download_file(block);
        }
        
        if (strstr(block, "<LINES>") AND strstr(block, "</LINES>")) { 
          //Определяем, сколько команд нас ожидает
          char lines[6];
          get_value(block, "<LINES>", "</LINES>", lines);
          
          download_lines_count = atoi(lines);      //Преобразуем следующее за равно число в Int 
        } 
        

        //Которая строка скачана
        if (strstr(block, "<NO>") AND strstr(block, "</NO>")) { 
          //Определяем, сколько команд нас ожидает
          char line[6];
          get_value(block, "<NO>", "</NO>", line);
          
          last_downloaded_line = atoi(line);      //Преобразуем следующее за равно число в Int 

          //goto EndOfProccessing не пишем, а то следующеие за ним данные не извлечет
        } 
        
        

            
        //Пришло время для RTC  ------------------------------------------------------
        if (strstr(block, "<TIME>") AND strstr(block, "</TIME>")) {
          char tmp[12];
          
          DEBUG("SERVER DATE TIME.\r\n");
          // <TIME><YR>14</YR><MO>06</MO><DY>01</DY><HR>13</HR><MN>52</MN><SE>23</SE><MS>0913</MS></TIME>
          //Год
          get_value(block, "<YR>", "</YR>", tmp);
          uint8_t y = atoi(tmp);
          
          //Месяц
          get_value(block, "<MO>", "</MO>", tmp);
          uint8_t m = atoi(tmp);

          //День
          get_value(block, "<DY>", "</DY>", tmp);
          uint8_t d = atoi(tmp);
          
          //Час
          get_value(block, "<HR>", "</HR>", tmp);
          uint8_t h = atoi(tmp);

          get_value(block, "<MN>", "</MN>", tmp);
          uint8_t mi = atoi(tmp);
          
          //Секунды
          get_value(block, "<SE>", "</SE>", tmp);
          uint8_t s = atoi(tmp);
          

          RTC_SetDateTime(d, m, y, h, mi, s); //Пишем в RTC. Внимание! Он не ставит milliseconds 
          
          //Поменялись секунды, и в прерывании SysTick milliseconds сбросились в 0;
          //Миллисекунды
          get_value(block, "<MS>", "</MS>", tmp);
          milliseconds = atoi(tmp);          
          RTC_DateTimePrint();
          
          goto EndOfProccessing;
      } //Пришло время для RTC

        if (strstr(block, "+IPD")) {
          WIFI_period = WIFI_CONNECTION_CHECK_PERIOD;   //Сбрасываем отсчет до проверки соединения wifi чтобы избежать отправки левой информации
          DEBUG("---- +IPD is found\r\n");
        }
        
        //Команда --------------------------------------------------------------------
        if (strstr(block, "<CMD>") AND strstr(block, "</CMD>")){
          if (!CmdIdExec) process_command(block);       //Если никакая команда не выполняется -- выполним эту
        } //Команда
        
  //Быстрое окончание обработки
  EndOfProccessing:
        block[0] = (char)'\0';  //Очистка строки
        return 1;
  } //Проверка блока
  
    return NULL;
} //END OF function
  
void WIFI_set_settings(void) {

}

//Запрашивает время с сервера для синхронизации часов
//О приходе времени смотри CommandProcessing
void WIFI_request_datetime (void) {
  DEBUG(">SRV DATE TIME? ");
  //RTC_DateTimePrint();
  printf("AT+S.HTTPGET=www.kaija.ru,/device/datetime.php\r\n");     
}

//Запрашивает команды с сервера
//О приходе команд смотри CommandProcessing
void WIFI_request_commands (void) {
  if (request_commands) {
    DEBUG("< CMDS?\r\n");
    request_commands = false; //Сброс флага. Т.е. Не запрашивать команды
    printf("AT+S.HTTPGET=www.kaija.ru,/device/command.php?SensorID=%i\r\n", SETS.SensorID);
  }
}


//Отправляет все зарезервированные файлы для отправки на сервер
void WIFI_send_files() {
  static long long moment;
  DIR dir;
  FILINFO fileInfo;
  FRESULT result;
  bool flag_found = false;
  char new_file_name[20]; //С учетом пути вроде 0:\active.TS
  char old_file_name[20];
  

  
  //Находим первый файл .ts = to send

  if (WIFI == WIFI_ONLINE AND cnt_timer-moment > 300 AND   sflag) {        //Без WIFI ничего не делаем и ждем 300 мс прежде чем снова читаем каталог иначе DMA или SDIO падает
    
  
  if (f_mount(&Fatfs, "0:", 1) == FR_OK){                     //Монтируемся    
    result = f_opendir(&dir, "0:/");
      if (result == FR_OK){
         while ((f_readdir(&dir, &fileInfo) == FR_OK) AND !flag_found AND fileInfo.fname[0] != 0){
            if (strstr(fileInfo.fname, ".TS")){        //Этот файл отмечен на отправку To Send
               // DEBUG("File %s\r\n", fileInfo.fname);
                flag_found = true;
                //Файл нашли Обработаем его Остальные найдем снова когда функция будет вызвана из main
                goto next_step;
            }
         } //Перебор файлов
      }

    f_mount(NULL, "0:", 1);
next_step:
    if (flag_found){              //Нашелся файл на отправку
        sflag = false;
      //отправляем его
    //  if (WIFI_send_file(fileInfo.fname) == R_OK AND fWIFI_file_sent){        //Если отправка прошла успешно. ST!
        if (ESP_send_file(fileInfo.fname) == R_OK AND fWIFI_file_sent) {
        //переименовываем в .st = sent
        old_file_name[0]=0;
        strcat(old_file_name, "0:");
        strcat(old_file_name, fileInfo.fname);
        
        if (f_mount(&Fatfs, "0:", 1) == FR_OK){   //ТК в WIFI_send_file в конце f_mount(NULL, "0:", 1);
          strcpy(new_file_name, old_file_name);
          char * p     = strchr (new_file_name, (int)'.');     //ищем точку
          *p++;   p[0] = (char)'S';
          *p++;   p[0] = (char)'T';
          *p++;   p[0] = (char)'\n';
          
          if (f_rename(old_file_name, new_file_name) == FR_OK) { 
            DEBUG("FILE IS SENT. Renamed to %s\r\n", new_file_name);
          } else {
            DEBUG("ERROR RENAME %s TO %s\r\n", old_file_name, new_file_name);
          }
         
         
         f_mount(NULL, "0:", 1);
        } //mount
      } //Send file
    } //flag_found
  

  }     //mount
  moment = cnt_timer;  

  }
}

//Ждем от модуля ОК или по таймауту продолжаем
//Годно для ST и для ESP
WIFI_MODULE_RESULT WIFI_TimeOut_OK(unsigned long ms) {
        unsigned long long moment;                      //Для отсчета таймаутов в мкс

        //Подготовка к общению с модулем
        fWIFI_OK = false;                               //Флаг ответа сбрасываем
       // DEBUG("WAIT...");
        moment = cnt_timer;                             //Готовимся прервать ожидание по таймауту
        
        //просто ждем ответа ОК, но поступающее от WIFI обрабатываем
        while (cnt_timer-moment < (long long) ms AND !fWIFI_OK){
          //Выполним критичные функции main
          FLUSH();                                      //Передача между DEBUG и WIFI. Обмен через буфферы.
          WIFI_data_process("WIFI_TimeOut_OK");         //Обрабатываем данные, поступившие от WI-FI. Специально обработка сделана не из прерывания.          
          
          //ВНИМАНИЕ! Нельзя делать flush_to_sd. Т.к. функция WIFI_TimeOut_OK итак может быть вызвана при открытом файле. Например, при чтении файла при отправке данных на сервер
          //flush_to_sd();                                //Запись буффера на SD
        }
        
        if (fWIFI_OK) return RESULT_OK; 
        else return RESULT_TIMEOUT;
       // if (WIFI != WIFI_ONLINE) DEBUG("TIMEOUT\r\n");
       // else DEBUG("recieved OK\r\n");
}

//Ждем таймаута передачи данных или </DATA>
void WIFI_TimeOut_Sock(unsigned long ms) {


        //Подготовка к общению с модулем
        last_data_moment = cnt_timer;                             //Готовимся прервать ожидание по таймауту
        fDataEnd = false;
        
        while (cnt_timer-last_data_moment < (long long) ms AND !fDataEnd){
          FLUSH();                                      //Передача между DEBUG и WIFI. Обмен через буфферы.
          WIFI_data_process("WIFI_TimeOut_Sock");         //Обрабатываем данные, поступившие от WI-FI. Специально обработка сделана не из прерывания.          
          flush_to_sd();                                //Запись буффера на SD
        }
}


//Отправляет пинг устройства на сервер вместе с его статусом.
void WIFI_ping (void) {
  DEBUG("< PING MODE=%i\r\n", MODE);
  RTC_DateTimePrint();
  char dt[20];
  timestamp(&dt[0]);
  printf("AT+S.HTTPGET=www.kajacloud.com,/device/ping.php?SensorID=%i&Battery=%i&RTC=%s&Data=%i&State=%i\r\n", SETS.SensorID, battery, dt, 2, MODE);
}


//Отправляет содержимое файла POST запросом на сервер для дальнейшей обработки
//Для ESP
SENDRES ESP_send_file(char * filename) {
  char buff [255];             //Модуль принимает не более 4096 байт. Но надо учесть возможности printf и команды в строке. Опыт показал, что 255 - оптимальное число
  char heads[200];              //Заголовки
  char datablock[1300];          //Начало блока с данными. Сюда пишем SensorID и др. поля
  char datablockend[150];       //Окончание блока с данными
  
  heads[0]              = 0;
  buff[0]               = 0;
  datablock[0]          = 0;
  datablockend[0]       = 0;

  FIL file;
  long long moment;
  unsigned int ttl;
  
  DEBUG("< SEND FILE %s ---------------------------- \r\n", filename);
  RTC_DateTimePrint();
  
  fWIFI_file_sent = false;                              //Сбрасываем флаг ответа о том, что файл принят сервером
  

  if (ESP_open_sock() == RESULT_OK) {                           //Открываем стек
    
     if (f_mount(&Fatfs, "0:", 1) == FR_OK){                 //Монтируемся
        char r=f_open(&file, filename, FA_READ);                  //Открываем файл для отправки
              
         //Готовим куски запроса       
        sprintf(datablock, "Content-Disposition: form-data; name=\"SensorID\"\r\n\r\n%i\r\n", SETS.SensorID );
        strcat(datablock, "--1BEF\r\n");
        
        //Времеено используем heads в работе. Потом очистим массив...
        sprintf(heads, "Content-Disposition: form-data; name=\"upload_file\"; filename=\"%s\"\r\n", filename);        
        strcat(datablock, heads);               //Добавим в буффер эту часть
        strcat(datablock, "Content-Type: text/plain\r\n");
        strcat(datablock, "\r\n");

        strcat(datablockend, "\r\n");
        strcat(datablockend, "--1BEF--\r\n");
        strcat(datablockend, "\r\n\r\n");
        
        heads[0] = 0; //Очистим heads
        
        //Отправляем заголовки HTTP запроса, в том числе и с размером отправляемого контента
        sprintf(heads, "POST /device/postfile.php HTTP/1.1\r\nHost: kajacloud.com\r\nConnection: keep-alive\r\nContent-Type: multipart/form-data; boundary=1BEF\r\nAccept: text/html\r\nContent-Length: %i\r\n\r\n", strlen(datablock) + strlen(datablockend) + file.fsize);
      
        moment = cnt_timer;
             //А. Пишем в сокет заголовки ----------------------------------------------------
             DEBUG("SEND HEADERS %i bytes...\r\n", strlen(heads));
             
             fESP_send_ready = false;                              //Флаг приглашения сбрасываем и ждем его
             printf("AT+CIPSEND=%i\r\n", strlen(heads));
        
             ESP_wait_send_ready();  //Ждем приглашения от модуля
        
             if (fESP_send_ready) {       //По готовности принять данные модулем
               //DEBUG("OK, now send headers\r\n");
               printf("%s", heads);       //Отправляем заголовки
               WIFI_TimeOut_OK(1500);     //Ждем "SEND OK" ESP v922
             }
             
             //Б. Пишем начало блока данных  ---------------------------------------------------
             //Часть, предваряющая содержимое запроса -----------------------------------------------------------------
             
             fESP_send_ready = false;                              //Флаг приглашения сбрасываем и ждем его
             printf("AT+CIPSEND=%i\r\n", strlen(datablock));

             //Флаг надо до, иначе бывают сбои. Упускает приход > даже несмотря на прерывания, т.к. на AT+CIPSEND ">" приходит до ESP_wait_send_ready
             ESP_wait_send_ready();  //Ждем приглашения от модуля
        
             if (fESP_send_ready) {       //По готовности принять данные модулем
               //DEBUG("OK, now send start part\r\n");
               printf("%s", datablock);       //Отправляем заголовки
               WIFI_TimeOut_OK(1500);     //Ждем "SEND OK" ESP v922
             } else {
                  DEBUG("ERROR: Module does not sent ready char!\r\n");
             }
             
             //В. Пишем содержимое файла. Кусками. Весь файл модуль разом не проглотит
             DEBUG("\r\n\r\nSTART POST DATA FROM FILE----------------\r\n\r\n");
        
              //Пересылаем в теле запроса содержимое файла --------------------------------------------------------------
              //Если пытаться записать одним сообщением модулю весь файл он сойдет с ума. Входной буффер у него ограничен.
              
             //Построчно вычитываем и копим буффер для отправки. Отправляем его.
             short packetlen=0;
             
             datablock[0] = 0;
             while (f_gets(buff, sizeof(buff), &file)) {
                short length = strlen(buff);

                if ((strlen(datablock) + length) < sizeof(datablock)) {
                  strcat(datablock, buff);
                  packetlen = packetlen + length;
                
                } else {
                
                    //Постим буффер с данными из файла
                    fESP_send_ready = false;                              //Флаг приглашения сбрасываем и ждем его
                    printf("AT+CIPSEND=%i\r\n", packetlen);
                    
                    //Флаг надо до, иначе бывают сбои. Упускает приход > даже несмотря на прерывания, т.к. на AT+CIPSEND ">" приходит до ESP_wait_send_ready
                    ESP_wait_send_ready();                          //Ждем приглашения от модуля

                    if (fESP_send_ready) {                          //По готовности принять данные модулем
                      // DEBUG("OK, send %i bytes\r\n", packetlen);
                       printf("%s", datablock);                     //Отправляем заголовки
                       ttl += packetlen;
                       WIFI_TimeOut_OK(1500);                       //Ждем "SEND OK" ESP v922
                    } else {
                      DEBUG("ERROR: Module does not sent ready char!\r\n");
                      break;
                    }
                    
                    datablock[0] = 0;
                    packetlen = 0;
                }
                
              } //while
             
               //TODO И ОТПРАВЛЯЕМ НЕПОЛНЫЙ ОСТАТОЧЕК!

              DEBUG("%i bytes in %i ms\r\n", ttl, cnt_timer-moment);
             
             
        f_close(&file);                 //Закрываем файл
        f_mount(NULL, "0:", 1) ;                            //UNMOUNT

          RTC_DateTimePrint();
        Delay(10000);
    }
    ESP_socket_close();
  } else {
    return R_UNDEFINED_ERR;
  }
}


//Запрашивает команды с сервера
//О приходе команд смотри CommandProcessing
void ESP_get_commands (void) {
  if (request_commands) {
    DEBUG("< CMDS?\r\n");
    request_commands = false; //Сброс флага. Т.е. Не запрашивать команды
    ESP_HTTPGET("device/command.php", true , "SensorID=%i", SETS.SensorID);
  }
}

void ESP_get_datetime(void) {
  DEBUG(">SERV DATE TIME?\r\n");
  //RTC_DateTimePrint();
  ESP_HTTPGET("device/datetime.php", true,  "");
}

void ESP_server_ping(void) {
  DEBUG(">SERV PING MODE=%i\r\n", MODE);
  RTC_DateTimePrint();
  char dt[20];
  timestamp(&dt[0]);
  
  ESP_HTTPGET("device/ping.php", true, "SensorID=%i&Battery=%i&RTC=%s&Data=%i&State=%i", SETS.SensorID, battery, dt, 2, MODE);

}

//Скачивает файл с новой прошивкой с сервера
void update_fw(void) {
  //Установим кол-во строк в файле с прошивкой
  if (!download_lines_count && WIFI==WIFI_ONLINE) {
    ESP_HTTPGET("device/firmware.php", true, "");     //Пустой запрос выдаст информацию о числе строк для скачивания
  }
  
}

//Все данные будут валиться из сокета без излишнего +IPD 
//  printf("AT+CIPMODE1\r\n");
//}

//Реализация метода GET на ESP. Для ST не требуется, итак есть в модуле
//page - Адрес страницы
//остальное - строка с параметрами, включая вопрос
//www.kaija.ru/device/ping.php?SensorID=%i&Battery=%i&RTC=%s&Data=%i&State=%i
void ESP_HTTPGET(char const* page, bool close ,char const* fmt, ... ) {
  unsigned int line=0; //Строка, которую скачиваем из большого файла
  
  char buff[512+1];
  buff[0] = 0;
  
  va_list   uk_arg;
  va_start (uk_arg, fmt);
  marker = 11;
  
   if (ESP_open_sock() == RESULT_OK) {    
     int slen = vsnprintf(buff, 512, fmt, uk_arg);       //Форматирование строки

     //Флаг надо до, иначе бывают сбои. Упускает приход > даже несмотря на прерывания, т.к. на AT+CIPSEND ">" приходит до ESP_wait_send_ready
     printf("AT+CIPMODE=1\r\n");                //Прозрачный режим sock-usart. Отключение режима +++ без \r\n
     Delay(100);
     DEBUG("DBG: Sock is open\r\n");
     fESP_send_ready = false;                              //Флаг приглашения сбрасываем и ждем его
     
      //GET / HTTP/1.1\r\nHost: kaija.ru\r\n\r\n - 34 байт
     //printf("AT+CIPSEND=%i\r\n", strlen(buff)+strlen(page)+34);      //В ответ придет > мол готов отправить n байт
     printf("AT+CIPSEND\r\n");
     //Приходит ">" без \r\n. Поэтому в WIFI_data_proccess срабатывания не будет. 
    //Но в прерывании при получении символа '>' выставляется признак fESP_send_ready = true
     
     //Флаг=false надо выставлять заранее, иначе бывают сбои. 
     //Упускает приход > даже несмотря на прерывания, т.к. на AT+CIPSEND ">" приходит до ESP_wait_send_ready 
     ESP_wait_send_ready();
     
     //Был таймаут или готовность
     if (fESP_send_ready) {     //По готовности
       DEBUG("DBG: ESP ready to send\r\n");
       
       printf("GET /%s", page);
       if (strlen(buff)) printf("?%s", buff); //Сама строка с параметрами ?...
       printf(" HTTP/1.1\r\nHost: kaija.ru\r\n");
       
       /*if (close) {
          printf("Connection: close\r\n");
       } else {
          printf("Connection: Keep-Alive\r\n");
       }*/
       printf("\r\n");
       
       //Ждем таймаута ответа от сервера или окончания передачи - </DATA>
       WIFI_TimeOut_Sock(2500);       //Ждем конца передачи по признаку </DATA> или таймаута на передачу данных (аргумент в мкс)
       
       //Для больших объемов данных буффера на прием не хватит и скорость записи на SD подведет. 
       //Поэтому сначала получаем исло строк в файле <LINES>download_lines_count</LINES> и не отклюяая коннект построчно запрашиваем файл
       //по тегам внутри строки разбираемся что делать в обработчике WIFI_data_proccess. Причем, пока пришедшая строчка не отработана
       //новая строка не запрашивается.
       
       last_downloaded_line = 0;
       if (download_lines_count){
         //open_download_file();
           while (download_lines_count != line){
             line = last_downloaded_line+1;
             printf("GET /%s", page);
             if (strlen(buff)) {
               printf("?%s&line=%i", buff, line); //Сама строка с параметрами ? + номер скачиваемой строки
             } else {
               printf("?line=%i", line); //Сама строка с параметрами ? + номер скачиваемой строки
             }
             printf(" HTTP/1.1\r\nHost: kaija.ru\r\n");
             
             /*if (close) {
                printf("Connection: close\r\n");
             } else {
                printf("Connection: Keep-Alive\r\n");
             }*/
             printf("\r\n");
             
             WIFI_TimeOut_Sock(1000);       //Ждем конца передачи по признаку </DATA> или таймаута на передачу данных (аргумент в мкс)
         }
         //close_download_file();
     }
       
     } else {
       DEBUG("DBG: ERROR: Module does not sent ready char!\r\n");
       printf("\r\n\r\n"); //Для гарантии закрытия сокета
     }
     

     
     //Отключаем режим прямой связи UART-сокет в модуле
     printf("+++");
     Delay(10);
     printf("+\r\n");
     ESP_socket_close();                //Закрываем сокет
  } else {
    DEBUG("DBG: ERROR: SOCKET PROBLEM!\r\n");
  }
}


//Ждет приглашения отдавать данные ">" от ESP
void ESP_wait_send_ready(void) {
    unsigned long long moment;                      //Для отсчета таймаутов в мкс
     moment = cnt_timer;                                    //Готовимся прервать ожидание по таймауту
    
     //Не успевает Надо заранее сбрасывать флаг
     // fESP_send_ready = false;                              //Флаг приглашения сбрасываем и ждем его
     
     //Ждем таймаута готовности отправить или прихода приглашения через прерывание USART
     while (cnt_timer-moment < 4000 AND !fESP_send_ready){
          //Выполним критичные функции main
          FLUSH();                                      //Передача между DEBUG и WIFI. Обмен через буфферы.
          WIFI_data_process("ESP_wait_send_ready");         //Обрабатываем данные, поступившие от WI-FI. Специально обработка сделана не из прерывания.          
          
          //ВНИМАНИЕ! Нельзя делать flush_to_sd. Т.к. функция WIFI_TimeOut_OK итак может быть вызвана при открытом файле. Например, при чтении файла при отправке данных на сервер
          //flush_to_sd();                                //Запись буффера на SD
      }
}

void ESP_socket_close(void) {
  DEBUG("DBG: Close socket\r\n");
  //RTC_DateTimePrint();
  printf("AT+CIPCLOSE\r\n");
}


WIFI_MODULE_RESULT ESP_open_sock(void) {
   
  if (WIFI == WIFI_ONLINE) {
    fWIFI_OK = false;   //Сбросить надо еще до 
    printf("AT+CIPSTART=\"TCP\",\"www.kaija.ru\",80\r\n");      //В последующих исправлено 

    if (ESP_TimeOut_Open_Socket(3500) == RESULT_OK) {
      DEBUG("DBG: Socket is ready!\r\n");
      return RESULT_OK;
    } else {
      DEBUG("DBG: ERROR Can't open sock!\r\n");
      ESP_socket_close(); // Для гарантированного закрытия сокета на случай, если он был уже открыт ранее
      return RESULT_FAIL;
    }
  } else {
    DEBUG("DBG: ERROR Can't open! No WIFI connnection!");
    return RESULT_FAIL;
  }
}

WIFI_MODULE_RESULT ESP_TimeOut_Open_Socket(unsigned long ms) {
        unsigned long long moment;                      //Для отсчета таймаутов в мкс

        //Сбросить надо еще до
        //fWIFI_OK = false;                               //Флаг ответа OK
       // DEBUG("WAIT...");
        moment = cnt_timer;                             //Готовимся прервать ожидание по таймауту
        //просто ждем ответа ОК, но поступающее от WIFI обрабатываем
        while (cnt_timer-moment < (long long) ms AND !fESP_sock_is_open){
          //Выполним критичные функции main
          FLUSH();                                      //Передача между DEBUG и WIFI. Обмен через буфферы.
          WIFI_data_process("ESP_TimeOut_Open_Socket");         //Обрабатываем данные, поступившие от WI-FI. Специально обработка сделана не из прерывания.          
          
          //ВНИМАНИЕ! Нельзя делать flush_to_sd. Т.к. функция итак может быть вызвана при открытом файле. Например, при чтении файла при отправке данных на сервер
          //flush_to_sd();                                //Запись буффера на SD
        }
        
        if (fWIFI_OK) return RESULT_OK; 
        else return RESULT_TIMEOUT;
}

//Отключает эхо у модуля ESP. Прошивка 9.2.2
void ESP_echo_off (void) {
  printf("ATE0\r\n");
  WIFI_TimeOut_OK(100);
}


//Проверяет, подключен ли к WIFI модуль. Для ESP
void ESP_is_connected (void) {
  if (WIFI_period == 0) {       //Пора запросить состояние подключения      TO-DO: Не делать это когда идет передача данных, пинг         
 //   DEBUG("DBG: WIFI check\r\n");
    printf("AT+CIFSR\r\n");     //Запросим IP. Если придет ответ, значит мы подключены
    if (WIFI_TimeOut_OK(500) == RESULT_OK) {
        WIFI = WIFI_ONLINE;
        WIFI_period = WIFI_CONNECTION_CHECK_PERIOD;                   //Проверить подключение снова через 20 секунд
     // DEBUG("ESP MODULE IS ONLINE!\r\n");
    } else {
        WIFI = WIFI_OFFLINE;
        WIFI_period =  WIFI_CONNECTION_CHECK_PERIOD/10;                   //Проверить подключение снова и чаще
    };
  }//WIFI_period
}