#include "../structures"
#include "convert"

in vec3 fTexCoordProj;
in mat4 inverseModeMatrix;

out vec4 fragColor;

uniform sampler2D decalTexture;

// https://bartwronski.com/2015/03/12/fixing-screen-space-deferred-decals/
// https://mtnphil.wordpress.com/2014/05/24/decals-deferred-rendering/
// http://martindevans.me/game-development/2015/02/27/Drawing-Stuff-On-Other-Stuff-With-Deferred-Screenspace-Decals/

void main() {

	vec2 texCoord = ((fTexCoordProj.xy / fTexCoordProj.z) + 1.0f) / 2.0f;
	
	float depth = texture(decalTexture, texCoord).r;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, texCoord);

	fragColor = vec4(0.0f);

}

