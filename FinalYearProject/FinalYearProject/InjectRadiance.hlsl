#define NUM_LIGHTS 4

//Input/Output volume
RWTexture3D<uint> RadianceVolume;

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

cbuffer VoxeliseVertexShaderBuffer
{
	matrix World;
	matrix mWorldToVoxelGrid;
	matrix WorldViewProj;
	matrix WorldInverseTranspose;
	matrix mAxisProjections[3];
};

//Functions..
float4 convRGBA8ToVec4(uint val)
{
	return float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
}

uint convVec4ToRGBA8(float4 val)
{
	return (uint (val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

//Shader
[numthreads(NUM_THREADS, NUM_THREADS, 1)]
void CSInjectRadiance(uint3 id: SV_DispatchThreadID)
{
	uint3 size = 0;
	RadianceVolume.GetDimensions(size.x, size.y, size.z);

	uint threadNumber = id.x + (id.y * NUM_THREADS * NUM_GROUPS) + (id.z * NUM_THREADS * NUM_THREADS * NUM_GROUPS);

	uint index = threadNumber * NUM_TEXELS_PER_THREAD;

	float3 texCoord;

	float3 lightPosVoxelGrid[NUM_LIGHTS];
	int iNumLights = 0;
	//Get the light in voxel space..
	for (int k = 0; k < NUM_LIGHTS; k++)
	{
		if (pointLights[k].colour.a > 0)
		{
			lightPosVoxelGrid[k] = mul(float4(pointLights[k].position.xyz, 1.f), mWorldToVoxelGrid).xyz;

			lightPosVoxelGrid[k] = float3((((lightPosVoxelGrid[k].x * 0.5) + 0.5f) * size.x),
				(((lightPosVoxelGrid[k].y * 0.5) + 0.5f) * size.x),
				(((lightPosVoxelGrid[k].z * 0.5) + 0.5f) * size.x));

			iNumLights++;
		}
	}
	
	
	for (int i = 0; i < NUM_TEXELS_PER_THREAD; i++)
	{
		texCoord.x = index / (size * size);
		texCoord.y = (index / size.x) % size.x;
		texCoord.z = index % size.x;
		float4 diffuseColour = convRGBA8ToVec4(RadianceVolume.Load(int4(texCoord, 0)));
		if (diffuseColour.a > 0)
		{
			diffuseColour /= 255.f;
			float4 colour = float4(0.f, 0.f, 0.f, 1.f);
			[unroll]
			for (int j = 0; j < iNumLights; j++)
			{
				
				float3 ToLight = lightPosVoxelGrid[j] - texCoord;
				float3 ToLNorm = normalize(ToLight);
				float4 tempCol = float4(0.f, 0.f, 0.f, 0.f);
				//shadow test
				int iShadowFactor = 1;
				int voxelsToLight = length(ToLight);
				//Start a few voxels away from surface to stop self intersection..
				for (int i = 4; i < voxelsToLight; i++)
				{
					float4 sampleCol = RadianceVolume.Load(int4(texCoord + (i*ToLNorm), 0));
					if (sampleCol.a > 0)
					{
						iShadowFactor = 0;
						break;
					}
				}
				colour += saturate(float4((diffuseColour.rgb * pointLights[j].colour * iShadowFactor), 0.f));
				
			}
			colour.a = 1.f;
			colour = saturate(colour);
			RadianceVolume[texCoord] = convVec4ToRGBA8(colour * 255.f);
		}
		index++;
	}
}