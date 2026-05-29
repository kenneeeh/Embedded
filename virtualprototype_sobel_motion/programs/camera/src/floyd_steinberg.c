#include <floyd_steinberg.h>

const uint8_t Floyd_Array[6] = {1,5,3,
                                7,0,0};

uint8_t threshold( int      x,
                   int      y,
                   int      width,
                   volatile uint8_t  *source,
                   volatile int16_t *error_array ) {
   int dx,dy,ex,ey;
   int16_t error;
   
   error = source[(y*width)+x] << 4;
   for (dy = 0 ; dy < 2 ; dy++) {
      for (dx = -1 ; dx < 2 ; dx ++) {
         ex=x+dx+1;
         ey=(y+dy)%2;
         error += error_array[ex+(ey*(width+2))]*
                  Floyd_Array[dy*3+(dx+1)];
      }
   }
   ex = x+1;
   ey = (y+1)%2;

   ey *= (width+2);
   if (error > (128<<4)) {
      error_array[ex+ey] = (error-(255<<4))>>4;
      return 255;
   } else {
      error_array[ex+ey] = error >> 4;
      return 0;
   }
}

void floyd_steinberg( volatile uint8_t  *source,
                      int      width,
                      int      height,
                      volatile uint8_t  *destination,
                      volatile int16_t *error_array ) {
   int x,y;
   
   for (y = 0 ; y < height ; y++) {
      for (x = 0 ; x < width ; x++) {
         destination[y*width+x] =
               threshold(x,y,width,source,error_array);
      }
   }
}
