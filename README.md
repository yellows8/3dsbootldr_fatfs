This bootloader running on the arm9 loads ARM9 and ARM11 binaries from the SD FAT filesystem, with the following filenames: "/load9.bin" and "/load11.bin". After successfully loading both binaries, it will call the loaded arm9bin, then it will return from main_() if the arm9bin returns.  

Each binary starts with an u32 for the load-address, the rest of the binary is loaded to this address. See main.c for the blacklisted memory ranges(MPU is disabled while this loader is running and when jumping to the arm9bin).

Prior to jumping to the arm9bin, it will handle booting the arm11(this requires code running on the arm11 which can handle this). The entrypoint is written to u32 0x1ffffff8+4, then magicnum 0x544f4f42("BOOT") is written to u32 0x1ffffff8+0. Lastly, this arm9code waits for the arm11 to change the value at 0x1ffffff8+0.

# Building
"make" can be used with the following options:
* "ENABLE_RETURNFROMCRT0=1" When the binaries were not loaded successfully, or when the arm9bin returned, return from the crt0 to the LR from the time of loader entry, instead executing an infinite loop.
* "UNPROTBOOT9_LIBPATH={path}" This should be used to specify the path for the unprotboot9_sdmmc library, aka the path for that repo.
* "DISABLE_ARM11=1" Completely disable *all* code involved with loading/booting the arm11 binary.
* "ARM9BIN_FILEPATH={sd filepath}" Override the default filepath for the arm9bin. For example: "ARM9BIN_FILEPATH=/ld9.bin". The filenames here are FAT 8.3.
* "ARM11BIN_FILEPATH={sd filepath}" This is the arm11bin version of the above option.

