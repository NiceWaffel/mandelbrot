#ifndef _MANDELBROT_COMMON_H_
#define _MANDELBROT_COMMON_H_

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

#endif
