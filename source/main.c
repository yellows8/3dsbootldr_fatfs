#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <3ds.h>

#include <unprotboot9_sdmmc.h>

#include "ff.h"

#ifndef ARM9BIN_FILEPATH
#define ARM9BIN_FILEPATH "/load9.bin"
#endif

#ifndef ARM11BIN_FILEPATH
#define ARM11BIN_FILEPATH "/load11.bin"
#endif

extern u32 _start, __end__;

#ifndef DISABLE_BINVERIFY
void sha256hw_calchash_codebin(u32 *outhash, u32 *buf, u32 buf_wordsize, u32 loadaddr, u32 footertype)
{
	u32 pos;
	vu32 *SHA_CNT = (vu32*)0x1000a000;
	vu32 *SHA_HASH = (vu32*)0x1000a040;
	vu32 *SHA_INFIFO = (vu32*)0x1000a080;

	*SHA_CNT = 0x9;

	while((*SHA_CNT) & 0x1);
	*SHA_INFIFO = loadaddr;

	pos = 0;
	do {
		while((*SHA_CNT) & 0x1);
		*SHA_INFIFO = buf[pos];
		pos++;
	} while(pos<buf_wordsize);

	while((*SHA_CNT) & 0x1);
	*SHA_INFIFO = footertype;

	*SHA_CNT = 0xa;
	while((*SHA_CNT) & 0x2);
	while((*SHA_CNT) & 0x1);

	for(pos=0; pos<(0x20>>2); pos++)outhash[pos] = SHA_HASH[pos];
}
#endif

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

	u32 minfilesize = 8;
	u32 extra_binsize = 4;
	u32 binsize = 0;
	u32 *bufptr;

	u32 pos;
	s32 ret;

	#ifndef DISABLE_BINVERIFY
	u32 calchash[0x20>>2];
	u32 footerdata[0x24>>2];

	minfilesize+= 0x24;
	extra_binsize+= 0x24;
	#endif

	res = f_open(&fil, path, FA_READ);
	errortable[0] = res;
	if(res!=FR_OK)return res;

	filesize = f_size(&fil);
	ret = 0;
	if((filesize < minfilesize) || (filesize>>31) || (filesize & 0x3))ret = 0x40;
	errortable[1] = ret;
	if(ret)
	{
		f_close(&fil);
		return ret;
	}

	binsize = ((u32)filesize) - extra_binsize;

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

	ret = verify_binarymemrange((u32)*loadaddrptr, binsize);
	errortable[4] = ret;
	if(ret)
	{
		f_close(&fil);
		return ret;
	}

	errortable[5] = 0;
	totalread=0;
	res = f_read(&fil, (u32*)*loadaddrptr, binsize, &totalread);

	ret = 0;

	if(res!=FR_OK)
	{
		errortable[5] = res;
		ret = res;
	}
	else if(totalread != binsize)
	{
		ret = 0x41;
		errortable[5] = ret;
	}

	#ifndef DISABLE_BINVERIFY
	if(ret==0)
	{
		errortable[6] = 0;
		totalread=0;
		res = f_read(&fil, footerdata, sizeof(footerdata), &totalread);

		if(res!=FR_OK)
		{
			errortable[6] = res;
			ret = res;
		}
		else if(totalread != sizeof(footerdata))
		{
			ret = 0x41;
			errortable[6] = ret;
		}

		if(ret==0)
		{
			if(footerdata[0] != 0x1f40924e)ret = 0x42;//Validate the footertype.
			errortable[7] = ret;

			if(ret==0)
			{
				sha256hw_calchash_codebin(calchash, *loadaddrptr, binsize>>2, (u32)*loadaddrptr, footerdata[0]);

				for(pos=0; pos<(0x20>>2); pos++)
				{
					if(calchash[pos] != footerdata[1+pos])ret = 0x43;
				}

				errortable[7] = ret;
			}
		}
	}
	#endif

	f_close(&fil);

	if(ret)//Clear the memory for the binary load-addr range when reading the actual binary fails, or when verifying the hash fails.
	{
		bufptr = *loadaddrptr;
		for(pos=0; pos<(binsize>>2); pos++)bufptr[pos] = 0;
	}

	return ret;
}

int main_()
{
	s32 ret;
	s32 *errortable = (s32*)0x01ffcde4;
	
	FRESULT res;
	FATFS fs;

	u32 *loadaddr9 = 0;
	s32 (*arm9_entrypoint)();

	#ifndef DISABLE_ARM11
	u32 *loadaddr11 = 0;
	vu32 *arm11boot_ptr = (u32*)0x1ffffff8;
	#endif

	ret = unprotboot9_sdmmc_initialize();
	errortable[0] = (u32)ret;
	if(ret)return ret;

	ret = unprotboot9_sdmmc_initdevice(unprotboot9_sdmmc_deviceid_sd);
	errortable[1] = (u32)ret;
	if(ret)return ret;

	res = f_mount(&fs, "", 1);//Mount the FS.
	errortable[2] = res;
	if(res!=FR_OK)return res;

	ret = load_binary(ARM9BIN_FILEPATH, &errortable[4], &loadaddr9);//Load the arm9 and arm11 binaries.
	if(ret)return ret;

	#ifndef DISABLE_ARM11
	ret = load_binary(ARM11BIN_FILEPATH, &errortable[4+8], &loadaddr11);
	if(ret)return ret;
	#endif

	res = f_mount(NULL, "", 1);//Unmount
	errortable[3] = res;
	if(res!=FR_OK)return res;

	#ifndef DISABLE_ARM11
	#ifndef ALTARM11BOOT
	//Have the ARM11 jump to loadaddr11.
	arm11boot_ptr[1] = (u32)loadaddr11;
	arm11boot_ptr[0] = 0x544f4f42;//"BOOT"
	while(arm11boot_ptr[0] == 0x544f4f42);//Wait for the arm11 to write to this field, which is done before/after calling the payload.
	#else
	while(arm11boot_ptr[0] != 0);
	arm11boot_ptr[0] = (u32)loadaddr11;
	#endif
	#endif

	arm9_entrypoint = (void*)loadaddr9;
	ret = arm9_entrypoint();

	return ret;
}

