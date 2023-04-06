#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D ao;

vec3 lightInvDirection = vec3(0, 1, 0);

void main()
{             
    // retrieve data from gbuffer
    vec3 Normal = 2.0 * texture(gNormal, TexCoord).rgb - 1.0;
    vec3 Diffuse = texture(gAlbedo, TexCoord).rgb;
    float AmbientOcclusion = texture(ao, TexCoord).r;
    
    float diffuseFactor = 0.5 * dot(Normal, lightInvDirection);
    float ambientFactor = 0.2;
    vec3 color = (diffuseFactor + ambientFactor) * Diffuse * AmbientOcclusion;
    FragColor.rgb = color;
}
