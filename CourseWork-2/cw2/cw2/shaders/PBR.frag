#version 450

// PBR material (example):
layout( set = 1, binding = 0, std140 ) uniform UMaterial
{
	vec4 emissive;
	vec4 albedo;
	float shininess;
	float metalness;
} uMaterial;

// Light and Uniform Buffer
layout(set = 0, binding = 0) uniform UScene
{
			mat4 camera;
			mat4 projection;
			mat4 projCam;
			vec3 camPos;
} uScene;

layout(set = 0, binding = 1, std140) uniform Light
{
	vec3 position;
	vec3 colour;
} light;

// In and Out
layout(location = 0) in vec3 v2fPos;
layout(location = 1) in vec3 v2fNormal;

layout(location = 0) out vec4 outColour;

void main()
{
	//modular code :)
	vec3 normal = normalize(v2fNormal);
	vec3 lightDir = normalize(light.position - v2fPos);
	vec3 viewDir = normalize(uScene.camPos - v2fPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	const float PI = 3.1415926f;
	float nh = max(dot(normal, halfwayDir), 0.f);
	float nv = max(dot(normal, viewDir), 0.f);
	float nl = max(dot(normal, lightDir), 0.f);
	float vh = dot(normal, halfwayDir);

	//Le
	vec3 Lemit = uMaterial.emissive.xyz;

	//Lamibent
	vec3 Lamibent = vec3(0.02f, 0.02f, 0.02f) * uMaterial.albedo.xyz;

	//Fresnel Term
	vec3 F0 = (1 - uMaterial.metalness) * vec3(0.04f, 0.04f, 0.04f) + uMaterial.metalness * uMaterial.albedo.xyz;   
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
	vec3 Lspec = BRDF * light.colour * nl;

	outColour = vec4((Lemit + Lamibent + Lspec), 1.f);
	//outColour = vec4(G*F, 1.f);
}
