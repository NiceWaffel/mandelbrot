#include "mandelbrot_cpu.h"
#include "config.h"
#include "logger.h"

#include "util.h"
#include <SDL.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

MandelBuffer mandelbuffer_cpu;
int max_iterations_cpu = DEFAULT_ITERATIONS;
int exponent_cpu = DEFAULT_EXPONENT;

static int nthreads = 0;
static SDL_Thread **threads;
static SDL_ThreadFunction mandelbrot_function;

void iterate(float x0, float y0, int pow, float *x, float *y) {
	float retx = *x;
	float rety = *y;
	for(int i = 0; i < pow - 1; i++) {
		float tmpx = retx * x[0] - rety * y[0];
		rety = x[0] * rety + retx * y[0];
		retx = tmpx;
	}
	x[0] = retx + x0;
	y[0] = rety + y0;
}

int getIterationsCpu(float x0, float y0, float escape_rad, int max_iters, int pow) {
	int iteration = 0;
	float x = 0.0;
	float y = 0.0;

	while(x*x + y*y <= escape_rad * escape_rad && iteration < max_iters) {
		iterate(x0, y0, pow, &x, &y);
		iteration++;
	}
	return iteration;
}

int iterationsToColorCpu(int iterations, int max_iters) {
	if(iterations >= max_iters)
		return 0x000000; // Black

	float hue = (int)(sqrt((float)iterations) * 10.0);
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
		cx = cx / (float)(args->pix_w) * args->rect.w + args->rect.x;
		cy = cy / (float)(args->pix_h) * args->rect.h + args->rect.y;

		int iters = getIterationsCpu(cx, cy, args->escape_rad, args->max_iters, args->pow);
		int color = iterationsToColorCpu(iters, args->max_iters);
		args->out[i] = 0xff000000 | color; // Write color with full alpha into output
	}
	return 0;
}

void changeIterationsCpu(int diff) {
	int new_iters = clamp(max_iterations_cpu + diff, 1, 5000);
	mandelLog(INFO, "Changing Maximum Iterations to %d\n", new_iters);
	max_iterations_cpu = new_iters;
}

void changeExponentCpu(int diff) {
	int new_exponent = clamp(exponent_cpu + diff, 1, 200);
	mandelLog(INFO, "Changing Exponent to %d\n", new_exponent);
	exponent_cpu = new_exponent;
}

int mandelbrotCpuInit(int w, int h, int no_simd) {
	mandelLog(VERBOSE, "Starting CPU Mandelbrot Engine...\n");
	int *img_data = (int *)malloc(w * h * sizeof(int));
	if(img_data == NULL) {
		mandelLog(ERROR, "Could not allocate rgb buffer!\n");
		goto error;
	}
	mandelbuffer_cpu = (MandelBuffer){w, h, w*h, img_data};

	nthreads = SDL_GetCPUCount();

	if(nthreads < 1 || nthreads > 256) {
		mandelLog(WARN, "Could not determine CPU core count. "
		          "Using a default of 8 threads.\n");
		nthreads = 8;
	}
	mandelLog(VERBOSE, "Rendering with %d threads.\n", nthreads);

	threads = (SDL_Thread **)malloc(nthreads * sizeof(SDL_Thread *));

	if(threads == NULL) {
		mandelLog(ERROR, "Could not allocate thread data!\n");
		goto error;
	}

	mandelbrot_function = mandelbrot;
#if ENABLE_AVX
	if(__builtin_cpu_supports("avx2")) {
		if(no_simd) {
			mandelLog(VERBOSE, "CPU supports AVX2 but SIMD instructions were explicitly disabled.\n");
		} else {
			mandelLog(VERBOSE, "CPU supports AVX2. Using SIMD instructions to speed up rendering.\n");
			mandelLog(VERBOSE, "To not use SIMD instructions specify the --no-simd command line flag.\n");
			mandelbrot_function = mandelbrotIntrin;
		}
	} else {
		mandelLog(VERBOSE, "CPU does not support AVX2. Not using SIMD instructions.\n");
	}
#endif

	return 0;
error:
	if(img_data != NULL)
		free(img_data);
	if(threads != NULL)
		free(threads);
	return -1;
}

int resizeFramebufferCpu(int new_w, int new_h) {
	mandelbuffer_cpu.rgb_data = (int *)realloc(mandelbuffer_cpu.rgb_data, new_w * new_h * sizeof(int));
	if(mandelbuffer_cpu.rgb_data == NULL) {
		mandelLog(ERROR, "Could not allocate rgb buffer!\n");
		return -1;
	}
	mandelbuffer_cpu.w = new_w;
	mandelbuffer_cpu.h = new_h;
	return 0;
}

void mandelbrotCpuCleanup() {
	mandelLog(VERBOSE, "Cleaning up CPU Mandelbrot Engine...\n");
	free(mandelbuffer_cpu.rgb_data);
	free(threads);
}

void generateImageCpu(Rectangle coord_rect, int *out_argb) {
	int SCALEDOWN = 1; // Optionally render in lower resolution on first pass
	int scl_w = mandelbuffer_cpu.w / SCALEDOWN;
	int scl_h = mandelbuffer_cpu.h / SCALEDOWN;
	int *out_ptr = malloc(scl_w * scl_h * sizeof(int));

	MandelbrotArgs *args_list = (MandelbrotArgs *)malloc(nthreads * sizeof(MandelbrotArgs));
	if(args_list == NULL) {
		mandelLog(ERROR, "Could not create Argslist: %s\n", SDL_GetError());
		return;
	}

	int i;
	for(i = 0; i < nthreads; i++) {
		args_list[i].pix_w = scl_w;
		args_list[i].pix_h = scl_h;
		args_list[i].rect = coord_rect;
		args_list[i].escape_rad = ESCAPE_RADIUS;
		args_list[i].max_iters = max_iterations_cpu;
		args_list[i].pow = exponent_cpu;
		args_list[i].out = out_ptr;
		args_list[i].thread_idx = i;
		args_list[i].nthreads = nthreads;
		threads[i] = SDL_CreateThread(mandelbrot_function, "WorkerThread", args_list + i);
		if(threads[i] == NULL) {
			mandelLog(ERROR, "Could not create SDL_Thread: %s\n", SDL_GetError());
			return;
		}
	}
	for(i = 0; i < nthreads; i++) {
		mandelLog(DEBUG, "Waiting for Thread %d\n", i);
		SDL_WaitThread(threads[i], NULL);
	}

	// Scale half res image to be full size
	for(int y = 0; y < mandelbuffer_cpu.h; y++) {
		for(int x = 0; x < mandelbuffer_cpu.w; x++) {
			out_argb[y * mandelbuffer_cpu.w + x] = out_ptr[y / SCALEDOWN * scl_w + x / SCALEDOWN];
		}
	}
	free(out_ptr);
	free(args_list);
}

void generateImageCpuWH(int w, int h, Rectangle coord_rect, int *out_argb) {
	if(w < 1 || h < 1 || out_argb == NULL)
		return;

	MandelbrotArgs *args_list = (MandelbrotArgs *)malloc(nthreads * sizeof(MandelbrotArgs));
	if(args_list == NULL) {
		mandelLog(ERROR, "Could not create Argslist: %s\n", SDL_GetError());
		return;
	}

	int i;
	for(i = 0; i < nthreads; i++) {
		args_list[i].pix_w = w;
		args_list[i].pix_h = h;
		args_list[i].rect = coord_rect;
		args_list[i].escape_rad = ESCAPE_RADIUS;
		args_list[i].max_iters = max_iterations_cpu;
		args_list[i].pow = exponent_cpu;
		args_list[i].out = out_argb;
		args_list[i].thread_idx = i;
		args_list[i].nthreads = nthreads;
		threads[i] = SDL_CreateThread(mandelbrot_function, "WorkerThread", args_list + i);
		if(threads[i] == NULL) {
			mandelLog(ERROR, "Could not create SDL_Thread: %s\n", SDL_GetError());
			return;
		}
	}
	for(i = 0; i < nthreads; i++) {
		mandelLog(DEBUG, "Waiting for Thread %d\n", i);
		SDL_WaitThread(threads[i], NULL);
	}

	free(args_list);
}

Vec2 calculateShift(Rectangle coord_rect, int aa_counter) {
	float shift_amount_x, shift_amount_y;
	float shift_x = 0;
	float shift_y = 0;
	if(aa_counter < 4) {
		shift_amount_x = coord_rect.w / (float)mandelbuffer_cpu.w / 3.0;
		shift_amount_y = coord_rect.h / (float)mandelbuffer_cpu.h / 3.0;

		/* Go for every corner by using bit pattern of last two bits */
		shift_x = coord_rect.x + ((aa_counter & 2) ? 1.0 : -1.0) * shift_amount_x;
		shift_y = coord_rect.y + ((aa_counter & 1) ? 1.0 : -1.0) * shift_amount_y;
	}
	else if(aa_counter < 8) {
		shift_amount_x = coord_rect.w / (float)mandelbuffer_cpu.w / 2.0;
		shift_amount_y = coord_rect.h / (float)mandelbuffer_cpu.h / 2.0;

		/*
		 * When aa_counter is:
		 * - 4: We shift in positive x direction
		 * - 5: We shift in positive y direction
		 * - 6: We shift in negative x direction
		 * - 7: We shift in negative y direction
		 */
		int even = aa_counter % 2 == 0;
		shift_x = coord_rect.x + ( even ? 1.0 : 0.0) *
				shift_amount_x * (aa_counter > 5 ? -1.0 : 1.0);
		shift_y = coord_rect.y + (!even ? 1.0 : 0.0) *
				shift_amount_y * (aa_counter > 5 ? -1.0 : 1.0);
	}

	return (Vec2){shift_x, shift_y};
}

// aa_counter defines the shift and blend percentage
void doAntiAliasCpu(Rectangle coord_rect, int *argb_buf, int aa_counter) {
	if(argb_buf == NULL)
		return;
	if(aa_counter < 0 || aa_counter > 7)
		return;

	Vec2 shift = calculateShift(coord_rect, aa_counter);

	MandelbrotArgs *args_list = (MandelbrotArgs *)malloc(nthreads * sizeof(MandelbrotArgs));
	if(args_list == NULL) {
		mandelLog(ERROR, "Could not create Argslist: %s\n", SDL_GetError());
		return;
	}
	int i;
	for(i = 0; i < nthreads; i++) {
		args_list[i].pix_w = mandelbuffer_cpu.w;
		args_list[i].pix_h = mandelbuffer_cpu.h;
		args_list[i].rect.x = shift.x;
		args_list[i].rect.y = shift.y;
		args_list[i].rect.w = coord_rect.w;
		args_list[i].rect.h = coord_rect.h;
		args_list[i].escape_rad = ESCAPE_RADIUS;
		args_list[i].max_iters = max_iterations_cpu;
		args_list[i].pow = exponent_cpu;
		args_list[i].out = mandelbuffer_cpu.rgb_data;
		args_list[i].thread_idx = i;
		args_list[i].nthreads = nthreads;
		threads[i] = SDL_CreateThread(mandelbrotIntrin, "WorkerThread", args_list + i);
		if(threads[i] == NULL) {
			mandelLog(ERROR, "Could not create SDL_Thread: %s\n", SDL_GetError());
			return;
		}
	}
	for(i = 0; i < nthreads; i++) {
		mandelLog(DEBUG, "Waiting for Thread %d\n", i);
		SDL_WaitThread(threads[i], NULL);
	}
	free(args_list);

	aa_counter += 2;

	// blend them together
	for(i = 0; i < mandelbuffer_cpu.w * mandelbuffer_cpu.h; i++) {
		int blend_color = blend(argb_buf[i],
				mandelbuffer_cpu.rgb_data[i], 1.0 / aa_counter);
		argb_buf[i] = 0xff000000 | blend_color; // apply full alpha
	}
}
