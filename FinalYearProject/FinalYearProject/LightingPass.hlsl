#define NUM_LIGHTS 4
#define PI 3.14159265359

static const float DepthBias = 0.00001f;

//Globals
cbuffer MatrixBuffer
{
	matrix mWorld;
	matrix mView;
	matrix mProjection;
};

struct PointLight
{
	float4 colour;
	float range;
	float3 position;
};

cbuffer LightBuffer
{
	float4 AmbientColour;
	float4 DirectionalLightColour;
	float3 DirectionalLightDirection;
	PointLight pointLights[NUM_LIGHTS];
};

cbuffer CameraBuffer
{
	float3 cameraPosition;
	float padding; //needs to be a multiple of 16 for CreateBuffer Requirements
};

//Structs
struct VertexInput
{
	float4 Position : POSITION;
	float2 tex		: TEXCOORD0;
};

struct PixelInput
{
	float4 Position : SV_POSITION;
	float2 tex		: TEXCOORD0;
};


//Resources
Texture2D	WorldPosition;
Texture2D	DiffuseColour;		//Metallic stored in the w component
Texture2D	Normals;			//Roughness stored in the w component
TextureCube ShadowMap[NUM_LIGHTS];

//Sample State
SamplerState SampleTypePoint;
SamplerComparisonState ShadowMapSampler;

//Functions
float3 CalculateLambertDiffuseBRDF(float3 DiffuseColour, float Metallic)
{
	// Lerp with metallic value to find the good diffuse and specular.
	float3 RealDiffuse = DiffuseColour - DiffuseColour * Metallic;
	return saturate(RealDiffuse * (1.f / PI));
}

//GGX/Trowbridge Reitz Normal Distribution Function (Disney/Epic: https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)
float NormalDistributionFunction(float NdotH, float Roughness)
{
	float a = Roughness * Roughness;
	float aSquared = a * a;
	float NdotHSquared = NdotH * NdotH;
	float DenomPart = (NdotHSquared * (aSquared - 1)) + 1;
	float Denom = PI * (DenomPart * DenomPart);

	return abs(aSquared / Denom);
}

//Schlick Fresnel with spherical gaussian approximation ( same source as above)
float SchlickFresnel(float Specular, float VdotH)
{
	float SpecularClamped = clamp(Specular, 0.02f, 0.99f);
	float SphericalGaussian = exp2((-5.55473f * VdotH - 6.98316f) * VdotH);
	return saturate(SpecularClamped + (1.f - SpecularClamped) * SphericalGaussian);
}

//Schlick Geometric Attenuation with Disney modification ( same source as above)
float SchlickGeometricAttenuation(float Roughness, float NdotV, float NdotL)
{
	float k = ((Roughness + 1) * (Roughness + 1)) * 0.125f;
	float G1 = NdotV / ((NdotV * (1 - k)) + k);
	float G2 = NdotL / ((NdotL * (1 - k)) + k);

	return saturate(G1 * G2);
}


//This is the cook torrance microfacet BRDF for physically based rendering
float4 CookTorranceBRDF(float3 ToLight, float3 ToCamera, float3 SurfaceNormal, float Roughness, float Metallic, float3 DiffuseColour)
{
	float4 Colour = float4(1.f, 1.f, 1.f, 1.f);

	float3 ToL = normalize(ToLight);
	float3 H = normalize(ToCamera + ToL);
	float NdotH = saturate(dot(SurfaceNormal, H));
	float VdotH = saturate(dot(ToCamera, H));
	float NdotL = saturate(dot(SurfaceNormal, ToL));
	float NdotV = abs(dot(SurfaceNormal, ToCamera)) + 1e-5f; //Add this really small number so this doesn't go to 0.. just to ensure no divide by 0 occurs..
	float Denom = 4 * NdotL * NdotV;

	// 0.03 default specular value for dielectric.]
	float3 realSpecularColour = lerp(0.03f, DiffuseColour, Metallic);

	float D = NormalDistributionFunction(NdotH, Roughness);
	float3 F = SchlickFresnel(1.f - Roughness, VdotH) * realSpecularColour;
	float G = SchlickGeometricAttenuation(Roughness, NdotV, NdotL);

	Colour.rgb = (CalculateLambertDiffuseBRDF(DiffuseColour, Metallic) * NdotH * (1.f - F)) + (((D * F * G) / Denom));
	return saturate(Colour);
}


//Calculate the attenuation based on the distance to the light, and the lights range.
float CalculateAttenuation(float3 ToLight, float LightRange)
{
	//Attenuation
	float DistToLight = length(ToLight);
	float DistToLightNorm = 1.f - saturate(DistToLight * LightRange); //Pointlightrangercp = 1/Range - can be sent through from the app as this..
	return DistToLightNorm * DistToLightNorm;
}

float CalculateShadowFactor(int lightIdx, float3 ToLight, float ReciprocalRange)
{
	float LightDistanceSq = dot(ToLight, ToLight);
	float shadowFactor, x, y, z;

	for (z = -1.5; z <= 1.5; z += 1.5)
	{
		for (y = -1.5; y <= 1.5; y += 1.5)
		{
			for (x = -1.5; x <= 1.5; x += 1.5)
			{
				shadowFactor += saturate(ShadowMap[lightIdx].SampleCmp(ShadowMapSampler, -ToLight + float3(x, y, z), (LightDistanceSq * (ReciprocalRange * ReciprocalRange)) - DepthBias));
			}
		}
	}
	return (shadowFactor / 27.0);
}



//Vertex Shader
PixelInput VSMain(VertexInput input)
{
	PixelInput output;

	input.Position.w = 1.0f;

	output.Position = mul(input.Position, mWorld);
	output.Position = mul(output.Position, mView);
	output.Position = mul(output.Position, mProjection);

	output.tex = input.tex;

	return output;
}


//Pixel Shader
float4 PSMain(PixelInput input) : SV_TARGET
{
	float4 FinalColour = float4(1,1,1,1);

	float4 NormalSample = Normals.Sample(SampleTypePoint, input.tex);
	float3 Normal = normalize(NormalSample.rgb);
	float Roughness = NormalSample.a;
	float4 DiffuseColourSample = DiffuseColour.Sample(SampleTypePoint, input.tex);
	float4 WorldPositionSample = WorldPosition.Sample(SampleTypePoint, input.tex);
	float4 MetallicSample = DiffuseColourSample.a;
	DiffuseColourSample.a = 1.f;

	float3 ToCamera = cameraPosition.xyz - WorldPositionSample.xyz;
	ToCamera = normalize(ToCamera);

	FinalColour = AmbientColour * DiffuseColourSample;

	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		float3 ToLight = pointLights[i].position.xyz - WorldPositionSample.xyz;
		float4 Light = CookTorranceBRDF(ToLight, ToCamera, Normal, Roughness, MetallicSample.r, DiffuseColourSample.rgb);
		FinalColour += Light * CalculateAttenuation(ToLight, pointLights[i].range) * pointLights[i].colour * pointLights[i].colour.w * CalculateShadowFactor(i, ToLight, pointLights[i].range);
	}
	
	

	return saturate(FinalColour);
}