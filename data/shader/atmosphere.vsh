layout(location=0)in vec3 vPosition;

// This shader has issues when going from day to night while being in the outer atmosphere, because the integration 
// along the camera ray is somewhat limited due to performance concerns.
// https://atomworld.wordpress.com/2014/12/22/flexible-physical-accurate-atmosphere-scattering-part-1/

out vec3 fPosition;
out vec3 totalRlh;
out vec3 totalMie;

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform vec3 cameraLocation;
uniform vec3 sunDirection;

const vec3 planetCenter = -vec3(0.0f, 6371, 0.0f);
const float rayScaleHeight = 8.0f;
const float mieScaleHeight = 1.2f; 

void atmosphere(vec3 r, vec3 r0, vec3 pSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie);

void main() {
	
    vec4 pos = vec4(vPosition * 6871.0f - vec3(0.0f, 6371, 0.0f), 1.0);
	fPosition = pos.xyz - cameraLocation;
	gl_Position = (pMatrix * vMatrix * pos).xyww;
	
	atmosphere(
        normalize(fPosition),           // normalized ray direction
        vec3(cameraLocation),               // ray origin
        -sunDirection,                        // position of the sun
        6371,                         // radius of the planet in meters
        6871,                         // radius of the atmosphere in meters
        vec3(5.5e-3, 13.0e-3, 22.4e-3), // Rayleigh scattering coefficient
        21e-3                          // Mie scattering coefficient
    );	
	
}

#define PI 3.141592
#define iSteps 16
#define jSteps 8

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

void atmosphere(vec3 r, vec3 r0, vec3 pSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie) {
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);
	
	totalRlh = vec3(0,0,0);
    totalMie = vec3(0,0,0);

	vec2 p;
	if (!RayIntersection(r0, r, planetCenter, rAtmos, p.x, p.y))
		return;
	
	p.x = max(0.0f, p.x);
	p.y = max(0.0f, p.y);
	
	vec2 pb;
	if (RayIntersection(r0, r, planetCenter, rPlanet, pb.x, pb.y))
		p.y = pb.x < 0 ? p.y : pb.x;
	
	if (pb.x > 0)
		return;
	
    float iStepSize = (p.y - p.x) / float(iSteps);

    // Initialize the primary ray time.
    float iTime = p.x;

    // Initialize accumulators for Rayleigh and Mie scattering.

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0;
    float iOdMie = 0.0;
	
    // Sample the primary ray.
    for (int i = 0; i < iSteps; i++) {

        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);

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
	
	totalMie = totalMie * 20.0f * kMie;
	totalRlh = totalRlh * 20.0f * kRlh;
	
}