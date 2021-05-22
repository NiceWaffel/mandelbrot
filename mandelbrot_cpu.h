#ifndef _MANDELBROT_CPU_H_
#define _MANDELBROT_CPU_H_

#include "mandelbrot_common.h"
#include <pthread.h>

int mandelbrotCpuInit(int w, int h);
void mandelbrotCpuCleanup();

void generateImageCpu(Rectangle coord_rect, int *out_argb);
void generateImageCpu2(int w, int h, Rectangle coord_rect, int *out_argb);
void doAntiAliasCpu(Rectangle coord_rect, int *argb_buf, int aa_counter);

#endif
