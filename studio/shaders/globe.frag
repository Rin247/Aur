#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 0, std140) uniform GlobeUniforms
{
    vec4 globe;      // x = yaw, y = pitch, z = zoom, w = curvature
    vec4 viewport;   // xy = widget pixels, z = yUpInNdc, w = hasDetail
    vec4 detailRect; // xy = uv0, zw = uvSpan
} uniforms;

layout(binding = 1) uniform sampler2D terrain;
layout(binding = 2) uniform sampler2D detailTerrain;

const float PI = 3.14159265358979323846;

void main()
{
    vec2 size = uniforms.viewport.xy;
    float yUp = uniforms.viewport.z;
    vec2 normUv = vec2(uv.x, mix(1.0 - uv.y, uv.y, yUp));
    vec2 offset = (normUv - vec2(0.5)) * size;
    float zoom = max(uniforms.globe.z, 0.05);
    float curvature = clamp(uniforms.globe.w, 0.0, 1.0);
    float radius = min(size.x, size.y) * 0.46 * zoom;
    vec2 p = offset / radius;
    float r2 = dot(p, p);
    if (r2 > 1.0)
    {
        fragColor = vec4(0.090196, 0.105882, 0.125490, 1.0);
        return;
    }

    float zSphere = sqrt(1.0 - r2);
    float yaw = uniforms.globe.x;
    float pitch = uniforms.globe.y;
    float cp = cos(pitch);
    float sp = sin(pitch);
    float y1 = p.y * cp + zSphere * sp;
    float z1 = -p.y * sp + zSphere * cp;
    float cy = cos(yaw);
    float sy = sin(yaw);
    float ox = p.x * cy - z1 * sy;
    float oz = p.x * sy + z1 * cy;

    float uSphere = fract(atan(ox, oz) / (2.0 * PI) + 0.5);
    float vSphere = 0.5 - asin(clamp(y1, -1.0, 1.0)) / PI;

    float uCenter = fract(-yaw / (2.0 * PI) + 0.5);
    float vCenter = 0.5 - pitch / PI;
    float latScale = max(abs(cp), 0.25);
    float uFlat = fract(uCenter + p.x / (2.0 * PI * latScale));
    float vFlat = clamp(vCenter - p.y / PI, 0.0, 1.0);

    float duWrap = uFlat - uSphere;
    if (duWrap > 0.5) duWrap -= 1.0;
    else if (duWrap < -0.5) duWrap += 1.0;
    float u = fract(uSphere + (1.0 - curvature) * duWrap + 1.0);
    float v = mix(vFlat, vSphere, curvature);

    vec3 colour = texture(terrain, vec2(u, v)).rgb;
    if (uniforms.viewport.w > 0.5 && uniforms.detailRect.z > 0.0 && uniforms.detailRect.w > 0.0)
    {
        float du = fract(u - uniforms.detailRect.x + 1.0);
        float dv = v - uniforms.detailRect.y;
        if (du >= 0.0 && du < uniforms.detailRect.z && dv >= 0.0 && dv < uniforms.detailRect.w)
        {
            vec2 detailUv = vec2(du / uniforms.detailRect.z, dv / uniforms.detailRect.w);
            colour = texture(detailTerrain, detailUv).rgb;
        }
    }
    float shade = mix(1.0, 0.45 + 0.55 * zSphere, curvature);
    fragColor = vec4(colour * shade, 1.0);
}
