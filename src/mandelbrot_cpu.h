#ifndef _MANDELBROT_CPU_H_
#define _MANDELBROT_CPU_H_

#include "mandelbrot_common.h"
#include "mandelbrot_cpu_intrin.h"

int mandelbrotCpuInit(int w, int h, int no_simd);
void mandelbrotCpuCleanup();

int resizeFramebufferCpu(int new_w, int new_h);

void generateImageCpu(Rectangle coord_rect, int *out_argb);
void generateImageCpuWH(int w, int h, Rectangle coord_rect, int *out_argb);
void doAntiAliasCpu(Rectangle coord_rect, int *argb_buf, int aa_counter);

void changeIterationsCpu(int diff);
void changeExponentCpu(int diff);

#endif
