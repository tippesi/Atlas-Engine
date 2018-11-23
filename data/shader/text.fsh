layout (location = 0) out vec4 color;

in vec3 fTexCoord;

uniform sampler2DArray glyphsTexture;
uniform vec3 diffuseColor = vec3(1.0f);

void main() {    
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(glyphsTexture, fTexCoord).r);
    color = vec4(diffuseColor, 1.0) * sampled;
}