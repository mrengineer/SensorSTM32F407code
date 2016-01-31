
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "main.h"
#include "acc.h"
#include "logic.h"
#include "rtc.h"
#include "usart.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4_discovery.h"
#include <stdint.h>
#include "sdio_sd.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
extern uint8_t Buffer[6];
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern unsigned int milliseconds;       //rtc ms
extern unsigned char seconds;           //rtc seconds
extern unsigned char minutes;
extern unsigned char hours;             //rtc hours
extern unsigned long long cnt_timer;    //������� ��� ����������� ��������� ��������� � ����
extern unsigned long mains;

extern unsigned char MODE;              //����� ������ ����������


extern bool sflag;

unsigned short marker = 0;

extern unsigned short irqs;

extern void battery_read(void);


//FOR USART
//#define USARTx_IRQHANDLER   USART3_IRQHandler

#define TXBUFFERSIZE   (countof(TxBuffer) - 1)
#define RXBUFFERSIZE   0x20

/* Private macro -------------------------------------------------------------*/
#define countof(a)   (sizeof(a) / sizeof(*(a)))

uint8_t RxBuffer[RXBUFFERSIZE];
uint8_t NbrOfDataToRead = RXBUFFERSIZE;
__IO uint8_t TxCounter = 0;
__IO uint16_t RxCounter = 0;


void SysTick_Handler(void){  //each 1 ms

  //��������! SYS_TICK ���������� ��������� DELAY ���� ��� ������� �� ����������
  static short blink;
  static char sec, reductor;
  RTC_TimeTypeDef   RTC_TimeStructure;
  
  cnt_timer++;
  blink++;
  
  if (blink==250){
      blink=0;
      
     // PE7_toggle();
    //  LED_toggle(LED_ORANGE);
  }
  

  TimingDelay_Decrement();              //��������� �������� Delay � Period*
  milliseconds ++;                      //������������ ��� rtc
  
//��� ����� �������� ������������
  RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
  if (RTC_TimeStructure.RTC_Seconds!=sec) { //������ �������

    battery_read();     //���������� ���������� ������� � ������� � ����������
    
    //DEBUG("IRQs: %i MAINS: %i\r\n", irqs, mains);
    milliseconds  = 0;
    seconds       = RTC_TimeStructure.RTC_Seconds;           //rtc seconds
    minutes       = RTC_TimeStructure.RTC_Minutes;           //minutes
    hours         = RTC_TimeStructure.RTC_Hours;             //rtc hours
    irqs          = 0;                  //���������� ����� ���������� �������������. ����������� � �������������� �������
    mains         = 0; 
  }
  
  //��� ����� �������� ������������
  sec = RTC_TimeStructure.RTC_Seconds;

 /* if (reductor == 1) {
    reductor = 0;
    acc_read(); //������ ��������� �������������
  } else {
    reductor++;
  }
  */
  irqs++;

//�������� ���� 400 �� ������������ ������ �� ������
}


/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  unsigned long i = 0;
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1){
    //������� ���� ����� �������� ��-�� ���������� �����. ��� � ����������, � ����� �������� ����� �������
    //��� ��� ������ RTC
    marker = marker;
 
    if (i == 1000000 ) {
      LED_toggle(LED_RED);
      i=0;
    }
      i++;

  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */



/******************************************************************************/
/*                 ���������� �� SDIO                   */
/******************************************************************************/
/**
  * @brief  This function handles SDIO global interrupt request.
  * @param  None
  * @retval None
  */
void SDIO_IRQHandler(void)
{
  /* Process All SDIO Interrupt Sources */
  SD_ProcessIRQSrc();
}

/**
  * @brief  This function handles DMA2 Stream3 or DMA2 Stream6 global interrupts
  *         requests.
  * @param  None
  * @retval None
  */
/*void SD_SDIO_DMA_IRQHANDLER(void)
{
  // Process DMA2 Stream3 or DMA2 Stream6 Interrupt Sources
  SD_ProcessDMAIRQ();
}*/

void DMA2_Stream3_IRQHandler(void)//SD_SDIO_DMA_IRQHANDLER
{
  /* Process DMA2 Stream3 or DMA2 Stream6 Interrupt Sources */
  SD_ProcessDMAIRQ();
}




/******************************************************************************/
/*                 STM32Fxxx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32fxxx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @brief  This function handles EXTI0_IRQ Handler.
  * @param  None
  * @retval None
  */
void EXTI0_IRQHandler(void){
  //������ ���������������� ������
  if(EXTI_GetITStatus(USER_BUTTON_EXTI_LINE) != RESET) {

    /* Display the current alarm on the Hyperterminal */
//    RTC_TimeShow();
   // RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
   // DEBUG("SEND FILES!!!! irq\r\n\r\n\r\n");
    //sflag = true;
    /* Clear the Wakeup Button EXTI line pending bit */
    EXTI_ClearITPendingBit(USER_BUTTON_EXTI_LINE);
  }
}

/**
  * @brief  This function handles EXTI15_10_IRQ Handler.
  * @param  None
  * @retval None
  */
void OTG_FS_WKUP_IRQHandler(void)
{
/*  if(USB_OTG_dev.cfg.low_power)
  {
	// Reset SLEEPDEEP and SLEEPONEXIT bits
	SCB->SCR &= (uint32_t)~((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));

	// After wake-up from sleep mode, reconfigure the system clock
	SystemInit();
    USB_OTG_UngateClock(&USB_OTG_dev);
  }
  EXTI_ClearITPendingBit(EXTI_Line18);
  */
}

/**
  * @brief  This function handles OTG_HS Handler.
  * @param  None
  * @retval None
  */
void OTG_FS_IRQHandler(void)
{
 // USBD_OTG_ISR_Handler (&USB_OTG_dev);
}



#ifdef USB_OTG_HS_DEDICATED_EP1_ENABLED
/**
  * @brief  This function handles EP1_IN Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_EP1_IN_IRQHandler(void)
{
 // USBD_OTG_EP1IN_ISR_Handler (&USB_OTG_dev);
}

/**
  * @brief  This function handles EP1_OUT Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_EP1_OUT_IRQHandler(void)
{
 // USBD_OTG_EP1OUT_ISR_Handler (&USB_OTG_dev);
}
#endif

/**
  * @brief  This function handles RTC Alarms interrupt request.
  * @param  None
  * @retval None
  */
void RTC_Alarm_IRQHandler(void){
 /* if(RTC_GetITStatus(RTC_IT_ALRA) != RESET) {
    // Clear RTC AlarmA Flags
    RTC_ClearITPendingBit(RTC_IT_ALRA);

  }
    else
  {
      // Disable the RTC Clock
      //RCC_RTCCLKCmd(DISABLE);
  }
  // Clear the EXTIL line 17
  EXTI_ClearITPendingBit(EXTI_Line17);
  //printf("ms=%i", milliseconds);
  //milliseconds = 0;
      //printf("Alarm!\r");
      LED_toggle(LED6); */
}



