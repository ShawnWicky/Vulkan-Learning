#version 450

layout(set = 0, binding = 0) uniform UScene
{
			mat4 camera;
			mat4 projection;
			mat4 projCam;
			vec3 position;
			vec3 colour;
			vec3 camPos;
} uScene;


layout(location = 0) in vec3 v2fPos;

layout(location = 0) out vec4 outColour;

void main()
{
	vec3 lightDir = normalize(uScene.position - v2fPos); 
	
	outColour = vec4(lightDir, 1.f);
}
