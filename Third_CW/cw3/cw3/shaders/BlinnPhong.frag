#version 450
#extension GL_KHR_vulkan_glsl: enable
layout(set = 0, binding = 0) uniform sampler2D inColour;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColour;

void main()
{	
	outColour = texture(inColour, inUV);
}
