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
}
