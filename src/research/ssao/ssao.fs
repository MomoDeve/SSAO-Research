#version 330 core

out float FragColor;

in vec2 TexCoords;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
int kernelSize = 64;
float radius = 0.5;

// tile noise texture over screen based on screen dimensions divided by noise size
const float SCREEN_WIDTH = 1600;
const float SCREEN_HEIGHT = 900;
const vec2 noiseScale = vec2(SCREEN_WIDTH / 4.0, SCREEN_HEIGHT / 4.0); 

uniform mat4 viewProj;
uniform mat4 invViewProj;

vec3 reconstructPosition(float z, vec2 texcoords)
{
    vec4 projected = vec4(2.0 * texcoords - 1.0, z, 1.0f);
    vec4 position = invViewProj * projected;
    return position.xyz / position.w;
}

mat3 computeTBN(vec3 normal)
{
    vec3 randomVec = 2.0 * normalize(texture(texNoise, TexCoords * noiseScale).xyz) - 1.0;

    vec3 tangent = cross(randomVec, normal);
    vec3 bitangent = cross(normal, tangent);
    return mat3(tangent, bitangent, normal);
}

void main()
{
    float fragDepth = texture(gDepth, TexCoords).r;
    vec3 fragPos = reconstructPosition(fragDepth, TexCoords);
    vec3 normal = 2.0 * texture(gNormal, TexCoords).rgb - 1.0;
    mat3 TBN = computeTBN(normal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    float bias = (1.0 - fragDepth) * 0.01;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = fragPos + samplePos * radius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset = viewProj * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xy = offset.xy * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
        float sampleDepth = texture(gDepth, offset.xy).r; // get depth value of kernel sample

        float offscreenFading = 1.0 - dot(TexCoords - 0.5, TexCoords - 0.5);
        offscreenFading *= offscreenFading;
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragDepth - sampleDepth));
        occlusion += (fragDepth >= sampleDepth + bias ? 1.0 : 0.0) * rangeCheck * offscreenFading;           
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    
    FragColor = occlusion;
}
