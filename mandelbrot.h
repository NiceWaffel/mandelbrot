#ifndef _MANDELBROT_H_
#define _MANDELBROT_H_

#include "mandelbrot_common.h"

#define INTERP_NN 1
#define INTERP_LINEAR 2

int mandelbrotInit(int w, int h);
void mandelbrotCleanup();

void generateImage(Rectangle coord_rect, int *out_argb);
void generateImage2(int w, int h, Rectangle coord_rect, int *out_argb);
void doAntiAlias(Rectangle coord_rect, int *argb_buf, int aa_counter);

void changeIterations(int diff);

void scaleImage(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb, int interp_method);

#endif
