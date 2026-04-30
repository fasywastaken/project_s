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

    // 1. SCALE FIRST (Local Space)
    vec2 scaledPos = centeredPos * push.scaleAndRot.xy;

    // 2. ROTATE SECOND (Around the centered, scaled origin)
    vec2 rotatedPos;
    rotatedPos.x = (scaledPos.x * push.scaleAndRot.z) - (scaledPos.y * push.scaleAndRot.w);
    rotatedPos.y = (scaledPos.x * push.scaleAndRot.w) + (scaledPos.y * push.scaleAndRot.z);

    // 3. TRANSLATE THIRD (To world position)
    vec2 pixelPos = rotatedPos + push.screenAndPos.zw;

    // 4. CONVERT TO NDC (And flip Y for SDL3)
    vec2 ndc = (pixelPos / push.screenAndPos.xy) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
}