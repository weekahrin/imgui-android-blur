#pragma once

static const char* kBlurVS = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* kBlurFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
out vec4 FragColor;
void main() {
    vec2 off = uDirection * uTexel;
    vec4 c = texture(uTex, vUV) * 0.227027;
    c += texture(uTex, vUV + off * 1.384615) * 0.316216;
    c += texture(uTex, vUV - off * 1.384615) * 0.316216;
    c += texture(uTex, vUV + off * 3.230769) * 0.070270;
    c += texture(uTex, vUV - off * 3.230769) * 0.070270;
    FragColor = c;
}
)";

static const char* kBoxFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
out vec4 FragColor;
void main() {
    vec2 off = uDirection * uTexel;
    vec4 c = texture(uTex, vUV) * 0.2;
    c += texture(uTex, vUV + off) * 0.2;
    c += texture(uTex, vUV - off) * 0.2;
    c += texture(uTex, vUV + off * 2.0) * 0.2;
    c += texture(uTex, vUV - off * 2.0) * 0.2;
    FragColor = c;
}
)";

static const char* kFrostedFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
uniform float uStrength;
out vec4 FragColor;
float hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p.x + p.y) * 43758.5453123);
}
void main() {
    vec2 rnd = vec2(hash(vUV), hash(vUV.yx)) - 0.5;
    vec2 off = uDirection * uTexel;
    vec2 jitter = rnd * uTexel * uStrength;
    vec4 c = texture(uTex, vUV + jitter) * 0.35;
    c += texture(uTex, vUV + off + jitter) * 0.25;
    c += texture(uTex, vUV - off + jitter) * 0.25;
    c += texture(uTex, vUV + off * 2.0 + jitter) * 0.075;
    c += texture(uTex, vUV - off * 2.0 + jitter) * 0.075;
    FragColor = c;
}
)";

static const char* kKawaseFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
uniform float uStrength;
out vec4 FragColor;
void main() {
    vec2 o = uDirection * uTexel * max(1.0, uStrength);
    vec4 c = vec4(0.0);
    c += texture(uTex, vUV + vec2( o.x,  o.y));
    c += texture(uTex, vUV + vec2(-o.x,  o.y));
    c += texture(uTex, vUV + vec2( o.x, -o.y));
    c += texture(uTex, vUV + vec2(-o.x, -o.y));
    FragColor = c * 0.25;
}
)";

static const char* kBokehFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
uniform float uStrength;
out vec4 FragColor;
void main() {
    vec2 o = uDirection * uTexel * max(1.0, uStrength);
    vec4 c = texture(uTex, vUV) * 0.2;
    c += texture(uTex, vUV + o) * 0.18;
    c += texture(uTex, vUV - o) * 0.18;
    c += texture(uTex, vUV + vec2(-o.y, o.x)) * 0.18;
    c += texture(uTex, vUV + vec2(o.y, -o.x)) * 0.18;
    c += texture(uTex, vUV + o * 1.7) * 0.14;
    c += texture(uTex, vUV - o * 1.7) * 0.14;
    FragColor = min(c, vec4(1.0));
}
)";

static const char* kMotionFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
uniform float uStrength;
out vec4 FragColor;
void main() {
    vec2 dir = normalize(vec2(max(0.0001, uDirection.x), max(0.0001, uDirection.y)));
    vec2 stepv = dir * uTexel * max(1.0, uStrength);
    vec4 c = vec4(0.0);
    c += texture(uTex, vUV - stepv * 3.0) * 0.10;
    c += texture(uTex, vUV - stepv * 2.0) * 0.15;
    c += texture(uTex, vUV - stepv * 1.0) * 0.20;
    c += texture(uTex, vUV) * 0.10;
    c += texture(uTex, vUV + stepv * 1.0) * 0.20;
    c += texture(uTex, vUV + stepv * 2.0) * 0.15;
    c += texture(uTex, vUV + stepv * 3.0) * 0.10;
    FragColor = c;
}
)";

static const char* kZoomFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
uniform float uStrength;
out vec4 FragColor;
void main() {
    vec2 center = vec2(0.5, 0.5);
    vec2 dir = center - vUV;
    float s = max(0.5, uStrength * 0.5);
    vec4 c = texture(uTex, vUV) * 0.2;
    c += texture(uTex, vUV + dir * 0.04 * s) * 0.2;
    c += texture(uTex, vUV + dir * 0.08 * s) * 0.2;
    c += texture(uTex, vUV - dir * 0.04 * s) * 0.2;
    c += texture(uTex, vUV - dir * 0.08 * s) * 0.2;
    FragColor = c;
}
)";

static const char* kPixelateFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDirection;
uniform vec2 uTexel;
uniform float uStrength;
out vec4 FragColor;
void main() {
    float blocks = max(8.0, 96.0 / max(1.0, uStrength));
    vec2 uv = floor(vUV * blocks) / blocks;
    vec4 c = texture(uTex, uv) * 0.6;
    c += texture(uTex, uv + uTexel * 0.5) * 0.4;
    FragColor = c;
}
)";

static const char* kBlitVS = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* kBlitFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    vec4 c = texture(uTex, vUV);
    FragColor = vec4(c.rgb, 1.0);
}
)";
