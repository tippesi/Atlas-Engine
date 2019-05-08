#include "../common/PI"
#include "../common/complex"

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) writeonly uniform image2D butteflyTextureX;
layout (binding = 1, rgba32f) writeonly uniform image2D butteflyTextureY;
layout (binding = 2, rgba32f) writeonly uniform image2D butteflyTextureZ;

uniform int stage;

void main() {

	

}