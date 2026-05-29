#include <stdio.h>
#include <vga.h>
#include <spr.h>

int main () {
  int reg;
  vga_clear();
  printf("Hello World!\n" );
  asm volatile ("l.ori %[out1],r1,0":[out1]"=r"(reg));
  printf("My stacktop = 0x%08X\n", reg);
}
