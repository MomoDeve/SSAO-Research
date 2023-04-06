#version 330 core

layout (location = 0) out vec3 gAlbedo;
layout (location = 1) out vec3 gNormal;

in vec2 TexCoords;
in vec3 Normal;

void main()
{    
    gNormal = 0.5 * Normal + 0.5;
    gAlbedo.rgb = vec3(0.95);
}