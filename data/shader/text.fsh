layout (location = 0) out vec4 color;

in vec2 fTexCoord;

uniform sampler2D diffuseMap;
uniform vec3 diffuseColor;

void main()
{    
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(diffuseMap, fTexCoord).r);
    color = vec4(diffuseColor, 1.0) * sampled;
}