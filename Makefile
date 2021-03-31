CC=nvcc
CFLAGS=
LFLAGS=-I/usr/include/SDL2 -lSDL2

app: render.o mandelbrot.o
	$(CC) application.cpp -o app $(LFLAGS) $(CFLAGS) render.o mandelbrot.o

render.o:
	$(CC) -c render.cpp -o render.o $(LFLAGS) $(CFLAGS)

mandelbrot.o:
	$(CC) -c mandelbrot.cu -o mandelbrot.o $(CFLAGS)

