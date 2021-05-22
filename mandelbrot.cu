#include "mandelbrot.h"
#include "config.h"
#include "logger.h"

extern "C" {
#include <stdlib.h>
#include <string.h>
}

typedef struct {
	int w;
	int h;
	int *rgb_data;
} MandelBuffer;

MandelBuffer mandelbuffer;

__device__
int getIterations(float x0, float y0, float escape_rad) {
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

__device__
int iterationsToColor(int iterations) {
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

__global__
void mandelbrot(int pix_w, int pix_h, float coord_x, float coord_y,
				float coord_w, float coord_h, float escape_rad, int *out) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < pix_w * pix_h; i += stride) {
		float cx = (float)(i % pix_w);
		float cy = (float)(i / pix_w);
		cx = cx / (float)pix_w * coord_w + coord_x;
		cy = cy / (float)pix_h * coord_h + coord_y;

		int iters = getIterations(cx, cy, escape_rad);
		int color = iterationsToColor(iters);
		out[i] = 0xff000000 | color; // Write color with full alpha into output
    }
}

__host__
inline int round_simple(float f) {
	return (int)(f + 0.5);
}

__host__
int blend(int in_color, int blend_color, float ratio) {
	float r_in = in_color & 0xff;
	float g_in = (in_color & 0xff00) >> 8;
	float b_in = (in_color & 0xff0000) >> 16;

	float r_blend = blend_color & 0xff;
	float g_blend = (blend_color & 0xff00) >> 8;
	float b_blend = (blend_color & 0xff0000) >> 16;

	r_in = r_in * (1.0 - ratio) + r_blend * ratio;
	g_in = g_in * (1.0 - ratio) + g_blend * ratio;
	b_in = b_in * (1.0 - ratio) + b_blend * ratio;

	return (int)r_in + (int)g_in * 256 + (int)b_in * 65536;
}

__host__
int clamp(int i, int min, int max) {
	return i < min ? min : i > max ? max : i;
}

__host__
void scaleLinear(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb) {
	float scale_x = (float)w_in / (float)w_out;
	float scale_y = (float)h_in / (float)h_out;

	for(int out_y = 0; out_y < h_out; out_y++) {
		float in_y = scale_y * (float)out_y;
		for(int out_x = 0; out_x < w_out; out_x++) {
			float in_x = scale_x * (float)out_x;

			float px = in_x - (int)in_x;
			float py = in_y - (int)in_y;

			int x = (int)in_x;
			int y = (int)in_y;

			float p00, p01, p10, p11;
			p00 = px * py;
			p01 = (1.0 - px) * py;
			p10 = px * (1.0 - py);
			p11 = (1.0 - px) * (1.0 - py);

			int color = 0;
			color = blend(color, in_rgb[clamp(y, 0, h_in) * w_in +
					clamp(x, 0, w_in)], p00);
			color = blend(color, in_rgb[clamp(y, 0, h_in) * w_in +
					clamp(x + 1, 0, w_in)], p01);
			color = blend(color, in_rgb[clamp(y + 1, 0, h_in) * w_in +
					clamp(x, 0, w_in)], p10);
			color = blend(color, in_rgb[clamp(y + 1, 0, h_in) * w_in +
					clamp(x + 1, 0, w_in)], p11);

			out_rgb[out_y * w_out + out_x] = 0xff000000 | color;
		}
	}
}

__host__
void scaleNN(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb) {
	float scale_x = (float)w_in / (float)w_out;
	float scale_y = (float)h_in / (float)h_out;

	for(int y = 0; y < h_out; y++) {
		int in_y = round_simple(scale_y * (float)y);
		for(int x = 0; x < w_out; x++) {
			int in_x = round_simple(scale_x * (float)x);
			out_rgb[y * w_out + x] = in_rgb[in_y * w_in + in_x];
		}
	}
}

int get_device_attributes() {

	int ret;
	int cur_device;

	ret = cudaGetDevice(&cur_device);
	if(ret != cudaSuccess) goto error;
	log(DEBUG, "Running on device %d\n", cur_device);

	cudaDeviceProp props;
	ret = cudaGetDeviceProperties(&props, cur_device);
	if(ret != cudaSuccess) goto error;
	log(DEBUG, "Running on GPU %s\n", props.name);
	log(DEBUG, "Device Cuda Version: %d.%d\n", props.major, props.minor);
	log(DEBUG, "Managed memory is%s supported\n",
			props.managedMemory ? "" : " not");

	return 0;
error:
	return -1;
}

int mandelbrotInit(int w, int h) {
	log(INFO, "Starting Mandelbrot Engine...\n");
	int *img_data = NULL;
	int ret = cudaMallocManaged(&img_data, w * h * sizeof(int));
	if(ret != cudaSuccess) goto error;
	mandelbuffer = {w, h, img_data};

	ret = get_device_attributes();
	if(ret != cudaSuccess) goto error;

	return 0;
error:
	if(img_data != NULL)
		cudaFree(img_data);
	return ret;
}

void mandelbrotCleanup() {
	cudaFree(mandelbuffer.rgb_data);
}

void generateImage(Rectangle coord_rect, int *out_argb) {
	if(out_argb == NULL)
		return;

	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(mandelbuffer.w, mandelbuffer.h,
			coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, mandelbuffer.rgb_data);
	cudaDeviceSynchronize();

	memcpy(out_argb, mandelbuffer.rgb_data,
			mandelbuffer.w * mandelbuffer.h * sizeof(int));
}

void generateImage2(int w, int h, Rectangle coord_rect, int *out_argb) {
	if(out_argb == NULL)
		return;

	int *out;
	cudaMallocManaged(&out, w * h * sizeof(int));
	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(w, h, coord_rect.x, coord_rect.y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, out);
	cudaDeviceSynchronize();

	memcpy(out_argb, out, w * h * sizeof(int));
	cudaFree(out);
}

// aa_counter defines the shift and blend percentage
void doAntiAlias(Rectangle coord_rect, int *argb_buf, int aa_counter) {
	if(argb_buf == NULL)
		return;

	float shift_amount_x = coord_rect.w / (float)mandelbuffer.w / 3.0;
	float shift_amount_y = coord_rect.h / (float)mandelbuffer.h / 3.0;

	float shift_x = coord_rect.x +
			((aa_counter & 2) ? 1.0 : -1.0) * shift_amount_x;
	float shift_y = coord_rect.y +
			((aa_counter & 1) ? 1.0 : -1.0) * shift_amount_y;

	mandelbrot<<<RENDER_THREAD_BLOCKS, RENDER_THREADS>>>(mandelbuffer.w, mandelbuffer.h,
			shift_x, shift_y,
			coord_rect.w, coord_rect.h, ESCAPE_RADIUS, mandelbuffer.rgb_data);
	cudaDeviceSynchronize();

	aa_counter += 2;

	// blend them together
	for(int i = 0; i < mandelbuffer.w * mandelbuffer.h; i++) {
		int blend_color = blend(argb_buf[i],
				mandelbuffer.rgb_data[i], 1.0 / aa_counter);
		argb_buf[i] = 0xff000000 | blend_color; // apply full alpha
	}
}

void scaleImage(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb, int interp_method) {

	switch(interp_method) {
		case INTERP_LINEAR:
			scaleLinear(w_in, h_in, w_out, h_out, in_rgb, out_rgb);
			break;
		case INTERP_NN:
			/* FALLTHRU */
		default:
			scaleNN(w_in, h_in, w_out, h_out, in_rgb, out_rgb);
	}
}

