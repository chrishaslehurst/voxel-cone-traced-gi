#define NUM_LIGHTS 4

//input textures
Texture3D<float4> VoxelTex_Colour;
Texture3D<float4> VoxelTex_Normals;
TextureCube ShadowMap[NUM_LIGHTS];

//Output volume
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
	VoxelTex_Colour.GetDimensions(size.x, size.y, size.z);

	uint threadNumber = id.x + (id.y * NUM_THREADS) + (id.z * NUM_THREADS * NUM_THREADS);

	uint index = threadNumber * NUM_TEXELS_PER_THREAD;

	float3 texCoord;

	float3 lightPosVoxelGrid[NUM_LIGHTS];
	//Get the light in voxel space..
	for (int k = 0; k < NUM_LIGHTS; k++)
	{
		lightPosVoxelGrid[k] = mul(float4(pointLights[k].position.xyz, 1.f), mWorldToVoxelGrid).xyz;
		
		lightPosVoxelGrid[k] = ((((lightPosVoxelGrid[k].x * 0.5) + 0.5f) * size.x),
			(((lightPosVoxelGrid[k].y * 0.5) + 0.5f) * size.x),
			(((lightPosVoxelGrid[k].z * 0.5) + 0.5f) * size.x));
	}

	for (int i = 0; i < NUM_TEXELS_PER_THREAD; i++)
	{
		texCoord.x = index / (size * size);
		texCoord.y = (index / size.x) % size.x;
		texCoord.z = index % size.x;
		float4 diffuseColour = VoxelTex_Colour.Load(int4(texCoord, 0));
		if (diffuseColour.a > 0)
		{
			//Convert the normal back from 0-1 range to -1 to +1
			float3 normal = VoxelTex_Normals.Load(int4(texCoord, 0)).xyz;
			normal = (normal - float3(0.5f, 0.5f, 0.f)) * 2.f;
			normal = normalize(normal);

			float4 colour = float4(0.f, 0.f, 0.f, 1.f);

			for (int j = 0; j < NUM_LIGHTS; j++)
			{
				float3 ToLight = lightPosVoxelGrid[j] - texCoord;
				float3 ToLNorm = normalize(ToLight);
				colour += saturate(dot(ToLNorm, normal) * pointLights[j].colour * diffuseColour);
			}
			colour = saturate(colour);
			RadianceVolume[texCoord] = convVec4ToRGBA8(colour * 255.f);
		}
		index++;
	}
}