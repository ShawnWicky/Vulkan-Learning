#version 450
#extension GL_KHR_vulkan_glsl: enable

layout (location = 0) in vec2 v2fTexCoord;

layout (set = 1, binding = 0) uniform sampler2D texCoord;

layout (location = 0) out vec4 oColour;

void main()
{
	oColour = vec4(texture(texCoord, v2fTexCoord).rgb, 1.f);
}
