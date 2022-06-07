#version 450
#extension GL_KHR_vulkan_glsl: enable

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

layout(set = 1, binding = 0) uniform sampler2D TexPos;
layout(set = 1, binding = 1) uniform sampler2D TexNorm;
layout(set = 1, binding = 2) uniform sampler2D TexEmissive;
layout(set = 1, binding = 3) uniform sampler2D TexAlbedo;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 oColour;

void main()
{
	//pass info from G-buffer
	vec3 fragPos = texture(TexPos, inUV).xyz;
	vec3 fragNorm = texture(TexNorm, inUV).xyz;
	vec3 fragEmissive = texture(TexEmissive, inUV).xyz;
	vec3 fragAlbedo = texture(TexAlbedo, inUV).xyz;
	float fragShininess = texture(TexEmissive, inUV).w;
	float fragMetalness = texture(TexAlbedo, inUV).w;
	
	//modular code
	const float PI = 3.1415926f;
	vec3 normal = normalize(fragNorm);
	vec3 viewDir = normalize(uScene.camPos - fragPos);
	float nv = max(dot(normal, viewDir), 0.f);

	//Le
	vec3 Lemit = fragEmissive;

	//Lamibent
	vec3 Lamibent = vec3(0.02f, 0.02f, 0.02f) * fragAlbedo;
	
	//Fresnel Term
	vec3 F0 = (1 - fragMetalness) * vec3(0.04f, 0.04f, 0.04f) + fragMetalness * fragAlbedo;
	//multiple lights

	vec4 fragColour = vec4((Lemit + Lamibent),1.f);

	for(int i = 0; i < uScene.constant; i++)
	{
		vec3 lightDir = normalize(uScene.light[i].position.xyz - fragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float nh = max(dot(normal, halfwayDir), 0.f);
		float nl = max(dot(normal, lightDir), 0.f);
		float vh = dot(viewDir, halfwayDir);

		//Fresnel Term
		vec3 F = F0 + (1 - F0) * pow((1-dot(halfwayDir, viewDir)), 5); 

		//Ldiffuse
		vec3 Ldiffuse = (fragAlbedo/PI) * (vec3(1.f, 1.f, 1.f) - F) * (1 -fragMetalness);

		//normal distribution function
		float D = ((fragShininess+2) / 2*PI) * pow(nh, fragShininess);

		//masking term
		float G = min(1, min(2*nh*nv/vh, 2*nh*nl/vh));

		//BRDF
		vec3 BRDF = Ldiffuse + (D * F * G / 4 * nv * nl);

		//specular 
		vec3 Lspec = BRDF * uScene.light[i].colour.xyz * nl;

		fragColour += vec4(Lspec, 1.f);
	}

	oColour =  fragColour;
}