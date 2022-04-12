#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#extension GL_GOOGLE_include_directive : enable
#include "ubo.inc"

layout (binding = 1) uniform sampler2D shadowMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec4 inShadowCoord;

layout (constant_id = 0) const int enablePCF = 0;

layout (location = 0) out vec4 outFragColor;

#define ambient 0.1

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambient;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main() 
{	
	float shadow = (enablePCF == 1) ? filterPCF(inShadowCoord / inShadowCoord.w) : textureProj(inShadowCoord / inShadowCoord.w, vec2(0.0));

	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec.xyz);
	vec3 R = normalize(-reflect(L, N));
	vec3 diffuse = max(dot(R, V), ambient) * vec3(inColor.xyz);

	outFragColor = vec4(diffuse * shadow, 1.0);
        //HACK
	outFragColor = inColor;
	outFragColor = vec4(diffuse,1.0);


    vec3 E;
    vec4 Iamb =vec4(0.0,0.0,0.0,0.0);
    vec4 Idiff=vec4(0.0,0.0,0.0,0.0);
    vec4 Ispec=vec4(0.0,0.0,0.0,0.0);

    vec3 norm = normalize(N);

    E = normalize(-inViewVec.xyz);

    R = normalize(-reflect(inLightVec,norm));
    Idiff += max(dot(N,L), 0.0);
    float y = 1.0;
    Ispec += pow(max(dot(R,E),0.0),0.2*128.0);
    Ispec.a = y;

    vec4 color = inColor*1.2*Idiff+Ispec;

    float FogFragCoord = abs(inViewVec.z/inViewVec.w);
    float fogFactor = (ubo.fog_end - FogFragCoord)/(ubo.fog_end - ubo.fog_start);
    fogFactor = clamp(fogFactor,0.0,1.0);
    
    outFragColor = mix(ubo.fogColor, color, fogFactor );

}
