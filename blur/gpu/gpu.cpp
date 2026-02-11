#include "../../blur.hpp"
#include "../math.hpp"
#include "shaders/shaders.hpp"
#include <algorithm>

static const char* FragmentForType(Blur::Type type) {
    switch (type) {
        case Blur::Type::Gaussian:
            return kBlurFS;
        case Blur::Type::Box:
            return kBoxFS;
        case Blur::Type::Frosted:
            return kFrostedFS;
        case Blur::Type::Kawase:
            return kKawaseFS;
        case Blur::Type::Bokeh:
            return kBokehFS;
        case Blur::Type::Motion:
            return kMotionFS;
        case Blur::Type::Zoom:
            return kZoomFS;
        case Blur::Type::Pixelate:
            return kPixelateFS;
        case Blur::Type::Default:
        default:
            return kBlurFS;
    }
}

static int PassesForType(Blur::Type type, float radius) {
    int r = std::max(1, static_cast<int>(radius));
    switch (type) {
        case Blur::Type::Gaussian:
            return std::max(2, r / 3);
        case Blur::Type::Box:
            return std::max(1, r / 4);
        case Blur::Type::Frosted:
            return std::max(2, r / 5);
        case Blur::Type::Kawase:
            return std::max(2, std::min(8, r / 4 + 1));
        case Blur::Type::Bokeh:
            return std::max(2, r / 4);
        case Blur::Type::Motion:
            return std::max(1, r / 6);
        case Blur::Type::Zoom:
            return std::max(2, r / 5);
        case Blur::Type::Pixelate:
            return 1;
        case Blur::Type::Default:
        default:
            return 1;
    }
}

static void SetupTexture(GLuint tex, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

static GLuint CompileShader(GLenum type, const char* src) {
    if (!src) {
        return 0;
    }
    GLuint shader = glCreateShader(type);
    if (!shader) {
        return 0;
    }
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint CreateProgram(const char* vs_src, const char* fs_src) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    if (!prog) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint status = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (status != GL_TRUE) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

void gpu_ensure(Blur& t) {
    if (!glCreateShader || !glShaderSource || !glCompileShader || !glGetShaderiv ||
        !glCreateProgram || !glAttachShader || !glLinkProgram || !glGetProgramiv ||
        !glGetUniformLocation || !glGenFramebuffers || !glGenTextures ||
        !glGenVertexArrays || !glGenBuffers || !glBindVertexArray ||
        !glBindBuffer || !glBufferData || !glEnableVertexAttribArray ||
        !glVertexAttribPointer || !glBindTexture || !glTexParameteri ||
        !glTexImage2D) {
        return;
    }
    if (!t.gpu_ready_) {
        t.blur_program_ = CreateProgram(kBlurVS, FragmentForType(t.type));
        t.blit_program_ = CreateProgram(kBlitVS, kBlitFS);
        if (!t.blur_program_ || !t.blit_program_) {
            return;
        }
        t.loc_tex_ = glGetUniformLocation(t.blur_program_, "uTex");
        t.loc_direction_ = glGetUniformLocation(t.blur_program_, "uDirection");
        t.loc_texel_ = glGetUniformLocation(t.blur_program_, "uTexel");
        t.loc_strength_ = glGetUniformLocation(t.blur_program_, "uStrength");
        t.loc_blit_tex_ = glGetUniformLocation(t.blit_program_, "uTex");

        glGenFramebuffers(1, &t.fbo_);
        glGenTextures(1, &t.src_tex_);
        glGenTextures(1, &t.ping_tex_);
        glGenTextures(1, &t.pong_tex_);
        if (t.tex == 0) {
            glGenTextures(1, &t.tex);
        }

        glGenVertexArrays(1, &t.vao_);
        glGenBuffers(1, &t.vbo_);
        glBindVertexArray(t.vao_);
        glBindBuffer(GL_ARRAY_BUFFER, t.vbo_);
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

        t.gpu_ready_ = true;
        t.gpu_program_type_ = t.type;
    } else if (t.gpu_program_type_ != t.type) {
        if (t.blur_program_) {
            glDeleteProgram(t.blur_program_);
            t.blur_program_ = 0;
        }
        t.blur_program_ = CreateProgram(kBlurVS, FragmentForType(t.type));
        if (!t.blur_program_) {
            t.gpu_ready_ = false;
            return;
        }
        t.loc_tex_ = glGetUniformLocation(t.blur_program_, "uTex");
        t.loc_direction_ = glGetUniformLocation(t.blur_program_, "uDirection");
        t.loc_texel_ = glGetUniformLocation(t.blur_program_, "uTexel");
        t.loc_strength_ = glGetUniformLocation(t.blur_program_, "uStrength");
        t.gpu_program_type_ = t.type;
    }

    if (t.width_ > 0 && t.height_ > 0) {
        int ds = std::max(1, t.downscale_);
        t.blur_w_ = std::max(1, t.width_ / ds);
        t.blur_h_ = std::max(1, t.height_ / ds);
        SetupTexture(t.src_tex_, t.width_, t.height_);
        SetupTexture(t.tex, t.width_, t.height_);
        SetupTexture(t.ping_tex_, t.blur_w_, t.blur_h_);
        SetupTexture(t.pong_tex_, t.blur_w_, t.blur_h_);
    }
}

void gpu_release(Blur& t) {
    if (t.blur_program_) glDeleteProgram(t.blur_program_);
    if (t.blit_program_) glDeleteProgram(t.blit_program_);
    t.blur_program_ = 0;
    t.blit_program_ = 0;
    if (t.fbo_) glDeleteFramebuffers(1, &t.fbo_);
    if (t.src_tex_) glDeleteTextures(1, &t.src_tex_);
    if (t.ping_tex_) glDeleteTextures(1, &t.ping_tex_);
    if (t.pong_tex_) glDeleteTextures(1, &t.pong_tex_);
    if (t.vao_) glDeleteVertexArrays(1, &t.vao_);
    if (t.vbo_) glDeleteBuffers(1, &t.vbo_);
    t.fbo_ = 0;
    t.src_tex_ = 0;
    t.ping_tex_ = 0;
    t.pong_tex_ = 0;
    t.vao_ = 0;
    t.vbo_ = 0;
    t.gpu_ready_ = false;
    t.gpu_program_type_ = Blur::Type::Default;
}

void gpu_apply(Blur& t) {
    if (!glGetIntegerv || !glIsEnabled || !glDisable || !glBindTexture ||
        !glCopyTexSubImage2D || !glUseProgram || !glBindVertexArray ||
        !glViewport || !glBindFramebuffer || !glFramebufferTexture2D ||
        !glActiveTexture || !glUniform1i || !glUniform2f || !glDrawArrays ||
        !glBindBuffer || !glEnable || !glGetIntegerv) {
        return;
    }
    if (!t.gpu_ready_) {
        return;
    }

    GLint prev_program = 0;
    GLint prev_fbo = 0;
    GLint prev_read_fbo = 0;
    GLint prev_draw_fbo = 0;
    GLint prev_viewport[4] = {0, 0, 0, 0};
    GLint prev_scissor_box[4] = {0, 0, 0, 0};
    GLint prev_active = 0;
    GLint prev_tex_active = 0;
    GLint prev_tex0 = 0;
    GLint prev_vao = 0;
    GLint prev_vbo = 0;
    GLint prev_ebo = 0;
    GLint prev_blend_src_rgb = 0;
    GLint prev_blend_dst_rgb = 0;
    GLint prev_blend_src_alpha = 0;
    GLint prev_blend_dst_alpha = 0;
    GLint prev_blend_eq_rgb = 0;
    GLint prev_blend_eq_alpha = 0;
    GLfloat prev_blend_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLboolean prev_blend = GL_FALSE;
    GLboolean prev_depth = GL_FALSE;
    GLboolean prev_cull = GL_FALSE;
    GLboolean prev_scissor = GL_FALSE;
    GLboolean prev_stencil = GL_FALSE;
    GLboolean prev_color_mask[4] = {1, 1, 1, 1};
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
#ifdef GL_READ_FRAMEBUFFER_BINDING
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
#endif
#ifdef GL_DRAW_FRAMEBUFFER_BINDING
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
#endif
    glGetIntegerv(GL_VIEWPORT, prev_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, prev_scissor_box);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex0);
    glActiveTexture(prev_active);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prev_ebo);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prev_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prev_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_blend_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_blend_dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &prev_blend_eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prev_blend_eq_alpha);
    if (glGetFloatv) {
        glGetFloatv(GL_BLEND_COLOR, prev_blend_color);
    }
    prev_blend = glIsEnabled(GL_BLEND);
    prev_depth = glIsEnabled(GL_DEPTH_TEST);
    prev_cull = glIsEnabled(GL_CULL_FACE);
    prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
    prev_stencil = glIsEnabled(GL_STENCIL_TEST);
    if (glGetBooleanv) {
        glGetBooleanv(GL_COLOR_WRITEMASK, prev_color_mask);
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    if (glColorMask) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glBindTexture(GL_TEXTURE_2D, t.src_tex_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t.x_, t.y_, t.w_, t.h_);

    glUseProgram(t.blur_program_);
    glBindVertexArray(t.vao_);
    glViewport(0, 0, t.blur_w_, t.blur_h_);

    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo_);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(t.loc_tex_, 0);
    if (t.loc_strength_ >= 0 && glUniform1f) {
        glUniform1f(t.loc_strength_, std::max(1.0f, t.radius_ * 0.35f));
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.ping_tex_, 0);
    glBindTexture(GL_TEXTURE_2D, t.src_tex_);
    glUniform2f(t.loc_texel_, 1.0f / static_cast<float>(t.width_), 1.0f / static_cast<float>(t.height_));
    glUniform2f(t.loc_direction_, 1.0f, 0.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    int passes = PassesForType(t.type, t.radius_);
    GLuint inTex = t.ping_tex_;
    GLuint outTex = t.pong_tex_;
    for (int i = 0; i < passes; ++i) {
        float o = 1.0f;
        if (t.type == Blur::Type::Kawase) {
            float tpass = (passes > 1) ? (static_cast<float>(i) / static_cast<float>(passes - 1)) : 0.0f;
            o = 1.0f + tpass * std::max(1.0f, t.radius_ * 0.22f);
        } else if (t.type == Blur::Type::Motion) {
            o = std::max(1.0f, t.radius_ * 0.20f);
        } else if (t.type == Blur::Type::Pixelate) {
            o = std::max(1.0f, t.radius_ * 0.50f);
        } else if (t.type == Blur::Type::Bokeh || t.type == Blur::Type::Zoom) {
            o = std::max(1.0f, t.radius_ * 0.16f);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
        glBindTexture(GL_TEXTURE_2D, inTex);
        glUniform2f(t.loc_texel_, 1.0f / static_cast<float>(t.blur_w_), 1.0f / static_cast<float>(t.blur_h_));
        if (t.type == Blur::Type::Motion) {
            glUniform2f(t.loc_direction_, 1.0f, 0.35f);
        } else {
            glUniform2f(t.loc_direction_, o, 0.0f);
        }
        if (t.loc_strength_ >= 0 && glUniform1f) {
            glUniform1f(t.loc_strength_, std::max(1.0f, o));
        }
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        std::swap(inTex, outTex);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
        glBindTexture(GL_TEXTURE_2D, inTex);
        if (t.type == Blur::Type::Motion) {
            glUniform2f(t.loc_direction_, 1.0f, 0.35f);
        } else if (t.type == Blur::Type::Pixelate) {
            glUniform2f(t.loc_direction_, 1.0f, 0.0f);
        } else {
            glUniform2f(t.loc_direction_, 0.0f, o);
        }
        if (t.loc_strength_ >= 0 && glUniform1f) {
            glUniform1f(t.loc_strength_, std::max(1.0f, o));
        }
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        std::swap(inTex, outTex);
    }

    glUseProgram(t.blit_program_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    glViewport(0, 0, t.width_, t.height_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inTex);
    glUniform1i(t.loc_blit_tex_, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
#ifdef GL_READ_FRAMEBUFFER
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
#endif
#ifdef GL_DRAW_FRAMEBUFFER
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
#endif
    glUseProgram(prev_program);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    if (glScissor) glScissor(prev_scissor_box[0], prev_scissor_box[1], prev_scissor_box[2], prev_scissor_box[3]);
    if (glColorMask) glColorMask(prev_color_mask[0], prev_color_mask[1], prev_color_mask[2], prev_color_mask[3]);
    glActiveTexture(prev_active);
    glBindTexture(GL_TEXTURE_2D, prev_tex_active);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prev_tex0);
    glActiveTexture(prev_active);
    glBindVertexArray(prev_vao);
    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prev_ebo);
    if (prev_blend) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    if (glBlendFuncSeparate) {
        glBlendFuncSeparate((GLenum)prev_blend_src_rgb, (GLenum)prev_blend_dst_rgb,
                            (GLenum)prev_blend_src_alpha, (GLenum)prev_blend_dst_alpha);
    } else if (glBlendFunc) {
        glBlendFunc((GLenum)prev_blend_src_rgb, (GLenum)prev_blend_dst_rgb);
    }
    if (glBlendEquationSeparate) {
        glBlendEquationSeparate((GLenum)prev_blend_eq_rgb, (GLenum)prev_blend_eq_alpha);
    } else if (glBlendEquation) {
        glBlendEquation((GLenum)prev_blend_eq_rgb);
    }
    if (glBlendColor) {
        glBlendColor(prev_blend_color[0], prev_blend_color[1], prev_blend_color[2], prev_blend_color[3]);
    }
    if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (prev_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (prev_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (prev_stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}
