#version 450
#extension GL_KHR_vulkan_glsl: enable
// PBR material (example):
layout(set = 1, binding = 0) uniform sampler2D samplerPosition;
layout(set = 1, binding = 1) uniform sampler2D samplerNormal;
layout(set = 1, binding = 2) uniform sampler2D samplerAlbedo;
layout(set = 1, binding = 3) uniform sampler2D samplerEmissive;
//layout(set = 1, binding = 4) uniform sampler2D samplerMetalness;
//layout(set = 1, binding = 5) uniform sampler2D samplerShininess;
 
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


layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;


void main()
{
	vec3 fragPos = texture(samplerPosition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 raw_albedo = texture(samplerAlbedo, inUV);
	vec4 raw_emissive = texture(samplerEmissive, inUV);
	vec3 albedo = raw_albedo.xyz;
	float shininess = raw_albedo.w;
	vec3 emissive = raw_emissive.xyz;
	float metalness = raw_emissive.w;
	//modular code :)
	const float PI = 3.1415926f;
	vec3 Nnormal = normalize(normal);
	vec3 viewDir = normalize(uScene.camPos - fragPos);
	float nv = max(dot(Nnormal, viewDir), 0.f);

	//Le
	vec3 Lemit = emissive;

	//Lamibent
	vec3 Lamibent = vec3(0.02f, 0.02f, 0.02f) * albedo.xyz;
	
	//Fresnel Term
	vec3 F0 = (1 - metalness) * vec3(0.04f, 0.04f, 0.04f) + metalness * albedo;
	//multiple lights

	vec4 fragColour = vec4((Lemit + Lamibent),1.f);

	for(int i = 0; i < uScene.constant; i++)
	{
		vec3 lightDir = normalize(uScene.light[i].position.xyz - fragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float nh = max(dot(Nnormal, halfwayDir), 0.f);
		float nl = max(dot(Nnormal, lightDir), 0.f);
		float vh = dot(Nnormal, halfwayDir);

		//Fresnel Term
		vec3 F = F0 + (1 - F0) * pow((1-dot(halfwayDir, viewDir)), 5); 

		//Ldiffuse
		vec3 Ldiffuse = (albedo/PI) * (vec3(1.f, 1.f, 1.f) - F) * (1 - metalness);

		//normal distribution function
		float D = ((shininess+2) / 2*PI) * pow(nh, shininess);

		//masking term
		float G = min(1, min(2*nh*nv/vh, 2*nh*nl/vh));

		//BRDF
		vec3 BRDF = Ldiffuse + (D * F * G / 4 * nv * nl);

		//specular 
		vec3 Lspec = BRDF * uScene.light[i].colour.xyz * nl;

		fragColour += vec4(Lspec, 1.f);
	}

	outColour =  fragColour;
}
