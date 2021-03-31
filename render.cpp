#include "render.h"

Renderer createRenderer(int init_w, int init_h) {
	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_Window *window = SDL_CreateWindow("Mandelbrot Renderer",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, init_w, init_h,
			SDL_WINDOW_SHOWN);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	Renderer r = {window, renderer, init_w, init_h};

	return r;
}

void destroyRenderer(Renderer to_destroy) {
	SDL_DestroyRenderer(to_destroy.renderer);
	SDL_DestroyWindow(to_destroy.window);
	SDL_Quit();
}

void renderImage(Renderer renderer, int w, int h, int *argb_data) {
	Uint32 rmask, gmask, bmask, amask;
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;

	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(argb_data, w, h, 32, w * 4,
			rmask, gmask, bmask, amask);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer.renderer,
			surface);
	SDL_FreeSurface(surface);

	SDL_RenderCopy(renderer.renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer.renderer);

	SDL_DestroyTexture(texture);
}

void writeToBmp(const char *path, short width, short height, int *data) {
    FILE *out_fd = fopen(path, "wb");
	int line_padding = width % 4;
	unsigned char header[] = {0x42, 0x4d, 0x36, 0xa2, 0x4a, 0x04, 0x00, 0x00,
							0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
							0x00, 0x00, 0xa0, 0x0f, 0x00, 0x00, 0x70, 0x17,
							0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0xa2, 0x4a, 0x04, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int file_size = 54 + width * height * 3 + height * line_padding;
	printf("Writing %d bytes of data to %s\n", file_size, path);
	memcpy(header + 0x02, &file_size, 4);
	memcpy(header + 0x12, &width, 2);
	memcpy(header + 0x16, &height, 2);
	fwrite(header, 1, 0x36, out_fd);

    int zero = 0;
	int col;
    for (int i = 0; i < height; i++) {
        for(int j = 0; j < width; j++) {
			col = data[i * width + j];
			col = ((col & 0xff) << 16) | (col & 0xff00) |
					((col & 0xff0000) >> 16);
            fwrite(&col, 1, 3, out_fd);
        }
        fwrite(&zero, 1, line_padding, out_fd);
    }

    fclose(out_fd);
}

