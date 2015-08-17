This bootloader running on the arm9 loads ARM9 and ARM11 binaries from the SD FAT filesystem, with the following filenames: "/load9.bin" and "/load11.bin". After successfully loading both binaries, it will call the loaded arm9bin, it will return from main_() if the arm9bin returns. For now nothing is done with the ARM11 payload besides loading it into memory.  

Each binary starts with an u32 for the load-address, the rest of the binary is loaded to this address. See main.c for the blacklisted memory ranges(MPU is disabled while this loader is running and when jumping to the arm9bin).

# Building
"make" can be used with the following options:
* "ENABLE_RETURNFROMCRT0=1" When the binaries were not loaded successfully, or when the arm9bin returned, return from the crt0 to the LR from the time of loader entry, instead executing an infinite loop.
* "UNPROTBOOT9_LIBPATH={path}" This should be used to specify the path for the unprotboot9_sdmmc library, aka the path for that repo.

