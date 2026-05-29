#include "or32Print.h"
#include "vgaPrint.h"
#include "uart.h"
#include "flash.h"
#include "date.h"
#include "caches.h"
#include <stddef.h>
#include <stdint-gcc.h>

#define swapBytes( source , dest ) asm volatile ("l.nios_rrr %[out1],%[in1],r0,0x1":[out1]"=r"(dest):[in1]"r"(source));

#define MAGIC 0xDEAD1300

void *memcpy(void *dest, const void *src, size_t len) {
  switch (__builtin_object_size(dest,0)) {
    case 1  : short *ds = dest;
              const short *ss = src;
              while (len--)
                *ds++ = *ss++;
              return dest;
    case 2  : int *di = dest;
              const int *si = src;
              while (len--)
                *di++ = *si++;
              return dest;
    default : char *d = dest;
              const char *s = src;
              while (len--)
                *d++ = *s++;
              return dest;
  }
}

void sdramAdjust() {
  volatile unsigned int *sdram = (unsigned int *) 0;
  unsigned int count, errors;
  for (count = 0 ; count < 8192 ; count++)
    sdram[count] = ((count <<16)^0xFFFF0000)|count;
  errors = 0;
  for (count = 0 ; count < 8192 ; count++)
    if (sdram[count] != (((count <<16)^0xFFFF0000)|count)) {
      errors++;
    }
  if (errors == 0) return;
  asm volatile ("l.mtspr r0,%[in1],0xD801"::[in1]"r"(1));
  errors = 0;
  for (count = 0 ; count < 8192 ; count++)
    if (sdram[count] != (((count <<16)^0xFFFF0000)|count)) {
      errors++;
    }
  if (errors == 0) return;
  asm volatile ("l.mtspr r0,%[in1],0xD801"::[in1]"r"(2));
  errors = 0;
  for (count = 0 ; count < 8192 ; count++)
    if (sdram[count] != (((count <<16)^0xFFFF0000)|count)) {
      errors++;
    }
  if (errors == 0) return;
  asm volatile ("l.mtspr r0,%[in1],0xD801"::[in1]"r"(3));
  errors = 0;
  for (count = 0 ; count < 8192 ; count++)
    if (sdram[count] != (((count <<16)^0xFFFF0000)|count)) {
      errors++;
    }
  if (errors == 0) return;
  or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Sdram not working correctly, please change your board!\n");
  while(1) {};
}

void helpscreen() {
  const char *helpText[] =  {
    "Known RS232 commands:\n",
    "** Start the program loaded in target\n",
    "*p Set programming mode (default)\n",
    "*v Set verification mode\n",
    "*i Show info on program in target\n",
    "*t Toggle target between SDRam (default), soft-Bios and Flash\n",
    "*m Perform simple SDRam memcheck\n",
    "*s Check SPI-flash chip\n",
    "*e Erase SPI-Flash chip\n",
    "*f Store program loaded in SDRAM to SPI-Flash\n",
    "*c Compare program loaded in SDRAM with SPI-Flash\n",
    "*r Run program stored in SPI-Flash\n",
    "*q Toggle stack pointer from SDRAM (default) with SPM\n",
    "*h This helpscreen\n\n",
    0};
  volatile unsigned int *flash = (unsigned int *) FLASH_BASE;
  volatile unsigned int *gpio = (unsigned int *)0x50000080;
  volatile unsigned int *sdram = (unsigned int *) 0;
  unsigned int reg,id,supervisor, size,addr;
  vgaClear();
  do {
    asm volatile ("l.mfspr %[out1],r0,9":[out1]"=r"(reg));
  } while ((reg & 0xFFFFFF00) == 0);
  id = reg&0xF;
  char_output_provider rs232 = (id == 1) ? &sendRs232Char : 0;
  if (id == 1) sdramAdjust();
  if (id == 1 && flash[0] == MAGIC && (gpio[3]&0x00020000) == 0) {
    size = flash[1];
    for (addr = 0; addr < size; addr++) sdram[addr] = flash[addr];
    or32PrintMultiple(&vgaPrintChar, &sendRs232Char, ".\n");
    asm volatile ("l.mfspr %[out1],r0,17":[out1]"=r"(supervisor));
    addr = 0xFFFFFFFF;
    addr ^= (1<<14);
    supervisor &= addr;
    asm volatile ("l.mtspr r0,%[in1],17"::[in1]"r"(supervisor));
    addr ^= (3<<3);
    supervisor &= addr;
    asm volatile ("l.mfspr %[out1],%[in1],0xE000":[out1]"=r"(reg):[in1]"r"(13));
    or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Jumping to programm @0x%X\n", reg);
    asm volatile ("l.jr %[in1]; l.mtspr r0,%[in2],17"::[in1]"r"(reg), [in2]"r"(supervisor));
  }
  or32PrintMultiple(&vgaPrintChar, rs232, "CS-473 System programming for systems on chip\n");
  or32PrintMultiple(&vgaPrintChar, rs232, "Openrisc based virtual Prototype.\n");
  or32PrintMultiple(&vgaPrintChar, rs232, compiledate);
  or32PrintMultiple(&vgaPrintChar, rs232, "I am CPU %d of %d running at ", id , (reg >> 4)&0xF);
  or32PrintMultiple(&vgaPrintChar, rs232, "%d%d.%d%d MHz.\n\n", (reg >> 24)&0xF, (reg >> 20)&0xF, (reg >> 16)&0xF, (reg >> 12)&0xF );
  reg = 0;
  while (helpText[reg] != 0 && (id == 1)) {
    or32PrintMultiple(&vgaPrintChar, &sendRs232Char, (char *) helpText[reg++]);
  }
  if (id != 1) {
    unsigned int jump,stat;
    or32PrintMultiple(&vgaPrintChar, 0, "Waiting for CPU 1 to activate me\n");
    do {
      asm volatile ("l.mfspr %[out1],r0,0x5002":[out1]"=r"(stat));
    } while ((stat>>31) == 0);
    asm volatile ("l.mfspr %[out1],r0,0x5003":[out1]"=r"(jump));
    or32PrintMultiple(&vgaPrintChar, 0, "Jumping to main program @0x%X\n", jump);
    asm volatile ("l.jr %[in1]; l.nop; l.nop"::[in1]"r"(jump));
  }
}

int checkMagic( unsigned int value ) {
  if (value == MAGIC) return 0;
  if ((value&0xFFFF0000) == 0xDEAD0000) 
    or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Program present but not for this Target.\nDid you upload for the OR1300 platform?\n");
  return -1;
}

#define STACK_TOP_SPM 0xC0001FFC
#define STACK_TOP_SDRAM 0x007FFFFC

int bios() {
  volatile unsigned int supervisor;
  volatile unsigned int *sdram = (unsigned int *) 0;
  volatile unsigned int *softRom = (unsigned int *) 0xF0004000;
  volatile unsigned int *flash = (unsigned int *) FLASH_BASE;
  volatile unsigned int *target = (unsigned int *) 0;
  unsigned char kar, progmode;
  unsigned int max, addr, data, errorcount, count, value, address, reg, size, magic, repeat, bytecount;
  unsigned int stackTop = STACK_TOP_SDRAM;
  char codeTable[256][3];
  char str[3] = {0,0,0};
  progmode = 1;
  max = 0;
  repeat = 1;
  bytecount = 0;
  /* enable the I$ */
  setInstructionCacheConfig( CACHE_DIRECT_MAPPED | CACHE_SIZE_8k );
  enableInstructionCache( supervisor );

  init_rs232();
  helpscreen();
  do {
    kar = get_rs232_blocking();
    switch (kar) {
      case '#': or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Download: done\n");
                break;
      case '&': kar = get_rs232_blocking();
                or32PrintMultiple(&vgaPrintChar, 0, "Reading code table\n");
                int index = 0, karCnt;
                do {
                  while (kar == ' ') kar = get_rs232_blocking();
                  karCnt = 0;
                  do {
                    codeTable[index][karCnt++] = kar;
                    kar = get_rs232_blocking();
                  } while (kar != ' ');
                  codeTable[index][karCnt] = 0;
                  index++;
                } while (index < 256);
                break;
      case '@': data = GetHexRS232(' ');
                address = (data &0xFFFFFF) >> 2;
                bytecount = 0;
                or32PrintMultiple(&vgaPrintChar, 0, "Download: set address = 0x%X\n", data);
                if (address == 0) max = 0;
                break;
      case '*': kar = get_rs232_blocking();
                switch (kar) {
                  case '*': if (checkMagic(target[0]) != 0) {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Error, no program loaded!\n");
                            } else if (target == softRom) {
                              asm volatile ("l.mtspr r0,%[in1],0xE00F"::[in1]"r"(1));
                            } else {
                              asm volatile ("l.mfspr %[out1],r0,17":[out1]"=r"(supervisor));
                              addr = 0xFFFFFFFF;
                              addr ^= (1<<14);
                              supervisor &= addr;
                              asm volatile ("l.mtspr r0,%[in1],0x5020"::[in1]"r"(stackTop));
                              asm volatile ("l.mtspr r0,%[in1],17"::[in1]"r"(supervisor));
                              addr ^= (3<<3);
                              supervisor &= addr;
                              asm volatile ("l.mfspr %[out1],%[in1],0xE000":[out1]"=r"(reg):[in1]"r"(13));
                              reg |= (unsigned int) target;
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Jumping to programm\n");
                              asm volatile ("l.jr %[in1]; l.mtspr r0,%[in2],17"::[in1]"r"(reg), [in2]"r"(supervisor));
                            }
                break;
                  case 'h': or32PrintMultiple(&sendRs232Char, 0, "\n\n");
                            helpscreen();
                            break;
                  case 's': checkFlash();
                            break;
                  case 'r': if (checkMagic(flash[0]) != 0) {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Error, no program in Flash!\n");
                            } else {
                              size = flash[1];
                              for (addr = 0; addr < size; addr++) sdram[addr] = flash[addr];
                              asm volatile ("l.mfspr %[out1],r0,17":[out1]"=r"(supervisor));
                              addr = 0xFFFFFFFF;
                              addr ^= (1<<14);
                              supervisor &= addr;
                              asm volatile ("l.mtspr r0,%[in1],17"::[in1]"r"(supervisor));
                              addr ^= (3<<3);
                              supervisor &= addr;
                              asm volatile ("l.mfspr %[out1],%[in1],0xE000":[out1]"=r"(reg):[in1]"r"(13));
                              reg |= (unsigned int) target;
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Jumping to programm\n");
                              asm volatile ("l.jr %[in1]; l.mtspr r0,%[in2],17"::[in1]"r"(reg), [in2]"r"(supervisor));
                            }
                            break;
                  case 'p': or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Setting prog. mode\n");
                            progmode = 1;
                            break;
                  case 'v': or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Setting verif. mode\n");
                            progmode = 0;
                            break;
                  case 'i': if (checkMagic(target[0]) != 0) {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "No program present\n", 0 , target[1]); 
                            } else {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Program in mem from 0x%X to 0x%X\n", (unsigned int) target , 
                                                ((unsigned int) target | (target[1] <<2))-1 );
                            }
                            break;
                  case 't': if (target == sdram) {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Switched to soft-bios\n");
                              target = softRom;
                            } else if (target == softRom) {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Switched to Flash\n");
                              target = flash;
                            } else {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Switched to SDRam\n");
                              target = sdram;
                            }
                            max = 0;
                            break;
                  case 'c': if (target == softRom || target == flash) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Please change to the SDRAM by *t\n");
                            else if (checkMagic(target[0]) != 0) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "No program loaded in SDRam!\n");
                            else if (target[1] >= 4*1024*1024) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Program does not fit in Flash!\n");
                            else {
                              for (reg = 0; reg < sdram[1]; reg++) {
                                if (flash[reg] != sdram[reg]) {
                                  or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Compare error at 0x%X : 0x%X != 0x%X\n", (reg << 2)|FLASH_BASE , flash[reg], sdram[reg] ); 
                                }
                              }
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Compare done\n" );
                            }
                            break;
                  case 'f': if (target == softRom || target == flash) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Please change to the SDRAM by *t\n");
                            else if (checkMagic(target[0]) != 0) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "No program loaded in SDRam!\n");
                            else if (target[1] >= 4*1024*1024) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Program does not fit in Flash!\n");
                            else {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Checking if the flash is empty...\n"); 
                              for (reg = 0; reg < sdram[1]; reg++) {
                                if (flash[reg] != 0xFFFFFFFF) {
                                  or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Start flash erase cycle for page 0x%X\n", reg<<2);
                                  flashErase(reg << 2);
                                }
                              }
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Start programming flash\n" );
                              flashWrite((unsigned int *) sdram, sdram[1]);
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Programming finished\n" );
                            }
                            break;
                  case 'e': or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Checking if flash is 'dirty'\n");
                            for (reg = 0; reg < 4*1024*1024; reg++) {
                              if (flash[reg] != 0xFFFFFFFF) {
                                or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Start flash erase cycle for page 0x%X\n", reg<<2);
                                flashErase(reg << 2);
                              }
                            }
                            or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Flash is empty (erased).\n\n");
                            break;
                  case 'm': or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Starting simple SDRam memcheck.\n\n");
                            max = 0;
                            for (count = 0 ; count < 3 ; count ++) {
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Writing...\n");
                              for (addr = 0; addr < 8*1024*1024; addr++) {
                                switch (count) {
                                  case 0 : value = 0xFFFFFFFF;
                                  case 1 : swapBytes(addr<<2, value);
                                  default: value = addr<<2;
                                }
                                sdram[addr] = value;
                              }
                              or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Verifying...\n");
                              errorcount = 0;
                              for (addr = 0; addr < 8*1024*1024; addr++) {
                                switch (count) {
                                  case 0 : value = 0xFFFFFFFF;
                                  case 1 : swapBytes(addr<<2, value);
                                  default: value = addr<<2;
                                }
                                if (sdram[addr] != value)
                                  if (errorcount++ < 30) or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Error @0x%X : 0x%X != 0x%X\n", addr << 2, sdram[addr], value );
                              }
                              if (errorcount > 0) {
                                or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Nr of errors found : %d\n", errorcount );
                                break;
                              }
                            }
                            or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Memcheck done, %d errors\n\n", errorcount);
                            break;
                  case 'q' : if (stackTop == STACK_TOP_SDRAM) {
                               stackTop = STACK_TOP_SPM;
                               or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Setting stack to SPM\n");
                             } else {
                               stackTop = STACK_TOP_SDRAM;
                               or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Setting stack to SDRAM\n");
                             }
                             break;
                  default : break;
                }
                break;
      case '\r' :
      case '\n' :
      case ' '  : break;
      case '\'' : kar = get_rs232_blocking();
                  repeat = kar - '0';
                  kar = get_rs232_blocking();
                  repeat *= 10;
                  repeat += (kar - '0');
                  break;
      case '-':
      case '+':
      case '=': str[1] = get_rs232_blocking();
      default : str[0] = kar;
                int value = -1;
                for (int i = 0; i < 256; i++) {
                  if (str[0] == codeTable[i][0] && str[1] == codeTable[i][1]) {
                    value = i;
                    i = 256;
                  }
                }
                str[1] = 0;
                if (value < 0) {
                  or32PrintMultiple(&vgaPrintChar, 0 , "Unknown code!");
                } else {
                  while (repeat > 0) {
                    data = (bytecount == 0) ? 0 : data << 8;
                    data += value;
                    bytecount++;
                    if (bytecount == 4) {
                      if (target == softRom && address == 4095) {
                        or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Program too big to fit in Softbios, aborting!\n");
                        address++;
                      }
                      if (target == softRom && address > 4095) {
                        max = 0;
                        break;
                      }
                      if (target == flash && address == 0) {
                        or32PrintMultiple(&vgaPrintChar, &sendRs232Char, "Cannot program flash, aborting!\n");
                        address++;
                        break;
                      }
                      if (target == flash && address > 0) break;
                      if ((address&0x3FFF) == 0) or32PrintMultiple(&vgaPrintChar, 0, "Download: at 0x%X\n", address << 2);
                      if (progmode == 1) {
                        swapBytes(data, target[address]);
                      } else {
                        unsigned int val;
                        swapBytes(target[address], val);
                        if (val != data)
                          or32PrintMultiple(&vgaPrintChar, 0, "Verification error at 0x%X : 0x%X != 0x%X\n", address << 2 , val , data);
                      }
                      address++;
                      if (address > max) {
                        max = address;
                        if (progmode == 1) target[1] = max;
                      }
                      bytecount = 0;
                    }
                    repeat --;
                  }
                  repeat = 1;
                }
                break;
    }
  } while (1);
}
