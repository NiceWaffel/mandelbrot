# This is just a workaround to not need a proper configure
# We just read the value from the config file directly
ENABLE_CUDA:=$(shell grep "ENABLE_CUDA" src/config.h | cut -d " " -f 3)

NVCC=nvcc
CC=gcc

ifeq "$(ENABLE_CUDA)" "1"
	# TODO if nvcc doesn't exist we print a warning an compile without cuda
	CPP=nvcc
else
	CPP=g++
endif

TARGET=app

SOURCE_DIR=src
OBJECT_DIR=obj

NVCFLAGS=
CFLAGS=-Wall -Wextra -O3

NVLFLAGS=-I/usr/include/SDL2 -Isrc -lSDL2
LFLAGS=-I/usr/include/SDL2 -Isrc -lSDL2 -lm

MKDIR=mkdir
RM=rm

TARGET_DEPS=render.o mandelbrot_cpu.o mandelbrot_common.o logger.o util.o

# If cuda is enabled we need to use nvcc to compile application.cpp
# to prevent a linker error with the cuda library
ifeq "$(ENABLE_CUDA)" "1"
	CPP=nvcc
	TARGET_DEPS+=mandelbrot_cuda.o
endif


# -------------------------------------------------------------
# Rule definitions
.PHONY: all clean
all: pre-build main-build
pre-build:
	$(MKDIR) -p $(OBJECT_DIR)
main-build: $(TARGET)


$(TARGET): $(TARGET_DEPS)
	$(CPP) $(SOURCE_DIR)/application.cpp -o $(TARGET) $(NVLFLAGS) $(NVCFLAGS) $(patsubst %, $(OBJECT_DIR)/%, $(TARGET_DEPS))


# -------------------------------------------------------------
# Rules for compiling dependencies
# For now every file is added manually here
render.o:
	$(CC) -c $(SOURCE_DIR)/render.c -o $(OBJECT_DIR)/render.o $(LFLAGS) $(CFLAGS)

ifeq "$(ENABLE_CUDA)" "1"
mandelbrot_cuda.o:
	$(NVCC) -c $(SOURCE_DIR)/mandelbrot_cuda.cu -o $(OBJECT_DIR)/mandelbrot_cuda.o $(NVCFLAGS)
endif

mandelbrot_cpu.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_cpu.c -o $(OBJECT_DIR)/mandelbrot_cpu.o $(LFLAGS) $(CFLAGS)

logger.o:
	$(CC) -c $(SOURCE_DIR)/logger.c -o $(OBJECT_DIR)/logger.o $(LFLAGS) $(CFLAGS)

util.o:
	$(CC) -c $(SOURCE_DIR)/util.c -o $(OBJECT_DIR)/util.o $(LFLAGS) $(CFLAGS)

mandelbrot_common.o:
	$(CC) -c $(SOURCE_DIR)/mandelbrot_common.c -o $(OBJECT_DIR)/mandelbrot_common.o $(LFLAGS) $(CFLAGS)


# -------------------------------------------------------------
# Cleaning rule to get rid of build files (FIXME rm throws a warning if file doesn't exist)
clean:
	$(RM) -r $(OBJECT_DIR)
	$(RM) $(TARGET)
	$(RM) *.bmp
