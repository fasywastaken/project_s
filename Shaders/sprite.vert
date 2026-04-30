#version 450

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

layout(set = 1, binding = 0) uniform PushConstants {
    vec4 screenAndPos;
    vec4 scaleAndRot;
    vec4 uvRect;
    vec4 padding;
} push;

void main() {
    v_uv = (a_uv * push.uvRect.zw) + push.uvRect.xy;
    vec2 centeredPos = a_position - 0.5;

    vec2 scaledPos = centeredPos * push.scaleAndRot.xy;

    vec2 rotatedPos;
    rotatedPos.x = (scaledPos.x * push.scaleAndRot.z) - (scaledPos.y * push.scaleAndRot.w);
    rotatedPos.y = (scaledPos.x * push.scaleAndRot.w) + (scaledPos.y * push.scaleAndRot.z);

    vec2 pixelPos = rotatedPos + push.screenAndPos.zw;

    vec2 ndc = (pixelPos / push.screenAndPos.xy) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
}