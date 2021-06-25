#include "config.h"
#include "logger.h"
#include "render.h" //Includes SDL
#include "mandelbrot_cpu.h"

#if ENABLE_CUDA
#include "mandelbrot_cuda.h"
#endif

typedef enum {
	ENGINE_TYPE_CPU,
	ENGINE_TYPE_CUDA
} EngineType;

typedef struct Engine {
	EngineType type;
	void (*genImage)(Rectangle coord_rect, int *out_argb);
	void (*genImageWH)(int w, int h, Rectangle coord_rect, int *out_argb);
	void (*doAA)(Rectangle coord_rect, int *out_argb, int aa_counter);
	void (*changeIters)(int diff);
	void (*changeExponent)(int newExp);
	int (*resizeFramebuffer)(int new_w, int new_h);
} Engine;

static Renderer renderer;
static Engine engine;
static Rectangle rect;
static int engine_initialized = 0;
static int force_refresh;
static int quit = 0;

static int w, h;
static int disable_aa = 0;
static int force_cpu = 0;
static const char *screenshot_dir = ".";

static SDL_mutex *mutex;

int *framebuffer;

void init_engine() {
	clock_t time;
	mandelLog(VERBOSE, "Starting application in resolution %dx%d\n", w, h);
	time = clock();
	renderer = createRenderer(w, h);
	mandelLog(DEBUG, "Creating Renderer took %ld ticks\n", clock() - time);

#if ENABLE_CUDA
	// Init pixel data generating engine
	if(!force_cpu) {
		if(mandelbrotCudaInit(w, h)) { // Returns nonzero status on error
			mandelLog(WARN, "Could not initialize Cuda Mandelbrot Engine!\n");
			mandelLog(WARN, "Falling back to slower CPU implementation!\n");
			force_cpu = 1;
		} else {
			engine.type = ENGINE_TYPE_CUDA;
			engine.genImage = &generateImageCuda;
			engine.genImageWH = &generateImageCudaWH;
			engine.doAA = &doAntiAliasCuda;
			engine.resizeFramebuffer = &resizeFramebufferCuda;
			engine.changeIters = &changeIterationsCuda;
			engine.changeExponent = &changeExponentCuda;
			mandelLog(INFO, "Cuda Mandelbrot Engine successfully initialized\n");
		}
	}
	if(force_cpu) {
#endif
		mandelLog(INFO, "Using CPU Rendering. This will impact performance.\n");
		if(mandelbrotCpuInit(w, h)) { // Returns nonzero status on error
			mandelLog(ERROR, "Could not initialize Cpu Mandelbrot Engine!\n");
			exit(EXIT_FAILURE);
		}
		engine.type = ENGINE_TYPE_CPU;
		engine.genImage = &generateImageCpu;
		engine.genImageWH = &generateImageCpuWH;
		engine.doAA = &doAntiAliasCpu;
		engine.resizeFramebuffer = &resizeFramebufferCpu;
		engine.changeIters = &changeIterationsCpu;
		engine.changeExponent = &changeExponentCpu;
#if ENABLE_CUDA
	}
#endif
}

void alloc_framebuffer() {
	framebuffer = (int *) malloc(w * h * sizeof(int));
	if(framebuffer == NULL) {
		mandelLog(ERROR, "Could not allocate memory for Framebuffer!\n");
		exit(EXIT_FAILURE);
	}
}

void realloc_framebuffer(int f_w, int f_h) {
	framebuffer = (int *) realloc(framebuffer, f_w * f_h * sizeof(int));
	if(framebuffer == NULL) {
		mandelLog(ERROR, "Could not allocate memory for Framebuffer!\n");
		exit(EXIT_FAILURE);
	}
}

int renderLoop() {
	init_engine();
	alloc_framebuffer();

	engine_initialized = 1;

	// aa_counter starts at 0 and ends at 3
	int aa_counter = 0;
	Rectangle rect_cache;
	clock_t time;

	// stores the size of the framebuffer
	// when the window gets resized we finish rendering our image with the old framebuffer size
	int f_w = w;
	int f_h = h;

	while(!quit) {
		if(f_w != w || f_h != h) {
			f_w = w;
			f_h = h;
			realloc_framebuffer(f_w, f_h);
			if(engine.resizeFramebuffer(f_w, f_h) == -1) {
				mandelLog(ERROR, "Could not allocate memory for Engine Framebuffer!\n");
				exit(EXIT_FAILURE);
			}
		}
		if(force_refresh || memcmp(&rect_cache, &rect, sizeof(Rectangle))) {
			SDL_LockMutex(mutex);
			rect_cache = rect;
			SDL_UnlockMutex(mutex);

			force_refresh = 0;
			mandelLog(DEBUG, "Rectangle changed to {%f, %f, %f, %f}\n",
					rect.x, rect.y, rect.w, rect.h);
			aa_counter = 0; // Reset Antialias

			time = clock();
			engine.genImage(rect_cache, framebuffer);
			mandelLog(DEBUG, "Image generation took %6ld ticks\n", clock() - time);

			renderImage(renderer, f_w, f_h, framebuffer);
		} else if(!disable_aa && aa_counter < MAX_AA_COUNTER) {
			mandelLog(DEBUG, "Applying Antialias %d\n", aa_counter);

			engine.doAA(rect_cache, framebuffer, aa_counter);

			renderImage(renderer, f_w, f_h, framebuffer);
			aa_counter++;
		} else {
			// Check again in 30 milliseconds if there is something to render/update
			// 30 milliseconds is easily responsive enough and doesn't result in huge idle load
			SDL_Delay(30);
		}
	}

	return 0;
}

void make_screenshot() {
	// dirname + path seperator + filename + null terminator
	int pathlen = strlen(screenshot_dir) + 1 + strlen("output.bmp") + 1;
	char *path;
	path = (char *)malloc(pathlen * sizeof(char));
	snprintf(path, pathlen, "%s/%s", screenshot_dir, "output.bmp");

	SDL_LockMutex(mutex);
	int my_w = w;
	int my_h = h;
	Rectangle my_rect = rect;
	SDL_UnlockMutex(mutex);

	int *data = (int *) malloc(my_w * my_h * sizeof(int));
	engine.genImageWH(my_w, my_h, my_rect, data);
	writeToBmp(path, my_w, my_h, data);
	free(data);
}

void eventLoop() {
	while(!engine_initialized && !quit) {
		SDL_Delay(10);
	}
	mandelLog(DEBUG, "Starting Event Loop\n");

	int mouse_state = SDL_RELEASED;
	SDL_Event ev;
	while(SDL_WaitEvent(&ev)) {

		if(ev.type == SDL_MOUSEWHEEL) {
			int mx, my;
			SDL_GetMouseState(&mx, &my);
			float x_skew = (float)mx / (float)w;
			float y_skew = (float)my / (float)h;
			SDL_LockMutex(mutex);
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
			SDL_UnlockMutex(mutex);
		} else if(ev.type == SDL_KEYDOWN) {
			int iter_diff;
			SDL_LockMutex(mutex);
			switch(ev.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
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
					// zoom in towards center
					rect.x += 0.1 * rect.w;
					rect.y += 0.1 * rect.h;
					rect.w = 0.8 * rect.w;
					rect.h = 0.8 * rect.h;
					break;
				case SDLK_PAGEDOWN:
					// zoom out from center
					rect.x -= 0.125 * rect.w;
					rect.y -= 0.125 * rect.h;
					rect.w = 1.25 * rect.w;
					rect.h = 1.25 * rect.h;
					break;
				case SDLK_s: //screenshot
					SDL_UnlockMutex(mutex);
					make_screenshot();
					continue;
				case SDLK_i:
					iter_diff = ev.key.keysym.mod & KMOD_SHIFT ? 10 : 1;
					iter_diff *= ev.key.keysym.mod & KMOD_CTRL ? 100 : 1;
					engine.changeIters(iter_diff);
					force_refresh = 1;
					break;
				case SDLK_k:
					iter_diff = ev.key.keysym.mod & KMOD_SHIFT ? 10 : 1;
					iter_diff *= ev.key.keysym.mod & KMOD_CTRL ? 100 : 1;
					engine.changeIters(-iter_diff);
					force_refresh = 1;
					break;
				case SDLK_u:
					engine.changeExponent(1);
					force_refresh = 1;
					break;
				case SDLK_j:
					engine.changeExponent(-1);
					force_refresh = 1;
					break;
			}
			SDL_UnlockMutex(mutex);
		} else if(ev.type == SDL_MOUSEMOTION) {
			if(mouse_state == SDL_PRESSED) {
				SDL_LockMutex(mutex);
				rect.x -= ev.motion.xrel * rect.w / (float)w;
				rect.y -= ev.motion.yrel * rect.h / (float)h;
				SDL_UnlockMutex(mutex);
			} else {
				// We don't want any interaction when the mouse just moves over the window
				continue;
			}
		} else if(ev.type == SDL_MOUSEBUTTONDOWN ||
				ev.type == SDL_MOUSEBUTTONUP) {
			if(ev.button.button == SDL_BUTTON_LEFT)
				mouse_state = ev.button.state;
		} else if(ev.type == SDL_WINDOWEVENT) {
			float wh_ratio;
			float coord_height;
			SDL_LockMutex(mutex);
			switch (ev.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					// new width and height are stored in the event data
					w = ev.window.data1;
					h = ev.window.data2;
					mandelLog(DEBUG, "Window size changed to %dx%d\n", w, h);
					renderer.width = w;
					renderer.height = h;

					// Recalculate rect coordinates.
					// The width stays the same (thereby scaling the window in the horizontal axis scales the image).
					// The height is scaled down so that the rendered image gets cropped (rather than distorted).
					wh_ratio = (float)w / (float)h;
					coord_height = rect.w / wh_ratio;
					rect.y = rect.y + rect.h / 2.0 - coord_height / 2.0;
					rect.h = coord_height;

					force_refresh = 1;
					break;
				case SDL_WINDOWEVENT_CLOSE:
					SDL_UnlockMutex(mutex);
					return;
			}
			SDL_UnlockMutex(mutex);
		}
	}
}

void print_help() {
	printf("Usage: mandelbrot [options]\n"
	       "\n"
	       "Options:\n"
	       "  --help        Show this help page\n"
	       "  -w WIDTH      The width of the preview window\n"
	       "  -h HEIGHT     The height of the preview window\n"
	       "  -v            Increase verbosity level to VERBOSE\n"
	       "  -vv           Increase verbosity level to DEBUG\n"
	       "  --no-aa       Disable anti-aliasing in the preview\n"
	       "  --force-cpu   Force usage of CPU rendering,\n"
	       "                even if GPU is available\n"
		   "  --screenshot-dir\n"
		   "                Change the directory where screenshots are stored\n"
	       "\n"
	       "Bindings:\n"
	       " q, ESC    Quit the program\n"
	       "\n"
	       " Arrow Keys, Mouse Dragging\n"
	       "           Move the camera\n"
	       "\n"
	       " Page Up/Down, Scroll wheel\n"
	       "           Zoom in/out respectivly\n"
	       "\n"
	       " s         Make a screenshot and save as output.bmp\n"
	       "           in current working directory\n"
	       "\n"
	       " i, k      Increase / Decrease maximum iterations\n"
	       "           Hold shift for a step size of 10\n"
	       "           Hold ctrl for a step size of 100\n"
	       "           Hold ctrl and shift for a step size of 1000\n");
}

void parse_arguments(int argc, char **argv) {
	int i = 1;
	while(i < argc) {
		if(strcmp("--help", argv[i]) == 0) {
			print_help();
			exit(EXIT_SUCCESS);
		} else if(strcmp("-w", argv[i]) == 0
				|| strcmp("--width", argv[i]) == 0) { // Set window width
			i++;
			if(i < argc)
				w = atoi(argv[i]);
			if(w <= 0 || w > 16383)
				w = DEFAULT_WIDTH;
		} else if(strcmp("-h", argv[i]) == 0
				|| strcmp("--height", argv[i]) == 0) { // Set window height
			i++;
			if(i < argc)
				h = atoi(argv[i]);
			if(h <= 0 || h > 16383)
				h = DEFAULT_HEIGHT;
		} else if(strcmp("-v", argv[i]) == 0) {
			setLogLevel(VERBOSE);
			mandelLog(INFO, "Loglevel is set to VERBOSE\n");
		} else if(strcmp("-vv", argv[i]) == 0) {
			setLogLevel(DEBUG);
			mandelLog(INFO, "Loglevel is set to DEBUG\n");
		} else if(strcmp("--no-aa", argv[i]) == 0) {
			disable_aa = 1;
			mandelLog(INFO, "Disabled Anti-Alias\n");
		} else if(strcmp("--force-cpu", argv[i]) == 0) {
			force_cpu = 1;
		} else if(strcmp("--screenshot-dir", argv[i]) == 0) {
			i++;
			screenshot_dir = argv[i];
		}
		i++;
	}
}

int main(int argc, char **argv) {
	w = DEFAULT_WIDTH;
	h = DEFAULT_HEIGHT;
	setLogLevel(INFO);

	parse_arguments(argc, argv);

	float wh_ratio = (float)w / (float)h;
	float coord_height = 4.0 / wh_ratio;
	rect = (Rectangle) {-2.5, -coord_height / 2, 4.0, coord_height};

	mutex = SDL_CreateMutex();
	if(!mutex) {
		mandelLog(ERROR, "Could not create Mutex!\n");
		exit(EXIT_FAILURE);
	}

	SDL_Thread *renderThread = SDL_CreateThread(renderLoop,
			"RenderThread", NULL);
	if(renderThread == NULL) {
		mandelLog(ERROR, "Could not create Render Thread!\n");
		exit(EXIT_FAILURE);
	}
	eventLoop();

	quit = 1;
	SDL_WaitThread(renderThread, NULL);

#if ENABLE_CUDA
	if(engine.type == ENGINE_TYPE_CUDA)
		mandelbrotCudaCleanup();
	else
#endif
		mandelbrotCpuCleanup();

	mandelLog(DEBUG, "Destroying Renderer\n");
	free(framebuffer);
	destroyRenderer(renderer);
	SDL_DestroyMutex(mutex);
	return 0;
}
