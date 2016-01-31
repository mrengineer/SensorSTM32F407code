/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2007        */
/*-----------------------------------------------------------------------*/
/* This is a stub disk I/O module that acts as front end of the existing */
/* disk I/O modules and attach it to FatFs module with common interface. */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
//Software
#include "integer.h"

// chan-lib "stuff"
#include "ff.h"
#include "ffconf.h"

#include "sdio_sd.h"

extern SD_Error Status;
extern BYTE SD_State;



/*-----------------------------------------------------------------------*/
/* Correspondence between physical drive number and physical drive.      */

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */

DSTATUS disk_initialize (
	BYTE drv				/* Physical drive nmuber (0..) */
)
{
  DSTATUS stat;
  
 // if (stat & STA_NOINIT)	{  //часто вызывается из других мест
         Status = SD_Init();        
           
         if(Status == SD_OK) {
           stat = RES_OK;
           //DEBUG ("SD initialize is OK\r\n");
         } else { 
           stat = RES_ERROR;
           //DEBUG ("SD initialize is FAILED\r\n");
         }
 // } else {
 //   stat = RES_OK;
 // }
          
	return stat;

}



/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */

DSTATUS disk_status (
	BYTE drv		/* Physical drive nmuber (0..) */
)
{
	DSTATUS stat = 0;

        //Диск всегда вставлен, инициализирован и не защищен от записи
        
        stat = SD_State;
                                
        return stat;
}


DRESULT disk_read (
                   BYTE drv,			/* Physical drive number (0) */
                   BYTE *buff,			/* Pointer to the data buffer to store read data */
                   DWORD sector,		/* Start sector number (LBA) */
                   BYTE count			/* Sector count (1..255) */
                     )
{
  uint32_t timeout = 100000;
  SD_Error sdstatus = SD_OK;

        SD_ReadMultiBlocks((BYTE *)buff, (uint32_t )(sector * 512), 512, count);
        /* Check if the Transfer is finished */
        sdstatus = SD_WaitReadOperation();

        while(SD_GetStatus() != SD_TRANSFER_OK)
        {

          if (timeout-- == 0)
          {
            return RES_ERROR;
          }
        }

        if (sdstatus == SD_OK)
        {
          return RES_OK;
        }
      

    return RES_NOTRDY;
}
/**
  * @brief  write Sector(s) 
  * @param  drv : driver index
  * @param  buff : Pointer to the data to be written
  * @param  sector : Start sector number
  * @param  count :  Sector count (1..255)
  * @retval DSTATUS : operation status
  */

#if _READONLY == 0
DRESULT disk_write (
                    BYTE drv,			/* Physical drive number (0) */
                    const BYTE *buff,	/* Pointer to the data to be written */
                    DWORD sector,		/* Start sector number (LBA) */
                    BYTE count			/* Sector count (1..255) */
                      )
{

  SD_Error sdstatus = SD_OK;
  uint32_t timeout = 300000;


      SD_WriteMultiBlocks((BYTE *)buff, (uint32_t )(sector * 512), 512, count);
      /* Check if the Transfer is finished */
      sdstatus = SD_WaitWriteOperation();
      while(SD_GetStatus() != SD_TRANSFER_OK)
      {
        if (timeout-- == 0)
        {
          return RES_ERROR;
        }
      }

      if (sdstatus == SD_OK)
      {
        return RES_OK;
      }
  return RES_NOTRDY;
}

#endif /* _READONLY */



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
DRESULT res;
  SD_CardInfo SDCardInfo;

  res = RES_ERROR;

  //if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (ctrl) {
  case CTRL_SYNC :		/* Make sure that no pending write process */
    res = RES_OK;
    break;

  case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (DWORD) */
      SD_GetCardInfo(&SDCardInfo);
      *(DWORD*)buff = SDCardInfo.CardCapacity / 512;

    res = RES_OK;
    break;

  case GET_SECTOR_SIZE :	/* Get R/W sector size (WORD) */
    *(WORD*)buff = 512;
    res = RES_OK;
    break;

  case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
//    if(drv == 0)
//    {
    //  *(DWORD*)buff = 512;
//    }
//    else
//    {
      *(DWORD*)buff = 32;
//    }

          res = RES_OK;
    break;


  default:
    res = RES_PARERR;
  }
  
  return res;

}

