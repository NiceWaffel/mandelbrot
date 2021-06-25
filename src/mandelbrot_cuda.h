#ifndef _MANDELBROT_CUDA_H_
#define _MANDELBROT_CUDA_H_

#include "mandelbrot_common.h"

int mandelbrotCudaInit(int w, int h);
void mandelbrotCudaCleanup();

int resizeFramebufferCuda(int new_w, int new_h);

void generateImageCuda(Rectangle coord_rect, int *out_argb);
void generateImageCudaWH(int w, int h, Rectangle coord_rect, int *out_argb);
void doAntiAliasCuda(Rectangle coord_rect, int *argb_buf, int aa_counter);

void changeIterationsCuda(int diff);
void changeExponentCuda(int diff);

#endif
