#include "demo_blur.h"
#include "imgui.h"
#include <algorithm>

void demo_blur(Blur* blur, int screen_w, int screen_h) {
    if (!blur || screen_w <= 0 || screen_h <= 0) {
        return;
    }

    struct BlurPanelSettings {
        bool enabled = true;
        int backend = static_cast<int>(Hardware::GPU);
        float radius = 12.0f;
        float corner = 14.0f;
        int downscale = 4;
        float alpha = 1.0f;
    };

    static const int kTypeCount = 9;
    static const char* kTypeNames[kTypeCount] = {
        "Default",
        "Gaussian",
        "Box",
        "Frosted",
        "Kawase",
        "Bokeh",
        "Motion",
        "Zoom",
        "Pixelate"
    };
    static BlurPanelSettings settings[kTypeCount];
    static Blur* blurs[kTypeCount] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    static float panel_w_ratio = 0.24f;
    static float panel_h_ratio = 0.18f;
    static float panel_padding = 14.0f;

    for (int i = 0; i < kTypeCount; ++i) {
        if (!blurs[i]) {
            if (i == 0) {
                blurs[i] = blur;
            } else {
                blurs[i] = new Blur(Hardware::GPU);
            }
        }
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("demo_blur")) {
        ImGui::TextUnformatted("Blur Settings");
        ImGui::Separator();
        ImGui::SliderFloat("Panel Width Ratio", &panel_w_ratio, 0.10f, 0.35f, "%.2f");
        ImGui::SliderFloat("Panel Height Ratio", &panel_h_ratio, 0.10f, 0.35f, "%.2f");
        ImGui::SliderFloat("Panel Padding", &panel_padding, 4.0f, 40.0f, "%.1f");

        for (int i = 0; i < kTypeCount; ++i) {
            ImGui::PushID(i);
            if (ImGui::CollapsingHeader(kTypeNames[i], ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enabled", &settings[i].enabled);
                ImGui::RadioButton("GPU", &settings[i].backend, static_cast<int>(Hardware::GPU));
                ImGui::SameLine();
                ImGui::RadioButton("CPU", &settings[i].backend, static_cast<int>(Hardware::CPU));
                ImGui::SliderFloat("Radius", &settings[i].radius, 1.0f, 40.0f, "%.1f");
                ImGui::SliderFloat("Corner", &settings[i].corner, 0.0f, 40.0f, "%.1f");
                ImGui::SliderInt("Downscale", &settings[i].downscale, 1, 8);
                ImGui::SliderFloat("Alpha", &settings[i].alpha, 0.0f, 1.0f, "%.2f");
            }
            ImGui::PopID();
        }
    }
    ImGui::End();

    int enabled_count = 0;
    for (int i = 0; i < kTypeCount; ++i) {
        if (settings[i].enabled) {
            enabled_count++;
        }
    }
    if (enabled_count <= 0) {
        return;
    }

    const int columns = 3;
    int rows = (enabled_count + columns - 1) / columns;

    float box_w = static_cast<float>(screen_w) * panel_w_ratio;
    float max_box_w = (static_cast<float>(screen_w) - panel_padding * (columns + 1)) / columns;
    if (max_box_w > 1.0f && box_w > max_box_w) {
        box_w = max_box_w;
    }
    if (box_w < 40.0f) {
        box_w = 40.0f;
    }
    float box_h = static_cast<float>(screen_h) * panel_h_ratio;
    float max_box_h = (static_cast<float>(screen_h) - panel_padding * (rows + 1)) / rows;
    if (max_box_h > 1.0f && box_h > max_box_h) {
        box_h = max_box_h;
    }
    if (box_h < 40.0f) {
        box_h = 40.0f;
    }

    int used_columns = std::min(columns, enabled_count);
    float total_w = box_w * used_columns + panel_padding * (used_columns - 1);
    float total_h = box_h * rows + panel_padding * (rows - 1);
    float start_x = (static_cast<float>(screen_w) - total_w) * 0.5f;
    float start_y = (static_cast<float>(screen_h) - total_h) * 0.5f;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int enabled_index = 0;
    for (int i = 0; i < kTypeCount; ++i) {
        if (!settings[i].enabled) {
            continue;
        }

        int col = enabled_index % columns;
        int row = enabled_index / columns;
        float box_x = start_x + col * (box_w + panel_padding);
        float box_y = start_y + row * (box_h + panel_padding);
        int ix = static_cast<int>(box_x);
        int iy = static_cast<int>(box_y);
        int iw = static_cast<int>(box_w);
        int ih = static_cast<int>(box_h);
        int gl_y = screen_h - iy - ih; // OpenGL readback coordinates are bottom-left based.
        if (gl_y < 0) {
            gl_y = 0;
        }

        Blur* curr = blurs[i];
        curr->backend = static_cast<Hardware>(settings[i].backend);
        curr->type = static_cast<Blur::Type>(i);
        curr->process(ix, gl_y, iw, ih,
                      settings[i].radius, settings[i].downscale);

        ImU32 tint = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, settings[i].alpha));
        dl->AddImageRounded(
            (void*)(intptr_t)curr->tex,
            ImVec2(static_cast<float>(ix), static_cast<float>(iy)),
            ImVec2(static_cast<float>(ix + iw), static_cast<float>(iy + ih)),
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f),
            tint,
            settings[i].corner,
            ImDrawFlags_RoundCornersAll
        );
        dl->AddText(ImVec2(static_cast<float>(ix) + 8.0f, static_cast<float>(iy) - 20.0f), IM_COL32(255, 255, 255, 230), kTypeNames[i]);
        dl->AddRect(ImVec2(static_cast<float>(ix), static_cast<float>(iy)),
                    ImVec2(static_cast<float>(ix + iw), static_cast<float>(iy + ih)),
                    IM_COL32(255, 255, 255, 64), settings[i].corner);
        enabled_index++;
    }
}
