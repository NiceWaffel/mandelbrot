#include "render.h" //Includes SDL
#include "mandelbrot.h"
#include "logger.h"

static Renderer renderer;
static Rectangle rect;
static int quit = 0;
static int w, h;
static int disable_aa = 0;

int loglevel;

static SDL_mutex *mutex;

int renderLoop(void *ptr) {
	SDL_LockMutex(mutex);
	log(INFO, "Starting application in resolution %dx%d\n", w, h);
	renderer = createRenderer(w, h);
	if(mandelbrotInit(w, h)) {
		log(ERROR, "Could not initialize Mandelbrot Engine!\n");
		exit(EXIT_FAILURE);
	}
	int *rgb_data = (int *) malloc(w * h * sizeof(int));
	if(rgb_data == NULL) {
		log(ERROR, "Could not allocate memory for RGB-data!\n");
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
			log(DEBUG, "Image generation took %6ld ticks\n", clock() - time);
			renderImage(renderer, renderer.width, renderer.height, rgb_data);
		} else if(!disable_aa && aa_counter < 4) {
			log(DEBUG, "Applying Antialias %d\n", aa_counter);

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
			int mx, my;
			SDL_GetMouseState(&mx, &my);
			float x_skew = (float)mx / (float)w;
			float y_skew = (float)my / (float)h;
			if(ev.wheel.y > 0) { // scroll up
				rect.x += 0.2 * x_skew * rect.w;
				rect.y += 0.2 * y_skew * rect.h;
				rect.w = 0.8 * rect.w;
				rect.h = 0.8 * rect.h;
			} else if(ev.wheel.y < 0) { // scroll down
				rect.x -= 0.25 * x_skew * rect.w;
				rect.y -= 0.25 * y_skew * rect.h;
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

void parseArguments(int argc, char **argv) {
	int i = 1;
	while(i < argc) {
		if(strcmp("-w", argv[i]) == 0) { // Set window width
			i++;
			if(i < argc)
				w = atoi(argv[i]);
			if(w == 0)
				w = 1800;
		} else if(strcmp("-h", argv[i]) == 0) { // Set window height
			i++;
			if(i < argc)
				h = atoi(argv[i]);
			if(h == 0)
				h = 1000;
		} else if(strcmp("-v", argv[i]) == 0) {
			loglevel = INFO;
			log(INFO, "Increased loglevel to INFO\n");
		} else if(strcmp("-vv", argv[i]) == 0) {
			loglevel = DEBUG;
			log(INFO, "Increased loglevel to DEBUG\n");
		} else if(strcmp("--disable-aa", argv[i]) == 0) {
			disable_aa = 1;
		}
		i++;
	}
}

int main(int argc, char **argv) {
	w = 1800;
	h = 1000;
	loglevel = WARN;

	parseArguments(argc, argv);

	float wh_ratio = (float)w / (float)h;
	float coord_height = 4.0 / wh_ratio;
	rect = {-2.5, -coord_height / 2, 4.0, coord_height};

	mutex = SDL_CreateMutex();
	if(mutex == NULL) {
		log(ERROR, "Could not create mutex!\n");
		exit(EXIT_FAILURE);
	}

	SDL_Thread *renderThread = SDL_CreateThread(renderLoop,
			"RenderThread", NULL);
	if(renderThread == NULL) {
		log(ERROR, "Could not create Render Thread!\n");
		exit(EXIT_FAILURE);
	}
	eventLoop();

	quit = 1;
	SDL_WaitThread(renderThread, NULL);
	SDL_DestroyMutex(mutex);
	return 0;
}
