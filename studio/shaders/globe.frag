#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 0, std140) uniform GlobeUniforms
{
    vec4 globe;     // x = yaw, y = pitch
    vec4 viewport;  // xy = widget pixels
} uniforms;

layout(binding = 1) uniform sampler2D terrain;

const float PI = 3.14159265358979323846;

void main()
{
    vec2 size = uniforms.viewport.xy;
    vec2 offset = (uv - vec2(0.5)) * size;
    float radius = min(size.x, size.y) * 0.46;
    vec2 p = offset / radius;
    float r2 = dot(p, p);
    if (r2 > 1.0)
    {
        fragColor = vec4(0.043, 0.059, 0.078, 1.0);
        return;
    }

    float z = sqrt(1.0 - r2);
    float yaw = uniforms.globe.x;
    float pitch = uniforms.globe.y;
    float cp = cos(pitch);
    float sp = sin(pitch);
    float y1 = p.y * cp + z * sp;
    float z1 = -p.y * sp + z * cp;
    float cy = cos(yaw);
    float sy = sin(yaw);
    float ox = p.x * cy - z1 * sy;
    float oz = p.x * sy + z1 * cy;

    float u = fract(atan(ox, oz) / (2.0 * PI) + 0.5);
    float v = 0.5 - asin(clamp(y1, -1.0, 1.0)) / PI;
    vec3 colour = texture(terrain, vec2(u, v)).rgb;
    fragColor = vec4(colour * (0.45 + 0.55 * z), 1.0);
}
