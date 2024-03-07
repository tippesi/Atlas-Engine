layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D textureOut;
layout (set = 3, binding = 1) uniform sampler2D textureIn;

vec4 SampleCatmullRom(vec2 uv) {

    // http://advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
    // Credit: Jorge Jimenez (SIGGRAPH 2016)
    // Ignores the 4 corners of the 4x4 grid
    // Learn more: http://vec3.ca/bicubic-filtering-in-fewer-taps/
    vec2 resolution = vec2(textureSize(textureIn, 0));
    vec2 invResolution = 1.0 / resolution;

    vec2 position = uv * resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;

    vec2 tc0 = (center - 1.0) * invResolution;
    vec2 tc12 = (center + w2 / w12) * invResolution;
    vec2 tc3 = (center + 2.0) * invResolution;

    vec2 uv0 = clamp(vec2(tc12.x, tc0.y), vec2(0.0), vec2(1.0));
    vec2 uv1 = clamp(vec2(tc0.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv2 = clamp(vec2(tc12.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv3 = clamp(vec2(tc3.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv4 = clamp(vec2(tc12.x, tc3.y), vec2(0.0), vec2(1.0));

    float weight0 = w12.x * w0.y;
    float weight1 = w0.x * w12.y;
    float weight2 = w12.x * w12.y;
    float weight3 = w3.x * w12.y;
    float weight4 = w12.x * w3.y;

    vec4 sample0 = textureLod(textureIn, uv0, 0.0) * weight0;
    vec4 sample1 = textureLod(textureIn, uv1, 0.0) * weight1;
    vec4 sample2 = textureLod(textureIn, uv2, 0.0) * weight2;
    vec4 sample3 = textureLod(textureIn, uv3, 0.0) * weight3;
    vec4 sample4 = textureLod(textureIn, uv4, 0.0) * weight4;

    float totalWeight = weight0 + weight1 + 
        weight2 + weight3 + weight4;

    vec4 totalSample = sample0 + sample1 +
        sample2 + sample3 + sample4;

    return totalSample / totalWeight;    

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(textureOut).x ||
        pixel.y > imageSize(textureOut).y)
        return;

    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(imageSize(textureOut));

    vec4 color = SampleCatmullRom(uv);

    imageStore(textureOut, pixel, color);

}