#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D spriteTexture;

layout(set = 3, binding = 0) uniform PushConstants {
    vec2 screenSize;
    vec2 objPos;
    vec2 objSize;
    vec2 trig;
    vec4 uvRect;
    vec4 colorTint;
} pc;

void main() {
    vec4 texColor = texture(spriteTexture, v_uv);
    outColor = texColor * pc.colorTint;
}