out vec3 fragColor;

#define PI 3.141592

smooth in vec3 fPosition;
smooth in vec3 totalRlh;
smooth in vec3 totalMie;

uniform vec3 cameraLocation;
uniform vec3 sunDirection;

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g);

void main() {
	
	const float g = 0.99f;
	vec3 r = normalize(fPosition);
	vec3 pSun = normalize(-sunDirection);
	const float kMie = 21e-3;
	const vec3 kRlh = vec3(5.5e-3, 13.0e-3, 22.4e-3);
	const float iSun = 22.0f;
		
	// Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));
	
	fragColor = pRlh * totalRlh + pMie * totalMie;
	
}