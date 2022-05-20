#version 450

layout(location = 0) in vec3 v2fNormal;

layout(location = 0) out vec4 outColour;

void main()
{
	vec3 normal = normalize(v2fNormal);

	outColour = vec4(normal, 1.f);
}
