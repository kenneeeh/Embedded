#include <stdio.h>
#include <ov7670.h>
#include <swap.h>
#include <vga.h>

#define __WITH_DMA__

int main () {
  const uint32_t writeBit = 1<<9;
  const uint32_t busStartAddress = 1 << 10;
  const uint32_t memoryStartAddress = 2 << 10;
  const uint32_t blockSize = 3 << 10;
  const uint32_t burstSize = 4 << 10;
  const uint32_t statusControl = 5 << 10;
  const uint32_t usedCiRamAddress = 50;
  const uint32_t usedBlocksize = 256;
  const uint32_t usedBurstSize = 31;
  volatile uint16_t rgb565[640*480];
  volatile uint8_t grayscale[640*480];
  volatile uint32_t result, cycles,stall,idle;
  volatile unsigned int *vga = (unsigned int *) 0X50000020;
  volatile unsigned int *gpio = (unsigned int *) 0x40000000;
  camParameters camParams;
  vga_clear();

#ifdef __WITH_DMA__  
  /* set up the generic dma parameters */
  asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(blockSize | writeBit),[in2] "r"(usedBlocksize));
  asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(burstSize | writeBit),[in2] "r"(usedBurstSize));
#endif

  printf("Initialising camera (this takes up to 3 seconds)!\n" );
  camParams = initOv7670(VGA);
  printf("Done!\n" );
  printf("NrOfPixels : %d\n", camParams.nrOfPixelsPerLine );
  result = (camParams.nrOfPixelsPerLine <= 320) ? camParams.nrOfPixelsPerLine | 0x80000000 : camParams.nrOfPixelsPerLine;
  vga[0] = swap_u32(result);
  printf("NrOfLines  : %d\n", camParams.nrOfLinesPerImage );
  result =  (camParams.nrOfLinesPerImage <= 240) ? camParams.nrOfLinesPerImage | 0x80000000 : camParams.nrOfLinesPerImage;
  vga[1] = swap_u32(result);
  printf("PCLK (kHz) : %d\n", camParams.pixelClockInkHz );
  printf("FPS        : %d\n", camParams.framesPerSecond );
  uint32_t grayPixels;
  vga[2] = swap_u32(2);
  vga[3] = swap_u32((uint32_t) &grayscale[0]);
  while(1) {
    takeSingleImageBlocking((uint32_t) &rgb565[0]);
    asm volatile ("l.nios_rrr r0,r0,%[in2],12"::[in2]"r"(7));
    uint32_t * rgb = (uint32_t *) &rgb565[0];
    uint32_t * gray = (uint32_t *) &grayscale[0];
#ifndef __WITH_DMA__
    for (int pixel = 0; pixel < ((camParams.nrOfLinesPerImage*camParams.nrOfPixelsPerLine) >> 1); pixel +=2) {
      uint32_t pixel1 = rgb[pixel];
      uint32_t pixel2 = rgb[pixel+1];
      asm volatile ("l.nios_rrr %[out1],%[in1],%[in2],10":[out1]"=r"(grayPixels):[in1]"r"(pixel1),[in2]"r"(pixel2));
      *gray = grayPixels;
      gray++;
    }
#else
    uint32_t dmaBuffer = 0;
    uint32_t workBuffer = 256;
    uint32_t pixel1, pixel2, status;
    uint32_t rgbpointer = (uint32_t) &rgb[0];
    uint32_t graypointer = (uint32_t) &gray[0];
    /* perform the first initial DMA */
    asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(busStartAddress | writeBit),[in2] "r"(rgbpointer));
    rgbpointer += usedBlocksize*4;
    asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(memoryStartAddress | writeBit),[in2] "r"(dmaBuffer));
    asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(statusControl | writeBit),[in2] "r"(1));
    /* wait for the DMA to finish */
    do {
      asm volatile("l.nios_rrr %[out1],%[in1],r0,20" :[out1]"=r"(status):[in1] "r"(statusControl));
    } while (status != 0);
    for (int loop = 0 ; loop < 600; loop++) {
      /* swap buffers */
      status = dmaBuffer;
      dmaBuffer = workBuffer;
      workBuffer = status;
      /* perform DMA in */
      if (loop < 599) {
        asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(busStartAddress | writeBit),[in2] "r"(rgbpointer));
        rgbpointer += usedBlocksize*4;
        asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(memoryStartAddress | writeBit),[in2] "r"(dmaBuffer));
        asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(statusControl | writeBit),[in2] "r"(1));
      }
      /* perform transformation */
      for (int pixel = 0 ; pixel < usedBlocksize ; pixel += 2) {
        asm volatile("l.nios_rrr %[out1],%[in1],r0,20" :[out1]"=r"(pixel1):[in1] "r"(workBuffer+pixel));
        asm volatile("l.nios_rrr %[out1],%[in1],r0,20" :[out1]"=r"(pixel2):[in1] "r"(workBuffer+pixel+1));
        pixel1 = swap_u32(pixel1);
        pixel2 = swap_u32(pixel2);
        asm volatile ("l.nios_rrr %[out1],%[in1],%[in2],10":[out1]"=r"(grayPixels):[in1]"r"(pixel1),[in2]"r"(pixel2));
        grayPixels = swap_u32(grayPixels);
        asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"((workBuffer+(pixel>>1))| writeBit),[in2]"r"(grayPixels));
      }
      /* wait for the DMA to finish */
      do {
        asm volatile("l.nios_rrr %[out1],%[in1],r0,20" :[out1]"=r"(status):[in1] "r"(statusControl));
      } while (status != 0);
      /* perform DMA out */
      asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(busStartAddress | writeBit),[in2] "r"(graypointer));
      graypointer += (usedBlocksize << 1);
      asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(blockSize | writeBit),[in2] "r"((usedBlocksize >> 1)));
      asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(memoryStartAddress | writeBit),[in2] "r"(workBuffer));
      asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(statusControl | writeBit),[in2] "r"(2));
      /* wait for the DMA to finish */
      do {
        asm volatile("l.nios_rrr %[out1],%[in1],r0,20" :[out1]"=r"(status):[in1] "r"(statusControl));
      } while (status != 0);
      asm volatile("l.nios_rrr r0,%[in1],%[in2],20" ::[in1] "r"(blockSize | writeBit),[in2] "r"(usedBlocksize));
    }
#endif
    asm volatile ("l.nios_rrr %[out1],r0,%[in2],12":[out1]"=r"(cycles):[in2]"r"(1<<8|7<<4));
    asm volatile ("l.nios_rrr %[out1],%[in1],%[in2],12":[out1]"=r"(stall):[in1]"r"(1),[in2]"r"(1<<9));
    asm volatile ("l.nios_rrr %[out1],%[in1],%[in2],12":[out1]"=r"(idle):[in1]"r"(2),[in2]"r"(1<<10));
    printf("nrOfCycles: %d %d %d\n", cycles, stall, idle);
  }
}
