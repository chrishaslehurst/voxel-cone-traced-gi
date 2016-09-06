//#include "VoxelGICommon.hlsl"

RWTexture3D<uint> VoxelTex_Colour;

#define NUMTHREADS 16

float4 convRGBA8ToVec4(uint val)
{
	return float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
}


uint convVec4ToRGBA8(float4 val)
{
	
	return (uint (val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}


[numthreads(NUMTHREADS,NUMTHREADS,1)]
void CSClearVoxels(uint3 id: SV_DispatchThreadID)
{
	uint3 size = 0;
	VoxelTex_Colour.GetDimensions(size.x, size.y, size.z);
	uint numTexelsToCompute = size.x * size.x * size.x;
	uint numTexelsPerThread = numTexelsToCompute / NUMTHREADS * NUMTHREADS;
	uint threadNumber = id.x + (id.y * NUMTHREADS) + (id.z * NUMTHREADS * NUMTHREADS);

	float3 texCoord = float3(0.f, 0.f, 0.f);
	for (int i = threadNumber * numTexelsPerThread; i < (threadNumber+1) * numTexelsPerThread; i++)
	{
		texCoord.x = i / (size * size);
		texCoord.y = (i / size.x) % size.x;
		texCoord.z =  i % size.x;
		//texCoord = float3(0.f, 0.f, 0.f);
		VoxelTex_Colour[texCoord] = convVec4ToRGBA8(float4(0.f,0.f,0.f, 1.f) * 255.f);
	}
}