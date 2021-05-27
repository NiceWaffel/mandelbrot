#include "mandelbrot_cpu.h"
#include "config.h"
#include "logger.h"

#include "util.h"
#include "SDL.h"

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <math.h>
}

typedef struct {
	int pix_w, pix_h;
	float x, y, w, h;
	float escape_rad;
	int *out;
	int thread_idx;
	int max_iters;
} MandelbrotArgs;

MandelBuffer mandelbuffer_cpu;
int max_iterations_cpu = DEFAULT_ITERATIONS;

static int nthreads = 0;
static SDL_Thread **threads;
static MandelbrotArgs *args_list;

int getIterationsCpu(float x0, float y0, float escape_rad, int max_iters) {
	int iteration = 0;
	float x = 0.0;
	float y = 0.0;

	while(x*x + y*y <= escape_rad * escape_rad && iteration < max_iters) {
		float tmpx = x * x - y * y + x0;
		y = 2 * x * y + y0;
		x = tmpx;
		iteration++;
	}
	return iteration;
}

int iterationsToColorCpu(int iterations, int max_iters) {
	if(iterations >= max_iters)
		return 0x000000; // Black

	float hue = (int)(log2((float)iterations) * 12.0);
	float C = 1.0;
	float X = hue / 60.0;
	X = X - (int)X;
	float r, g, b;
	if(hue >= 0.0 && hue < 60.0) {
		r = C; g = X; b = 0;
	} else if(hue >= 60.0 && hue < 120.0) {
		r = 1 - X; g = C; b = 0;
	} else if(hue >= 120.0 && hue < 180.0) {
		r = 0; g = C; b = X;
	} else if(hue >= 180.0 && hue < 240.0) {
		r = 0; g = 1 - X; b = C;
	} else if(hue >= 240.0 && hue < 300.0) {
		r = X; g = 0; b = C;
	} else {
		r = C; g = 0; b = 1 - X;
	}
	r *= 255;
	g *= 255;
	b *= 255;
	return (int)r + (int)g * 256 + (int)b * 65536;
}

int mandelbrot(void *voidargs) {
	MandelbrotArgs *args = (MandelbrotArgs *)voidargs;
	for (int i = args->thread_idx; i < args->pix_w * args->pix_h; i += nthreads) {
		float cx = (float)(i % args->pix_w);
		float cy = (float)(i / args->pix_w);
		cx = cx / (float)(args->pix_w) * args->w + args->x;
		cy = cy / (float)(args->pix_h) * args->h + args->y;

		int iters = getIterationsCpu(cx, cy, args->escape_rad, args->max_iters);
		int color = iterationsToColorCpu(iters, args->max_iters);
		args->out[i] = 0xff000000 | color; // Write color with full alpha into output
	}
	return 0;
}

void changeIterationsCpu(int diff) {
	int new_iters = clamp(max_iterations_cpu + diff, 1, 5000);
	log(INFO, "Changing Maximum Iterations to %d\n", new_iters);
	max_iterations_cpu = new_iters;
}

int mandelbrotCpuInit(int w, int h) {
	log(VERBOSE, "Starting CPU Mandelbrot Engine...\n");
	int *img_data = (int *)malloc(w * h * sizeof(int));
	if(img_data == NULL) {
		log(ERROR, "Could not allocate rgb buffer!\n");
		goto error;
	}
	mandelbuffer_cpu = {w, h, img_data};

	nthreads = SDL_GetCPUCount();

	if(nthreads < 1 || nthreads > 256) {
		log(WARN, "Could not determine CPU core count. "
		          "Using a default of 8 threads.\n");
		nthreads = 8;
	}

	threads = (SDL_Thread **)malloc(nthreads * sizeof(SDL_Thread *));
	args_list = (MandelbrotArgs *)malloc(nthreads * sizeof(MandelbrotArgs));

	if(threads == NULL || args_list == NULL) {
		log(ERROR, "Could not allocate thread data!\n");
		goto error;
	}

	return 0;
error:
	if(img_data != NULL)
		free(img_data);
	if(threads != NULL)
		free(threads);
	if(args_list != NULL)
		free(args_list);
	return -1;
}

void mandelbrotCpuCleanup() {
	log(VERBOSE, "Cleaning up CPU Mandelbrot Engine...\n");
	free(mandelbuffer_cpu.rgb_data);
	free(threads);
	free(args_list);
}

void generateImageCpu(Rectangle coord_rect, int *out_argb) {
	int i;
	for(i = 0; i < nthreads; i++) {
		args_list[i].pix_w = mandelbuffer_cpu.w;
		args_list[i].pix_h = mandelbuffer_cpu.h;
		args_list[i].x = coord_rect.x;
		args_list[i].y = coord_rect.y;
		args_list[i].w = coord_rect.w;
		args_list[i].h = coord_rect.h;
		args_list[i].escape_rad = ESCAPE_RADIUS;
		args_list[i].max_iters = max_iterations_cpu;
		args_list[i].out = out_argb;
		args_list[i].thread_idx = i;
		threads[i] = SDL_CreateThread(mandelbrot, "WorkerThread", args_list + i);
		if(threads[i] == NULL) {
			log(ERROR, "Could not create SDL_Thread: %s\n", SDL_GetError());
			return;
		}
	}
	for(i = 0; i < nthreads; i++) {
		log(DEBUG, "Waiting for Thread %d\n", i);
		SDL_WaitThread(threads[i], NULL);
	}
}

void generateImageCpu2(int w, int h, Rectangle coord_rect, int *out_argb) {
	if(w < 1 || h < 1 || out_argb == NULL)
		return;
	int i;
	for(i = 0; i < nthreads; i++) {
		args_list[i].pix_w = w;
		args_list[i].pix_h = h;
		args_list[i].x = coord_rect.x;
		args_list[i].y = coord_rect.y;
		args_list[i].w = coord_rect.w;
		args_list[i].h = coord_rect.h;
		args_list[i].escape_rad = ESCAPE_RADIUS;
		args_list[i].max_iters = max_iterations_cpu;
		args_list[i].out = out_argb;
		args_list[i].thread_idx = i;
		threads[i] = SDL_CreateThread(mandelbrot, "WorkerThread", args_list + i);
		if(threads[i] == NULL) {
			log(ERROR, "Could not create SDL_Thread: %s\n", SDL_GetError());
			return;
		}
	}
	for(i = 0; i < nthreads; i++) {
		log(DEBUG, "Waiting for Thread %d\n", i);
		SDL_WaitThread(threads[i], NULL);
	}
}

// aa_counter defines the shift and blend percentage
void doAntiAliasCpu(Rectangle coord_rect, int *argb_buf, int aa_counter) {
	if(argb_buf == NULL)
		return;

	float shift_amount_x = coord_rect.w / (float)mandelbuffer_cpu.w / 3.0;
	float shift_amount_y = coord_rect.h / (float)mandelbuffer_cpu.h / 3.0;

	float shift_x = coord_rect.x +
			((aa_counter & 2) ? 1.0 : -1.0) * shift_amount_x;
	float shift_y = coord_rect.y +
			((aa_counter & 1) ? 1.0 : -1.0) * shift_amount_y;

	int i;
	for(i = 0; i < nthreads; i++) {
		args_list[i].pix_w = mandelbuffer_cpu.w;
		args_list[i].pix_h = mandelbuffer_cpu.h;
		args_list[i].x = shift_x;
		args_list[i].y = shift_y;
		args_list[i].w = coord_rect.w;
		args_list[i].h = coord_rect.h;
		args_list[i].escape_rad = ESCAPE_RADIUS;
		args_list[i].max_iters = max_iterations_cpu;
		args_list[i].out = mandelbuffer_cpu.rgb_data;
		args_list[i].thread_idx = i;
		threads[i] = SDL_CreateThread(mandelbrot, "WorkerThread", args_list + i);
		if(threads[i] == NULL) {
			log(ERROR, "Could not create SDL_Thread: %s\n", SDL_GetError());
			return;
		}
	}
	for(i = 0; i < nthreads; i++) {
		log(DEBUG, "Waiting for Thread %d\n", i);
		SDL_WaitThread(threads[i], NULL);
	}

	aa_counter += 2;

	// blend them together
	for(i = 0; i < mandelbuffer_cpu.w * mandelbuffer_cpu.h; i++) {
		int blend_color = blend(argb_buf[i],
				mandelbuffer_cpu.rgb_data[i], 1.0 / aa_counter);
		argb_buf[i] = 0xff000000 | blend_color; // apply full alpha
	}
}
