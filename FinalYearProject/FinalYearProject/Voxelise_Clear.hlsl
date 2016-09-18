RWTexture3D<uint> VoxelTex_Colour;

float4 convRGBA8ToVec4(uint val)
{
	return float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
}


uint convVec4ToRGBA8(float4 val)
{
	return (uint (val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

[numthreads(NUM_THREADS, NUM_THREADS,1)]
void CSClearVoxels(uint3 id: SV_DispatchThreadID)
{
	uint3 size = 0;
	VoxelTex_Colour.GetDimensions(size.x, size.y, size.z);
	
	uint threadNumber = id.x + (id.y * NUM_THREADS) + (id.z * NUM_THREADS * NUM_THREADS);

	uint index = threadNumber * NUM_TEXELS_PER_THREAD;

	float3 texCoord;

	for (int i = 0; i < NUM_TEXELS_PER_THREAD; i++)
	{
		texCoord.x = i / (size * size);
		texCoord.y = (i / size.x) % size.x;
		texCoord.z =  i % size.x;
		VoxelTex_Colour[texCoord] = convVec4ToRGBA8(float4(0.f,0.f,0.1f,0.0f) * 255.f);
		index++;
	}
}