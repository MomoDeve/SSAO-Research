// GTAO algorithm implementation. Code used as a reference: https://github.com/asylum2010/Asylum_Tutorials/blob/master/Media/ShadersGL/gtaoreference.frag 

#version 330

#define PI				3.1415926535897932
#define TWO_PI			6.2831853071795864
#define HALF_PI			1.5707963267948966
#define ONE_OVER_PI		0.3183098861837906

#define NUM_DIRECTIONS	8
#define NUM_STEPS		4
#define RADIUS			0.2		// in world space

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec4 projInfo;
uniform vec4 clipInfo;
uniform vec2 params;
uniform mat4 invView;

in vec2 TexCoord;

out float FragColor;

vec4 GetViewPosition(vec2 uv)
{
	vec2 basesize = vec2(textureSize(gDepth, 0));
	vec2 coord = (uv / basesize);

	float d = texture(gDepth, coord).r;
	vec4 ret = vec4(0.0, 0.0, 0.0, d);

	ret.z = clipInfo.x + d * (clipInfo.y - clipInfo.x);
	ret.xy = (uv * projInfo.xy + projInfo.zw) * ret.z;

	return ret;
}

#define FALLOFF_START2	0.01
#define FALLOFF_END2	0.5
float Falloff(float dist2, float cosh)
{
	return 2.0 * clamp((dist2 - FALLOFF_START2) / (FALLOFF_END2 - FALLOFF_START2), 0.0, 1.0);
}


void main()
{
	ivec2 loc = ivec2(gl_FragCoord.xy);
	vec4 vpos = GetViewPosition(gl_FragCoord.xy);

	if (vpos.w == 1.0) 
	{
		FragColor = 1.0;
		return;
	}

	vec4 s;
	vec3 vnorm	= texelFetch(gNormal, loc, 0).rgb;
	vec3 vdir	= normalize(-vpos.xyz);
	vec3 dir, ws;

	// calculation uses left handed system
	vnorm = vnorm * mat3(invView);
	vnorm.z = -vnorm.z;

	vec2 noises	= texelFetch(texNoise, loc % 4, 0).rg;
	vec2 offset;
	vec2 horizons = vec2(-1.0, -1.0);

	float radius = (RADIUS * clipInfo.z) / vpos.z;
	radius = max(NUM_STEPS, radius);

	float stepsize	= radius / NUM_STEPS;
	float phi		= 0.0;
	float ao		= 0.0;
	float division	= noises.y * stepsize;
	float currstep	= 1.0;
	float dist2, invdist, falloff, cosh;

	for (int k = 0; k < NUM_DIRECTIONS; ++k) {
		phi = float(k) * (PI / NUM_DIRECTIONS);
		currstep = 1.0 + division + 0.25 * stepsize * params.y;

		dir = vec3(cos(phi), sin(phi), 0.0);
		horizons = vec2(-1.0);

		// calculate horizon angles
		for (int j = 0; j < NUM_STEPS; ++j) {
			offset = round(dir.xy * currstep);

			// h1
			s = GetViewPosition(gl_FragCoord.xy + offset);
			ws = s.xyz - vpos.xyz;

			dist2 = dot(ws, ws);
			invdist = inversesqrt(dist2);
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist2, cosh);
			horizons.x = max(horizons.x, cosh - falloff);

			// h2
			s = GetViewPosition(gl_FragCoord.xy - offset);
			ws = s.xyz - vpos.xyz;

			dist2 = dot(ws, ws);
			invdist = inversesqrt(dist2);
			cosh = invdist * dot(ws, vdir);

			falloff = Falloff(dist2, cosh);
			horizons.y = max(horizons.y, cosh - falloff);

			// increment
			currstep += stepsize;
		}

		horizons = acos(horizons);

		// calculate gamma
		vec3 bitangent	= normalize(cross(dir, vdir));
		vec3 tangent	= cross(vdir, bitangent);
		vec3 nx			= vnorm - bitangent * dot(vnorm, bitangent);

		float nnx		= length(nx);
		float invnnx	= 1.0 / (nnx + 1e-6);			// to avoid division with zero
		float cosxi		= dot(nx, tangent) * invnnx;	// xi = gamma + HALF_PI
		float gamma		= acos(cosxi) - HALF_PI;
		float cosgamma	= dot(nx, vdir) * invnnx;
		float singamma2	= -2.0 * cosxi;					// cos(x + HALF_PI) = -sin(x)

		// clamp to normal hemisphere
		horizons.x = gamma + max(-horizons.x - gamma, -HALF_PI);
		horizons.y = gamma + min(horizons.y - gamma, HALF_PI);

		// Riemann integral is additive
		ao += nnx * 0.25 * (
			(horizons.x * singamma2 + cosgamma - cos(2.0 * horizons.x - gamma)) +
			(horizons.y * singamma2 + cosgamma - cos(2.0 * horizons.y - gamma)));
	}

	// PDF = 1 / pi and must normalize with pi because of Lambert
	ao = ao / float(NUM_DIRECTIONS);

	FragColor = ao;
}