layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2DArray displacementMap;
layout (set = 3, binding = 1, rgba32f) readonly uniform image2DArray pingpongMap;

void main() {

    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    
    float perm = bool((coord.x + coord.y) & 1) ? -1.0 : 1.0;
    
    vec3 displacement;
    
    float x = imageLoad(pingpongMap, coord).b;
    float y = imageLoad(pingpongMap, coord).r;
    float z = imageLoad(pingpongMap, coord).a;
        
    displacement = perm * vec3(x, y, z); 
    
    imageStore(displacementMap, coord, 
            vec4(displacement, 1.0));

}