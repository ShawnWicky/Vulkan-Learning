#version 450
#extension GL_KHR_vulkan_glsl: enable
// Blinn-Phong material (example):

layout( set = 1, binding = 0, std140 ) uniform UMaterial
{
	vec4 emissive;
	vec4 albedo;
	float shininess;
	float metalness;
} uMaterial;

// In and Out
layout(location = 0) in vec3 v2fPos;
layout(location = 1) in vec3 v2fNormal;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outColour;

// ambient factor

void main()
{
	outPosition = vec4(v2fPos, 1.f);

	outNormal = vec4(v2fNormal, 1.f);
	
	outColour = uMaterial.albedo;
}