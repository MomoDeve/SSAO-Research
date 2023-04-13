#version 330 core

layout (location = 0) out vec3 gAlbedo;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out float gDepth;

in vec2 TexCoord;
in vec3 Normal;
in vec3 Position;

uniform vec4 clipInfo;

void main()
{    
    gNormal = Normal;
    gAlbedo.rgb = vec3(0.95);
    gDepth = (-Position.z - clipInfo.x) / (clipInfo.y - clipInfo.x);
}