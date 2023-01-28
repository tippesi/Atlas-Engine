layout (local_size_x = 8, local_size_y = 8) in;

#ifdef IRRADIANCE
layout (set = 3, binding = 0, rgb10_a2) uniform image2DArray image;
#else
layout (set = 3, binding = 0, rg16f) uniform image2DArray image;
#endif

layout(push_constant) uniform constants {
	int probeRes;
} PushConstants;

ivec2 OctMapStitch(ivec2 pix, bool leftBorder, bool rightBorder,
    bool upBorder, bool downBorder) {
    // Check if we're in one of the corners
    if (leftBorder && upBorder) {
        return ivec2(PushConstants.probeRes, PushConstants.probeRes);
    }
    else if (rightBorder && upBorder) {
        return ivec2(1, PushConstants.probeRes);
    }
    else if (leftBorder && downBorder) {
        return ivec2(PushConstants.probeRes, 1);
    }
    else if (rightBorder && downBorder) {
        return ivec2(1, 1);
    }
    // Check on which side we are
    if (upBorder) {
        return ivec2(PushConstants.probeRes - pix.x + 1, 1);
    }
    if (downBorder) {
        return ivec2(PushConstants.probeRes - pix.x + 1, PushConstants.probeRes);
    }
    if (leftBorder) {
        return ivec2(1, PushConstants.probeRes - pix.y + 1);
    }
    if (rightBorder) {
        return ivec2(PushConstants.probeRes, PushConstants.probeRes - pix.y + 1);
    }
}

void main() {

	ivec2 pixel = ivec2(gl_GlobalInvocationID);
	if (pixel.x > imageSize(image).x ||
		pixel.y > imageSize(image).y)
		return;

    int probeResWithBorder = PushConstants.probeRes + 2;
    ivec2 pix = pixel % ivec2(probeResWithBorder);

	bool leftBorder = pix.x == 0;
    bool rightBorder = pix.x == probeResWithBorder - 1;

    bool upBorder = pix.y == 0;
    bool downBorder = pix.y == probeResWithBorder - 1;

    if (leftBorder || rightBorder || upBorder || downBorder) {
        ivec2 offset = OctMapStitch(pix, leftBorder, rightBorder, upBorder, downBorder);
        ivec2 base = (pixel / ivec2(probeResWithBorder)) * probeResWithBorder;

        vec4 color = imageLoad(image, ivec3(base + offset, int(gl_GlobalInvocationID.z)));
        imageStore(image, ivec3(pixel, int(gl_GlobalInvocationID.z)), color);
    }

}