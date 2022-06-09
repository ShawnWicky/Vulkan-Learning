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
			mat4 viewInv;
			mat4 projectionInv;
			Light light[4];
			vec3 camPos;
			int constant;

} uScene;

layout(set = 1, binding = 0) uniform sampler2D TexDepth;
layout(set = 1, binding = 1) uniform sampler2D TexNorm;
layout(set = 1, binding = 2) uniform sampler2D TexEmissive;
layout(set = 1, binding = 3) uniform sampler2D TexAlbedo;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 oColour;



vec3 GetWorldPos(float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(inUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = uScene.projectionInv * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = uScene.viewInv * viewSpacePosition;

    return worldSpacePosition.xyz;
}



void main()
{
	//pass info from G-buffer
	float fragDepth= texture(TexDepth, inUV).x;
	vec3 fragPos = GetWorldPos(fragDepth);

	vec3 fragNorm = texture(TexNorm, inUV).xyz;
	vec4 rawEmissive = texture(TexEmissive, inUV);
	vec4 rawAlbedo = texture(TexAlbedo, inUV);
	vec3 fragEmissive = rawEmissive.xyz;
	float fragShininess = rawEmissive.w;
	vec3 fragAlbedo = rawAlbedo.xyz;
	float fragMetalness = rawAlbedo.w;
	
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

	oColour = fragColour;

}