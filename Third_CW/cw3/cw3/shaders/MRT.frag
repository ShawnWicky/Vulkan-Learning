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
			int constant;

} uScene;

// In and Out
layout(location = 0) in vec3 v2fPos;
layout(location = 1) in vec3 v2fNormal;

layout(location = 0) out vec4 oPosition;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oEmissive;
layout(location = 3) out vec4 oAlbedo;

void main()
{
	oPosition = vec4(v2fPos, 1.f);
	oNormal = vec4(v2fNormal, 1.f);
	oEmissive = vec4(uMaterial.emissive.xyz, uMaterial.shininess);
	oAlbedo = vec4(uMaterial.albedo.xyz, uMaterial.metalness);
}