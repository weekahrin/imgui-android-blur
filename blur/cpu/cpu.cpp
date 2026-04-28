#include "../math.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cmath>

namespace {

constexpr int kChannels = 4;

static inline uint8_t ClampU8(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

static void DownsampleAverage(const uint8_t* input, int width, int height,
                              int smallW, int smallH, uint8_t* downsampled) {
    float xRatio = static_cast<float>(width) / static_cast<float>(smallW);
    float yRatio = static_cast<float>(height) / static_cast<float>(smallH);
    for (int y = 0; y < smallH; y++) {
        int srcYStart = static_cast<int>(y * yRatio);
        int srcYEnd = static_cast<int>((y + 1) * yRatio);
        if (y == smallH - 1) srcYEnd = height;
        for (int x = 0; x < smallW; x++) {
            int srcXStart = static_cast<int>(x * xRatio);
            int srcXEnd = static_cast<int>((x + 1) * xRatio);
            if (x == smallW - 1) srcXEnd = width;

            int sum[kChannels] = {0, 0, 0, 0};
            int count = 0;
            for (int sy = srcYStart; sy < srcYEnd; sy++) {
                for (int sx = srcXStart; sx < srcXEnd; sx++) {
                    int srcIdx = (sy * width + sx) * kChannels;
                    for (int c = 0; c < kChannels; ++c) {
                        sum[c] += input[srcIdx + c];
                    }
                    count++;
                }
            }

            int dstIdx = (y * smallW + x) * kChannels;
            if (count > 0) {
                for (int c = 0; c < kChannels; ++c) {
                    downsampled[dstIdx + c] = static_cast<uint8_t>(sum[c] / count);
                }
            } else {
                for (int c = 0; c < kChannels; ++c) {
                    downsampled[dstIdx + c] = 0;
                }
            }
        }
    }
}

static void UpsampleBilinear(const uint8_t* small, int smallW, int smallH,
                             uint8_t* out, int width, int height) {
    float upX = static_cast<float>(smallW) / static_cast<float>(width);
    float upY = static_cast<float>(smallH) / static_cast<float>(height);
    for (int y = 0; y < height; y++) {
        float srcY = y * upY;
        int y1 = static_cast<int>(srcY);
        int y2 = std::min(smallH - 1, y1 + 1);
        float dy = srcY - y1;
        float inv_dy = 1.0f - dy;

        for (int x = 0; x < width; x++) {
            float srcX = x * upX;
            int x1 = static_cast<int>(srcX);
            int x2 = std::min(smallW - 1, x1 + 1);
            float dx = srcX - x1;
            float inv_dx = 1.0f - dx;

            int idx11 = (y1 * smallW + x1) * kChannels;
            int idx12 = (y2 * smallW + x1) * kChannels;
            int idx21 = (y1 * smallW + x2) * kChannels;
            int idx22 = (y2 * smallW + x2) * kChannels;

            int dstIdx = (y * width + x) * kChannels;
            for (int c = 0; c < kChannels; c++) {
                float val1 = small[idx11 + c] * inv_dx + small[idx21 + c] * dx;
                float val2 = small[idx12 + c] * inv_dx + small[idx22 + c] * dx;
                out[dstIdx + c] = ClampU8(val1 * inv_dy + val2 * dy);
            }
        }
    }
}

static void ApplyBoxIntegral(const uint8_t* src, uint8_t* dst, int w, int h, int radius) {
    int sumStride = w + 1;
    size_t sumSize = static_cast<size_t>(w + 1) * static_cast<size_t>(h + 1) * kChannels;
    std::vector<uint32_t> sumBuffer(sumSize, 0u);

    for (int y = 1; y <= h; y++) {
        int srcRow = (y - 1) * w * kChannels;
        int sumRow = y * sumStride * kChannels;
        int sumRowPrev = (y - 1) * sumStride * kChannels;
        for (int x = 1; x <= w; x++) {
            int srcIdx = srcRow + (x - 1) * kChannels;
            int sumIdx = sumRow + x * kChannels;
            int sumLeft = sumRow + (x - 1) * kChannels;
            int sumUp = sumRowPrev + x * kChannels;
            int sumUpLeft = sumRowPrev + (x - 1) * kChannels;
            for (int c = 0; c < kChannels; ++c) {
                sumBuffer[sumIdx + c] = src[srcIdx + c] + sumBuffer[sumLeft + c] + sumBuffer[sumUp + c] - sumBuffer[sumUpLeft + c];
            }
        }
    }

    int r = std::max(1, radius);
    for (int y = 0; y < h; y++) {
        int y1 = std::max(0, y - r);
        int y2 = std::min(h - 1, y + r);
        int y1p = y1;
        int y2p = y2 + 1;

        for (int x = 0; x < w; x++) {
            int x1 = std::max(0, x - r);
            int x2 = std::min(w - 1, x + r);
            int x1p = x1;
            int x2p = x2 + 1;

            int idxA = (y1p * sumStride + x1p) * kChannels;
            int idxB = (y1p * sumStride + x2p) * kChannels;
            int idxC = (y2p * sumStride + x1p) * kChannels;
            int idxD = (y2p * sumStride + x2p) * kChannels;

            int area = (x2 - x1 + 1) * (y2 - y1 + 1);
            int dstIdx = (y * w + x) * kChannels;
            for (int c = 0; c < kChannels; ++c) {
                dst[dstIdx + c] = static_cast<uint8_t>((sumBuffer[idxD + c] - sumBuffer[idxB + c] - sumBuffer[idxC + c] + sumBuffer[idxA + c]) / area);
            }
        }
    }
}

static void BuildGaussianKernel(int radius, std::vector<float>& kernel) {
    int r = std::max(1, radius);
    float sigma = std::max(1.0f, r * 0.5f);
    kernel.resize(static_cast<size_t>(2 * r + 1));
    float sum = 0.0f;
    for (int i = -r; i <= r; ++i) {
        float w = std::exp(-(i * i) / (2.0f * sigma * sigma));
        kernel[static_cast<size_t>(i + r)] = w;
        sum += w;
    }
    if (sum > 0.0f) {
        for (float& w : kernel) {
            w /= sum;
        }
    }
}

static void ApplyGaussian(const uint8_t* src, uint8_t* tmp, uint8_t* dst, int w, int h, int radius) {
    std::vector<float> kernel;
    BuildGaussianKernel(radius, kernel);
    int r = std::max(1, radius);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float acc[kChannels] = {0, 0, 0, 0};
            for (int k = -r; k <= r; ++k) {
                int sx = std::clamp(x + k, 0, w - 1);
                int idx = (y * w + sx) * kChannels;
                float kw = kernel[static_cast<size_t>(k + r)];
                for (int c = 0; c < kChannels; ++c) {
                    acc[c] += src[idx + c] * kw;
                }
            }
            int outIdx = (y * w + x) * kChannels;
            for (int c = 0; c < kChannels; ++c) {
                tmp[outIdx + c] = ClampU8(acc[c]);
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float acc[kChannels] = {0, 0, 0, 0};
            for (int k = -r; k <= r; ++k) {
                int sy = std::clamp(y + k, 0, h - 1);
                int idx = (sy * w + x) * kChannels;
                float kw = kernel[static_cast<size_t>(k + r)];
                for (int c = 0; c < kChannels; ++c) {
                    acc[c] += tmp[idx + c] * kw;
                }
            }
            int outIdx = (y * w + x) * kChannels;
            for (int c = 0; c < kChannels; ++c) {
                dst[outIdx + c] = ClampU8(acc[c]);
            }
        }
    }
}

static uint32_t Hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void AddFrostedNoise(uint8_t* buf, int w, int h, int amount) {
    int noise = std::max(1, amount);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint32_t h1 = Hash32(static_cast<uint32_t>(x * 73856093u) ^ static_cast<uint32_t>(y * 19349663u));
            int jitter = static_cast<int>(h1 % static_cast<uint32_t>(2 * noise + 1)) - noise;
            int idx = (y * w + x) * kChannels;
            for (int c = 0; c < 3; ++c) {
                int v = static_cast<int>(buf[idx + c]) + jitter;
                buf[idx + c] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            }
        }
    }
}

static void ApplyKawaseCPU(const uint8_t* src, uint8_t* tmp, uint8_t* dst, int w, int h, int radius) {
    std::copy(src, src + (w * h * kChannels), tmp);
    int passes = std::max(2, std::min(8, radius / 3 + 1));
    for (int p = 0; p < passes; ++p) {
        float t = (passes > 1) ? (static_cast<float>(p) / static_cast<float>(passes - 1)) : 0.0f;
        int off = std::max(1, static_cast<int>(1.0f + t * std::max(1, radius)));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int x1 = std::clamp(x - off, 0, w - 1);
                int x2 = std::clamp(x + off, 0, w - 1);
                int y1 = std::clamp(y - off, 0, h - 1);
                int y2 = std::clamp(y + off, 0, h - 1);

                int i1 = (y1 * w + x1) * kChannels;
                int i2 = (y1 * w + x2) * kChannels;
                int i3 = (y2 * w + x1) * kChannels;
                int i4 = (y2 * w + x2) * kChannels;
                int o = (y * w + x) * kChannels;
                for (int c = 0; c < kChannels; ++c) {
                    int v = (tmp[i1 + c] + tmp[i2 + c] + tmp[i3 + c] + tmp[i4 + c]) / 4;
                    dst[o + c] = static_cast<uint8_t>((v * 3 + tmp[o + c]) / 4);
                }
            }
        }
        std::swap(tmp, dst);
    }
    std::copy(tmp, tmp + (w * h * kChannels), dst);
}

static void ApplyMotionCPU(const uint8_t* src, uint8_t* dst, int w, int h, int radius) {
    int r = std::max(1, radius);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float acc[kChannels] = {0, 0, 0, 0};
            float wsum = 0.0f;
            for (int k = -r; k <= r; ++k) {
                int sx = std::clamp(x + k, 0, w - 1);
                int sy = std::clamp(y + k / 3, 0, h - 1);
                float kw = 1.0f - (std::abs(static_cast<float>(k)) / static_cast<float>(r + 1));
                int idx = (sy * w + sx) * kChannels;
                for (int c = 0; c < kChannels; ++c) {
                    acc[c] += src[idx + c] * kw;
                }
                wsum += kw;
            }
            int o = (y * w + x) * kChannels;
            for (int c = 0; c < kChannels; ++c) {
                dst[o + c] = ClampU8((wsum > 0.0f) ? (acc[c] / wsum) : src[o + c]);
            }
        }
    }
}

static void ApplyZoomCPU(const uint8_t* src, uint8_t* dst, int w, int h, int radius) {
    float cx = (w - 1) * 0.5f;
    float cy = (h - 1) * 0.5f;
    float strength = std::max(0.5f, radius * 0.08f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = cx - static_cast<float>(x);
            float dy = cy - static_cast<float>(y);
            int sx1 = std::clamp(static_cast<int>(x + dx * 0.04f * strength), 0, w - 1);
            int sy1 = std::clamp(static_cast<int>(y + dy * 0.04f * strength), 0, h - 1);
            int sx2 = std::clamp(static_cast<int>(x - dx * 0.04f * strength), 0, w - 1);
            int sy2 = std::clamp(static_cast<int>(y - dy * 0.04f * strength), 0, h - 1);
            int o = (y * w + x) * kChannels;
            int i0 = o;
            int i1 = (sy1 * w + sx1) * kChannels;
            int i2 = (sy2 * w + sx2) * kChannels;
            for (int c = 0; c < kChannels; ++c) {
                dst[o + c] = static_cast<uint8_t>((src[i0 + c] * 2 + src[i1 + c] + src[i2 + c]) / 4);
            }
        }
    }
}

static void ApplyPixelateCPU(const uint8_t* src, uint8_t* dst, int w, int h, int radius) {
    int block = std::max(2, radius / 2 + 1);
    for (int by = 0; by < h; by += block) {
        for (int bx = 0; bx < w; bx += block) {
            int ex = std::min(w, bx + block);
            int ey = std::min(h, by + block);
            int sum[kChannels] = {0, 0, 0, 0};
            int cnt = 0;
            for (int y = by; y < ey; ++y) {
                for (int x = bx; x < ex; ++x) {
                    int i = (y * w + x) * kChannels;
                    for (int c = 0; c < kChannels; ++c) sum[c] += src[i + c];
                    cnt++;
                }
            }
            uint8_t avg[kChannels];
            for (int c = 0; c < kChannels; ++c) avg[c] = static_cast<uint8_t>(sum[c] / std::max(1, cnt));
            for (int y = by; y < ey; ++y) {
                for (int x = bx; x < ex; ++x) {
                    int i = (y * w + x) * kChannels;
                    for (int c = 0; c < kChannels; ++c) dst[i + c] = avg[c];
                }
            }
        }
    }
}

} // namespace

void cpu_apply(const uint8_t* input, uint8_t* output, int width, int height, float radius, int downscale, int type) {
    if (!input || !output || width <= 0 || height <= 0) {
        return;
    }

    int intRadius = std::max(1, static_cast<int>(radius));
    int ds = std::max(1, downscale);
    int smallW = std::max(1, width / ds);
    int smallH = std::max(1, height / ds);

    size_t smallSize = static_cast<size_t>(smallW) * static_cast<size_t>(smallH) * kChannels;
    std::vector<uint8_t> downsampled(smallSize);
    std::vector<uint8_t> blurA(smallSize);
    std::vector<uint8_t> blurB(smallSize);

    DownsampleAverage(input, width, height, smallW, smallH, downsampled.data());

    int smallRadius = std::max(1, intRadius / ds);
    switch (type) {
        case 1: // Gaussian
            ApplyGaussian(downsampled.data(), blurB.data(), blurA.data(), smallW, smallH, smallRadius);
            break;
        case 2: // Box
            ApplyBoxIntegral(downsampled.data(), blurA.data(), smallW, smallH, smallRadius);
            break;
        case 3: // Frosted
            ApplyGaussian(downsampled.data(), blurB.data(), blurA.data(), smallW, smallH, smallRadius);
            AddFrostedNoise(blurA.data(), smallW, smallH, std::max(2, smallRadius));
            break;
        case 4: // Kawase
            ApplyKawaseCPU(downsampled.data(), blurA.data(), blurB.data(), smallW, smallH, smallRadius);
            std::copy(blurB.begin(), blurB.end(), blurA.begin());
            break;
        case 5: // Bokeh
            ApplyGaussian(downsampled.data(), blurB.data(), blurA.data(), smallW, smallH, std::max(1, smallRadius));
            ApplyGaussian(blurA.data(), blurB.data(), blurA.data(), smallW, smallH, std::max(1, smallRadius / 2));
            break;
        case 6: // Motion
            ApplyMotionCPU(downsampled.data(), blurA.data(), smallW, smallH, std::max(1, smallRadius));
            break;
        case 7: // Zoom
            ApplyZoomCPU(downsampled.data(), blurA.data(), smallW, smallH, std::max(1, smallRadius));
            break;
        case 8: // Pixelate
            ApplyPixelateCPU(downsampled.data(), blurA.data(), smallW, smallH, std::max(1, smallRadius));
            break;
        case 0: // Default
        default:
            ApplyGaussian(downsampled.data(), blurB.data(), blurA.data(), smallW, smallH, std::max(1, smallRadius));
            break;
    }

    UpsampleBilinear(blurA.data(), smallW, smallH, output, width, height);

    for (int i = 0; i < width * height; ++i) {
        output[i * kChannels + 3] = 255;
    }
}
