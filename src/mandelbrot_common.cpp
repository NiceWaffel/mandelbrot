#include "mandelbrot_common.h"
#include "util.h"

void scaleLIN(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
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

void scaleImage(int w_in, int h_in, int w_out, int h_out, int *in_rgb,
		int *out_rgb, int interp_method) {

	switch(interp_method) {
		case INTERP_LINEAR:
			scaleLIN(w_in, h_in, w_out, h_out, in_rgb, out_rgb);
			break;
		case INTERP_NN:
			/* FALLTHRU */
		default:
			scaleNN(w_in, h_in, w_out, h_out, in_rgb, out_rgb);
	}
}
