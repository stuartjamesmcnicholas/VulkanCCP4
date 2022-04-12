#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;

#extension GL_GOOGLE_include_directive : enable
#include "ubo.inc"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec4 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec4 outShadowCoord;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() 
{
    outColor = inColor;

    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos.xyz, 1.0);

    vec4 pos = ubo.model * vec4(inPos, 1.0);
    outNormal = (transpose(inverse(ubo.view)) *ubo.model * vec4(inNormal,1.0)).xyz;
    outLightVec = ubo.lightPos.xyz;
    outViewVec = (ubo.view * ubo.model * vec4(inPos.xyz, 1.0));			

    //This is probably nonsense.
    outShadowCoord = ( biasMat * ubo.lightSpace * ubo.model ) * vec4(inPos, 1.0);	

}

