#include <../common/normalencode.hsh>

layout (location = 0) out vec2 normal;

layout(location=0) in vec3 fNormal;

void main() {
	
    normal = EncodeNormal(fNormal);
	
}