#include <structures.hsh>

// Updates the bounds of a BVH with a fixed topology. Each dispatch updates the nodes of one tree level,
// starting with the deepest level, such that the bounds of the child nodes are always up to date.

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 0) buffer Nodes {
    PackedBVHNode nodes[];
};

layout(std430, set = 3, binding = 1) readonly buffer BVHTriangles {
    PackedBVHTriangle bvhTriangles[];
};

layout(std430, set = 3, binding = 2) readonly buffer LevelNodes {
    int levelNodes[];
};

// Bounds of each node (min, max) for its parent
layout(std430, set = 3, binding = 3) buffer NodeBounds {
    vec4 nodeBounds[];
};

layout(push_constant) uniform constants {
    uint nodeOffset;
    uint nodeCount;
} PushConstants;

void GetChildBounds(int nodePtr, out vec3 boundsMin, out vec3 boundsMax) {

    if (nodePtr >= 0) {
        boundsMin = nodeBounds[2 * nodePtr].xyz;
        boundsMax = nodeBounds[2 * nodePtr + 1].xyz;
        return;
    }

    boundsMin = vec3(1e30);
    boundsMax = vec3(-1e30);

    int triPtr = ~nodePtr;
    bool endOfNode = false;
    while (!endOfNode) {
        vec3 v0 = bvhTriangles[triPtr].v0.xyz;
        vec3 v1 = bvhTriangles[triPtr].v1.xyz;
        vec3 v2 = bvhTriangles[triPtr].v2.xyz;
        endOfNode = bvhTriangles[triPtr].v0.w > 0.0;

        boundsMin = min(boundsMin, min(v0, min(v1, v2)));
        boundsMax = max(boundsMax, max(v0, max(v1, v2)));
        triPtr++;
    }

}

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.nodeCount)
        return;

    int nodeIdx = levelNodes[PushConstants.nodeOffset + idx];

    int leftPtr = floatBitsToInt(nodes[nodeIdx].data3.x);
    int rightPtr = floatBitsToInt(nodes[nodeIdx].data3.y);

    vec3 leftMin, leftMax, rightMin, rightMax;
    GetChildBounds(leftPtr, leftMin, leftMax);
    GetChildBounds(rightPtr, rightMin, rightMax);

    nodes[nodeIdx].data0 = vec4(leftMin, leftMax.x);
    nodes[nodeIdx].data1 = vec4(leftMax.yz, rightMin.xy);
    nodes[nodeIdx].data2 = vec4(rightMin.z, rightMax);

    nodeBounds[2 * nodeIdx] = vec4(min(leftMin, rightMin), 0.0);
    nodeBounds[2 * nodeIdx + 1] = vec4(max(leftMax, rightMax), 0.0);

}
