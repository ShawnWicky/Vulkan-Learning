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

layout(set = 0, binding = 1) uniform Light
{
	vec3 position;
	vec3 colour;
} light;

layout(set = 0, binding = 2) uniform Ambient
{
	vec3 colour;
} ambient;


// In and Out
layout(location = 0) in vec3 v2fPos;
layout(location = 1) in vec3 v2fNormal;

layout(location = 0) out vec4 outColour;

// ambient factor

void main()
{
	//emit
	vec3 Cemit = vec3(uMaterial.emissive.xyz);

	//diffuse * ambient
	vec3 diffuse = vec3(uMaterial.diffuse.xyz);
	vec3 Camibent = diffuse * ambient.colour;

	//diffuse
	vec3 normal = normalize(v2fNormal);
	vec3 lightDir = normalize(light.position - v2fPos);
	const float PI = 3.1415926;
	vec3 Cdiff = dot(normal, lightDir) + diffuse * light.colour / PI;

	//specular
	vec3 viewDir = normalize(uScene.camPos - v2fPos);
	float shinessFactor = (uMaterial.shininess + 2.f) / 8.f;
	vec3 Cspec = 


	vec3 specular = 
}
