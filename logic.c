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

#define RTC_UPD         6               //����������� ����� ���� ��� �� ����������� ������ RTC_UPD ������.
#define ADJ             0.8             //����� ���������� �� �������� ��������� ����� a � b ��� ���������� ������ ������
#define MAX_BUFFER      1000            //������������ ������ ������� � ������� ��� �����������
#define READS           10000           //�� ���������� ������� ����������  �� ������ ���� ���������� ��� ���������� ���� ����������

extern unsigned short marker;           //��� ������ ������� Hard_Fault

extern unsigned short acc_reads;        //����� ���������� �������������. ������������ ������ ������� ��� ����� ��-�����.
extern unsigned short battery;          //���������� � ������� �������
extern uint32_t PING_period;            //������ �����
extern uint32_t RTC_upd_period;         //������ ������� �������
extern int32_t WIFI_period;             //������ �������� ����������� � WIFI

extern bool sflag;

extern FATFS Fatfs;

enum device_modes {                     //��������� ����������
  IDLE                  = 0,            //������ �� �����
  FILTER_ADJUST         = 1,            //���������� ����������
  NORMAL_OPERATION      = 2,            //������� ���������. ���� ������, �������� �� ������ �� ���� ����������. �����.
  SLEEP_OPERATION       = 3,            //������ � ��������� SLEEP
  FW_DOWNLOAD           = 4             //��������� �������� ������ ��
};


unsigned char MODE = IDLE;                      //����� ������ ����������.
unsigned char MODE_BEFORE_COMMAND = IDLE;       //����� ������ ���������� �� ������ ���������� �������

WIFI_STATE WIFI=WIFI_OFFLINE;                   //��������� �� WIFI � ����� �������? ���� �� ������ � �������?
char WIFI_searches;                             //������� ��� �������� ����� ����. ��� ST

//��� ������ �� ESP
bool fESP_sock_is_open;                           //������ WIFI ������ �����. ������������ ������ ��� ������ ESP
bool fESP_send_ready;                             //������� ������ '>' ��� ����������� ���������� ������ � ����� ������ �� ESP
bool fDataEnd;                                    //�������������� true ���� ������ ����� �������� ������ </DATA> ����� �� ������� timeout

extern bool direct;

extern unsigned int milliseconds;       //������� ������������ rtc. ������������ � ���� ������ �������
extern unsigned char seconds;           //rtc seconds
extern unsigned char minutes;           //rtc minutes
extern unsigned char hours;             //rtc hours
extern unsigned long long cnt_timer;    //������� ��� ����������� ��������� ��������� � ����


//��� ��������� ������� � ��������� RAW ������
bool fRawRecord = false;                //��� ��������� ������ �� � ����
char fileRaw[13];                       //��� ����� ��� ������

Settings SETS;                          //��������� �������

float corrX, corrY, corrZ;              //���������� X Y Z ��� ����� �������� 0. ��������, ��-�� ����������. ������������ �����������.

//����� ������ ������ ���� ������ 2 ��������� � define � usart.c!
char WIFIdata[WIFI_DATA_BUFFER];        //������ ��� ���������� ������ �� WIFI ��� ����������� ��������� ������� �� main +1 �� /0
unsigned short WIFIStartIx = 0;          //������ ���������� �������
unsigned short WIFICount = 0;            //������ ����� �������� ��������

//FATFS ������� �������� SD, ������� ������ ������� � ������ � �� � �����
bool request_commands = false;          //���� ������������� ��������� ������� � �������
//��� ��������� ������
unsigned int CmdIdExec;                 //ID ����������� �������. 0 = �������
unsigned int cmd_timout;                //������� �� ���������� �������

//��� ������������� �������
unsigned int new_reads;                 //����� �������� ���������� �������������� ������� � ������� ��������� ��������� ���� �����������
bool new_saves = false;                 //�������, ��� ����� ��������� ������ ���� ����������� ���� ���������� �� ������. �.�. ���� ������� �������

//�������� �����
bool fWIFI_file_sent = false;           //���� ������ ��������. �� ������� �������� ����� ����� post

//������������� ������ ��� �������� POST ��������
signed char SocketID            = -1;   //�������� �� �������� ������, �������� ID: 00
signed short SocketDataLen      = 0;    //������� ������ � ������ ������ (AT.S+SOCKQ...)

bool fWIFI_OK           = false;        //���� - ������� ��� �� �������� OK �� ������ � ����� �� �������
bool WIFI_need_connect  = false;        //������� ������������� ����������� � wifi

unsigned long long last_data_moment;    //����� ���� �������� ��������� ������ ����� �����. ����� ��� �������� � WIFI_TimeOut_Sock
unsigned int download_lines_count;      //���-�� ����� � ����������� �����. ����� ����� ����������� ��������� ���� � �������, ����� ����� �� SD �� ��������� �� ����������� �������
unsigned int last_downloaded_line;      //����� ��������� �������� (� ���������� � ����) ������

void WIFIdataDel (void) {               //������� �� ������� 1� ����
    WIFICount--;
    WIFIStartIx++;
    if (WIFIStartIx >= WIFI_DATA_BUFFER) WIFIStartIx = WIFIStartIx - WIFI_DATA_BUFFER;
}

void WIFIdataAdd (char data){           //��������� � ������
   
  unsigned short ix;
  if (WIFICount !=  WIFI_DATA_BUFFER){                  //���� ������ ��� �� �����
                                                        //��������� ������
    if (WIFIStartIx + WIFICount < WIFI_DATA_BUFFER) {   //��������� ������ ����� ������ �������
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
  
  if (ix <= WIFICount){                                 //�� ��������� ����� �������
        if (WIFIStartIx + ix < WIFI_DATA_BUFFER) {      //��������� ������ ����� �������� �������
          i = WIFIStartIx + ix;
        } else i =  WIFIStartIx+ix - WIFI_DATA_BUFFER;   //���� �� ������� ������ ������
        
        return WIFIdata[i];
  } else {      //��������� ����� �������
    return NULL;
  }
}

//������� �������� ��� �� �������� ���� ���� ����, ������������� ��� ����������
void adjust_zero (signed short X, signed short Y, signed short Z){
  static float prevX, prevY, prevZ;      //���������� ���������� �������� �� ����
  float cX, cY, cZ;                      //����� ���������� �������� �� ����. ������ �����������
  float dcX, dcY, dcZ;                   //����������� ���������� ������
  static unsigned int unchanged;         //������� ��������� �� ���������� ���������, ��. dc(?) = 0. �����, ����� �������� �� ��������� ��������� ���� �������� ������ ����� �������.


   cX = (1.0/(float)SETS.Window)*(float)X;
   cX += (((float)SETS.Window-1.0)/(float)SETS.Window)*prevX;        //X/w + (prevX*(w-1)/w)

   dcX = cX - prevX;                                                    //��������� �����������
   prevX = cX;                                                          //��������� ������� �������� ��� ���������� �������


   cY = (1.0/(float)SETS.Window)*(float)Y;
   cY += (((float)SETS.Window-1.0)/(float)SETS.Window)*prevY;

   dcY = cY - prevY;
   prevY = cY;


   cZ = (1.0/(float)SETS.Window)*(float)Z;
   cZ += (((float)SETS.Window-1.0)/(float)SETS.Window)*prevZ;

   dcZ = cZ - prevZ;
   prevZ = cZ;

   unchanged++;                         //��������� �������� ������� ������������ �������� ������ (����)
   if (fabs(dcX) > 0.03 OR fabs(dcY) > 0.03 OR fabs(dcZ) > 0.03) unchanged = 0;  //���� ��������� ����������� ��������� ������ �������, �������� �������

   if (unchanged > SETS.ZeroPoints ) {  //���� ��������� �� ���� �������� ����� �����, ��������� ����� ������� �������. // ��� ����� ������ ������ ��������� ���������� ��������, �� ���� ������ ���� ����� ����
     if (fabs(cX-corrX) > 1.0) { corrX = cX; DEBUG("New X corrector = %0.2f\r\n", corrX);}  //���� ���������� �������� ������ ������������
     if (fabs(cY-corrY) > 1.0) { corrY = cY; DEBUG("New Y corrector = %0.2f\r\n", corrY);}  //���� ���������� �������� ������ ������������
     if (fabs(cZ-corrZ) > 1.0) { corrZ = cZ; DEBUG("New Z corrector = %0.2f\r\n", corrZ);}  //���� ���������� �������� ������ ������������
     unchanged = 0;
   }
}

void adjust_filter(){
          //NB! ��� ���������� ���������� ����� ������� (�������� �� ���) ��� ���� ����������� �������� ����, ����� ������� �� ����� ���������� ������.
          // ����� ���� �� ���������� ������ ����� ������, �.�. ����� � ����������� ������� ������� ���� 0
          new_reads ++;                             //��������� �������� ����������

          if (new_reads <= SETS.Filter+1) new_saves = false;  //���� �������� ��� ���� ���������� �� ������ �������.
                                                              //�� ��������������� � true ������ ���������� �����

          if (new_reads >  SETS.Filter+10) {                   //��������, ��� ������ ���������� ��������� �������� ������� �������.
                                                              //��� ����� � ���������� ��� ����������.
             if (new_saves == true) {                         //����������� ���� ����������, ���� �� ����� �� ���� � ��������� ����� ���� ���������� �����

                  SETS.Filter ++;                             //�������� ����
                  printf("Set filter=%i on reads %i\r", SETS.Filter, new_reads);
                  new_reads = 0;                              //������� ����� ���������� ����� �� �������� ���� � ������� �������
                  new_saves = false;
              } else {
                if (new_reads == READS + SETS.Filter+10){       //���� READS �������� ���������� �� ����
                  SETS.Filter += 2;                             //��� ���������� ��������� ��� ��� �����
                  command_done("SUCCESS");
                  MODE = NORMAL_OPERATION;                                   //��������� � ������ ����������� ����������������
                  DEBUG("FILTER AJUSTMENT IS FINISHED.\r\nFilter=%i, on step %i\r\n", SETS.Filter, new_reads);
                  settings_to_sd();
                }
              }
           }
}

//������������ ����� ������ �� �������
void process_data(signed short X, signed short Y, signed short Z){
if (fRawRecord) save_raw_values(X, Y, Z);
  //������ ����� ������ � ������ ������ ��� ������ �� SD
 /*   float fX,  fY,  fZ;                           //����������� ����������� �������� �� ������� ������
    static Datum buffer[MAX_BUFFER];              //������ ��� ����������� ����������� ������ ���������� �������
    static unsigned short cix=0;                  //������ � �������

    float difX, difY, difZ;                       //������� ����� ��������� � �������� ���������� �����
    static float fX0, fY0, fZ0;                   //���������� �������� � ���������� ����������� �����. �.�. ����� ��� � ������� ����� ���� ����� �����

    static float cX, cY, cZ;                      //��������� �������� ����� �� �����
    static float aX, aY, aZ;                      //������������ ��� X=a*t+b. ��������� ����������� ��� ���������� ����� ����������� �����
    static float bX, bY, bZ;                      //����� ����� ������������ �� ������, ��� �� �����������. ����� �� �����������, a � b ���������������, � ����� �����������
    static unsigned long dTx, dTy, dTz;           //�������� ������� (����� ���������) � ������� ���������� ���������� ����� ��� ���������� X=a*t+b.
                                                  //������ UINT32_MAX ���� �� �����
    static unsigned long Tx0, Ty0, Tz0;           //������� ���������� �� ������� ��� ������� a � b � ���������. ����� �� ��� X0, X1 � �����������. dT ������������� �� X1


//1. ���� �������� ���� ��-�� ������� �������, ������ �����������
    //��� ������� ������������ ����������� ������� �������� 0 � ���� ������ �������
    if (MODE != FILTER_ADJUST) adjust_zero(X, Y, Z);          //������� �������� ��� �� �������� ����. ���� ����, ������������� ��� ����������
                                                  //���������� �� ���������� ����� ������ ��� ��� ��������� ������� ���� ����������� � ����� ������� �����
                                                  //����������� �������� �������������� �������. � �� ����� ���� ������ ������� � ��
                                                  //����������� ���������� ���� �������

//2. ����������� ������ (���������� ���������� �������)
    //��������� � ������ ������������������� �� �������� ���� ��������
    buffer[cix].X = (float)X - corrX;
    buffer[cix].Y = (float)Y - corrY;
    buffer[cix].Z = (float)Z - corrZ;

    //��������� ����� � �������
    for(int j = 0; j < SETS.Filter; ++j){
      fX = fX + ((float)buffer[j].X /(float)SETS.Filter);
      fY = fY + ((float)buffer[j].Y /(float)SETS.Filter);
      fZ = fZ + ((float)buffer[j].Z /(float)SETS.Filter);
    }

    //�������� � ������
    buffer[cix].fX = fX;
    buffer[cix].fY = fY;
    buffer[cix].fZ = fZ;

    cix++;                         //��������� ������� � �������
    if (cix>=SETS.Filter) cix = 0; //������ ������ ��� �����. ������ ��� ��� ���������� SETS.Filter ����� ��������,
                                   //�.�. cix >> SETS.Filter � == ������� �� ��������.

//3. ���������� ���� �����������, ���� ������ ���� �����
    if (MODE == FILTER_ADJUST) {             //���� �� � ������ �������������� �������
      adjust_filter();
    }

//4. ���������� ���������� ������ ��� ������ �������� ������������
//   ����� ����� ������

//    ������ ��� �� ���� ������ ��������� ������������ a � b ������. ����� ��� ����� X0(T0) � X1(T1)
//    ����� ��������� ������� Xc ��� ������� dT ������������ ������� T1.
//    � T1 �� ���������� �� ������ ��������� (T1-T0).
//    ��� ������ ��������� ������������� dT ������������ � 0.
//    ��� ������� � ��������� ���������� Xc = aX*(Tx0+dTx) + bX, ��� Tx0 � ���� ��� T1-T0
//    � ���� ����� �� ���� ����� � ������� ������.
//    ��� ���� dTx � Tx0 ����� ��� ������� ���� ������� ��������. ������. � ��������� ������ ������������.
//    365*24*60*60*1000 ��. � unsigned long ��� �� ����� 49 ����.  ������� ��� unsigned long, ����� "��������" ���������� ����� �� ���� ��� ��� 49 ����
    

        //�������� ������. ������ �������� ��� ���� ���������� ���������� ����� ������, ����� �� �� ��������� � ������, ��� ��� ����������
        //��������� ������� ����� ���������� ���������� ����������� ��������� � ������� ���������� � ����������

        //�������������� �������� ���������� ��������� � ������� ���������� ���������� �����
        if (dTx < UINT32_MAX) dTx ++;                     //����� �� ��������� ������������ ������������ �������� UINT32_MAX
        if (dTy < UINT32_MAX) dTy ++;                     //��� ������������� unsigned long
        if (dTz < UINT32_MAX) dTz ++;


        //������� ����� �������� �� ������ ������ ���� � ���� �����
        cX = aX * (float)dTx + aX * (float)Tx0 + bX;    //��� �������� �� ��������� ������������. �� �������� ����������� � max(float)
        cY = aY * (float)dTy + aY * (float)Ty0 + bY;
        cZ = aZ * (float)dTz + aZ * (float)Tz0 + bZ;

        difX = fX - cX;                                 //������� ����� �������� � ����������
        difY = fY - cY;
        difZ = fZ - cZ;

        
        if (fabs(difX) > (float)ADJ AND fabs(fX0 - fX)>(float)ADJ) {         //���� ������� �� ��� X ������ ��������� ��������, �.� (X - aT+b) > U
        LED_on(LED_RED);                                           //� ��� ���������� �� ���������� ����������� ����� ���������� ������� �������
        //��������� ������������ ����� ������, � ����� ��������� � ������
          aX = (fX - fX0)/(float)dTx;
          bX =  fX - aX*(float)dTx;

          if (MODE != FILTER_ADJUST) {             //���� �� � ������ �������������� �������
            //�� ������ ��� �������� � ����� ������������� ������. �������� �� � ����� ��� ����������� �������� �� ������
            if (MODE != IDLE) save_value((char)'X', fX, aX, bX);
          } else {
            new_saves = true;          //� ������ �������������� ������ ���� ��� ���� ����������, ����� ��������� ����
          }

          fX0 = fX;
          Tx0 = dTx;    //��������� ����� ����� ���������� ������� ����� T0 � T1 �� ������ �������� ������ �� ������� ������ ������ ��� �� ������ ������� ��������.
                        //������ T1 ��� � ���� ��� ������ ������� ����� �� �������� ������ ����� ������
          dTx = 0;      //�������� ����� ������� ����� ����� �� ��������� ����������� �����
         LED_off(LED_RED);
        }

        if (fabs(difY) > (float)ADJ AND fabs(fY0 - fY)>(float)ADJ) {         //���������� ��� Y
          aY = (fY - fY0)/(float)dTy;
          bY =  fY - aY*(float)dTy;

          if (MODE != FILTER_ADJUST) {             //���� �� � ������ �������������� �������
            if (MODE != IDLE) save_value((char)'Y', fY, aY, bY);
          } else {
            new_saves = true;          //� ������ �������������� ������ ���� ��� ���� ����������, ����� ��������� ����
          }

          fY0 = fY;
          Ty0 = dTy;

          dTy = 0;
        }

        if (fabs(difZ) > (float)ADJ AND fabs(fZ0 - fZ)>(float)ADJ) {         //���������� ��� Z
          aZ = (fZ - fZ0)/(float)dTz;
          bZ =  fZ - aZ*(float)dTz;

          if (MODE != FILTER_ADJUST) {             //���� �� � ������ �������������� �������
            if (MODE != IDLE) save_value((char)'Y', fY, aY, bY);
          } else {
            new_saves = true;          //� ������ �������������� ������ ���� ��� ���� ����������, ����� ��������� ����
          }

          fZ0 = fZ;
          Tz0 = dTz;

          dTz = 0;
        }*/
}

//���������� �������
void command_exec(void){
  if (CmdIdExec > 0){                      //����������� �������
    if (cmd_timout == 0) {                 //��������� ������� �������
      DEBUG("Command %i is timedout\r\n", CmdIdExec);
      MODE = MODE_BEFORE_COMMAND;       //������ ���������� ����� ������
      command_done("ERR_TIMEOUT");      //�������� ������� ��������� ����������
    }
  }
}

void command_done(char *res){
  DEBUG("i COMMAND %i is DONE\r\n", CmdIdExec);
  printf("AT+S.HTTPGET=www.kajacloud.com,/device/command.php?SensorID=%i&CmdID=%i&done=1&responce=%s\r\n", SETS.SensorID, CmdIdExec, res);
  CmdIdExec     = 0;                    //������� ������� �� �����������
  cmd_timout    = 0;
  request_commands = true;            //������� ����������. ����� ����������� ��������
}

//��������� ������ � �������� �� �������
void process_command(char * block){
    char tmp[50];
    char cmd[11];       //������� +\0 
    
    request_commands = false;   //�������� ��� �� �� �������� �������� ����� ����� �������

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
    
    //�������� �������, ��� ������� �����������
    printf("AT+S.HTTPGET=www.kajacloud.com,/device/command.php?SensorID=%i&CmdID=%i&execution=1\r\n", SETS.SensorID, CmdIdExec);
    Delay(50);
    
    if (strcmp(cmd, "ADJ")==0){           //������ ������� ����������
      MODE_BEFORE_COMMAND = MODE;         //��������� ���������� �����
      MODE = FILTER_ADJUST;
    } else if (strcmp(cmd, "IDLE")==0) {  //�������� ������
      MODE = IDLE;
      command_done("SUCCESS");
    } else if (strcmp(cmd, "RUN")==0) {   //������ ������
      MODE = NORMAL_OPERATION;
      command_done("SUCCESS");
    } else {  
        DEBUG("Unknown command!\r\n");
         //�������� �������, ��� ������� "���������" � �� ����������
        command_done("ERR_UNKNOWN_CMD");
        cmd_timout      = 0;
    }
    WIFI_ping();    //���������, �������� ���� MODE ����� � ������ ������� �������� ������. �������� �����
}

//�������� ���������� ����� ������ <...> � </...> � ������
void get_value(char *input_str, char *left_str, char *right_str, char *output_str) {
  //���� ��� ���������� ^ � <...>^ 
    char * cmdstr = strstr(input_str, left_str) + strlen(left_str);
    //������� �������� ����� <...> � </...>
    int  length = strstr(input_str, right_str) - cmdstr;
    
    //�������� ���, ��� ����� ������ � ������������ ������
    strncpy(output_str, cmdstr, length); //������ ��� ����������� ��� � ��������
    
    if (length > 0)  output_str[length] = 0;
    else output_str[0] = 0; //����� �����
}

void init_settings(void){
	SETS.SensorID	          = 1;
	SETS.XOperationMode	  = 0;   //���������
        SETS.YOperationMode	  = 0;   //���������
        SETS.ZOperationMode	  = 0;   //���������
        SETS.GravityInfluenceAxis = 0;   //���������
        SETS.AwakeSampleRate      = 500; //500 ����� � ������������� � ������ ����������
        SETS.HitDetector          = 0;   //���������
        SETS.HitTrishold          = 100; //����� ������������ �� ����. ��� ����������. NB: ��� �����! ����� �� ����� ���� �� �������� ��������

	SETS.Filter	          = 110;  //��� ������ ����� �����. ��� ����������. �� ����� 10!
        SETS.Sensivity            = 2;   //����������������
	SETS.RadioAwakePeriod     = 5;   //������ ����� ������� ����������� ����� � ���������� PING � ����, ���
        SETS.SleepAfterIdle       = 80;  //������ ����� ����� ����� ����������� ������� ������ ��������� � ���������� ��� �� ��������������
	SETS.SleepSampleRate      = 50;  //50 ����� ��������� � �������������
        SETS.Window               = 200; //���� ����������� ��� ����������� 0
        SETS.ZeroPoints           = 5000; //����� ������� ����� ������ ����� ������������� ������� ����

        strcpy(SETS.SSID, "WirelessNet");
        strcpy(SETS.pass, "AwseDr00");
}

//���� 0, �� ������������ �������
void DEBUG_processing  (char data) {
  static char buff[31]; //������ ��� ���������
  char i = strlen(buff);
  char tmp[12];
  static uint8_t y, m, d, h, mi, s;
  static bool tobe_proccessed = false;
  
  if (data != 0){       //���� �� ���� ������ ����� � ������� ������
    if (i<(31-1-1)) {   //���� ������ ������� �� ��������� ��� ������ - 1 ������ (���� ��� \0)
      buff[i]  = data;  //����������� ������
      buff[i+1]= 0;     //���� ����� ������������ ������ ��������� �������� �� ������ ������ � �������
    }
    if (data == (char)('\r') OR data == (char)('\n')){  //���� ����������� ��������� �������
      tobe_proccessed = true;
    }
  } else {      //��������� ������
    if (tobe_proccessed) {
     tobe_proccessed = false;
          //<YR>14</YR>
          //<MO>06</MO>
          //<DY>01</DY>
          //<HR>13</HR>
          //<MN>52</MN>
          //<SE>23</SE>
      if (strstr(buff, "<YR>") AND strstr(buff, "</YR>")) {    
          //���
          get_value(buff, "<YR>", "</YR>", tmp);
          y = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<MO>") AND strstr(buff, "</MO>")) {
          //�����
          get_value(buff, "<MO>", "</MO>", tmp);
          m = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }

      if (strstr(buff, "<DY>") AND strstr(buff, "</DY>")) {
          //����
          get_value(buff, "<DY>", "</DY>", tmp);
          d = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<HR>") AND strstr(buff, "</HR>")) {
          //���
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
          //�������
          get_value(buff, "<SE>", "</SE>", tmp);
          s = atoi(tmp);
          DEBUG("OK. Make <SETTIME> for apply");
      }
      
      if (strstr(buff, "<SETTIME>")) {  
        RTC_SetDateTime(d, m, y, h, mi, s); //����� � RTC. ��������! �� �� ������ milliseconds 
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
      
     buff[0] = 0; //������� ������
    } //tobe_proccessed
  }
}

//�������� fromwhere ��� ������������ ������ ��� �����
// fromwhere - ������ (main, irq) ����� ��� ������. ������ ��� ��� ����� ��� �������
char WIFI_data_process (char * fromwhere) {  //��������� �����, ��� ���� �� WIFI (�� �� DEBUG!)
  //� ������ ������� �������� �� ����������. �������������� �����.
  //������ ����� ��� ��������� �������� ������, ��������������� \r ��� \n
  //������ � ������ ��������� ���� �� ����� �� ���������.
  //������� �������� ������ ���� �� FIFO � ����� ����� ��� ������������.
  //+ ������ ������� ������������ ������������ ��������� � ������� �� FIFO
  //�������� ���� ��� ���������, ������� ��� �� FIFO
  
  
  static char block[STR_BLOCK_SIZE+1];     //+1 ��� \0  //���� ��� �������. ���� �������������� ������ �� FIFO 
  char * p;     //��������� ��� ��������� �����
   
  short j = strlen(block);      //������� ������ � ������� �������� (������ �� ����������). ����� ������������ ��� ������ ��� ������ � ���������
  char c;                       //���������� char
  char SSID_NAME[33];          //�������� WIFI AP, ���������� ��� ������
  
  while (WIFICount) {           //���������� ��� ������� � FIFO
    
    //��������� ����� ������� �� WIFI. ��������� ������ ����� ��� ���� ��������. ����� ��� ��������� �������� ������ �� �������
    last_data_moment = cnt_timer;
    
    c = WIFIdataGet(0);         //������� ������ ������ �� FIFO
    
    WIFIdataDel();              //������� ������ �� ������ FIFO

    //��������� ������ � ���� ��� ���������, ������� ����� � ����������
    if (j<=STR_BLOCK_SIZE) {
      block[j]    = c;
      block[j+1]  = (char)'\0';
      j++;
    }
    
    if (c == (char)'\r' OR c == (char)'\n') break; //��� ����������� ����� ������ ��������������� � ������������ ����
  }


  //��������� ���������� �������-����� --------------------------------------------------------
  if (strlen(block)>1 AND (c == (char)'\r' OR c == (char)'\n')) {

        if (strstr(block, "SSID:")){          //������� ����� �������. ������ �� ��������, ��� ������������ �������� ��� ������ ST
          //�������� ������ ���� SSID: 'Ruspolymer' CAPS: 0431 WPA2
                    
           p = strchr (block, (int)'\'') + 1;                 //���� "'", ������ �� ��������� ������ ����� '
           
           //����� ���� ��������� ', ������� ��������� ��������
           j = 0; //�������� ���������� ���������� ��� ������
           
           //�������� �������� WIFI ����� ������� � ������
           while (p[0] != (char)'\''){
              SSID_NAME[j] = p[0]; 
              j++;
              *p++;
           }
           SSID_NAME[j]  = 0; //����� ������
           
           DEBUG("FOUND '%s' ", SSID_NAME);
           
           //���������� ��� �����. ���������� ����� �������� ����� �� �������� �� ������� ���� WPA� �������� ����� �������
           //�����: ���� ���� ��������, �� ������ �������������� ���������� ����. ������� ���������� if-�� ������ ����
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

        if (fWIFI_OK AND strstr(block, "Linked")){       //���� ��������� �� � ������ �����. ��������� �� ������ WIFI ESP. �������� v9.2.2
          DEBUG("DBG: LINKED!\r\n");
          fESP_sock_is_open = true;
        }

        if (fWIFI_OK AND strstr(block, "Unlink")){       //���� ��������� ��  � ������ �����. ��������� �� ������ WIFI ESP. �������� v9.2.2
          DEBUG("DBG: UNLINKED!\r\n");
          fESP_sock_is_open = true;
        }
                
        if (strstr(block, "<RESULT>") AND strstr(block, "POSTED") AND strstr(block, "</RESULT>")){
          fWIFI_file_sent = true;       //���� ������ ��������. ��. ������� �������� ������ �� ������
          goto EndOfProccessing;
        }
    
        if (strstr(block, "ID:") AND !strstr(block, "SSID:")) {            //������� ID ������ ��� ������������������� �������� POST � ������� (�� ��������� ������ �� ������)
          p = strchr (block, (int)':');                 //���� ���������
          p[strlen(p)-1] = (char)'\0';                  //������ \r ��� \n ������ \0, ����� ����� �� �������
          
          if (p != NULL) SocketID = atoi(p+1);          //���������� SOCKET ID
        }
    
        if (strstr(block, "OK\n\r") OR strstr(block, "OK\r\n") OR strstr(block, "\nOK\r") OR strstr(block, "\rOK\n") OR strstr(block, "SEND OK")){               //������� ����� OK �� �����-�� �������. ��� ESP � ST
          fWIFI_OK = true;
          DEBUG("DBG: OK detected\r\n");
          goto EndOfProccessing;
        }
        
        if (strstr(block, "ERROR\n\r")){               //������� ����� ERROR �� �����-�� �������. ��� ESP
          fWIFI_OK = false;
          DEBUG("DBG: ERROR detected\r\n");
          goto EndOfProccessing;
        }

        //����� �� ������� �� PING -----------------------------------------------------------
        if (strstr(block, "<PING>") AND strstr(block, "</PING>")) { 
         PING_period = 5*60*1000;                      //���� ������ 5� ������ 
          //����������, ������� ������ ��� �������
          char cmds[6];
          short dcmds;                                  //����� ������
          get_value(block, "<CMDS>", "</CMDS>", cmds);
            
          dcmds = atoi(cmds);                                   //����������� ��������� �� ����� ����� � Int 
          //������ �������, ���� ������� ������ ��� �������`
          DEBUG("> PING ANS, CMDS=%i ", dcmds);
          RTC_DateTimePrint();
        
          if (dcmds != 0 && CmdIdExec == 0){            //���� ���� ������� � ������ ������� �� �����������                              
              request_commands = true;                   //�� main ������� � ������� �������. � ���� �������� ��������� ��������� ������
          }
        goto EndOfProccessing;
        } //����� �� ������� �� PING
        
        if (strstr(block, "<PINGBACK>")){               //������ ���-�� �������� � ������ ������� ��� ������������
          PING_period = 0;                              //������ ���, ��� ���� �����������
          goto EndOfProccessing;
        }
        
        if (strstr(block, "<") AND strstr(block, ">"))  {                   //������ ����� ��� �� �������
          fDataEnd = false;                             //���������� ������ ������ �� �������
          WIFI_period = WIFI_CONNECTION_CHECK_PERIOD;   //���������� ������ �� �������� ���������� wifi ����� �������� �������� ����� ����������
        }
        
        if (strstr(block, "<DATA>"))  {
        }
        
        if (strstr(block, "</DATA>")) {
          fDataEnd = true;          //���������� ������ ������ �� �������
        }
        
        //������ ����� firmware
        if (strstr(block, "<FW>") AND strstr(block, "</FW>"))  {
          char dta[70];
          get_value(block, "<FW>", "</FW>", dta);
          save(DOWNLOAD, "%s\r\n", dta);
          //write_download_file(block);
        }
        
        if (strstr(block, "<LINES>") AND strstr(block, "</LINES>")) { 
          //����������, ������� ������ ��� �������
          char lines[6];
          get_value(block, "<LINES>", "</LINES>", lines);
          
          download_lines_count = atoi(lines);      //����������� ��������� �� ����� ����� � Int 
        } 
        

        //������� ������ �������
        if (strstr(block, "<NO>") AND strstr(block, "</NO>")) { 
          //����������, ������� ������ ��� �������
          char line[6];
          get_value(block, "<NO>", "</NO>", line);
          
          last_downloaded_line = atoi(line);      //����������� ��������� �� ����� ����� � Int 

          //goto EndOfProccessing �� �����, � �� ���������� �� ��� ������ �� ��������
        } 
        
        

            
        //������ ����� ��� RTC  ------------------------------------------------------
        if (strstr(block, "<TIME>") AND strstr(block, "</TIME>")) {
          char tmp[12];
          
          DEBUG("SERVER DATE TIME.\r\n");
          // <TIME><YR>14</YR><MO>06</MO><DY>01</DY><HR>13</HR><MN>52</MN><SE>23</SE><MS>0913</MS></TIME>
          //���
          get_value(block, "<YR>", "</YR>", tmp);
          uint8_t y = atoi(tmp);
          
          //�����
          get_value(block, "<MO>", "</MO>", tmp);
          uint8_t m = atoi(tmp);

          //����
          get_value(block, "<DY>", "</DY>", tmp);
          uint8_t d = atoi(tmp);
          
          //���
          get_value(block, "<HR>", "</HR>", tmp);
          uint8_t h = atoi(tmp);

          get_value(block, "<MN>", "</MN>", tmp);
          uint8_t mi = atoi(tmp);
          
          //�������
          get_value(block, "<SE>", "</SE>", tmp);
          uint8_t s = atoi(tmp);
          

          RTC_SetDateTime(d, m, y, h, mi, s); //����� � RTC. ��������! �� �� ������ milliseconds 
          
          //���������� �������, � � ���������� SysTick milliseconds ���������� � 0;
          //������������
          get_value(block, "<MS>", "</MS>", tmp);
          milliseconds = atoi(tmp);          
          RTC_DateTimePrint();
          
          goto EndOfProccessing;
      } //������ ����� ��� RTC

        if (strstr(block, "+IPD")) {
          WIFI_period = WIFI_CONNECTION_CHECK_PERIOD;   //���������� ������ �� �������� ���������� wifi ����� �������� �������� ����� ����������
          DEBUG("---- +IPD is found\r\n");
        }
        
        //������� --------------------------------------------------------------------
        if (strstr(block, "<CMD>") AND strstr(block, "</CMD>")){
          if (!CmdIdExec) process_command(block);       //���� ������� ������� �� ����������� -- �������� ���
        } //�������
        
  //������� ��������� ���������
  EndOfProccessing:
        block[0] = (char)'\0';  //������� ������
        return 1;
  } //�������� �����
  
    return NULL;
} //END OF function
  
void WIFI_set_settings(void) {

}

//����������� ����� � ������� ��� ������������� �����
//� ������� ������� ������ CommandProcessing
void WIFI_request_datetime (void) {
  DEBUG(">SRV DATE TIME? ");
  //RTC_DateTimePrint();
  printf("AT+S.HTTPGET=www.kaija.ru,/device/datetime.php\r\n");     
}

//����������� ������� � �������
//� ������� ������ ������ CommandProcessing
void WIFI_request_commands (void) {
  if (request_commands) {
    DEBUG("< CMDS?\r\n");
    request_commands = false; //����� �����. �.�. �� ����������� �������
    printf("AT+S.HTTPGET=www.kaija.ru,/device/command.php?SensorID=%i\r\n", SETS.SensorID);
  }
}


//���������� ��� ����������������� ����� ��� �������� �� ������
void WIFI_send_files() {
  static long long moment;
  DIR dir;
  FILINFO fileInfo;
  FRESULT result;
  bool flag_found = false;
  char new_file_name[20]; //� ������ ���� ����� 0:\active.TS
  char old_file_name[20];
  

  
  //������� ������ ���� .ts = to send

  if (WIFI == WIFI_ONLINE AND cnt_timer-moment > 300 AND   sflag) {        //��� WIFI ������ �� ������ � ���� 300 �� ������ ��� ����� ������ ������� ����� DMA ��� SDIO ������
    
  
  if (f_mount(&Fatfs, "0:", 1) == FR_OK){                     //�����������    
    result = f_opendir(&dir, "0:/");
      if (result == FR_OK){
         while ((f_readdir(&dir, &fileInfo) == FR_OK) AND !flag_found AND fileInfo.fname[0] != 0){
            if (strstr(fileInfo.fname, ".TS")){        //���� ���� ������� �� �������� To Send
               // DEBUG("File %s\r\n", fileInfo.fname);
                flag_found = true;
                //���� ����� ���������� ��� ��������� ������ ����� ����� ������� ����� ������� �� main
                goto next_step;
            }
         } //������� ������
      }

    f_mount(NULL, "0:", 1);
next_step:
    if (flag_found){              //������� ���� �� ��������
        sflag = false;
      //���������� ���
    //  if (WIFI_send_file(fileInfo.fname) == R_OK AND fWIFI_file_sent){        //���� �������� ������ �������. ST!
        if (ESP_send_file(fileInfo.fname) == R_OK AND fWIFI_file_sent) {
        //��������������� � .st = sent
        old_file_name[0]=0;
        strcat(old_file_name, "0:");
        strcat(old_file_name, fileInfo.fname);
        
        if (f_mount(&Fatfs, "0:", 1) == FR_OK){   //�� � WIFI_send_file � ����� f_mount(NULL, "0:", 1);
          strcpy(new_file_name, old_file_name);
          char * p     = strchr (new_file_name, (int)'.');     //���� �����
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

//���� �� ������ �� ��� �� �������� ����������
//����� ��� ST � ��� ESP
WIFI_MODULE_RESULT WIFI_TimeOut_OK(unsigned long ms) {
        unsigned long long moment;                      //��� ������� ��������� � ���

        //���������� � ������� � �������
        fWIFI_OK = false;                               //���� ������ ����������
       // DEBUG("WAIT...");
        moment = cnt_timer;                             //��������� �������� �������� �� ��������
        
        //������ ���� ������ ��, �� ����������� �� WIFI ������������
        while (cnt_timer-moment < (long long) ms AND !fWIFI_OK){
          //�������� ��������� ������� main
          FLUSH();                                      //�������� ����� DEBUG � WIFI. ����� ����� �������.
          WIFI_data_process("WIFI_TimeOut_OK");         //������������ ������, ����������� �� WI-FI. ���������� ��������� ������� �� �� ����������.          
          
          //��������! ������ ������ flush_to_sd. �.�. ������� WIFI_TimeOut_OK ���� ����� ���� ������� ��� �������� �����. ��������, ��� ������ ����� ��� �������� ������ �� ������
          //flush_to_sd();                                //������ ������� �� SD
        }
        
        if (fWIFI_OK) return RESULT_OK; 
        else return RESULT_TIMEOUT;
       // if (WIFI != WIFI_ONLINE) DEBUG("TIMEOUT\r\n");
       // else DEBUG("recieved OK\r\n");
}

//���� �������� �������� ������ ��� </DATA>
void WIFI_TimeOut_Sock(unsigned long ms) {


        //���������� � ������� � �������
        last_data_moment = cnt_timer;                             //��������� �������� �������� �� ��������
        fDataEnd = false;
        
        while (cnt_timer-last_data_moment < (long long) ms AND !fDataEnd){
          FLUSH();                                      //�������� ����� DEBUG � WIFI. ����� ����� �������.
          WIFI_data_process("WIFI_TimeOut_Sock");         //������������ ������, ����������� �� WI-FI. ���������� ��������� ������� �� �� ����������.          
          flush_to_sd();                                //������ ������� �� SD
        }
}


//���������� ���� ���������� �� ������ ������ � ��� ��������.
void WIFI_ping (void) {
  DEBUG("< PING MODE=%i\r\n", MODE);
  RTC_DateTimePrint();
  char dt[20];
  timestamp(&dt[0]);
  printf("AT+S.HTTPGET=www.kajacloud.com,/device/ping.php?SensorID=%i&Battery=%i&RTC=%s&Data=%i&State=%i\r\n", SETS.SensorID, battery, dt, 2, MODE);
}


//���������� ���������� ����� POST �������� �� ������ ��� ���������� ���������
//��� ESP
SENDRES ESP_send_file(char * filename) {
  char buff [255];             //������ ��������� �� ����� 4096 ����. �� ���� ������ ����������� printf � ������� � ������. ���� �������, ��� 255 - ����������� �����
  char heads[200];              //���������
  char datablock[1300];          //������ ����� � �������. ���� ����� SensorID � ��. ����
  char datablockend[150];       //��������� ����� � �������
  
  heads[0]              = 0;
  buff[0]               = 0;
  datablock[0]          = 0;
  datablockend[0]       = 0;

  FIL file;
  long long moment;
  unsigned int ttl;
  
  DEBUG("< SEND FILE %s ---------------------------- \r\n", filename);
  RTC_DateTimePrint();
  
  fWIFI_file_sent = false;                              //���������� ���� ������ � ���, ��� ���� ������ ��������
  

  if (ESP_open_sock() == RESULT_OK) {                           //��������� ����
    
     if (f_mount(&Fatfs, "0:", 1) == FR_OK){                 //�����������
        char r=f_open(&file, filename, FA_READ);                  //��������� ���� ��� ��������
              
         //������� ����� �������       
        sprintf(datablock, "Content-Disposition: form-data; name=\"SensorID\"\r\n\r\n%i\r\n", SETS.SensorID );
        strcat(datablock, "--1BEF\r\n");
        
        //�������� ���������� heads � ������. ����� ������� ������...
        sprintf(heads, "Content-Disposition: form-data; name=\"upload_file\"; filename=\"%s\"\r\n", filename);        
        strcat(datablock, heads);               //������� � ������ ��� �����
        strcat(datablock, "Content-Type: text/plain\r\n");
        strcat(datablock, "\r\n");

        strcat(datablockend, "\r\n");
        strcat(datablockend, "--1BEF--\r\n");
        strcat(datablockend, "\r\n\r\n");
        
        heads[0] = 0; //������� heads
        
        //���������� ��������� HTTP �������, � ��� ����� � � �������� ������������� ��������
        sprintf(heads, "POST /device/postfile.php HTTP/1.1\r\nHost: kajacloud.com\r\nConnection: keep-alive\r\nContent-Type: multipart/form-data; boundary=1BEF\r\nAccept: text/html\r\nContent-Length: %i\r\n\r\n", strlen(datablock) + strlen(datablockend) + file.fsize);
      
        moment = cnt_timer;
             //�. ����� � ����� ��������� ----------------------------------------------------
             DEBUG("SEND HEADERS %i bytes...\r\n", strlen(heads));
             
             fESP_send_ready = false;                              //���� ����������� ���������� � ���� ���
             printf("AT+CIPSEND=%i\r\n", strlen(heads));
        
             ESP_wait_send_ready();  //���� ����������� �� ������
        
             if (fESP_send_ready) {       //�� ���������� ������� ������ �������
               //DEBUG("OK, now send headers\r\n");
               printf("%s", heads);       //���������� ���������
               WIFI_TimeOut_OK(1500);     //���� "SEND OK" ESP v922
             }
             
             //�. ����� ������ ����� ������  ---------------------------------------------------
             //�����, ������������ ���������� ������� -----------------------------------------------------------------
             
             fESP_send_ready = false;                              //���� ����������� ���������� � ���� ���
             printf("AT+CIPSEND=%i\r\n", strlen(datablock));

             //���� ���� ��, ����� ������ ����. �������� ������ > ���� �������� �� ����������, �.�. �� AT+CIPSEND ">" �������� �� ESP_wait_send_ready
             ESP_wait_send_ready();  //���� ����������� �� ������
        
             if (fESP_send_ready) {       //�� ���������� ������� ������ �������
               //DEBUG("OK, now send start part\r\n");
               printf("%s", datablock);       //���������� ���������
               WIFI_TimeOut_OK(1500);     //���� "SEND OK" ESP v922
             } else {
                  DEBUG("ERROR: Module does not sent ready char!\r\n");
             }
             
             //�. ����� ���������� �����. �������. ���� ���� ������ ����� �� ���������
             DEBUG("\r\n\r\nSTART POST DATA FROM FILE----------------\r\n\r\n");
        
              //���������� � ���� ������� ���������� ����� --------------------------------------------------------------
              //���� �������� �������� ����� ���������� ������ ���� ���� �� ������ � ���. ������� ������ � ���� ���������.
              
             //��������� ���������� � ����� ������ ��� ��������. ���������� ���.
             short packetlen=0;
             
             datablock[0] = 0;
             while (f_gets(buff, sizeof(buff), &file)) {
                short length = strlen(buff);

                if ((strlen(datablock) + length) < sizeof(datablock)) {
                  strcat(datablock, buff);
                  packetlen = packetlen + length;
                
                } else {
                
                    //������ ������ � ������� �� �����
                    fESP_send_ready = false;                              //���� ����������� ���������� � ���� ���
                    printf("AT+CIPSEND=%i\r\n", packetlen);
                    
                    //���� ���� ��, ����� ������ ����. �������� ������ > ���� �������� �� ����������, �.�. �� AT+CIPSEND ">" �������� �� ESP_wait_send_ready
                    ESP_wait_send_ready();                          //���� ����������� �� ������

                    if (fESP_send_ready) {                          //�� ���������� ������� ������ �������
                      // DEBUG("OK, send %i bytes\r\n", packetlen);
                       printf("%s", datablock);                     //���������� ���������
                       ttl += packetlen;
                       WIFI_TimeOut_OK(1500);                       //���� "SEND OK" ESP v922
                    } else {
                      DEBUG("ERROR: Module does not sent ready char!\r\n");
                      break;
                    }
                    
                    datablock[0] = 0;
                    packetlen = 0;
                }
                
              } //while
             
               //TODO � ���������� �������� ���������!

              DEBUG("%i bytes in %i ms\r\n", ttl, cnt_timer-moment);
             
             
        f_close(&file);                 //��������� ����
        f_mount(NULL, "0:", 1) ;                            //UNMOUNT

          RTC_DateTimePrint();
        Delay(10000);
    }
    ESP_socket_close();
  } else {
    return R_UNDEFINED_ERR;
  }
}


//����������� ������� � �������
//� ������� ������ ������ CommandProcessing
void ESP_get_commands (void) {
  if (request_commands) {
    DEBUG("< CMDS?\r\n");
    request_commands = false; //����� �����. �.�. �� ����������� �������
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

//��������� ���� � ����� ��������� � �������
void update_fw(void) {
  //��������� ���-�� ����� � ����� � ���������
  if (!download_lines_count && WIFI==WIFI_ONLINE) {
    ESP_HTTPGET("device/firmware.php", true, "");     //������ ������ ������ ���������� � ����� ����� ��� ����������
  }
  
}

//��� ������ ����� �������� �� ������ ��� ��������� +IPD 
//  printf("AT+CIPMODE1\r\n");
//}

//���������� ������ GET �� ESP. ��� ST �� ���������, ���� ���� � ������
//page - ����� ��������
//��������� - ������ � �����������, ������� ������
//www.kaija.ru/device/ping.php?SensorID=%i&Battery=%i&RTC=%s&Data=%i&State=%i
void ESP_HTTPGET(char const* page, bool close ,char const* fmt, ... ) {
  unsigned int line=0; //������, ������� ��������� �� �������� �����
  
  char buff[512+1];
  buff[0] = 0;
  
  va_list   uk_arg;
  va_start (uk_arg, fmt);
  marker = 11;
  
   if (ESP_open_sock() == RESULT_OK) {    
     int slen = vsnprintf(buff, 512, fmt, uk_arg);       //�������������� ������

     //���� ���� ��, ����� ������ ����. �������� ������ > ���� �������� �� ����������, �.�. �� AT+CIPSEND ">" �������� �� ESP_wait_send_ready
     printf("AT+CIPMODE=1\r\n");                //���������� ����� sock-usart. ���������� ������ +++ ��� \r\n
     Delay(100);
     DEBUG("DBG: Sock is open\r\n");
     fESP_send_ready = false;                              //���� ����������� ���������� � ���� ���
     
      //GET / HTTP/1.1\r\nHost: kaija.ru\r\n\r\n - 34 ����
     //printf("AT+CIPSEND=%i\r\n", strlen(buff)+strlen(page)+34);      //� ����� ������ > ��� ����� ��������� n ����
     printf("AT+CIPSEND\r\n");
     //�������� ">" ��� \r\n. ������� � WIFI_data_proccess ������������ �� �����. 
    //�� � ���������� ��� ��������� ������� '>' ������������ ������� fESP_send_ready = true
     
     //����=false ���� ���������� �������, ����� ������ ����. 
     //�������� ������ > ���� �������� �� ����������, �.�. �� AT+CIPSEND ">" �������� �� ESP_wait_send_ready 
     ESP_wait_send_ready();
     
     //��� ������� ��� ����������
     if (fESP_send_ready) {     //�� ����������
       DEBUG("DBG: ESP ready to send\r\n");
       
       printf("GET /%s", page);
       if (strlen(buff)) printf("?%s", buff); //���� ������ � ����������� ?...
       printf(" HTTP/1.1\r\nHost: kaija.ru\r\n");
       
       /*if (close) {
          printf("Connection: close\r\n");
       } else {
          printf("Connection: Keep-Alive\r\n");
       }*/
       printf("\r\n");
       
       //���� �������� ������ �� ������� ��� ��������� �������� - </DATA>
       WIFI_TimeOut_Sock(2500);       //���� ����� �������� �� �������� </DATA> ��� �������� �� �������� ������ (�������� � ���)
       
       //��� ������� ������� ������ ������� �� ����� �� ������ � �������� ������ �� SD ��������. 
       //������� ������� �������� ���� ����� � ����� <LINES>download_lines_count</LINES> � �� �������� ������� ��������� ����������� ����
       //�� ����� ������ ������ ����������� ��� ������ � ����������� WIFI_data_proccess. ������, ���� ��������� ������� �� ����������
       //����� ������ �� �������������.
       
       last_downloaded_line = 0;
       if (download_lines_count){
         //open_download_file();
           while (download_lines_count != line){
             line = last_downloaded_line+1;
             printf("GET /%s", page);
             if (strlen(buff)) {
               printf("?%s&line=%i", buff, line); //���� ������ � ����������� ? + ����� ����������� ������
             } else {
               printf("?line=%i", line); //���� ������ � ����������� ? + ����� ����������� ������
             }
             printf(" HTTP/1.1\r\nHost: kaija.ru\r\n");
             
             /*if (close) {
                printf("Connection: close\r\n");
             } else {
                printf("Connection: Keep-Alive\r\n");
             }*/
             printf("\r\n");
             
             WIFI_TimeOut_Sock(1000);       //���� ����� �������� �� �������� </DATA> ��� �������� �� �������� ������ (�������� � ���)
         }
         //close_download_file();
     }
       
     } else {
       DEBUG("DBG: ERROR: Module does not sent ready char!\r\n");
       printf("\r\n\r\n"); //��� �������� �������� ������
     }
     

     
     //��������� ����� ������ ����� UART-����� � ������
     printf("+++");
     Delay(10);
     printf("+\r\n");
     ESP_socket_close();                //��������� �����
  } else {
    DEBUG("DBG: ERROR: SOCKET PROBLEM!\r\n");
  }
}


//���� ����������� �������� ������ ">" �� ESP
void ESP_wait_send_ready(void) {
    unsigned long long moment;                      //��� ������� ��������� � ���
     moment = cnt_timer;                                    //��������� �������� �������� �� ��������
    
     //�� �������� ���� ������� ���������� ����
     // fESP_send_ready = false;                              //���� ����������� ���������� � ���� ���
     
     //���� �������� ���������� ��������� ��� ������� ����������� ����� ���������� USART
     while (cnt_timer-moment < 4000 AND !fESP_send_ready){
          //�������� ��������� ������� main
          FLUSH();                                      //�������� ����� DEBUG � WIFI. ����� ����� �������.
          WIFI_data_process("ESP_wait_send_ready");         //������������ ������, ����������� �� WI-FI. ���������� ��������� ������� �� �� ����������.          
          
          //��������! ������ ������ flush_to_sd. �.�. ������� WIFI_TimeOut_OK ���� ����� ���� ������� ��� �������� �����. ��������, ��� ������ ����� ��� �������� ������ �� ������
          //flush_to_sd();                                //������ ������� �� SD
      }
}

void ESP_socket_close(void) {
  DEBUG("DBG: Close socket\r\n");
  //RTC_DateTimePrint();
  printf("AT+CIPCLOSE\r\n");
}


WIFI_MODULE_RESULT ESP_open_sock(void) {
   
  if (WIFI == WIFI_ONLINE) {
    fWIFI_OK = false;   //�������� ���� ��� �� 
    printf("AT+CIPSTART=\"TCP\",\"www.kaija.ru\",80\r\n");      //� ����������� ���������� 

    if (ESP_TimeOut_Open_Socket(3500) == RESULT_OK) {
      DEBUG("DBG: Socket is ready!\r\n");
      return RESULT_OK;
    } else {
      DEBUG("DBG: ERROR Can't open sock!\r\n");
      ESP_socket_close(); // ��� ���������������� �������� ������ �� ������, ���� �� ��� ��� ������ �����
      return RESULT_FAIL;
    }
  } else {
    DEBUG("DBG: ERROR Can't open! No WIFI connnection!");
    return RESULT_FAIL;
  }
}

WIFI_MODULE_RESULT ESP_TimeOut_Open_Socket(unsigned long ms) {
        unsigned long long moment;                      //��� ������� ��������� � ���

        //�������� ���� ��� ��
        //fWIFI_OK = false;                               //���� ������ OK
       // DEBUG("WAIT...");
        moment = cnt_timer;                             //��������� �������� �������� �� ��������
        //������ ���� ������ ��, �� ����������� �� WIFI ������������
        while (cnt_timer-moment < (long long) ms AND !fESP_sock_is_open){
          //�������� ��������� ������� main
          FLUSH();                                      //�������� ����� DEBUG � WIFI. ����� ����� �������.
          WIFI_data_process("ESP_TimeOut_Open_Socket");         //������������ ������, ����������� �� WI-FI. ���������� ��������� ������� �� �� ����������.          
          
          //��������! ������ ������ flush_to_sd. �.�. ������� ���� ����� ���� ������� ��� �������� �����. ��������, ��� ������ ����� ��� �������� ������ �� ������
          //flush_to_sd();                                //������ ������� �� SD
        }
        
        if (fWIFI_OK) return RESULT_OK; 
        else return RESULT_TIMEOUT;
}

//��������� ��� � ������ ESP. �������� 9.2.2
void ESP_echo_off (void) {
  printf("ATE0\r\n");
  WIFI_TimeOut_OK(100);
}


//���������, ��������� �� � WIFI ������. ��� ESP
void ESP_is_connected (void) {
  if (WIFI_period == 0) {       //���� ��������� ��������� �����������      TO-DO: �� ������ ��� ����� ���� �������� ������, ����         
 //   DEBUG("DBG: WIFI check\r\n");
    printf("AT+CIFSR\r\n");     //�������� IP. ���� ������ �����, ������ �� ����������
    if (WIFI_TimeOut_OK(500) == RESULT_OK) {
        WIFI = WIFI_ONLINE;
        WIFI_period = WIFI_CONNECTION_CHECK_PERIOD;                   //��������� ����������� ����� ����� 20 ������
     // DEBUG("ESP MODULE IS ONLINE!\r\n");
    } else {
        WIFI = WIFI_OFFLINE;
        WIFI_period =  WIFI_CONNECTION_CHECK_PERIOD/10;                   //��������� ����������� ����� � ����
    };
  }//WIFI_period
}