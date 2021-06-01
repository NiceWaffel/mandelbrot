#ifndef _MANDELBROT_COMMON_H_
#define _MANDELBROT_COMMON_H_

#define INTERP_NN 1
#define INTERP_LINEAR 2

typedef struct {
	int w;
	int h;
	int *rgb_data;
} MandelBuffer;

typedef struct Rectangle {
	float x;
	float y;
	float w;
	float h;
} Rectangle;

void scale_NN(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb);
void scale_LIN(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb);
void scaleImage(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb, int interp_method);

#endif
