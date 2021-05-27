#ifndef _RENDER_H_
#define _RENDER_H_

#include <SDL.h>
#include <stdlib.h>
#include <time.h>

#include "logger.h"

typedef struct Renderer {
	SDL_Window *window;
	SDL_Renderer *renderer;
	int width;
	int height;
} Renderer;

Renderer createRenderer(int init_w, int init_h);

void renderImage(Renderer renderer, int w, int h, int *argb_data);

void destroyRenderer(Renderer to_destroy);

void writeToBmp(const char *path, short width, short height, int *data);

#endif
