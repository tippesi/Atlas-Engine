// Hit objects don't invoke miss shaders, the pipeline just needs one
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT uvec2 unusedPayload;

void main() {

}
