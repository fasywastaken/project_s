#version 450

layout(set = 2, binding = 0) uniform sampler2D spriteSampler;
layout(set = 2, binding = 1) uniform sampler2D paletteSampler;

layout(set = 3, binding = 0) uniform PushConstants {
    vec2 screenSize;
    vec2 pos;
    vec2 scale;
    vec2 rot;
    vec4 uvRect;
    vec4 tintColor;
    vec4 padding;
} pc;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(spriteSampler, fragUV);

    if (pc.tintColor.a < 0.0) {
        outColor = texel;
        return;
    }

    if (texel.a < 0.8) discard;

    vec3 n = texel.rgb / max(texel.a, 0.001);

    vec2 localUV = (fragUV - pc.uvRect.xy) / max(pc.uvRect.zw, 0.001);
    vec2 uvTL = localUV * 0.5;
    vec2 uvTR = vec2(localUV.x * 0.5 + 0.5, localUV.y * 0.5);
    vec2 uvBL = vec2(localUV.x * 0.5, localUV.y * 0.5 + 0.5);
    vec2 uvBR = vec2(localUV.x * 0.5 + 0.5, localUV.y * 0.5 + 0.5);

    vec3 baseYellow  = vec3(1.0, 1.0, 0.0);
    vec3 baseMagenta = vec3(1.0, 0.0, 1.0);
    vec3 baseRed     = vec3(1.0, 0.0, 0.0);
    vec3 baseGreen   = vec3(0.0, 1.0, 0.0);
    vec3 baseBlue    = vec3(0.0, 0.0, 1.0);

    float dYellow  = distance(n, baseYellow);
    float dMagenta = distance(n, baseMagenta);
    float dRed     = distance(n, baseRed);
    float dGreen   = distance(n, baseGreen);
    float dBlue    = distance(n, baseBlue);

    float minDist = min(min(min(min(dYellow, dMagenta), dRed), dGreen), dBlue);

    vec3 dyedColor = n;

    if (minDist < 0.5) {
        if (minDist == dYellow) {
            dyedColor = texture(paletteSampler, uvTR).rgb;
        }
        else if (minDist == dMagenta) {
            dyedColor = pc.tintColor.rgb;
        }
        else if (minDist == dRed) {
            dyedColor = texture(paletteSampler, uvTL).rgb;
        }
        else if (minDist == dGreen) {
            dyedColor = texture(paletteSampler, uvBL).rgb;
        }
        else if (minDist == dBlue) {
            dyedColor = texture(paletteSampler, uvBR).rgb;
        }
    }

    outColor = vec4(dyedColor * texel.a, texel.a);
}