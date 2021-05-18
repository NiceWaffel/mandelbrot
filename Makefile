CC=nvcc
CFLAGS=
LFLAGS=-I/usr/include/SDL2 -lSDL2

RM=rm

app: render.o mandelbrot.o logger.o
	$(CC) application.cpp -o app $(LFLAGS) $(CFLAGS) render.o mandelbrot.o logger.o

render.o:
	$(CC) -c render.cpp -o render.o $(LFLAGS) $(CFLAGS)

mandelbrot.o:
	$(CC) -c mandelbrot.cu -o mandelbrot.o $(CFLAGS)

logger.o:
	$(CC) -c logger.cpp -o logger.o $(LFLAGS) $(CFLAGS)

clean:
	$(RM) *.o
	$(RM) app
	$(RM) *.bmp
