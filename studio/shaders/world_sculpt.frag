#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    vec4 mapRect;
    vec4 world;
    vec4 viewportLight;
    vec4 effects;
    vec4 environment;
    vec4 ambient;
} params;

layout(binding = 1) uniform sampler2D heightTexture;
layout(binding = 2) uniform sampler2D biomeTexture;
layout(binding = 3) uniform sampler2D biomeLut;
layout(binding = 4) uniform sampler2D waterTexture;

vec3 srgbToLinear(vec3 color)
{
    bvec3 low = lessThanEqual(color, vec3(0.04045));
    vec3 linearLow = color / 12.92;
    vec3 linearHigh = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(linearHigh, linearLow, low);
}

vec3 linearToSrgb(vec3 color)
{
    color = max(color, vec3(0.0));
    bvec3 low = lessThanEqual(color, vec3(0.0031308));
    vec3 linearLow = color * 12.92;
    vec3 linearHigh = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(linearHigh, linearLow, low);
}

vec3 waterColor(float depth)
{
    vec3 shelf = srgbToLinear(vec3(0.49, 0.76, 0.72));
    vec3 shallows = srgbToLinear(vec3(0.20, 0.58, 0.64));
    vec3 mid = srgbToLinear(vec3(0.075, 0.30, 0.46));
    vec3 deep = srgbToLinear(vec3(0.025, 0.095, 0.23));
    float shelfBand = smoothstep(0.008, 0.055, depth);
    float abyssBand = smoothstep(0.15, 0.48, depth);
    vec3 color = mix(shelf, shallows, shelfBand);
    color = mix(color, mid, smoothstep(0.04, 0.25, depth));
    color = mix(color, deep, abyssBand);
    float contour = 1.0 - smoothstep(0.006, 0.018,
        abs(fract(depth * 72.0) - 0.5));
    return mix(color, color * 0.84, contour * 0.22 * smoothstep(0.03, 0.1, depth));
}

float forestHash(ivec2 cell, uint salt)
{
    uint value = uint(cell.x) * 73856093u ^ uint(cell.y) * 19349663u ^ salt;
    value = (value ^ (value >> 13)) * 1274126177u;
    value ^= value >> 16;
    return float(value & 0x00ffffffu) / 16777215.0;
}

void main()
{
    vec2 screen = vec2(uv.x, 1.0 - uv.y) * params.viewportLight.xy;
    vec2 worldPosition = (screen - params.mapRect.xy) / params.mapRect.zw;
    bool wrapsX = params.effects.w > 0.5;
    if (worldPosition.y < 0.0 || worldPosition.y >= 1.0 ||
        (!wrapsX && (worldPosition.x < 0.0 || worldPosition.x >= 1.0))) {
        // The margin around the world is the canvas background, not a void:
        // the CPU path paints it #171b20, and the two must match or the
        // editor shows black bars above and below the map.
        fragColor = vec4(0.090196, 0.105882, 0.125490, 1.0);
        return;
    }
    if (wrapsX) worldPosition.x = fract(worldPosition.x);

    vec2 texel = 1.0 / params.world.xy;
    vec2 sampleUv = worldPosition;
    float h = texture(heightTexture, sampleUv).r;
    float waterType = texture(waterTexture, sampleUv).r * 255.0;
    float west = texture(heightTexture, sampleUv - vec2(texel.x, 0.0)).r;
    float east = texture(heightTexture, sampleUv + vec2(texel.x, 0.0)).r;
    float north = texture(heightTexture, sampleUv - vec2(0.0, texel.y)).r;
    float south = texture(heightTexture, sampleUv + vec2(0.0, texel.y)).r;
    vec2 gradient = vec2(east - west, south - north) * 0.5;
    vec3 normal = normalize(vec3(-gradient * 42.0, 1.0));
    vec3 lightDirection = normalize(vec3(-0.45, -0.62, 0.64));
    if (params.environment.y > 0.5) {
        const float pi = 3.14159265;
        float solarAngle = (params.environment.x - 6.0) * pi / 12.0;
        float sunHeight = max(sin(solarAngle), 0.0);
        lightDirection = normalize(vec3(cos(solarAngle), sin(solarAngle),
                                        0.16 + sunHeight * 0.84));
    }
    float sunlight = 1.0 - params.ambient.a;
    float directionalShade = 0.54 + 0.46 * max(dot(normal, lightDirection), 0.0);
    directionalShade *= mix(0.82, 1.0, sunlight);
    float hillshade = mix(1.0, directionalShade, params.viewportLight.z);
    float slope = length(gradient) * 42.0;

    vec3 color;
    if (waterType > 1.5) {
        color = waterColor(0.035);
        color = mix(color, srgbToLinear(vec3(0.12, 0.39, 0.49)), 0.28);
    } else if (waterType > 0.5) {
        color = waterColor(0.018);
        color = mix(color, srgbToLinear(vec3(0.14, 0.42, 0.52)), 0.35);
    } else if (h <= params.world.z) {
        color = waterColor(params.world.z - h);
    } else {
        float biomeId = texture(biomeTexture, sampleUv).r * 255.0;
        float heightBand = clamp(floor(h * 8.0), 0.0, 7.0);
        vec2 lutUv = vec2((floor(biomeId + 0.5) + 0.5) / 32.0,
                          (heightBand + 0.5) / 8.0);
        vec4 biomeSample = texture(biomeLut, lutUv);
        color = srgbToLinear(biomeSample.rgb);
        color = mix(color, srgbToLinear(vec3(0.49, 0.43, 0.36)),
                    smoothstep(0.20, 0.55, slope) * 0.75);
        float waterWest = texture(waterTexture, sampleUv - vec2(texel.x, 0.0)).r * 255.0;
        float waterEast = texture(waterTexture, sampleUv + vec2(texel.x, 0.0)).r * 255.0;
        float waterNorth = texture(waterTexture, sampleUv - vec2(0.0, texel.y)).r * 255.0;
        float waterSouth = texture(waterTexture, sampleUv + vec2(0.0, texel.y)).r * 255.0;
        float coast = 0.0;
        coast += max(step(params.world.z, west), step(0.5, waterWest));
        coast += max(step(params.world.z, east), step(0.5, waterEast));
        coast += max(step(params.world.z, north), step(0.5, waterNorth));
        coast += max(step(params.world.z, south), step(0.5, waterSouth));
        float coastAo = coast * 0.055;
        color *= 1.0 - coastAo;
        float beach = 1.0 - smoothstep(0.0, 0.024, h - params.world.z);
        color = mix(color, srgbToLinear(vec3(0.82, 0.73, 0.51)), beach * 0.85);
        float contourDistance = abs(fract((h - params.world.z) / params.world.w) - 0.025);
        float contour = 1.0 - smoothstep(0.008, 0.026, contourDistance);
        color *= 1.0 - contour * params.effects.x * 0.14;

        float forestMask = biomeSample.a;
        if (forestMask > 0.5) {
            vec2 canopyGrid = worldPosition * params.world.xy / 2.8;
            ivec2 canopyCell = ivec2(floor(canopyGrid));
            vec2 canopyLocal = fract(canopyGrid) - 0.5;
            float placement = forestHash(canopyCell, 0x4f1bbcdcu);
            float radius = mix(0.30, 0.45,
                               forestHash(canopyCell, 0x1b873593u));
            vec2 offset = (vec2(
                forestHash(canopyCell, 0x85ebca6bu),
                forestHash(canopyCell, 0xc2b2ae35u)) - 0.5) * 0.08;
            vec2 crownCoord = (canopyLocal - offset) * vec2(1.0, 1.28);
            float crown = 1.0 - smoothstep(radius * 0.66, radius,
                                           length(crownCoord));
            vec2 sideCrownCoord =
                (canopyLocal - offset - vec2(radius * 0.28, -radius * 0.10)) *
                vec2(1.0, 1.34);
            crown = max(crown, 1.0 - smoothstep(radius * 0.54, radius * 0.78,
                                                length(sideCrownCoord)));
            float shadow = 1.0 - smoothstep(
                radius * 0.34, radius * 1.20,
                length((canopyLocal - offset - vec2(0.03, 0.12)) *
                       vec2(1.0, 0.68)));
            float canopyVisible = smoothstep(0.12, 0.34, placement) * forestMask;
            float leafTone = forestHash(canopyCell, 0x27d4eb2fu);
            vec3 canopyColor = srgbToLinear(mix(
                vec3(0.13, 0.28, 0.17), vec3(0.32, 0.49, 0.26), leafTone));
            vec3 canopyLight = srgbToLinear(vec3(0.46, 0.60, 0.33));
            vec2 highlightCoord =
                (canopyLocal - offset - vec2(-radius * 0.18, -radius * 0.32)) *
                vec2(1.0, 1.45);
            float highlight = 1.0 - smoothstep(
                radius * 0.08, radius * 0.34, length(highlightCoord));
            color = mix(color, srgbToLinear(vec3(0.09, 0.19, 0.13)),
                        shadow * canopyVisible * 0.30);
            color = mix(color, canopyColor, crown * canopyVisible * 0.78);
            color = mix(color, canopyLight, highlight * crown * canopyVisible * 0.32);
        }
    }
    if (params.effects.y > 0.5 && params.effects.z > 0.5) {
        vec2 cell = worldPosition * params.world.xy;
        vec2 edge = min(fract(cell), 1.0 - fract(cell));
        vec2 aa = fwidth(cell);
        vec2 grid = 1.0 - smoothstep(aa * 0.45, aa * 1.35, edge);
        color = mix(color, color * 0.72, max(grid.x, grid.y) * 0.22);
    }
    color *= mix(vec3(1.0), srgbToLinear(params.ambient.rgb), params.ambient.a);
    color *= hillshade;
    fragColor = vec4(clamp(linearToSrgb(color), 0.0, 1.0), 1.0);
}
