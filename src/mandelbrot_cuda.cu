extern "C" {
#include "mandelbrot_cuda.h"

#include "util.h"
#include "logger.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
}

MandelBuffer mandelbuffer;

int max_iterations = DEFAULT_ITERATIONS;
int exponent = DEFAULT_EXPONENT;

__device__
void iterate(float x0, float y0, int power, float *x, float *y) {
	float retx = *x;
	float rety = *y;
	for(int i = 0; i < power - 1; i++) {
		float tmpx = retx * x[0] - rety * y[0];
		rety = x[0] * rety + retx * y[0];
		retx = tmpx;
	}
	x[0] = retx + x0;
	y[0] = rety + y0;
}

__device__
int getIterations(float x0, float y0, float escape_rad, int max_iters, int power) {
	int iteration = 0;
	float x = 0.0;
	float y = 0.0;

	while(x*x + y*y <= escape_rad * escape_rad && iteration < max_iters) {
		iterate(x0, y0, power, &x, &y);
		iteration++;
	}
	return iteration;
}

__device__
int iterationsToColor(int iterations, int max_iters) {
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

__global__
void mandelbrot(int pix_w, int pix_h, float coord_x, float coord_y,
				float coord_w, float coord_h, float escape_rad,
				int *out, int max_iters, int power) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < pix_w * pix_h; i += stride) {
		float cx = (float)(i % pix_w);
		float cy = (float)(i / pix_w);
		cx = cx / (float)pix_w * coord_w + coord_x;
		cy = cy / (float)pix_h * coord_h + coord_y;

		int iters = getIterations(cx, cy, escape_rad, max_iters, power);
		int color = iterationsToColor(iters, max_iters);
		out[i] = 0xff000000 | color; // Write color with full alpha into output
    }
}


/* Host functions declared extern "C" to be linkable with C code */
extern "C" {

void changeIterationsCuda(int diff) {
	int new_iters = clamp(max_iterations + diff, 1, 5000);
	mandelLog(INFO, "Changing Maximum Iterations to %d\n", new_iters);
	max_iterations = new_iters;
}

void changeExponentCuda(int diff) {
	int new_exponent = clamp(exponent + diff, 1, 200);
	mandelLog(INFO, "Changing Exponent to %d\n", new_exponent);
	exponent = new_exponent;
}

int getDeviceAttributes() {
	int ret;
	int cur_device;

	ret = cudaGetDevice(&cur_device);
	if(ret != cudaSuccess) goto error;
	mandelLog(DEBUG, "Running on device %d\n", cur_device);

	cudaDeviceProp props;
	ret = cudaGetDeviceProperties(&props, cur_device);
	if(ret != cudaSuccess) goto error;
	mandelLog(DEBUG, "Device name: %s\n", props.name);
	mandelLog(DEBUG, "Device Cuda Version: %d.%d\n", props.major, props.minor);
	mandelLog(DEBUG, "Managed memory is%s supported\n",
			props.managedMemory ? "" : " not");

	return 0;
error:
	return -1;
}

int mandelbrotCudaInit(int w, int h) {
	mandelLog(VERBOSE, "Starting Cuda Mandelbrot Engine...\n");
	mandelLog(VERBOSE, "To not use Cuda, specify the --force-cpu command line flag.\n");
	int *img_data = NULL;
	int ret = cudaMalloc(&img_data, w * h * sizeof(int));
	if(ret != cudaSuccess) goto error;
	mandelbuffer = {w, h, w*h, img_data};

	ret = getDeviceAttributes();
	if(ret != cudaSuccess) goto error;

	return 0;
error:
	if(img_data != NULL)
		cudaFree(img_data);
	return ret;
}

void mandelbrotCudaCleanup() {
	mandelLog(VERBOSE, "Cleaning up Cuda Mandelbrot Engine...\n");
	cudaFree(mandelbuffer.rgb_data);
}

int resizeFramebufferCuda(int new_w, int new_h) {
	int alloc_diff = mandelbuffer.alloc_size - new_w * new_h;
	if(alloc_diff > 0 && alloc_diff < OVERALLOC_LIMIT) {
		// When the necessary size is already overallocated and below the
		// overallocation-limit we don't reallocate
		mandelbuffer.w = new_w;
		mandelbuffer.h = new_h;
		return 0;
	}

	// Reallocation is needed
	// We overallocate half of the overallocation limit for good flexibility
	cudaFree(mandelbuffer.rgb_data);
	int *img_data = NULL;
	int alloc_size = new_w * new_h + (OVERALLOC_LIMIT / 2);
	if(cudaMalloc(&img_data, alloc_size * sizeof(int)) != cudaSuccess) {
		return -1;
	}
	mandelbuffer = {new_w, new_h, alloc_size, img_data};
	return 0;
}

void generateImageCuda(Rectangle coord_rect, int *out_argb) {
	if(out_argb == NULL)
		return;

	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(mandelbuffer.w, mandelbuffer.h,
			coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, mandelbuffer.rgb_data, max_iterations, exponent);
	cudaDeviceSynchronize();

	cudaMemcpy(out_argb, mandelbuffer.rgb_data,
			mandelbuffer.w * mandelbuffer.h * sizeof(int), cudaMemcpyDeviceToHost);
}

void generateImageCudaWH(int w, int h, Rectangle coord_rect, int *out_argb) {
	if(out_argb == NULL)
		return;

	int *out;
	if(cudaMalloc(&out, w * h * sizeof(int)) != cudaSuccess) {
		// Allocation error
		return;
	}
	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(w, h, coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, out, max_iterations, exponent);
	cudaDeviceSynchronize();

	cudaMemcpy(out_argb, out, w * h * sizeof(int), cudaMemcpyDeviceToHost);
	cudaFree(out);
}

// aa_counter defines the shift and blend percentage
void doAntiAliasCuda(Rectangle coord_rect, int *argb_buf, int aa_counter) {
	if(argb_buf == NULL)
		return;
	if(aa_counter < 0 || aa_counter > 7)
		return;

	float shift_amount_x, shift_amount_y;
	float shift_x, shift_y;
	if(aa_counter < 4) {
		shift_amount_x = coord_rect.w / (float)mandelbuffer.w / 3.0;
		shift_amount_y = coord_rect.h / (float)mandelbuffer.h / 3.0;

		/* Go for every corner by using bit pattern of last two bits */
		shift_x = coord_rect.x + ((aa_counter & 2) ? 1.0 : -1.0) * shift_amount_x;
		shift_y = coord_rect.y + ((aa_counter & 1) ? 1.0 : -1.0) * shift_amount_y;
	}
	else if(aa_counter < 8) {
		shift_amount_x = coord_rect.w / (float)mandelbuffer.w / 2.0;
		shift_amount_y = coord_rect.h / (float)mandelbuffer.h / 2.0;

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

	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(mandelbuffer.w, mandelbuffer.h,
			shift_x, shift_y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, mandelbuffer.rgb_data, max_iterations, exponent);
	cudaDeviceSynchronize();

	aa_counter += 2;

	int *rgb_data = (int *)malloc(mandelbuffer.w * mandelbuffer.h * sizeof(int));
	cudaMemcpy(rgb_data, mandelbuffer.rgb_data,
			mandelbuffer.w * mandelbuffer.h * sizeof(int), cudaMemcpyDeviceToHost);

	// blend them together
	for(int i = 0; i < mandelbuffer.w * mandelbuffer.h; i++) {
		int blend_color = blend(argb_buf[i], rgb_data[i], 1.0 / aa_counter);
		argb_buf[i] = 0xff000000 | blend_color; // apply full alpha
	}
	free(rgb_data);
}

}
