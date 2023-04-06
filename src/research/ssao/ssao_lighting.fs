#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gDepth;
uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D ssao;

uniform mat4 invViewProj;
uniform vec3 viewPos;

vec3 reconstructPosition(float z, vec2 texcoords)
{
    vec4 projected = vec4(2.0 * texcoords - 1.0, z, 1.0f);
    vec4 position = invViewProj * projected;
    return position.xyz / position.w;
}

vec3 lightInvDirection = vec3(0, 1, 0);

void main()
{             
    // retrieve data from gbuffer
    float fragDepth = texture(gDepth, TexCoords).r;
    vec3 fragPos = reconstructPosition(fragDepth, TexCoords);
    vec3 Normal = 2.0 * texture(gNormal, TexCoords).rgb - 1.0;
    vec3 Diffuse = texture(gAlbedo, TexCoords).rgb;
    float AmbientOcclusion = texture(ssao, TexCoords).r;
    
    float diffuseFactor = 0.5 * dot(Normal, lightInvDirection);
    float ambientFactor = 0.2;
    vec3 color = (diffuseFactor + ambientFactor) * Diffuse * AmbientOcclusion;
    FragColor.rgb = color;
}
