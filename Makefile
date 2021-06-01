NVCC=nvcc
CC=gcc

NVCFLAGS=
CFLAGS=-Wall -Wextra -O3

NVLFLAGS=-I/usr/include/SDL2 -lSDL2
LFLAGS=-I/usr/include/SDL2 -lSDL2 -lm

RM=rm

app: render.o mandelbrot_cuda.o mandelbrot_cpu.o mandelbrot_common.o logger.o util.o
	$(NVCC) application.cpp -o app $(NVLFLAGS) $(NVCFLAGS) render.o mandelbrot_cuda.o mandelbrot_cpu.o mandelbrot_common.o logger.o util.o

render.o:
	$(CC) -c render.cpp -o render.o $(LFLAGS) $(CFLAGS)

mandelbrot_cuda.o:
	$(NVCC) -c mandelbrot_cuda.cu -o mandelbrot_cuda.o $(NVCFLAGS)

mandelbrot_cpu.o:
	$(CC) -c mandelbrot_cpu.cpp -o mandelbrot_cpu.o $(LFLAGS) $(CFLAGS)

logger.o:
	$(CC) -c logger.cpp -o logger.o $(LFLAGS) $(CFLAGS)

util.o:
	$(CC) -c util.cpp -o util.o $(LFLAGS) $(CFLAGS)

mandelbrot_common.o:
	$(CC) -c mandelbrot_common.cpp -o mandelbrot_common.o $(LFLAGS) $(CFLAGS)

clean:
	$(RM) *.o
	$(RM) app
	$(RM) *.bmp
