#ifndef __logic_h
#define __logic_h

#include "rtc.h"
#include <string.h>
#include <stdbool.h>


#define CMDS_LEN                32              //Число хранимых команд в буффере. Кратно 2 и менее 256!
#define WIFI_DATA_BUFFER        2048            //Сколько символов может оказаться по приходу от сервера в буффере
#define STR_BLOCK_SIZE          256             //Сколько в строке для обработки


#define WIFI_CONNECTION_CHECK_PERIOD    30 * 1000     //Каждые n миллисекунд проверять, что модуль подключен к точке доступа

typedef struct {
	float  	        	X;              //float, т.к. тут сразу откорректировно на базовый уровень (смещение нуля), а он float
	float 	        	Y;
	float    		Z;

        float     		fX;             //отфильтрованные по всему массиву-буфферу данные
	float    		fY;             //и из них уже вычтено смещение нуля
	float    		fZ;

	unsigned short		Battery;	//Значение канала от батареи

	rtc_data		datetime;
} Datum;


typedef struct {
  unsigned short   	SensorID;                 //ID датчика 5

  unsigned char 	XOperationMode;           //Режим работы по оси X 5
  unsigned char 	YOperationMode;           //Режим работы по оси Y 5
  unsigned char	        ZOperationMode;           //Режим работы по оси Z 5

           char         GravityInfluenceAxis;     //Ось где учитывать гравитацию. 0 - нигде не учитывать 1
  unsigned short	AwakeSampleRate;          //Частота дискретизации акселерометра в активном режиме. Т.е. когда что-то происходит.
  unsigned short        SleepSampleRate;          //Частота дискретизации акселерометра в режиме сна
  unsigned short   	SleepAfterIdle;           //Переход в режим сна по прошествии

  unsigned short        Sensivity;                //Чувствительность

  unsigned short        RadioAwakePeriod;         //Периодичность выхода на связь. В том числе через сколько отправлять пинг

  unsigned short        Filter;                   //Фильтр по осям Это число точек в буффере для осреднения. Не может привышать MAX_BUFFER

            char        HitDetector;              //1- вкл 0- выкл
  unsigned short        HitTrishold;              //Порог срабатывания датчика удара. Порог по акселерометру, без фильтрации!

  //Параметры фильтрации для определения базового уровня (смещения нуля)
  unsigned short        Window;                   //Окно сглаживания для определения 0
  unsigned short        ZeroPoints;               //Через сколько одинаковых (без отклонения) измерений по всем осям устанавливаются новые базовые значения

  //Настройки WI-FI
  char SSID[21];
  char pass[15];
} Settings;

typedef struct {
  unsigned char	coord;
  signed short	C;
  signed short	dC;

  unsigned short battery;

  rtc_data	datetime;
} Record;

typedef enum {
	R_OK = 0,
	R_UNDEFINED_ERR,		/* 1 */
	R_MOUNT_ERR, 
        R_NO_CORRECT_RESPONCE
} SENDRES;

typedef enum {
	WIFI_OFFLINE = 0,
	WIFI_ONLINE,		/* 1 */
	WIFI_EVALUTING
} WIFI_STATE;

typedef enum  {
  RESULT_OK     =0,
  RESULT_FAIL   =1,
  RESULT_TIMEOUT=2
} WIFI_MODULE_RESULT;

void            process_data            (signed short X, signed short Y, signed short Z);
void            adjust_zero             (signed short X, signed short Y, signed short Z);
void            adjust_filter           (void);
void            ProceedPing             (char inc);
char            WIFI_data_process       (char * fromwhere);
void            DEBUG_processing        (char data);
void            init_settings           (void);

WIFI_MODULE_RESULT            WIFI_TimeOut_OK         (unsigned long ms);
void            WIFI_TimeOut_Sock       (unsigned long ms);

void            get_value               (char *input_str, char *left_str, char *right_str, char *output_str);

void            WIFIdataAdd             (char data);
void            WIFIdataDel             (void);
char            WIFIdataGet             (unsigned short ix);

void            WIFI_request_datetime   (void);
void            WIFI_request_commands   (void);
void            WIFI_ping               (void);
void            WIFI_send_files         (void);

void            command_exec            (void);
void            command_done            (char * result);

void            update_fw               (void);

//For module with ESP

void            ESP_echo_off            (void);
void            ESP_is_connected        (void);
WIFI_MODULE_RESULT ESP_TimeOut_Open_Socket(unsigned long ms);
void            ESP_socket_close        (void);
WIFI_MODULE_RESULT ESP_open_sock        (void);
void            ESP_server_ping         (void);
void            ESP_get_datetime        (void);
void            ESP_HTTPGET             (char const* page, bool close, char const* fmt, ... );
void            ESP_get_commands        (void);
SENDRES         ESP_send_file           (char * filename);
void            ESP_wait_send_ready     (void);


#endif /* __logic_h */