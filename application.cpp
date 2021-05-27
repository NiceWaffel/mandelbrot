#include "render.h" //Includes SDL
#include "mandelbrot.h"
#include "mandelbrot_cpu.h"
#include "logger.h"
#include "config.h"

static Renderer renderer;
static Rectangle rect;
static int refresh;
static int quit = 0;
static int w, h;
static int disable_aa = 0;

static int use_cpu = 0;

int loglevel;

static SDL_mutex *mutex;

int renderLoop(void *ptr) {
	clock_t time;

	SDL_LockMutex(mutex);
	log(VERBOSE, "Starting application in resolution %dx%d\n", w, h);
	time = clock();
	renderer = createRenderer(w, h);
	log(DEBUG, "Creating Renderer took %ld ticks\n", clock() - time);

	if(!use_cpu) {
		if(mandelbrotInit(w, h)) {
			log(WARN, "Could not initialize Cuda Mandelbrot Engine!\n");
			log(WARN, "Falling back to slower CPU implementation!\n");
			use_cpu = 1;
		} else {
			log(INFO, "Cuda Mandelbrot Engine successfully initialized\n");
		}
	}
	if(use_cpu) {
		log(INFO, "Using CPU Rendering. This will impact performance.\n");
		mandelbrotCpuInit(w, h);
	}
	int *rgb_data = (int *) malloc(w * h * sizeof(int));
	if(rgb_data == NULL) {
		log(ERROR, "Could not allocate memory for RGB-data!\n");
		exit(EXIT_FAILURE);
	}
	SDL_UnlockMutex(mutex);

	Rectangle rect_cache;
	int aa_counter = 0;
	while(!quit) {
		if(refresh || memcmp(&rect_cache, &rect, sizeof(Rectangle))) {
			refresh = 0;
			log(DEBUG, "Rectangle changed to {%f, %f, %f, %f}\n",
					rect.x, rect.y, rect.w, rect.h);
			aa_counter = 0; // Reset Antialias
			rect_cache = rect;
			time = clock();
			if(!use_cpu) {
				generateImage(rect, rgb_data);
			} else {
				generateImageCpu(rect, rgb_data);
			}
			log(DEBUG, "Image generation took %6ld ticks\n", clock() - time);
			renderImage(renderer, renderer.width, renderer.height, rgb_data);
		} else if(!disable_aa && aa_counter < 4) {
			log(DEBUG, "Applying Antialias %d\n", aa_counter);

			// aa_counter starts at 0 and ends at 3
			if(use_cpu)
				doAntiAliasCpu(rect, rgb_data, aa_counter);
			else
				doAntiAlias(rect, rgb_data, aa_counter);
			renderImage(renderer, renderer.width, renderer.height, rgb_data);
			aa_counter++;
		} else {
			SDL_Delay(20);
		}
	}

	mandelbrotCleanup();
	log(DEBUG, "Destroying Renderer\n");
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
			int iter_diff;
			int *data;
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
					data = (int *) malloc(3840 * 2160 * sizeof(int));
					generateImage2(3840, 2160, rect, data);
					writeToBmp("output.bmp", 3840, 2160, data);
					free(data);
					break;
				case SDLK_i:
					// TODO we need some logging here
					// Maybe save in another variable and give as parameter to mandelbrot function
					iter_diff = ev.key.keysym.mod & KMOD_SHIFT ? 10 : 1;
					iter_diff *= ev.key.keysym.mod & KMOD_CTRL ? 100 : 1;
					if(use_cpu)
						changeIterationsCpu(iter_diff);
					else
						changeIterations(iter_diff);
					refresh = 1;
					break;
				case SDLK_k:
					iter_diff = ev.key.keysym.mod & KMOD_SHIFT ? 10 : 1;
					iter_diff *= ev.key.keysym.mod & KMOD_CTRL ? 100 : 1;
					if(use_cpu)
						changeIterationsCpu(-iter_diff);
					else
						changeIterations(-iter_diff);
					refresh = 1;
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

void print_help() {
	printf("Usage: mandelbrot [options]\n"
	       "Options:\n"
	       "  --help        Show this help page\n"
	       "  -w WIDTH      The width of the preview window\n"
	       "  -h HEIGHT     The height of the preview window\n"
	       "  -v            Increase verbosity level to INFO\n"
	       "  -vv           Increase verbosity level to DEBUG\n"
	       "  --disable-aa  Disable anti-aliasing in the preview\n"
	       "  --force-cpu   Force usage of CPU rendering, even if GPU is available\n"
	       "\n"
	       "Bindings:\n"
	       " Q               Quit the program\n"
	       " Arrow Keys      Move the camera\n"
	       " Page Up/Down\n"
	       " Scroll wheel    Zoom in/out respectivly\n"
	       " S               Make a screenshot and save as output.bmp in\n"
	       "                 current working directory\n"
	       " I / K           Increase / Decrease maximum iterations\n"
	       "                 Hold shift for a step size of 10\n"
	       "                 Hold ctrl for a step size of 100\n"
	       "                 Hold ctrl and shift for a step size of 1000\n");
}

void parse_arguments(int argc, char **argv) {
	int i = 1;
	while(i < argc) {
		if(strcmp("--help", argv[i]) == 0) {
			print_help();
			exit(EXIT_SUCCESS);
		} else if(strcmp("-w", argv[i]) == 0) { // Set window width
			i++;
			if(i < argc)
				w = atoi(argv[i]);
			if(w <= 0 || w > 16383)
				w = DEFAULT_WIDTH;
		} else if(strcmp("-h", argv[i]) == 0) { // Set window height
			i++;
			if(i < argc)
				h = atoi(argv[i]);
			if(h <= 0 || h > 16383)
				h = DEFAULT_HEIGHT;
		} else if(strcmp("-v", argv[i]) == 0) {
			loglevel = VERBOSE;
			log(INFO, "Loglevel is set to VERBOSE\n");
		} else if(strcmp("-vv", argv[i]) == 0) {
			loglevel = DEBUG;
			log(INFO, "Loglevel is set to DEBUG\n");
		} else if(strcmp("--no-aa", argv[i]) == 0) {
			disable_aa = 1;
			log(INFO, "Disabled Anti-Alias\n");
		} else if(strcmp("--force-cpu", argv[i]) == 0) {
			use_cpu = 1;
		}
		i++;
	}
}

int main(int argc, char **argv) {
	w = DEFAULT_WIDTH;
	h = DEFAULT_HEIGHT;
	loglevel = INFO;

	parse_arguments(argc, argv);

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
