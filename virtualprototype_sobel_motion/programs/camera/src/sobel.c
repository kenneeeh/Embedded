/*
 * sobel.c
 *
 *  Created on: Sep 12, 2015
 *      Author: theo
 */

#include <stdio.h>
#include <stdint.h>

void edgeDetection( volatile uint8_t *grayscale,
                    volatile uint8_t *sobelResult,
                    int32_t width,
                    int32_t height,
                    int32_t threshold ) {
  const int32_t gx_array[3][3] = {{-1,0,1},
                                  {-2,0,2},
                                  {-1,0,1}};
  const int32_t gy_array[3][3] = { {1, 2, 1},
                                   {0, 0, 0},
                                   {-1,-2,-1}};
  int32_t valueX,valueY, result;
  for (int line = 1; line < height - 1; line++) {
    for (int pixel = 1; pixel < width - 1; pixel++) {
      valueX = valueY = 0;
      for (int dx = -1; dx < 2; dx++) {
        for (int dy = -1; dy < 2; dy++) {
          uint32_t index = ((line+dy)*width)+dx+pixel;
          int32_t gray = grayscale[index];
          valueX += gray*gx_array[dy+1][dx+1];
          valueY += gray*gy_array[dy+1][dx+1];
        }
      }
      result = (valueX < 0) ? -valueX : valueX;
      result += (valueY < 0) ? -valueY : valueY;
      sobelResult[line*width+pixel] = (result > threshold) ? 0xFF : 0;
    }
  }
}


