#version 450

// Blinn-Phong material (example):

layout( set = 1, binding = 0) uniform UMaterial
{
	vec4 emissive;
	vec4 diffuse;
	vec4 specular;
	float shininess; // Last to make std140 alignment easier.
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

layout(location = 0) out vec4 outColour;

// ambient factor

void main()
{

	//ambient colour
	vec3 ambient = vec3(0.02f, 0.02f, 0.02f);
	//emit
	vec3 Cemit = uMaterial.emissive.xyz;

	//diffuse * ambient
	vec3 diffuse = uMaterial.diffuse.xyz;
	vec3 Camibent = diffuse * ambient;

	//normal
	vec3 normal = normalize(v2fNormal);
	//PI
	const float PI = 3.1415926f;
	
	//viewDir and shiniessFactor
	vec3 viewDir = normalize(uScene.camPos - v2fPos);
	float shinessFactor = (uMaterial.shininess + 2.f) / 8.f;

	vec4 fragColour = vec4((Cemit+Camibent),1.f);
	for(int i = 0; i < uScene.constant; i++)
	{
		//light direction
		vec3 lightDir = normalize(uScene.light[i].position.xyz - v2fPos);
		//diffuse
		vec3 Cdiff = max(0.f, dot(normal, lightDir)) * diffuse * uScene.light[i].colour.xyz / PI;
		//half way direction
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float Pspec = shinessFactor * (pow(max(dot(normal, halfwayDir), 0.f),uMaterial.shininess));
		//specular
		vec3 Cspec = Pspec * max(0.f,dot(normal, lightDir)) * uMaterial.specular.xyz * uScene.light[i].colour.xyz;

		fragColour += vec4((Cspec + Cdiff),1.f);
	}

	outColour = fragColour;
}
