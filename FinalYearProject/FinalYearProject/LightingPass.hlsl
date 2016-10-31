#define NUM_LIGHTS 4
#define PI 3.14159265359

static const float DepthBias = 0.001f;

//Diffuse GI Constants for tracing cones---------------------------

static const float3 ConeSampleDirections[] =
{
	float3(0.0f, 1.0f, 0.0f),
	float3(0.0f, 0.5f, 0.866025f),
	float3(0.823639f, 0.5f, 0.267617f),
	float3(0.509037f, 0.5f, -0.7006629f),
	float3(-0.50937f, 0.5f, -0.7006629f),
	float3(-0.823639f, 0.5f, 0.267617f)
};
static const float ConeSampleWeights[] =
{
	PI / 4.0f,
	3 * PI / 20.0f,
	3 * PI / 20.0f,
	3 * PI / 20.0f,
	3 * PI / 20.0f,
	3 * PI / 20.0f,
};
static const float diffuseRadiusRatio = sin(0.53f);//60 degree cones

//-------------------------------------------------------------
#define MIP_COUNT 4

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
	matrix mWorldToVoxelGrid;
	float3 cameraPosition;
	float voxelScale; //size of one voxel in world units..
};

//These defines are for comparing against the render flags, they are equivalent to the enum defined in the renderer.
#define NO_GI 0
#define FULL_GI 1
#define DIFFUSE_ONLY 2
#define SPECULAR_ONLY 3
#define AO_ONLY 4

cbuffer RenderFlags
{
	int eRenderGlobalIllumination;
	int3 padding;
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

Texture3D<float4> RadianceVolume[MIP_COUNT];

//Sample State
SamplerState SampleTypePoint;
SamplerComparisonState ShadowMapSampler;
SamplerState VoxelSampler;

//Functions
float3 CalculateLambertDiffuseBRDF(float3 DiffuseColour, float Metallic)
{
	// Lerp with metallic value to find the good diffuse and specular.
	float3 RealDiffuse = DiffuseColour - DiffuseColour * Metallic;
	return saturate(RealDiffuse * (1.f /PI ));
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
	shadowFactor = 0.f;
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


//GI Functions---------------------------
void accumulateColorOcclusion(float4 sampleColor, inout float3 colorAccum, inout float occlusionAccum)
{
	colorAccum = occlusionAccum * colorAccum + (1.0f - sampleColor.a) * sampleColor.a * sampleColor.rgb;
	occlusionAccum = occlusionAccum + (1.0f - occlusionAccum) * sampleColor.a;
}

float getMipLevelFromRadius(float radius)
{
	return (radius / voxelScale) - 1;
}


//Based off of GGX roughness; gets angle that encompasses 90% of samples in the IBL image approximation
float calculateSpecularConeHalfAngle(float roughness)
{
	return acos(sqrt(0.11111f / (roughness * roughness + 0.11111f)));
}

float4 sampleVoxelVolume(Texture3D<float4> RadianceVolume, float4 worldPosition, float coneRadius, inout bool outsideVolume)
{
	float4 voxPos = mul(worldPosition, mWorldToVoxelGrid);

	float3 texCoord = float3((((voxPos.x * 0.5) + 0.5f)),
		(((voxPos.y * 0.5) + 0.5f)),
		(((voxPos.z * 0.5) + 0.5f)));
	
	if (any(texCoord.xyz < 0.0f) || any(texCoord.xyz > 1.f))
	{
		//Early out if we've traced out of the volume..
		outsideVolume = true;
		return float4(0.f, 0.f, 0.f, 0.f);
	}

	int MipLevel = clamp(getMipLevelFromRadius(coneRadius), 0, 4);
	
	return RadianceVolume.SampleLevel(VoxelSampler, texCoord, MipLevel);;
}

float4 TraceSpecularCone(float4 StartPos, float3 Normal, float3 Direction, float RadiusRatio, float voxelScale, int distanceInVoxelsToTrace)
{
	float4 GIColour = float4(0.f, 0.f, 0.f, 0.f);
	float3 SamplePos = StartPos.xyz + (Normal*voxelScale*1.5f); //offset to avoid self intersect..
	
	float distance = voxelScale * 1.5f;
	for (int i = 0; i < distanceInVoxelsToTrace; i++)
	{
		SamplePos += Direction * voxelScale * 0.5f;
		distance += voxelScale * 0.5f;
		float currentRadius = RadiusRatio * distance;

		bool outsideVolume = false;
		float4 tempGI = sampleVoxelVolume(RadianceVolume[0], float4(SamplePos.xyz, 1.f), currentRadius, outsideVolume);
		if (outsideVolume)
		{
			break;
		}
		if (tempGI.a > 1.f - GIColour.a)
		{
			tempGI.a = 1.f - GIColour.a;
		}
		GIColour.rgb += tempGI.rgb * tempGI.a;
		GIColour.a += tempGI.a;
		if (GIColour.a > 0.95f)
		{
			break;
		}
		SamplePos += Direction * voxelScale;
		distance += voxelScale;
	}
	return GIColour;
}

float4 TraceDiffuseCone(float4 StartPos, float3 Normal, float3 Direction, float RadiusRatio, float voxelScale, int distanceInVoxelsToTrace, inout float AOAccumulation)
{
	float4 GIColour = float4(0.f, 0.f, 0.f, 0.f);
	float3 SamplePos = StartPos.xyz + (Normal*voxelScale*2.1f); //offset to avoid self intersect..
	float distance = voxelScale * 2.1f;
	float AccumulatedOcclusion = 0.0f;
	for (int i = 0; i < 16; i++)
	{
		SamplePos += Direction * voxelScale * 0.25f;
		distance += voxelScale * 0.25f;

		float currentRadius = RadiusRatio * distance;
		bool outsideVolume = false;
		float4 tempGI = sampleVoxelVolume(RadianceVolume[0], float4(SamplePos.xyz, 1.f), currentRadius, outsideVolume);
		tempGI *= 1.f / PI;
		if (outsideVolume)
		{
			break;
		}

		GIColour.rgb += tempGI.a * tempGI.rgb;
		AccumulatedOcclusion = AccumulatedOcclusion + tempGI.a;
	
		AOAccumulation += tempGI.a * (voxelScale / (distance + 1.f));
		//AOAccumulation = 0.f;
		if (AccumulatedOcclusion >= 0.99f)
		{
			break;
		}
	}
	return float4(GIColour.rgb, 1.f);
}

//----------------------------------------


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
	float4 NormalSample = Normals.Sample(SampleTypePoint, input.tex);
	float3 Normal = normalize(NormalSample.rgb);
	float Roughness = NormalSample.a;
	float4 DiffuseColourSample = DiffuseColour.Sample(SampleTypePoint, input.tex);
	float4 WorldPositionSample = WorldPosition.Sample(SampleTypePoint, input.tex);
	float4 MetallicSample = DiffuseColourSample.a;
	DiffuseColourSample.a = 1.f;

	float3 ToCamera = cameraPosition.xyz - WorldPositionSample.xyz;
	ToCamera = normalize(ToCamera);

	int3 texDimensions;
	RadianceVolume[0].GetDimensions(texDimensions.x, texDimensions.y, texDimensions.z);

	//Specular GI------------------------------------
	
	float occlusion = 0;
	float4 SpecularGI = float4(0.f, 0.f, 0.f, 0.f);
	float3 reflectVector = -((2 * (dot(Normal, -ToCamera) * Normal)) + ToCamera);
	reflectVector = normalize(reflectVector);

	float radiusRatio = sin(calculateSpecularConeHalfAngle(Roughness * Roughness));
	
	float3 H = normalize(ToCamera + reflectVector);
	float VdotH = saturate(dot(ToCamera, H));

	SpecularGI = TraceSpecularCone(WorldPositionSample, Normal, reflectVector, radiusRatio, voxelScale, texDimensions.x);
	SpecularGI = saturate(SchlickFresnel(1.f - Roughness, VdotH) * SpecularGI);

	//---------------------

	//Diffuse GI-------------------------------------
	
	float3 up = (Normal.y * Normal.y) > 0.95f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
	float3 right = cross(Normal, up);
	up = cross(Normal, right);

	float4 accumulatedDiffuse = float4(0.f, 0.f, 0.f, 0.f);
	float accumulatedDiffuseOcclusion = 0.f;

	for (int coneIndex = 0; coneIndex < 6; coneIndex++)
	{
		float3 coneDirection = Normal;
		coneDirection = ConeSampleDirections[coneIndex].x * right + ConeSampleDirections[coneIndex].z * up;
		coneDirection = normalize(coneDirection);

		float accumulatedAO = 0.f;
		float4 accumulatedColour = TraceDiffuseCone(WorldPositionSample, Normal, coneDirection, diffuseRadiusRatio, voxelScale, 8, accumulatedAO);

		accumulatedDiffuse += accumulatedColour * ConeSampleWeights[coneIndex];
		accumulatedDiffuseOcclusion += accumulatedAO * ConeSampleWeights[coneIndex];
	}
	accumulatedDiffuseOcclusion = saturate(accumulatedDiffuseOcclusion);
	float4 DiffuseGI = accumulatedDiffuse * (1.f - accumulatedDiffuseOcclusion);
	SpecularGI = SpecularGI * (1.f - accumulatedDiffuseOcclusion);
//	if (accumulatedDiffuseOcclusion >= 0.f)
//	{
//		return SpecularGI;
//		return DiffuseGI;
//		return float4(1.f - accumulatedDiffuseOcclusion, 1.f - accumulatedDiffuseOcclusion, 1.f - accumulatedDiffuseOcclusion, 1.f);
//	}
	//---------------------

	float4 FinalColour = float4(0.f, 0.f, 0.f, 1.f);
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		float3 ToLight = pointLights[i].position.xyz - WorldPositionSample.xyz;
		float4 Light = CookTorranceBRDF(ToLight, ToCamera, Normal, Roughness, MetallicSample.r, DiffuseColourSample.rgb);
		FinalColour += Light * CalculateAttenuation(ToLight, pointLights[i].range) * pointLights[i].colour * pointLights[i].colour.w * CalculateShadowFactor(i, ToLight, pointLights[i].range);
	}
	switch (eRenderGlobalIllumination)
	{
	case NO_GI:
		return FinalColour;
	case FULL_GI:
		return (FinalColour + DiffuseGI + SpecularGI);
	case DIFFUSE_ONLY:
		return DiffuseGI;
	case SPECULAR_ONLY:
		return SpecularGI;
	case AO_ONLY:
		return float4(1.f - accumulatedDiffuseOcclusion, 1.f - accumulatedDiffuseOcclusion, 1.f - accumulatedDiffuseOcclusion, 1.f);
	}
	return float4(1.f, 0.f, 0.f, 1.f);
}