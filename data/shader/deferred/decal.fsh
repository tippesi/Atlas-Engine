#include "../structures"
#include "convert"

in vec3 fTexCoordProj;
in mat4 inverseModelMatrix;

out vec4 fragColor;

uniform sampler2D depthTexture;
uniform sampler2D decalTexture;
uniform mat4 ivMatrix;

// https://bartwronski.com/2015/03/12/fixing-screen-space-deferred-decals/
// https://mtnphil.wordpress.com/2014/05/24/decals-deferred-rendering/
// http://martindevans.me/game-development/2015/02/27/Drawing-Stuff-On-Other-Stuff-With-Deferred-Screenspace-Decals/

void main() {

	vec2 texCoord = ((fTexCoordProj.xy / fTexCoordProj.z) + 1.0f) / 2.0f;
	
	float depth = texture(depthTexture, texCoord).r;
	
	vec3 worldPos = vec3(ivMatrix * vec4(ConvertDepthToViewSpace(depth, texCoord), 1.0f));
	vec3 objectPos = vec3(inverseModelMatrix * vec4(worldPos, 1.0f));	
	
	if (1.0f - abs(objectPos.x) < 0.0f ||
		1.0f - abs(objectPos.y) < 0.0f ||
		1.0f - abs(objectPos.z) < 0.0f)
		discard;
	
	vec2 textureCoord = 0.5f * objectPos.xz + 0.5f;

	fragColor = texture(decalTexture, textureCoord);

}