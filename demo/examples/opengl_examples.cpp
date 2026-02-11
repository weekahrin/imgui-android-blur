#include <GLES3/gl3.h>
#include "../../blur.hpp"
#include "../../blur/gpu/shaders/shaders.hpp"

// opengl_examples.cpp
// Demonstrates raw OpenGL usage with different blur types and backends.
// Показывает использование blur в чистом OpenGL с разными типами и backend.

static GLuint Compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

static GLuint CreateProgram() {
    GLuint vs = Compile(GL_VERTEX_SHADER, kBlitVS);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kBlitFS);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static GLuint gQuadVao = 0;
static GLuint gQuadVbo = 0;
static GLuint gProgram = 0;
static bool gInitialized = false;

static Blur gBlur(Hardware::GPU);

static void EnsureGLResources() {
    if (gInitialized) {
        return;
    }

    gProgram = CreateProgram();
    glGenVertexArrays(1, &gQuadVao);
    glGenBuffers(1, &gQuadVbo);
    glBindVertexArray(gQuadVao);
    glBindBuffer(GL_ARRAY_BUFFER, gQuadVbo);
    const float quad[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    gInitialized = true;
}

static void DrawBlurTexture(GLuint tex) {
    glUseProgram(gProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(gProgram, "uTex"), 0);
    glBindVertexArray(gQuadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);
}

void ExampleFrame(int screen_w, int screen_h) {
    if (screen_w <= 0 || screen_h <= 0) {
        return;
    }
    EnsureGLResources();

    // 9 usage examples (all types): Default, Gaussian, Box, Frosted, Kawase, Bokeh, Motion, Zoom, Pixelate.
    // 9 примеров использования (все типы): Default, Gaussian, Box, Frosted, Kawase, Bokeh, Motion, Zoom, Pixelate.
    const Blur::Type types[] = {
        Blur::Type::Default,
        Blur::Type::Gaussian,
        Blur::Type::Box,
        Blur::Type::Frosted,
        Blur::Type::Kawase,
        Blur::Type::Bokeh,
        Blur::Type::Motion,
        Blur::Type::Zoom,
        Blur::Type::Pixelate
    };
    const float radii[] = {12.0f, 16.0f, 10.0f, 14.0f, 9.0f, 12.0f, 13.0f, 11.0f, 8.0f};
    const int dss[] = {4, 4, 3, 4, 2, 4, 3, 4, 2};

    for (int i = 0; i < 9; ++i) {
        gBlur.backend = (i == 4) ? Hardware::CPU : Hardware::GPU;
        gBlur.type = types[i];
        int col = i % 3;
        int row = i / 3;
        int x = screen_w / 10 + col * (screen_w / 4);
        int y = screen_h / 10 + row * (screen_h / 4);
        int w = screen_w / 4;
        int h = screen_h / 5;
        gBlur.process(x, y, w, h, radii[i], dss[i]);
    }

    // Draw last processed blur texture on full-screen quad (simple presentation path).
    // Рисуем последний обработанный blur texture на полный экран (упрощенный путь).
    DrawBlurTexture(gBlur.tex);
}
