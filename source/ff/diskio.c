#include "diskio.h"

#include <3ds.h>
#include <unprotboot9_sdmmc.h>

DSTATUS disk_status (BYTE pdrv)
{
	return RES_OK;
}

DSTATUS disk_initialize(BYTE pdrv)
{
	return RES_OK;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	DRESULT res = RES_OK;
	s32 retval;

	retval = unprotboot9_sdmmc_readrawsectors((u32)sector, (u32)count, (u32*)buff);
	if(retval)res = RES_ERROR;

	return res;
}

