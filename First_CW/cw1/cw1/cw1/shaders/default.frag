#version 450


layout(location = 0) in vec3 v2fColour;

layout(location = 0) out vec4 outColour;

void main()
{
	outColour = vec4(v2fColour, 1.f);
}
