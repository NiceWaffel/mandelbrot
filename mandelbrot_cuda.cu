#include "mandelbrot_cuda.h"
#include "config.h"
#include "logger.h"

#include "util.h"

extern "C" {
#include <stdlib.h>
#include <string.h>
}

MandelBuffer mandelbuffer;

int max_iterations = DEFAULT_ITERATIONS;

__device__
int getIterations(float x0, float y0, float escape_rad, int max_iters) {
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

__device__
int iterationsToColor(int iterations, int max_iters) {
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

__global__
void mandelbrot(int pix_w, int pix_h, float coord_x, float coord_y,
				float coord_w, float coord_h, float escape_rad,
				int *out, int max_iters) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < pix_w * pix_h; i += stride) {
		float cx = (float)(i % pix_w);
		float cy = (float)(i / pix_w);
		cx = cx / (float)pix_w * coord_w + coord_x;
		cy = cy / (float)pix_h * coord_h + coord_y;

		int iters = getIterations(cx, cy, escape_rad, max_iters);
		int color = iterationsToColor(iters, max_iters);
		out[i] = 0xff000000 | color; // Write color with full alpha into output
    }
}

__host__
void changeIterationsCuda(int diff) {
	int new_iters = clamp(max_iterations + diff, 1, 5000);
	log(INFO, "Changing Maximum Iterations to %d\n", new_iters);
	max_iterations = new_iters;
}

int getDeviceAttributes() {
	int ret;
	int cur_device;

	ret = cudaGetDevice(&cur_device);
	if(ret != cudaSuccess) goto error;
	log(DEBUG, "Running on device %d\n", cur_device);

	cudaDeviceProp props;
	ret = cudaGetDeviceProperties(&props, cur_device);
	if(ret != cudaSuccess) goto error;
	log(DEBUG, "Device name: %s\n", props.name);
	log(DEBUG, "Device Cuda Version: %d.%d\n", props.major, props.minor);
	log(DEBUG, "Managed memory is%s supported\n",
			props.managedMemory ? "" : " not");

	return 0;
error:
	return -1;
}

int mandelbrotCudaInit(int w, int h) {
	log(VERBOSE, "Starting Mandelbrot Engine...\n");
	int *img_data = NULL;
	int ret = cudaMallocManaged(&img_data, w * h * sizeof(int));
	if(ret != cudaSuccess) goto error;
	mandelbuffer = {w, h, img_data};

	ret = getDeviceAttributes();
	if(ret != cudaSuccess) goto error;

	return 0;
error:
	if(img_data != NULL)
		cudaFree(img_data);
	return ret;
}

void mandelbrotCudaCleanup() {
	log(VERBOSE, "Cleaning up Mandelbrot Engine...\n");
	cudaFree(mandelbuffer.rgb_data);
}

void generateImageCuda(Rectangle coord_rect, int *out_argb) {
	if(out_argb == NULL)
		return;

	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(mandelbuffer.w, mandelbuffer.h,
			coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, mandelbuffer.rgb_data, max_iterations);
	cudaDeviceSynchronize();

	memcpy(out_argb, mandelbuffer.rgb_data,
			mandelbuffer.w * mandelbuffer.h * sizeof(int));
}

void generateImageCudaWH(int w, int h, Rectangle coord_rect, int *out_argb) {
	if(out_argb == NULL)
		return;

	int *out;
	cudaMallocManaged(&out, w * h * sizeof(int));
	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(w, h, coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, out, max_iterations);
	cudaDeviceSynchronize();

	memcpy(out_argb, out, w * h * sizeof(int));
	cudaFree(out);
}

// aa_counter defines the shift and blend percentage
void doAntiAliasCuda(Rectangle coord_rect, int *argb_buf, int aa_counter) {
	if(argb_buf == NULL)
		return;
	if(aa_counter < 0 || aa_counter > 3)
		return;

	float shift_amount_x = coord_rect.w / (float)mandelbuffer.w / 3.0;
	float shift_amount_y = coord_rect.h / (float)mandelbuffer.h / 3.0;

	// Use the last two bits of aa_counter as shift indicator
	// For now this works fine, as the aa_counter is only allowed in a range of 0 to 3
	float shift_x = coord_rect.x +
			((aa_counter & 2) ? 1.0 : -1.0) * shift_amount_x;
	float shift_y = coord_rect.y +
			((aa_counter & 1) ? 1.0 : -1.0) * shift_amount_y;

	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(mandelbuffer.w, mandelbuffer.h,
			shift_x, shift_y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, mandelbuffer.rgb_data, max_iterations);
	cudaDeviceSynchronize();

	aa_counter += 2;

	// blend them together
	for(int i = 0; i < mandelbuffer.w * mandelbuffer.h; i++) {
		int blend_color = blend(argb_buf[i],
				mandelbuffer.rgb_data[i], 1.0 / aa_counter);
		argb_buf[i] = 0xff000000 | blend_color; // apply full alpha
	}
}

