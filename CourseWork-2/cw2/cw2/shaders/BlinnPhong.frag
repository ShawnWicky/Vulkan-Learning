#version 450

// Blinn-Phong material (example):

layout( set = 1, binding = 0, std140 ) uniform UMaterial
{
	vec4 emissive;
	vec4 diffuse;
	vec4 specular;
	float shininess; // Last to make std140 alignment easier.
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

// ambient factor

void main()
{

	//ambient colour
	vec3 ambient = vec3(0.1f, 0.1f, 0.1f);
	//emit
	vec3 Cemit = vec3(uMaterial.emissive.xyz);

	//diffuse * ambient
	vec3 diffuse = vec3(uMaterial.diffuse.xyz);
	vec3 Camibent = diffuse * ambient;

	//diffuse
	vec3 normal = normalize(v2fNormal);
	vec3 lightDir = normalize(light.position - v2fPos);
	const float PI = 3.1415926f;
	vec3 Cdiff = max(0.f, dot(normal, lightDir)) + diffuse * light.colour / PI;

	//specular
	vec3 viewDir = normalize(uScene.camPos - v2fPos);
	float shinessFactor = (uMaterial.shininess + 2.f) / 8.f;
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float Pspec = shinessFactor * (pow(max(dot(normal, halfwayDir), 0.f),uMaterial.shininess));
	vec3 Cspec = Pspec * max(0.f,dot(normal, lightDir)) * vec3(uMaterial.specular.xyz) * light.colour;

	outColour = vec4((Cemit + Camibent + Cdiff + Cspec),1.f);
	outColour = vec4(Cemit,1.f);
}
