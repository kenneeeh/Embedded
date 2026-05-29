#include <stdio.h>
#include <defs.h>
#include <stdint.h>
#include "spr.h"
#include <cpu3.h>

__weak void main3() {
   puts("Hello world from cpu3\n");
}

__weak void bus_error_handler3() {
    puts("bus error!");
}

__weak void data_page_fault_handler3() {
    puts("Data page fault");
}

__weak void instruction_page_fault_handler3() {
    puts("i page fault");
}

__weak void tick_timer_handler3() {
    puts("tick");
}

__weak void allignment_exception_handler3() {
    asm volatile ("l.trap 15");
    puts("allig!");
}

__weak void illegal_instruction_handler3() {
    puts("???? ");
}

__weak void external_interrupt_handler3() {
    puts("ping");
}

__weak void dtlb_miss_handler3() {
    puts("dtlb");
}

__weak void itlb_miss_handler3() {
    puts("itlb");
}

__weak void range_exception_handler3() {
    puts("Range!");
}

__weak void system_call_handler3() {
    puts("Syscall");
}

__weak void trap_handler3() {
    puts("Trap!");
    printf("EPCR = 0x%08X\n", SPR_READ(32));
    printf("EEAR = 0x%08X\n", SPR_READ(48));
    printf("ESR  = 0x%08X\n", SPR_READ(64));
    printf("Instruction = 0x%08X\n", SPR_READ(80));
    int i;
    for (i = 0 ; i < 256; i++) {
      printf ("0x%08X\n", SPR_READ2(0x3100,i));
    }
    while (1) {};
}

__weak void break_point_handler3() {
    puts("Break");
}

void * vectors3[] = {
  &bus_error_handler3,
  &data_page_fault_handler3,
  &instruction_page_fault_handler3,
  &tick_timer_handler3,
  &allignment_exception_handler3,
  &illegal_instruction_handler3,
  &external_interrupt_handler3,
  &dtlb_miss_handler3,
  &itlb_miss_handler3,
  &range_exception_handler3,
  &system_call_handler3,
  &break_point_handler3,
  &trap_handler3 };

void exception_handler3() {
  asm volatile ("    l.addi      r1,r1,-124;\
    l.sw        0x00(r1),r2;\
    l.sw        0x04(r1),r3;\
    l.sw        0x08(r1),r4;\
    l.sw        0x0C(r1),r5;\
    l.sw        0x10(r1),r6;\
    l.sw        0x14(r1),r7;\
    l.sw        0x18(r1),r8;\
    l.sw        0x1C(r1),r9;\
    l.sw        0x20(r1),r10;\
    l.sw        0x24(r1),r11;\
    l.sw        0x28(r1),r12;\
    l.sw        0x2C(r1),r13;\
    l.sw        0x30(r1),r14;\
    l.sw        0x34(r1),r15;\
    l.sw        0x38(r1),r16;\
    l.sw        0x3C(r1),r17;\
    l.sw        0x40(r1),r18;\
    l.sw        0x44(r1),r19;\
    l.sw        0x48(r1),r20;\
    l.sw        0x4C(r1),r21;\
    l.sw        0x50(r1),r22;\
    l.sw        0x54(r1),r23;\
    l.sw        0x58(r1),r24;\
    l.sw        0x5C(r1),r25;\
    l.sw        0x60(r1),r26;\
    l.sw        0x64(r1),r27;\
    l.sw        0x68(r1),r28;\
    l.sw        0x6C(r1),r29;\
    l.sw        0x70(r1),r30;\
    l.sw        0x74(r1),r31;\
    l.mfspr     r31,r0,0x12;\
    l.slli      r31,r31,2;\
    l.movhi     r30,hi(vectors3);\
    l.ori       r30,r30,lo(vectors3);\
    l.add       r30,r30,r31;\
    l.lwz       r31,0x0(r30);\
    l.jalr      r31;\
    l.nop;\
    l.lwz       r2,0x00(r1);\
    l.lwz       r3,0x04(r1);\
    l.lwz       r4,0x08(r1);\
    l.lwz       r5,0x0C(r1);\
    l.lwz       r6,0x10(r1);\
    l.lwz       r7,0x14(r1);\
    l.lwz       r8,0x18(r1);\
    l.lwz       r9,0x1C(r1);\
    l.lwz       r10,0x20(r1);\
    l.lwz       r11,0x24(r1);\
    l.lwz       r12,0x28(r1);\
    l.lwz       r13,0x2C(r1);\
    l.lwz       r14,0x30(r1);\
    l.lwz       r15,0x34(r1);\
    l.lwz       r16,0x38(r1);\
    l.lwz       r17,0x3C(r1);\
    l.lwz       r18,0x40(r1);\
    l.lwz       r19,0x44(r1);\
    l.lwz       r20,0x48(r1);\
    l.lwz       r21,0x4C(r1);\
    l.lwz       r22,0x50(r1);\
    l.lwz       r23,0x54(r1);\
    l.lwz       r24,0x58(r1);\
    l.lwz       r25,0x5C(r1);\
    l.lwz       r26,0x60(r1);\
    l.lwz       r27,0x64(r1);\
    l.lwz       r28,0x68(r1);\
    l.lwz       r29,0x6C(r1);\
    l.lwz       r30,0x70(r1);\
    l.lwz       r31,0x74(r1);\
    l.addi      r1,r1,124;\
    l.rfe;\
    l.nop;");
}

void init_cpu3() {
  asm volatile("l.mfspr     r1,r0,0x5005;l.nop;l.nop"); // set stack top
  uint32_t super = SPR_READ(17);
  super &= ((1<<14)^0xFFFFFFFF);
  SPR_WRITE(17,super);
  for (int i = 0; i < 13; i++)
    asm volatile ("l.mtspr %[in1],%[in2],0xE000"::[in1]"r"(i),[in2]"r"(&exception_handler3));
  main3();
  printf("CPU3 Execution ended!\n");
  while(1) {};
}

void set_stack_cpu3(unsigned int off) {
  unsigned int spCpu1, spCpu3;
  asm volatile ("l.mfspr %[out1],r0,0x5005;l.nop;l.nop":[out1]"=r"(spCpu1));
  if (off > spCpu1) return;
  spCpu3 = ((spCpu1 >> 24) == 0) ? spCpu1 - off : 0xC0001FFC;
  asm volatile("l.mtspr r0,%[in1],0x5021"::[in1]"r"(spCpu3));
}
