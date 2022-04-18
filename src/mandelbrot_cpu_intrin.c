#include "mandelbrot_cpu_intrin.h"

#include <immintrin.h>
#include <math.h>

void iterateIntrin(__m256 x0, __m256 y0, __m256 *x, __m256 *y, int pow) {
	__m256 retx = *x;
	__m256 rety = *y;

	for(int i = 0; i < pow - 1; i++) {
		__m256 tmpx = _mm256_sub_ps(_mm256_mul_ps(*x, retx), _mm256_mul_ps(*y, rety));
		rety = _mm256_add_ps(_mm256_mul_ps(*x, rety), _mm256_mul_ps(retx, *y));
		retx = tmpx;
	}

	x[0] = _mm256_add_ps(retx, x0);
	y[0] = _mm256_add_ps(rety, y0);
}

__m256i getIterationsCpuIntrin(__m256 x0, __m256 y0, float escape_rad_sq, int max_iters, int pow) {
	__m256 x = _mm256_setzero_ps();
	__m256 y = _mm256_setzero_ps();
	__m256 escapeRadVec = _mm256_set1_ps(escape_rad_sq);

	__m256i retVal = _mm256_setzero_si256();

	int iteration = 0;
	while(iteration < max_iters) {
		__m256 dist = _mm256_add_ps(_mm256_mul_ps(x, x), _mm256_mul_ps(y, y));
		__m256 comp = _mm256_cmp_ps(dist, escapeRadVec, 2); // _CMP_LT_OS

		if(_mm256_testz_ps(comp, comp)) {
			break;
		}

		__m256i compInt = _mm256_castps_si256(comp);
		retVal = _mm256_sub_epi32(retVal, compInt);
		iteration++;

		iterateIntrin(x0, y0, &x, &y, pow);
	}

	return retVal;
}

int iterationsToColorCpuIntrin(int iterations, int max_iters) {
	if(iterations >= max_iters)
		return 0x000000; // Black

	const float C = 1.0;
	float hue = (int)(sqrt((float)iterations) * 10.0);
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

int extractInt(__m256i vector, char index) {
	// _mm256_extract_epi32 expects an immediate as second argument
	// Thereby we 'convert' the index to an immediate
	// FIXME Probably not the cleanest or fastest option available
	switch(index) {
		case 0: return _mm256_extract_epi32(vector, 7);
		case 1: return _mm256_extract_epi32(vector, 6);
		case 2: return _mm256_extract_epi32(vector, 5);
		case 3: return _mm256_extract_epi32(vector, 4);
		case 4: return _mm256_extract_epi32(vector, 3);
		case 5: return _mm256_extract_epi32(vector, 2);
		case 6: return _mm256_extract_epi32(vector, 1);
		case 7: return _mm256_extract_epi32(vector, 0);
	}
	return 0;
}

int mandelbrotIntrin(void *voidargs) {
	MandelbrotArgs *args = (MandelbrotArgs *)voidargs;

	__m256 vecRectX = _mm256_set1_ps(args->rect.x);
	__m256 vecRectY = _mm256_set1_ps(args->rect.y);
	__m256 vecRectW = _mm256_set1_ps(args->rect.w);
	__m256 vecRectH = _mm256_set1_ps(args->rect.h);
	__m256 vecPixW = _mm256_set1_ps(args->pix_w);
	__m256 vecPixH = _mm256_set1_ps(args->pix_h);

	__m256 counter = _mm256_set_ps(0, 1, 2, 3, 4, 5, 6, 7);

	float escapeRadSq = args->escape_rad * args->escape_rad;

	for(int x = args->thread_idx; x < args->pix_w; x += args->nthreads) {
		__m256 vecX = _mm256_set1_ps((float)x);
		vecX = _mm256_div_ps(_mm256_mul_ps(vecX, vecRectW), vecPixW);
		vecX = _mm256_add_ps(vecX, vecRectX);

		// TODO it probably makes more sense to switch x and y loops
		// to ensure args->out is accessed somewhat sequentially

		for(int y = 0; y < args->pix_h; y += 8) {
			__m256 vecY = _mm256_set1_ps((float)y);
			vecY = _mm256_add_ps(vecY, counter);
			vecY = _mm256_div_ps(_mm256_mul_ps(vecY, vecRectH), vecPixH);
			vecY = _mm256_add_ps(vecY, vecRectY);

			__m256i iterations = getIterationsCpuIntrin(vecX, vecY, escapeRadSq, args->max_iters, args->pow);

			for(int i = 0; i < 8 && y + i < args->pix_h; i++) {
				int color = iterationsToColorCpuIntrin(extractInt(iterations, i), args->max_iters);

				// Write color with full alpha into output
				args->out[(y + i) * args->pix_w + x] = 0xff000000 | color;
			}
		}
	}

	return 0;
}
