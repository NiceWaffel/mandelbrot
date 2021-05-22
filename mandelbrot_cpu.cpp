#include "mandelbrot_cpu.h"
#include "config.h"
#include "logger.h"

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <math.h>
}

typedef struct {
	int w;
	int h;
	int *rgb_data;
} MandelBuffer;

typedef struct {
	int pix_w, pix_h;
	float x, y, w, h;
	float escape_rad;
	int *out;
	int thread_idx;
} MandelbrotArgs;

MandelBuffer mandelbuffer_cpu;
static int nthreads = 0;
static pthread_t *threadIds;
static MandelbrotArgs *args_list;

int getIterationsCpu(float x0, float y0, float escape_rad) {
	int iteration = 0;
	float x = 0.0;
	float y = 0.0;

	while(x*x + y*y <= escape_rad * escape_rad && iteration < MAX_ITERATIONS) {
		float tmpx = x * x - y * y + x0;
		y = 2 * x * y + y0;
		x = tmpx;
		iteration++;
	}
	return iteration;
}

int iterationsToColorCpu(int iterations) {
	if(iterations >= MAX_ITERATIONS)
		return 0x000000; // Black

	float hue = (int)(sqrt((float)iterations) * 12.0);
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

void *mandelbrot(void *voidargs) {
	MandelbrotArgs *args = (MandelbrotArgs *)voidargs;
	for (int i = args->thread_idx; i < args->pix_w * args->pix_h; i += nthreads) {
		float cx = (float)(i % args->pix_w);
		float cy = (float)(i / args->pix_w);
		cx = cx / (float)(args->pix_w) * args->w + args->x;
		cy = cy / (float)(args->pix_h) * args->h + args->y;

		int iters = getIterationsCpu(cx, cy, args->escape_rad);
		int color = iterationsToColorCpu(iters);
		args->out[i] = 0xff000000 | color; // Write color with full alpha into output
	}
	return NULL;
}

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int mandelbrotCpuInit(int w, int h) {
	log(INFO, "Starting CPU Mandelbrot Engine...\n");
	int *img_data = (int *)malloc(w * h * sizeof(int));
	if(img_data == NULL) {
		log(ERROR, "Could not allocate rgb buffer!\n");
		goto error;
	}
	mandelbuffer_cpu = {w, h, img_data};

#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	nthreads = info.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
	nthreads = sysconf(_SC_NPROCESSORS_ONLN);
#endif
#endif
	if(nthreads < 1 || nthreads > 256) {
		log(WARN, "Could not determine CPU core count. "
		          "Using a default of 8 threads.\n");
		nthreads = 8;
	}

	threadIds = (pthread_t *)malloc(nthreads * sizeof(pthread_t));
	args_list = (MandelbrotArgs *)malloc(nthreads * sizeof(MandelbrotArgs));

	if(threadIds == NULL || args_list == NULL) {
		log(ERROR, "Could not allocate thread data!\n");
		goto error;
	}

	return 0;
error:
	if(img_data != NULL)
		free(img_data);
	if(threadIds != NULL)
		free(threadIds);
	if(args_list != NULL)
		free(args_list);
	return -1;
}

void mandelbrotCpuCleanup() {
	free(mandelbuffer_cpu.rgb_data);
	free(threadIds);
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
		args_list[i].out = out_argb;
		args_list[i].thread_idx = i;
		int ret = pthread_create(threadIds + i, NULL,
				mandelbrot, args_list + i);
		if(ret != 0) {
			log(ERROR, "Could not create pthread!\n");
			exit(EXIT_FAILURE);
		}
	}
	for(i = 0; i < nthreads; i++) {
		log(DEBUG, "Waiting for Thread %d\n", i);
		pthread_join(threadIds[i], NULL);
	}
	//memcpy(out_argb, mandelbuffer_cpu.rgb_data,
	//		mandelbuffer_cpu.w * mandelbuffer_cpu.h * sizeof(int));
}

void generateImageCpu2(int w, int h, Rectangle coord_rect, int *out_argb) {
	if(w || h || coord_rect.x > 0.0 || out_argb != NULL)
		return;
	return;
	/* TODO implement
	if(out_argb == NULL)
		return;

	int *out;
	mandelbrot(w, h, coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, out);

	memcpy(out_argb, out, w * h * sizeof(int));
	*/
}
