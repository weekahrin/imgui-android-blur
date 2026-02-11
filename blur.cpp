#include "blur.hpp"
#include "blur/math.hpp"
#include <cstddef>

Blur::Blur(Hardware backend_in) : backend(backend_in) {}
Blur::~Blur() { release(); }

void Blur::process(int x, int y, int w, int h, float radius, int downscale) {
    x_ = x;
    y_ = y;
    w_ = w;
    h_ = h;
    radius_ = radius;
    downscale_ = downscale;
    apply();
}

void Blur::process(const Rect& r) {
    process(r.x, r.y, r.w, r.h, r.radius, r.downscale);
}

#if defined(IMGUI_VERSION) && !defined(BLUR_NO_IMGUI)
void Blur::process(ImDrawList* draw, float corner_radius, float radius, int downscale, Hardware hw) {
    if (!draw) {
        return;
    }
    backend = hw;
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    process(static_cast<int>(pos.x), static_cast<int>(pos.y),
            static_cast<int>(size.x), static_cast<int>(size.y),
            radius, downscale);
    draw->AddImageRounded(
        (void*)(intptr_t)tex,
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f),
        IM_COL32(255, 255, 255, 255),
        corner_radius,
        ImDrawFlags_RoundCornersAll
    );
}

void Blur::process(const ImRect& rect, float radius, int downscale) {
    process(static_cast<int>(rect.Min.x), static_cast<int>(rect.Min.y),
            static_cast<int>(rect.Max.x - rect.Min.x),
            static_cast<int>(rect.Max.y - rect.Min.y),
            radius, downscale);
}

void Blur::process(const ImVec2& pos, const ImVec2& size, float radius, int downscale) {
    process(static_cast<int>(pos.x), static_cast<int>(pos.y),
            static_cast<int>(size.x), static_cast<int>(size.y),
            radius, downscale);
}
#endif

void Blur::release() {
    gpu_release(*this);
    if (tex) {
        glDeleteTextures(1, &tex);
        tex = 0;
    }
    if (cpu_input_) {
        delete[] cpu_input_;
        cpu_input_ = nullptr;
    }
    if (cpu_output_) {
        delete[] cpu_output_;
        cpu_output_ = nullptr;
    }
    cpu_size_ = 0;
    cpu_ready_ = false;
}

void Blur::reset() {
    release();
    x_ = 0;
    y_ = 0;
    w_ = 0;
    h_ = 0;
    width_ = 0;
    height_ = 0;
    blur_w_ = 0;
    blur_h_ = 0;
}

void Blur::ensure() {
    if (backend == Hardware::GPU) {
        gpu_ensure(*this);
    }
    if (backend == Hardware::CPU || !cpu_ready_) {
        if (tex == 0) {
            glGenTextures(1, &tex);
        }
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (width_ > 0 && height_ > 0) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        cpu_ready_ = true;
    }
}

void Blur::apply() {
    if (w_ <= 0 || h_ <= 0) {
        return;
    }
    if (width_ != w_ || height_ != h_) {
        width_ = w_;
        height_ = h_;
    }
    ensure();

    if (backend == Hardware::CPU) {
        if (!glReadPixels || !glBindTexture || !glTexSubImage2D) {
            return;
        }
        size_t required = static_cast<size_t>(w_) * static_cast<size_t>(h_) * 4u;
        if (cpu_size_ != required) {
            delete[] cpu_input_;
            delete[] cpu_output_;
            cpu_input_ = new uint8_t[required];
            cpu_output_ = new uint8_t[required];
            cpu_size_ = required;
        }
        glReadPixels(x_, y_, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, cpu_input_);
        cpu_apply(cpu_input_, cpu_output_, w_, h_, radius_, downscale_, static_cast<int>(type));
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, cpu_output_);
        return;
    }

    gpu_apply(*this);
}
