#include "imgui.h"
#include "../../blur.hpp"

// imgui_examples.cpp
// Demonstrates multiple ways to use Blur in ImGui.
// Показывает несколько способов использования Blur в ImGui.

static Blur gBlur(Hardware::GPU);

static const char* kTypeNames[] = {
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

void ExampleFrame(int screen_w, int screen_h) {
    if (screen_w <= 0 || screen_h <= 0) {
        return;
    }

    static int backend = static_cast<int>(Hardware::GPU);
    static int type = static_cast<int>(Blur::Type::Default);
    static float radius = 12.0f;
    static int downscale = 4;
    static float corner = 10.0f;
    static int mode = 0;

    ImGui::Begin("imgui_examples");
    ImGui::TextUnformatted("Multiple usage examples for imgui-android-blur");
    ImGui::RadioButton("GPU", &backend, static_cast<int>(Hardware::GPU));
    ImGui::SameLine();
    ImGui::RadioButton("CPU", &backend, static_cast<int>(Hardware::CPU));
    ImGui::Combo("Type", &type, kTypeNames, IM_ARRAYSIZE(kTypeNames));
    ImGui::SliderFloat("Radius", &radius, 1.0f, 40.0f, "%.1f");
    ImGui::SliderInt("Downscale", &downscale, 1, 8);
    ImGui::SliderFloat("Corner", &corner, 0.0f, 40.0f, "%.1f");

    const char* mode_items[] = {
        "DrawList overload",
        "ImRect overload",
        "Pos/Size overload",
        "Raw x,y,w,h"
    };
    ImGui::Combo("Usage", &mode, mode_items, IM_ARRAYSIZE(mode_items));

    gBlur.backend = static_cast<Hardware>(backend);
    gBlur.type = static_cast<Blur::Type>(type);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Mode 0: DrawList overload draws blur image automatically.
    // Режим 0: перегрузка DrawList сама отрисовывает блюр.
    if (mode == 0) {
        ImGui::Begin("Blur via DrawList overload", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::SetWindowSize(ImVec2(360.0f, 200.0f), ImGuiCond_Always);
        gBlur.process(ImGui::GetWindowDrawList(), corner, radius, downscale, gBlur.backend);
        ImGui::TextUnformatted("Blur::process(draw, corner, radius, downscale, backend)");
        ImGui::End();
    // Mode 1: blur via ImRect + manual AddImageRounded.
    // Режим 1: блюр через ImRect + ручной AddImageRounded.
    } else if (mode == 1) {
        ImVec2 min(40.0f, 100.0f);
        ImVec2 max(40.0f + 360.0f, 100.0f + 200.0f);
        ImRect rect(min, max);
        gBlur.process(rect, radius, downscale);
        dl->AddImageRounded((void*)(intptr_t)gBlur.tex, rect.Min, rect.Max,
                            ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f),
                            IM_COL32(255, 255, 255, 255), corner, ImDrawFlags_RoundCornersAll);
        dl->AddText(ImVec2(min.x + 8.0f, min.y + 8.0f), IM_COL32(255, 255, 255, 255),
                    "Blur::process(ImRect, radius, downscale)");
    // Mode 2: blur via position/size overload + manual draw.
    // Режим 2: блюр через перегрузку pos/size + ручная отрисовка.
    } else if (mode == 2) {
        ImVec2 pos(40.0f, 100.0f);
        ImVec2 size(360.0f, 200.0f);
        gBlur.process(pos, size, radius, downscale);
        dl->AddImageRounded((void*)(intptr_t)gBlur.tex, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                            ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f),
                            IM_COL32(255, 255, 255, 255), corner, ImDrawFlags_RoundCornersAll);
        dl->AddText(ImVec2(pos.x + 8.0f, pos.y + 8.0f), IM_COL32(255, 255, 255, 255),
                    "Blur::process(ImVec2 pos, ImVec2 size, radius, downscale)");
    // Mode 3: raw integer rectangle API.
    // Режим 3: сырой API через целочисленный прямоугольник.
    } else {
        int w = 360;
        int h = 200;
        int x = 40;
        int y = 100;
        gBlur.process(x, y, w, h, radius, downscale);
        dl->AddImageRounded((void*)(intptr_t)gBlur.tex, ImVec2((float)x, (float)y), ImVec2((float)(x + w), (float)(y + h)),
                            ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f),
                            IM_COL32(255, 255, 255, 255), corner, ImDrawFlags_RoundCornersAll);
        dl->AddText(ImVec2((float)x + 8.0f, (float)y + 8.0f), IM_COL32(255, 255, 255, 255),
                    "Blur::process(x, y, w, h, radius, downscale)");
    }

    ImGui::End();
}
