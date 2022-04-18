# This is just a workaround to not need a proper configure
# We just read the value from the config file directly
ENABLE_CUDA:=$(shell grep "ENABLE_CUDA" src/config.h | cut -d " " -f 3)
ENABLE_AVX2:=$(shell grep "ENABLE_AVX" src/config.h | cut -d " " -f 3)

NVCC=nvcc
CC=gcc
# Link with g++ or gcc -lstdc++
LINKER=g++

TARGET=mandelbrot

SOURCE_DIR=src
OBJECT_DIR=obj

NVCFLAGS=-O3

CFLAGS=-Wall -Wextra -O3 -I/usr/include/SDL2

MKDIR=mkdir
CHMOD=chmod
RM=rm

TARGET_DEPS=application.o render.o mandelbrot_cpu.o mandelbrot_common.o logger.o util.o

ifeq "$(ENABLE_AVX2)" "1"
	TARGET_DEPS+=mandelbrot_cpu_intrin.o
endif

LDFLAGS=-lSDL2 -lm
ifeq "$(ENABLE_CUDA)" "1"
	LDFLAGS+=-lcudart
	TARGET_DEPS+=mandelbrot_cuda.o
endif

# -------------------------------------------------------------
# Rule definitions
.PHONY: all clean
all: pre-build main-build post-build
pre-build:
	$(MKDIR) -p $(OBJECT_DIR)
main-build: $(TARGET)
post-build: main-build
	$(CHMOD) +x $(TARGET)


$(TARGET): $(TARGET_DEPS)
	$(LINKER) -o $(TARGET) $(patsubst %, $(OBJECT_DIR)/%, $(TARGET_DEPS)) $(LDFLAGS)


# -------------------------------------------------------------
# Rules for compiling dependencies
# For now every file is added manually here
application.o:
	$(CC) -c $(SOURCE_DIR)/application.c -o $(OBJECT_DIR)/application.o $(CFLAGS)

render.o:
	$(CC) -c $(SOURCE_DIR)/render.c -o $(OBJECT_DIR)/render.o $(CFLAGS)

mandelbrot_cuda.o:
	$(NVCC) -c $(SOURCE_DIR)/mandelbrot_cuda.cu -o $(OBJECT_DIR)/mandelbrot_cuda.o $(NVCFLAGS)

mandelbrot_cpu_intrin.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_cpu_intrin.c -o $(OBJECT_DIR)/mandelbrot_cpu_intrin.o $(CFLAGS) -mavx -mavx2

mandelbrot_cpu.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_cpu.c -o $(OBJECT_DIR)/mandelbrot_cpu.o $(CFLAGS)

logger.o:
	$(CC) -c $(SOURCE_DIR)/logger.c -o $(OBJECT_DIR)/logger.o $(CFLAGS)

util.o:
	$(CC) -c $(SOURCE_DIR)/util.c -o $(OBJECT_DIR)/util.o $(CFLAGS)

mandelbrot_common.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_common.c -o $(OBJECT_DIR)/mandelbrot_common.o $(CFLAGS)


# -------------------------------------------------------------
# Cleaning rule to get rid of build files (FIXME rm throws a warning if file doesn't exist)
clean:
	$(RM) -r $(OBJECT_DIR)
	$(RM) $(TARGET)
	$(RM) *.bmp
