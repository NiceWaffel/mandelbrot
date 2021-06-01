NVCC=nvcc
CC=gcc

TARGET=app

SOURCE_DIR=src
OBJECT_DIR=obj

NVCFLAGS=
CFLAGS=-Wall -Wextra -O3

NVLFLAGS=-I/usr/include/SDL2 -Isrc -lSDL2
LFLAGS=-I/usr/include/SDL2 -Isrc -lSDL2 -lm

MKDIR=mkdir
RM=rm

.PHONY: all clean

all: pre-build main-build

pre-build:
	$(MKDIR) -p $(OBJECT_DIR)

main-build: $(TARGET)

$(TARGET): render.o mandelbrot_cuda.o mandelbrot_cpu.o mandelbrot_common.o logger.o util.o
	$(NVCC) $(SOURCE_DIR)/application.cpp -o $(TARGET) $(NVLFLAGS) $(NVCFLAGS) \
		$(OBJECT_DIR)/render.o $(OBJECT_DIR)/mandelbrot_cuda.o $(OBJECT_DIR)/mandelbrot_cpu.o \
		$(OBJECT_DIR)/mandelbrot_common.o $(OBJECT_DIR)/logger.o $(OBJECT_DIR)/util.o

render.o:
	$(CC) -c $(SOURCE_DIR)/render.cpp -o $(OBJECT_DIR)/render.o $(LFLAGS) $(CFLAGS)

mandelbrot_cuda.o:
	$(NVCC) -c $(SOURCE_DIR)/mandelbrot_cuda.cu -o $(OBJECT_DIR)/mandelbrot_cuda.o $(NVCFLAGS)

mandelbrot_cpu.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_cpu.cpp -o $(OBJECT_DIR)/mandelbrot_cpu.o $(LFLAGS) $(CFLAGS)

logger.o:
	$(CC) -c $(SOURCE_DIR)/logger.cpp -o $(OBJECT_DIR)/logger.o $(LFLAGS) $(CFLAGS)

util.o:
	$(CC) -c $(SOURCE_DIR)/util.cpp -o $(OBJECT_DIR)/util.o $(LFLAGS) $(CFLAGS)

mandelbrot_common.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_common.cpp -o $(OBJECT_DIR)/mandelbrot_common.o $(LFLAGS) $(CFLAGS)

clean:
	$(RM) -r $(OBJECT_DIR)
	$(RM) $(TARGET)
	$(RM) *.bmp
