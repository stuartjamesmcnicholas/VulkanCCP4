#version 450

#extension GL_GOOGLE_include_directive : enable
#include "../ccp4vulkan/ubo.inc"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 inViewVec;
layout (location = 3) in vec3 inLightVec;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    vec3 N = normalize(inNormal);
    vec3 L = normalize(inLightVec);
    vec3 V = normalize(inViewVec.xyz);
    vec3 R = normalize(-reflect(L, N));
    
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
