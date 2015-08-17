#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <3ds.h>

#include <unprotboot9_sdmmc.h>

#include "ff.h"

extern u32 _start, __end__;

/*void sha256hw_calchash(u32 *outhash, u32 *buf, u32 buf_wordsize)
{
	u32 pos;
	vu32 *SHA_CNT = (vu32*)0x1000a000;
	vu32 *SHA_HASH = (vu32*)0x1000a040;
	vu32 *SHA_INFIFO = (vu32*)0x1000a080;

	*SHA_CNT = 0x9;

	pos = 0;
	do {
		while((*SHA_CNT) & 0x1);
		*SHA_INFIFO = buf[pos];
		pos++;
	} while(pos<buf_wordsize);

	*SHA_CNT = 0xa;
	while((*SHA_CNT) & 0x2);
	while((*SHA_CNT) & 0x1);

	for(pos=0; pos<(0x20>>2); pos++)outhash[pos] = SHA_HASH[pos];
}*/

s32 verify_binarymemrange(u32 loadaddr, u32 binsize)
{
	s32 ret = 0;
	u32 checkaddr=0;
	u32 pos;

	u32 firmsections_memrangeblacklist[6][2] = {//Blacklist all memory which gets used / etc.
	{0x080030fc, 0x080038fc},
	{_start, __end__},
	{0x08100000-0x2000, 0x08100000},//stack
	{0x08000000, 0x08000040},
	{0xfff00000, 0xfff04000},
	{0x3800, 0x7470}//Masked ITCM addrs, resulting in offsets within ITCM.
	};

	if((loadaddr + binsize) < loadaddr)return 0x50;

	ret = 0;
	for(pos=0; pos<6; pos++)
	{
		checkaddr = loadaddr;
		if(pos==5)
		{
			if(checkaddr >= 0x08000000)break;
			checkaddr &= 0x7fff;
		}

		if(checkaddr >= firmsections_memrangeblacklist[pos][0] && checkaddr < firmsections_memrangeblacklist[pos][1])
		{
			ret = 0x51;
			break;
		}

		if((checkaddr+binsize) >= firmsections_memrangeblacklist[pos][0] && (checkaddr+binsize) < firmsections_memrangeblacklist[pos][1])
		{
			ret = 0x52;
			break;
		}

		if(checkaddr < firmsections_memrangeblacklist[pos][0] && (checkaddr+binsize) > firmsections_memrangeblacklist[pos][0])
		{
			ret = 0x53;
			break;
		}
	}

	return ret;
}

s32 load_binary(char *path, s32 *errortable, u32 **loadaddrptr)
{
	FRESULT res;
	FIL fil;
	UINT totalread=0;
	DWORD filesize=0;

	s32 ret;

	res = f_open(&fil, path, FA_READ);
	errortable[0] = res;
	if(res!=FR_OK)return res;

	filesize = f_size(&fil);
	ret = 0;
	if((filesize < 8) || (filesize>>31))ret = 0x40;
	errortable[1] = ret;
	if(ret)
	{
		f_close(&fil);
		return ret;
	}

	totalread=0;
	res = f_read(&fil, loadaddrptr, 4, &totalread);
	errortable[2] = res;
	if(res!=FR_OK)
	{
		f_close(&fil);
		return res;
	}

	errortable[3] = 0;
	if(totalread!=4)
	{
		f_close(&fil);
		ret = 0x41;
		errortable[3] = ret;
		return ret;
	}

	ret = verify_binarymemrange((u32)*loadaddrptr, (u32)(filesize-4));
	errortable[4] = ret;
	if(ret)
	{
		f_close(&fil);
		return ret;
	}

	errortable[5] = 0;
	totalread=0;
	res = f_read(&fil, (u32*)*loadaddrptr, filesize-4, &totalread);
	f_close(&fil);

	if(res!=FR_OK)
	{
		errortable[5] = res;
		return res;
	}

	if(totalread != (filesize-4))
	{
		ret = 0x41;
		errortable[5] = ret;
		return ret;
	}

	return 0;
}

int main_()
{
	s32 ret;
	s32 *errortable = (s32*)0x01ffcde4;
	
	FRESULT res;
	FATFS fs;

	u32 *loadaddr9 = 0, *loadaddr11 = 0;
	s32 (*arm9_entrypoint)();

	ret = unprotboot9_sdmmc_initialize();
	errortable[0] = (u32)ret;
	if(ret)return ret;

	ret = unprotboot9_sdmmc_initdevice(unprotboot9_sdmmc_deviceid_sd);
	errortable[1] = (u32)ret;
	if(ret)return ret;

	res = f_mount(&fs, "", 1);
	errortable[2] = res;
	if(res!=FR_OK)return res;

	ret = load_binary("/load9.bin", &errortable[4], &loadaddr9);
	if(ret)return ret;

	ret = load_binary("/load11.bin", &errortable[4+8], &loadaddr11);
	if(ret)return ret;

	res = f_mount(NULL, "", 1);
	errortable[3] = res;
	if(res!=FR_OK)return res;

	arm9_entrypoint = (void*)loadaddr9;
	ret = arm9_entrypoint();

	return ret;
}

