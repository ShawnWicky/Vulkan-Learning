#version 450

// PBR material (example):
layout( set = 1, binding = 0, std140 ) uniform UMaterial
{
	vec4 emissive;
	vec4 albedo;
	float shininess;
	float metalness;
} uMaterial;

struct Light
{
	vec4 position;
	vec4 colour;
};

// Light and Uniform Buffer
layout(set = 0, binding = 0) uniform UScene
{
			mat4 camera;
			mat4 projection;
			mat4 projCam;
			Light light[4];
			vec3 camPos;

} uScene;

// In and Out
layout(location = 0) in vec3 v2fPos;
layout(location = 1) in vec3 v2fNormal;

layout(location = 0) out vec4 outColour;

void main()
{
	//modular code :)
	const float PI = 3.1415926f;
	vec3 normal = normalize(v2fNormal);
	vec3 viewDir = normalize(uScene.camPos - v2fPos);
	float nv = max(dot(normal, viewDir), 0.f);

	//Le
	vec3 Lemit = uMaterial.emissive.xyz;

	//Lamibent
	vec3 Lamibent = vec3(0.02f, 0.02f, 0.02f) * uMaterial.albedo.xyz;
	
	//Fresnel Term
	vec3 F0 = (1 - uMaterial.metalness) * vec3(0.04f, 0.04f, 0.04f) + uMaterial.metalness * uMaterial.albedo.xyz;
	//multiple lights

	vec4 fragColour = vec4((Lemit + Lamibent),1.f);

	for(int i = 0; i < 4; i++)
	{
		vec3 lightDir = normalize(uScene.light[i].position.xyz - v2fPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float nh = max(dot(normal, halfwayDir), 0.f);
		float nl = max(dot(normal, lightDir), 0.f);
		float vh = dot(normal, halfwayDir);

		//Fresnel Term
		vec3 F = F0 + (1 - F0) * pow((1-dot(halfwayDir, viewDir)), 5); 

		//Ldiffuse
		vec3 Ldiffuse = (uMaterial.albedo.xyz/PI) * (vec3(1.f, 1.f, 1.f) - F) * (1 - uMaterial.metalness);

		//normal distribution function
		float D = ((uMaterial.shininess+2) / 2*PI) * pow(nh, uMaterial.shininess);

		//masking term
		float G = min(1, min(2*nh*nv/vh, 2*nh*nl/vh));

		//BRDF
		vec3 BRDF = Ldiffuse + (D * F * G / 4 * nv * nl);

		//specular 
		vec3 Lspec = BRDF * uScene.light[i].colour.xyz * nl;

		fragColour += vec4(Lspec, 1.f);
	}

	outColour =  fragColour;
}
