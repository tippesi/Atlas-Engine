// Based on DDGI: 
// Majercik, Zander, et al. "Dynamic diffuse global illumination with ray-traced irradiance fields."
// Journal of Computer Graphics Techniques Vol 8.2 (2019).
// Majercik, Zander, et al. "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production."
// arXiv preprint arXiv:2009.10796 (2020).

layout (location = 0) out vec4 colorFS;

layout (binding = 0) uniform sampler2DArray volume;

uniform int probeRes;
uniform int volumeLayer;

// https://monter.handmade.network/blogs/p/7288-engine_work__global_illumination_with_irradiance_probes#22609
ivec2 OctMapStitch(ivec2 pix, bool leftBorder, bool rightBorder,
    bool upBorder, bool downBorder) {
    // Check if we're in one of the corners
    if (leftBorder && upBorder) {
        return ivec2(probeRes, probeRes);
    }
    else if (rightBorder && upBorder) {
        return ivec2(1, probeRes);
    }
    else if (leftBorder && downBorder) {
        return ivec2(probeRes, 1);
    }
    else if (rightBorder && downBorder) {
        return ivec2(1, 1);
    }
    // Check on which side we are
    if (upBorder) {
        return ivec2(probeRes - pix.x + 1, 1);
    }
    if (downBorder) {
        return ivec2(probeRes - pix.x + 1, probeRes);
    }
    if (leftBorder) {
        return ivec2(1, probeRes - pix.y + 1);
    }
    if (rightBorder) {
        return ivec2(probeRes, probeRes - pix.y + 1);
    }
}

void main() {

    int probeResWithBorder = probeRes + 2;
    ivec2 pix = ivec2(gl_FragCoord) % ivec2(probeResWithBorder);

	bool leftBorder = pix.x == 0;
    bool rightBorder = pix.x == probeResWithBorder - 1;

    bool upBorder = pix.y == 0;
    bool downBorder = pix.y == probeResWithBorder - 1;

    if (leftBorder || rightBorder || upBorder || downBorder) {
        ivec2 offset = OctMapStitch(pix, leftBorder, rightBorder, upBorder, downBorder);
        ivec2 base = (ivec2(gl_FragCoord) / ivec2(probeResWithBorder)) * probeResWithBorder;

        colorFS = texelFetch(volume, ivec3(base + offset, volumeLayer), 0);
    }
    else {
        discard;
    }

}
