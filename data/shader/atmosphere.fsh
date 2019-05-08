out vec3 fragColor;

#define PI 3.141592
#define iSteps 8
#define jSteps 8

in vec3 fPosition;

uniform vec3 cameraLocation;
uniform vec3 sunDirection;
uniform float sunIntensity;
uniform float atmosphereRadius;
uniform float planetRadius;
uniform vec3 planetCenter;

const float rayScaleHeight = 8.0e3f;
const float mieScaleHeight = 1.2e3f; 

void atmosphere(vec3 r, vec3 r0, vec3 pSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, out vec3 totalRlh, out vec3 totalMie);

const float gamma = 2.2f;

void main() {
	
	const float g = 0.7f;
	vec3 r = normalize(fPosition);
	vec3 pSun = normalize(-sunDirection);
	
	vec3 totalRlh;
	vec3 totalMie;
	
	atmosphere(
        normalize(fPosition),           // normalized ray direction
        vec3(0.0f, 1.0f, 0.0f),               // ray origin
        -sunDirection,                        // position of the sun
        planetRadius,                         // radius of the planet in meters
        atmosphereRadius,                         // radius of the atmosphere in meters
        vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
        21e-6,                          // Mie scattering coefficient
		totalRlh,
		totalMie
    );	
		
	// Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));
	
	fragColor = pow(pRlh * totalRlh + pMie * totalMie, vec3(gamma));
	
}

bool RayIntersection(vec3 r0,
	vec3 rd, 
	vec3 scenter, 
	float sr, 
	out float AO, 
	out float BO) {

	vec3 L = scenter - r0;
	float DT = dot(L, rd);
	float sr2 = sr * sr;
	
	float ct2 = dot(L, L) - DT * DT;
	
	if (ct2 > sr2)
		return false;
	
	float AT = sqrt(sr2 - ct2);
	float BT = AT;
	
	AO = DT - AT;
	BO = DT + BT;
	
	return true;
	
}

bool LightSampling(vec3 origin, 
	vec3 sunDirection,
	float planetRadius, 
	float atmosRadius, 
	out float opticalDepthMie,
	out float opticalDepthRay) {
	
	vec2 pa;
	RayIntersection(origin, sunDirection, planetCenter, atmosRadius, pa.x, pa.y);
	
	float time = 0.0f;
	
	opticalDepthMie = 0.0f;
	opticalDepthRay = 0.0f;
	
	float stepSize = pa.y / float(jSteps);
	
	for (int i = 0; i < jSteps; i++) {
		
		vec3 pos = origin + sunDirection * (time + 0.5f * stepSize);
		
		float height = distance(planetCenter, pos) - planetRadius;
		
		if (height < 0.0f)
			return false;
		
		opticalDepthMie += exp(-height / mieScaleHeight) * stepSize;
		opticalDepthRay += exp(-height / rayScaleHeight) * stepSize;
		
		time += stepSize;
		
	}	
	
	return true;
	
}

void atmosphere(vec3 r, vec3 r0, vec3 pSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, out vec3 totalRlh, out vec3 totalMie) {
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);
	
	totalRlh = vec3(0.0f);
    totalMie = vec3(0.0f);

	vec2 p;
	if (!RayIntersection(r0, r, planetCenter, rAtmos, p.x, p.y))
		return;
	
	p.x = max(0.0f, p.x);
	p.y = max(0.0f, p.y);
	
	vec2 pb;
	if (RayIntersection(r0, r, planetCenter, rPlanet, pb.x, pb.y))
		p.y = pb.x < 0.0f ? p.y : pb.x;
	
	if (pb.x > 0.0f)
		return;
	
    float iStepSize = (p.y - p.x) / float(iSteps);

    // Initialize the primary ray time.
    float iTime = p.x;

    // Initialize accumulators for Rayleigh and Mie scattering.

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0f;
    float iOdMie = 0.0f;
	
    // Sample the primary ray.
    for (int i = 0; i < iSteps; i++) {

        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5f);

        // Calculate the height of the sample.
        float iHeight = distance(iPos, planetCenter) - rPlanet;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float odStepRlh = exp(-iHeight / rayScaleHeight) * iStepSize;
        float odStepMie = exp(-iHeight / mieScaleHeight) * iStepSize;

        // Accumulate optical depth.
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        // Initialize optical depth accumulators for the secondary ray.
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        bool overground = LightSampling(iPos, pSun, rPlanet, rAtmos, jOdMie, jOdRlh);
		
		if (overground) {
		    // Calculate attenuation.
			vec3 transmittance = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

			// Accumulate scattering.
			totalRlh += odStepRlh * transmittance;
			totalMie += odStepMie * transmittance;		
		}

        // Increment the primary ray time.
        iTime += iStepSize;

    }
	
	totalMie = totalMie * sunIntensity * kMie;
	totalRlh = totalRlh * sunIntensity * kRlh;
	
}