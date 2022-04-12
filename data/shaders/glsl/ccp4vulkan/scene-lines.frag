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

void main() 
{	
    vec4 color = inColor;

    float FogFragCoord = abs(inViewVec.z/inViewVec.w);
    float fogFactor = (ubo.fog_end - FogFragCoord)/(ubo.fog_end - ubo.fog_start);
    fogFactor = clamp(fogFactor,0.0,1.0);
    
    outFragColor = mix(ubo.fogColor, color, fogFactor );

}
