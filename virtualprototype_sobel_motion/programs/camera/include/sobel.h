/*
 * sobel.h
 *
 *  Created on: Sep 12, 2015
 *      Author: theo
 */

#ifndef SOBEL_H_
#define SOBEL_H_

void edgeDetection( volatile uint8_t *grayscale,
                    volatile uint8_t *sobelResult,
                    int32_t width,
                    int32_t height,
                    int32_t threshold );

#endif /* SOBEL_H_ */
