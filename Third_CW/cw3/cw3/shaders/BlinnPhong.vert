#version 450

layout(set = 0, binding = 0) uniform UScene
{
			mat4 camera;
			mat4 projection;
			mat4 projCam;
			vec3 camPos;
} uScene;


layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;

layout(location = 0) out vec3 v2fPos;
layout(location = 1) out vec3 v2fNormal;
void main()
{
	gl_Position = uScene.projCam * vec4(iPosition, 1.f);
	
    v2fNormal = iNormal;
	v2fPos = iPosition;
}

