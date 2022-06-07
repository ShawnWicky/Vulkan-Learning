#version 450
#extension GL_KHR_vulkan_glsl: enable
layout(set = 0, binding = 0) uniform sampler2D inColour;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColour;

void main()
{	
        float Pixels = 500.0;
        float dx = 8.0 * (1.0 / Pixels);
        float dy = 8.0 * (1.0 / Pixels);
        vec2 Coord = vec2(dx * floor(inUV.x / dx),
                          dy * floor(inUV.y / dy));
        outColour = texture(inColour, Coord);
}
