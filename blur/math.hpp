#pragma once

#include <cstdint>

class Blur;

void cpu_apply(const uint8_t* input, uint8_t* output, int width, int height, float radius, int downscale, int type);
void gpu_ensure(Blur& t);
void gpu_apply(Blur& t);
void gpu_release(Blur& t);
