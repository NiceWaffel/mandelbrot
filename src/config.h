#ifndef _CONFIG_H_
#define _CONFIG_H_

#define ENABLE_CUDA 1
#define ENABLE_AVX 1

#define DEFAULT_WIDTH 1600
#define DEFAULT_HEIGHT 900

#define ESCAPE_RADIUS 3.0
#define DEFAULT_ITERATIONS 800
#define DEFAULT_EXPONENT 2

#define RENDER_THREAD_BLOCKS 32
#define RENDER_THREADS 256

#define MAX_AA_COUNTER 8

// Overallocation of the framebuffer in pixels
// Overallocation is limited to 4 MB (each pixel is 4 bytes)
#define OVERALLOC_LIMIT 1048576

#endif
