#version 450

struct Light
{
	vec4 position;
	vec4 colour;
};


layout(set = 0, binding = 0) uniform UScene
{
			mat4 camera;
			mat4 projection;
			mat4 projCam;
			Light light[4];
			vec3 camPos;
			int constant;
} uScene;


layout(location = 0) in vec3 iPosition;


layout(location = 0) out vec3 v2fPos;

void main()
{
	gl_Position = uScene.projCam * vec4(iPosition, 1.f);
	
	v2fPos = iPosition;
}

