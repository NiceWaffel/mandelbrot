#ifndef _MANDELBROT_CPU_H_
#define _MANDELBROT_CPU_H_

#include "mandelbrot_common.h"

int mandelbrotCpuInit(int w, int h);
void mandelbrotCpuCleanup();

void generateImageCpu(Rectangle coord_rect, int *out_argb);
void generateImageCpuWH(int w, int h, Rectangle coord_rect, int *out_argb);
void doAntiAliasCpu(Rectangle coord_rect, int *argb_buf, int aa_counter);

void changeIterationsCpu(int diff);

#endif
