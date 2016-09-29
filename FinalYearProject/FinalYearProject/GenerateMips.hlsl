Texture3D<float4> SourceVolume;
RWTexture3D<uint> DestVolume;

float4 convRGBA8ToVec4(uint val)
{
	return float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
}


uint convVec4ToRGBA8(float4 val)
{
	return (uint (val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

[numthreads(NUM_THREADS, NUM_THREADS, 1)]
void CSGenerateMips(uint3 id: SV_DispatchThreadID)
{
	const float3 offsets[8] = { float3(0.f, 0.f, 0.f),
									 float3(0.f, 0.f, 1.f),
									 float3(0.f, 1.f, 0.f),
									 float3(0.f, 1.f, 1.f),
									 float3(1.f, 0.f, 0.f),
									 float3(1.f, 1.f, 0.f),
									 float3(1.f, 1.f, 1.f),
									 float3(1.f, 0.f, 1.f)
									};

	uint3 size = 0;
	DestVolume.GetDimensions(size.x, size.y, size.z);

	uint threadNumber = id.x + (id.y * NUM_THREADS * NUM_GROUPS) + (id.z * NUM_THREADS * NUM_THREADS * NUM_GROUPS);
	
	uint index = threadNumber * NUM_TEXELS_PER_THREAD;
	if (index > (size.x * size.y * size.z))
	{
		return;
	}

	float3 destCoord;

	for (int i = 0; i < NUM_TEXELS_PER_THREAD; i++)
	{
		destCoord.x = index / (size * size);
		destCoord.y = (index / size.x) % size.x;
		destCoord.z = index % size.x;
		//
		float4 Colour = float4(0.f, 0.f, 0.f, 0.f);
		for (int j = 0; j < 8; j++)
		{
			Colour += SourceVolume.Load(int4(((destCoord*2) + offsets[j]), 0));
		}
		Colour.rgb = Colour.rgb / 8.f;
		if (Colour.a > 0)
		{
			Colour.a = 1.f;
		}
		
		//Colour = float4(1.f, 1.f, 1.f, 1.f);
		DestVolume[destCoord] = convVec4ToRGBA8(Colour * 255.f);

		index++;
	}
}