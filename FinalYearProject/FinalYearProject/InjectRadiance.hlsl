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

	for (int i = 0; i < NUM_TEXELS_PER_THREAD; i++)
	{
		texCoord.x = index / (size * size);
		texCoord.y = (index / size.x) % size.x;
		texCoord.z = index % size.x;
		float4 colour = VoxelTex_Colour.Load(int4(texCoord, 0));
		RadianceVolume[texCoord] = convVec4ToRGBA8(colour * 255.f);
		index++;
	}
}