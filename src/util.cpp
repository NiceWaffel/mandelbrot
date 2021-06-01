#include "util.h"

int round_simple(float f) {
	return (int)(f + 0.5);
}

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

int clamp(int i, int min, int max) {
	return i < min ? min : i > max ? max : i;
}
