#include <stdio.h>
#include <ov7670.h>
#include <swap.h>
#include <vga.h>
#include <floyd_steinberg.h>
#include <sobel.h>
#ifdef __OR1300__
#include <cache.h>
#endif

volatile uint16_t rgb565[640*480];
volatile uint8_t grayscale[640*480];
volatile uint8_t floyd[640*480];
volatile int16_t error_array[642<<1];

int main () {
  volatile int result;
  volatile unsigned int *vga = (unsigned int *) 0X50000020;
  camParameters camParams;
#ifdef __OR1300__
  icache_write_cfg(CACHE_SIZE_8K|CACHE_FOUR_WAY|CACHE_REPLACE_LRU);
  dcache_write_cfg(CACHE_SIZE_8K|CACHE_FOUR_WAY|CACHE_WRITE_BACK|CACHE_REPLACE_PLRU);
  icache_enable(1);
  dcache_enable(1);
#endif
  vga_clear();
  
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
  while(1) {
    vga[2] = swap_u32(1);
    vga[3] = swap_u32((uint32_t) &rgb565[0]);
    asm volatile ("l.nios_rrr r0,%[in1],%[in2],0x6"::[in1]"r"(5000000),[in2]"r"(1)); // set 5 seconds
    do {
      takeSingleImageBlocking((uint32_t) &rgb565[0]);
      asm volatile ("l.nios_rrr %[out1],r0,%[in2],0x6":[out1]"=r"(result):[in2]"r"(3));
    } while (result != 0);
    vga[2] = swap_u32(2);
    vga[3] = swap_u32((uint32_t) &grayscale[0]);
    asm volatile ("l.nios_rrr r0,%[in1],%[in2],0x6"::[in1]"r"(5000000),[in2]"r"(1)); // set 5 seconds
    uint32_t * rgb = (uint32_t *) &rgb565[0];
    uint32_t * gray = (uint32_t *) &grayscale[0];
    uint32_t grayPixels;
    do {
      takeSingleImageBlocking((uint32_t) &rgb565[0]);
      for (int line = 0; line < camParams.nrOfLinesPerImage; line++) {
        for (int pixel = 0; pixel < camParams.nrOfPixelsPerLine; pixel++) {
          uint16_t rgb = swap_u16(rgb565[line*camParams.nrOfPixelsPerLine+pixel]);
          uint32_t red1 = ((rgb >> 11) & 0x1F) << 3;
          uint32_t green1 = ((rgb >> 5) & 0x3F) << 2;
          uint32_t blue1 = (rgb & 0x1F) << 3;
          uint32_t gray = ((red1*54+green1*183+blue1*19) >> 8)&0xFF;
          grayscale[line*camParams.nrOfPixelsPerLine+pixel] = gray;
        }
      }
      asm volatile ("l.nios_rrr %[out1],r0,%[in2],0x6":[out1]"=r"(result):[in2]"r"(3));
    } while (result != 0);
    vga[2] = swap_u32(2);
    vga[3] = swap_u32((uint32_t) &floyd[0]);
    asm volatile ("l.nios_rrr r0,%[in1],%[in2],0x6"::[in1]"r"(5000000),[in2]"r"(1)); // set 5 seconds
    do {
      takeSingleImageBlocking((uint32_t) &rgb565[0]);
      for (int line = 0; line < camParams.nrOfLinesPerImage; line++) {
        for (int pixel = 0; pixel < camParams.nrOfPixelsPerLine; pixel++) {
          uint16_t rgb = swap_u16(rgb565[line*camParams.nrOfPixelsPerLine+pixel]);
          uint32_t red1 = ((rgb >> 11) & 0x1F) << 3;
          uint32_t green1 = ((rgb >> 5) & 0x3F) << 2;
          uint32_t blue1 = (rgb & 0x1F) << 3;
          uint32_t gray = ((red1*54+green1*183+blue1*19) >> 8)&0xFF;
          grayscale[line*camParams.nrOfPixelsPerLine+pixel] = gray;
        }
      }
      floyd_steinberg(grayscale, camParams.nrOfPixelsPerLine, camParams.nrOfLinesPerImage, floyd, error_array);
      asm volatile ("l.nios_rrr %[out1],r0,%[in2],0x6":[out1]"=r"(result):[in2]"r"(3));
    } while (result != 0);
    asm volatile ("l.nios_rrr r0,%[in1],%[in2],0x6"::[in1]"r"(5000000),[in2]"r"(1)); // set 5 seconds
    do {
      takeSingleImageBlocking((uint32_t) &rgb565[0]);
      for (int line = 0; line < camParams.nrOfLinesPerImage; line++) {
        for (int pixel = 0; pixel < camParams.nrOfPixelsPerLine; pixel++) {
          uint16_t rgb = swap_u16(rgb565[line*camParams.nrOfPixelsPerLine+pixel]);
          uint32_t red1 = ((rgb >> 11) & 0x1F) << 3;
          uint32_t green1 = ((rgb >> 5) & 0x3F) << 2;
          uint32_t blue1 = (rgb & 0x1F) << 3;
          uint32_t gray = ((red1*54+green1*183+blue1*19) >> 8)&0xFF;
          grayscale[line*camParams.nrOfPixelsPerLine+pixel] = gray;
        }
      }
      edgeDetection(grayscale,floyd, camParams.nrOfPixelsPerLine, camParams.nrOfLinesPerImage,128);
      asm volatile ("l.nios_rrr %[out1],r0,%[in2],0x6":[out1]"=r"(result):[in2]"r"(3));
    } while (result != 0);
  }
}
