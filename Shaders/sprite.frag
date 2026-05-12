#version 450

layout(set = 2, binding = 0) uniform sampler2D spriteSampler;
layout(set = 2, binding = 1) uniform sampler2D paletteSampler;

layout(set = 3, binding = 0) uniform PushConstants {
    vec2 screenSize;
    vec2 pos;
    vec2 scale;
    vec2 rot;
    vec4 uvRect;
    vec4 armorColor;
    vec4 padding;
} pc;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(spriteSampler, fragUV);

    // Bypass invisible pixels completely
    if (texel.a < 0.05) discard;

    // 1. ISOLATION: The -1.0f flag bypasses Terrain, UI, and Weapons perfectly
    if (pc.armorColor.a < 0.0) {
        outColor = texel;
        return;
    }

    // 2. UN-PREMULTIPLY ALPHA FOR CLEAN COLOR DETECTION
    // This strips away the "dark edge" of the anti-aliasing so the math works
    vec3 n = texel.rgb / max(texel.a, 0.001);

    // 3. GRADIENT UV MAPPING
    vec2 localUV = (fragUV - pc.uvRect.xy) / max(pc.uvRect.zw, 0.001);
    vec2 uvTL = localUV * 0.5;                             // Body
    vec2 uvTR = vec2(localUV.x * 0.5 + 0.5, localUV.y * 0.5); // Arms
    vec2 uvBL = vec2(localUV.x * 0.5, localUV.y * 0.5 + 0.5); // Backpack
    vec2 uvBR = vec2(localUV.x * 0.5 + 0.5, localUV.y * 0.5 + 0.5); // Stroke

    // 4. PURE CHANNEL DOMINANCE (Bulletproof Mask Detection)
    vec3 dyedColor = n; // Fallback to raw color just in case

    // Yellow (Arms): Red and Green are high, Blue is low
    if (n.r > 0.4 && n.g > 0.4 && n.b < 0.3) {
        dyedColor = texture(paletteSampler, uvTR).rgb;
    }
    // Magenta (Armor): Red and Blue are high, Green is low
    else if (n.r > 0.4 && n.b > 0.4 && n.g < 0.3) {
        dyedColor = pc.armorColor.rgb;
    }
    // Pure Red (Body): Red strictly dominates
    else if (n.r > n.g + 0.2 && n.r > n.b + 0.2) {
        dyedColor = texture(paletteSampler, uvTL).rgb;
    }
    // Pure Green (Backpack): Green strictly dominates
    else if (n.g > n.r + 0.2 && n.g > n.b + 0.2) {
        dyedColor = texture(paletteSampler, uvBL).rgb;
    }
    // Pure Blue (Stroke): Blue strictly dominates
    else if (n.b > n.r + 0.2 && n.b > n.g + 0.2) {
        dyedColor = texture(paletteSampler, uvBR).rgb;
    }

    // 5. RESTORE PERFECT ANTI-ALIASING
    // We apply the chosen color, but use the SVG's original soft alpha!
    outColor = vec4(dyedColor, texel.a);
}