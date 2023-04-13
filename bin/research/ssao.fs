#version 330 core

out float FragColor;

in vec2 TexCoord;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[16];

uniform float sampleRadius = 0.5;
uniform float bias = 0.025;

const float DEPTH_RANGE_MAX = 0.02;

const int kernelSize = 16;

const float SCREEN_WIDTH = 1600;
const float SCREEN_HEIGHT = 900;
const vec2 noiseScale = vec2(SCREEN_WIDTH / 4.0, SCREEN_HEIGHT / 4.0); 

uniform mat4 proj;
uniform mat4 invProj;

vec3 reconstructPosition(float z, vec2 texcoords)
{
    vec4 projected = vec4(2.0 * texcoords - 1.0, z, 1.0f);
    vec4 position = invProj * projected;
    return position.xyz / position.w;
}

mat3 computeTBN(vec3 normal)
{
    vec3 randomVec = texture(texNoise, TexCoord * noiseScale).xyz;

    vec3 tangent = cross(randomVec, normal);
    vec3 bitangent = cross(normal, tangent);
    return mat3(tangent, bitangent, normal);
}

vec4 projectPosition(vec3 position)
{
    vec4 p = vec4(position, 1.0);
    p = proj * p;
    p.xyz /= p.w;
    p.xy = p.xy * 0.5 + 0.5;
    return p;
}

void main()
{
    float fragDepth = texture(gDepth, TexCoord).r;
    vec3 fragPos = reconstructPosition(fragDepth, TexCoord);
    vec3 normal = 2.0 * texture(gNormal, TexCoord).rgb - 1.0;
    mat3 TBN = computeTBN(normal);
    
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        vec3 samplePos = TBN * samples[i];
        samplePos = fragPos + samplePos * sampleRadius; 
        
        vec4 offset = projectPosition(samplePos);

        float sampleDepth = texture(gDepth, offset.xy).r;
        vec3 sampleFragPos = reconstructPosition(sampleDepth, offset.xy);
        
        float rangeCheck = step(1.0, dot(samplePos - fragPos, samplePos - fragPos) / DEPTH_RANGE_MAX);
        occlusion += (sampleFragPos.z >= fragPos.z + bias ? 1.0 : 0.0) * rangeCheck;  
    }

    occlusion = 1.0 - (occlusion / kernelSize);
    
    FragColor = occlusion;
}
