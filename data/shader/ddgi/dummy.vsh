// Dummy vertex shader for all the fragment shader

layout(location=0) in vec2 vPosition;

void main() {

    gl_Position = vec4(vPosition, 0.0, 1.0);
    
}