# A simple mandelbrot renderer

This project's intention is for me to see the performance difference of a CPU and GPU and to just play around with SDL and the cuda toolkit a bit.

# How to build
These build instructions are only for Ubuntu Linux, as that is what I use primarily.
```
apt install libsdl2-dev nvidia-cuda-toolkit
git clone https://github.com/NiceWaffel/mandelbrot.git
cd mandelbrot
make
```
See https://en.wikipedia.org/wiki/Mandelbrot_set for more information about the Mandelbrot set.
