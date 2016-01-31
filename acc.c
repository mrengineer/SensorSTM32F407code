#include "stm32f4_discovery_lis302dl.h"
#include "main.h"
#include "acc.h"
#include "logic.h"


uint8_t Buffer[6]; //������ ��� ���������� ������ �� ��������������
signed char X, Y, Z;
     
void acc_init(){ 
LIS302DL_InitTypeDef  LIS302DL_InitStruct;
  
  // Set configuration of LIS302DL
  LIS302DL_InitStruct.Power_Mode      = LIS302DL_LOWPOWERMODE_ACTIVE;
  LIS302DL_InitStruct.Output_DataRate = LIS302DL_DATARATE_400;
  LIS302DL_InitStruct.Axes_Enable     = LIS302DL_X_ENABLE | LIS302DL_Y_ENABLE | LIS302DL_Z_ENABLE;
  LIS302DL_InitStruct.Full_Scale      = LIS302DL_FULLSCALE_2_3;
  LIS302DL_InitStruct.Self_Test       = LIS302DL_SELFTEST_NORMAL;
 
  LIS302DL_Init(&LIS302DL_InitStruct); //��-�� ACC ������ ��� ��� ��������� ��������� RTC
    
  /* Required delay for the MEMS Accelerometre: Turn-on time = 3/Output data Rate 
                                                             = 3/100 = 30ms */
  Delay(30);
}

//��������� ������� �������� ��������������, ���������� �� �� ���������� ���������
void acc_read (void){
  
     LIS302DL_Read(Buffer, LIS302DL_OUT_X_ADDR, 6);
      X = ((signed char)(Buffer[0]));
      Y = ((signed char)(Buffer[2]));
      Z = ((signed char)(Buffer[4]));
      

      //������ � logic.c ��� �������������� ������ ������ ������
      //��������������! ���� 500 �� �� 1000 �������
      process_data((signed short)X, (signed short)Y, (signed short)Z);     
}

void acc_print (void){
      DEBUG ("%i,%i,%i\r\n", X, Y, Z);
}

/**
  * @brief  MEMS accelerometre management of the timeout situation.
  * @param  None.
  * @retval None.
  */
uint32_t LIS302DL_TIMEOUT_UserCallback(void)
{
  /* MEMS Accelerometer Timeout error occured */
  while (1)
  {
  }
}