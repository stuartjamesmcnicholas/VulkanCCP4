layout (std140, binding = 0) uniform UBO 
{
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 lightSpace;
    vec4 lightPos;
    vec4 fogColor;
    float fog_start;
    int _pad0;
    float fog_end;
    int _pad1;
} ubo;


