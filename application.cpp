#include "render.h" //Includes SDL
#include "mandelbrot.h"

Renderer renderer;
Rectangle rect;
int quit = 0;
int w, h;

SDL_mutex *mutex;

int renderLoop(void *ptr) {
	SDL_LockMutex(mutex);
	renderer = createRenderer(w, h);
	if(mandelbrotInit(w, h)) {
		fprintf(stderr, "Error initializing Mandelbrot Engine!\n");
		exit(EXIT_FAILURE);
	}
	int *rgb_data = (int *) malloc(w * h * sizeof(int));
	if(rgb_data == NULL) {
		fprintf(stderr, "Error allocating memory for RGB-data!\n");
		exit(EXIT_FAILURE);
	}
	SDL_UnlockMutex(mutex);

	clock_t time;
	Rectangle rect_cache;
	int aa_counter = 0;
	while(!quit) {
		if(memcmp(&rect_cache, &rect, sizeof(Rectangle))) {
			aa_counter = 0; // Reset Antialias
			rect_cache = rect;
			time = clock();
			generateImage(rect, rgb_data);
			printf("Image generation took %6ld ticks\n", clock() - time);
			renderImage(renderer, renderer.width, renderer.height, rgb_data);
		} else if(aa_counter < 4) {
			printf("Applying AntiAlias %d\n", aa_counter);
			
			// aa_counter starts at 0 and ends at 3
			doAntiAlias(rect, rgb_data, aa_counter);
			renderImage(renderer, renderer.width, renderer.height, rgb_data);
			aa_counter++;
		} else {
			SDL_Delay(20);
		}
	}

	mandelbrotCleanup();
	free(rgb_data);
	destroyRenderer(renderer);
	return 0;
}

void eventLoop() {
	SDL_LockMutex(mutex);
	SDL_UnlockMutex(mutex);
	int mouse_state = SDL_RELEASED;
	SDL_Event ev;
	while(SDL_WaitEvent(&ev)) {

		if(ev.type == SDL_MOUSEWHEEL) {
			if(ev.wheel.y > 0) { // scroll up
				rect.x += 0.1 * rect.w;
				rect.y += 0.1 * rect.h;
				rect.w = 0.8 * rect.w;
				rect.h = 0.8 * rect.h;
			} else if(ev.wheel.y < 0) { // scroll down
				rect.x -= 0.125 * rect.w;
				rect.y -= 0.125 * rect.h;
				rect.w = 1.25 * rect.w;
				rect.h = 1.25 * rect.h;
			}
		} else if(ev.type == SDL_KEYDOWN) {
			switch(ev.key.keysym.sym) {
				case SDLK_q:
					return;
				case SDLK_UP:
					rect.y -= rect.h * 0.02;
					break;
				case SDLK_DOWN:
					rect.y += rect.h * 0.02;
					break;
				case SDLK_LEFT:
					rect.x -= rect.w * 0.02;
					break;
				case SDLK_RIGHT:
					rect.x += rect.w * 0.02;
					break;
				case SDLK_PAGEUP:
					rect.x += 0.1 * rect.w;
					rect.y += 0.1 * rect.h;
					rect.w = 0.8 * rect.w;
					rect.h = 0.8 * rect.h;
					break;
				case SDLK_PAGEDOWN:
					rect.x -= 0.125 * rect.w;
					rect.y -= 0.125 * rect.h;
					rect.w = 1.25 * rect.w;
					rect.h = 1.25 * rect.h;
					break;
				case SDLK_s: //screenshot
					int *data = (int *) malloc(3840 * 2160 * sizeof(int));
					generateImage2(3840, 2160, rect, data);
					writeToBmp("output.bmp", 3840, 2160, data);
					free(data);
					break;
			}
		} else if(ev.type == SDL_MOUSEMOTION) {
			if(mouse_state == SDL_PRESSED) {
				rect.x -= ev.motion.xrel * rect.w / (float)w;
				rect.y -= ev.motion.yrel * rect.h / (float)h;
			} else {
				continue;
			}
		} else if(ev.type == SDL_MOUSEBUTTONDOWN ||
				ev.type == SDL_MOUSEBUTTONUP) {
			if(ev.button.button == SDL_BUTTON_LEFT)
				mouse_state = ev.button.state;
		}
	}
}

int main(int argc, char **argv) {
	w = 1800;
	h = 1000;
	rect = {-2.5, -1.0, 3.6, 2.0};

	mutex = SDL_CreateMutex();
	if(mutex == NULL) {
		fprintf(stderr, "Error creating mutex!\n");
		exit(EXIT_FAILURE);
	}

	SDL_Thread *renderThread = SDL_CreateThread(renderLoop,
			"RenderThread", NULL);
	if(renderThread == NULL) {
		fprintf(stderr, "Error creating RenderThread! Exiting Program!\n");
		exit(EXIT_FAILURE);
	}
	eventLoop();

	quit = 1;
	SDL_WaitThread(renderThread, NULL);
	SDL_DestroyMutex(mutex);
	return 0;
}
