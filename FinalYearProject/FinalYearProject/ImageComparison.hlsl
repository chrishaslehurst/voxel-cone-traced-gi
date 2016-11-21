
Texture2D Input1; //Reg
Texture2D Input2; //Tiled

RWTexture2D<float4> Output;

//SamplerState SampleTypePoint;

[numthreads(32, 32, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 id2 = uint2(id.x, id.y);
	int2 screenSize;
	Output.GetDimensions(screenSize.x, screenSize.y);
	if (id2.x > screenSize.x || id.y > screenSize.y)
	{
		return;
	}
	float4 finalCol = float4(1.f, 1.f, 1.f, 1.f);
	float4 col1 = Input1.Load(id);
	float4 col2 = Input2.Load(id);

	finalCol = abs(col1 - col2);
	Output[id2] = finalCol;
}