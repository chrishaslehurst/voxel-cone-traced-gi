
cbuffer MatrixBuffer
{
	matrix mWorldMatrix;
	matrix mViewMatrix;
	matrix mProjectionMatrix;
	int mipLevel;
	int3 padding;
};

cbuffer PerCubeBuffer
{
	int3 VolumeCoord;
};

struct VertexInput
{
	float3 position : POSITION;
};

struct GSInput
{
	float4 worldPosition : SV_POSITION;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 worldPosition : TEXCOORD0;
	float4 colour : TEXCOORD1;
};

Texture3D<float4> VoxelVolume;

GSInput VSMain(VertexInput input)
{
	GSInput output;

	//pass the position straight through
	output.worldPosition = float4(input.position, 1.f);
	
	//output.tex = input.tex;

	return output;
}

[maxvertexcount(30)]
void GSMain(point GSInput input[1], inout TriangleStream<PixelInput> outputStream)
{
	float4 colour = VoxelVolume.Load(int4(input[0].worldPosition.xyz, mipLevel));
	if (colour.a > 0)
	{
		PixelInput outputs[8];

		outputs[0].position = float4((input[0].worldPosition.xyz + (float3(-1, -1, -1)* 0.5f)), 1.f);
		outputs[1].position = float4((input[0].worldPosition.xyz + (float3(-1, -1, 1) * 0.5f)), 1.f);
		outputs[2].position = float4((input[0].worldPosition.xyz + (float3(1, -1, -1) * 0.5f)), 1.f);
		outputs[3].position = float4((input[0].worldPosition.xyz + (float3(1, -1, 1)  * 0.5f)), 1.f);
		outputs[4].position = float4((input[0].worldPosition.xyz + (float3(-1, 1, -1) * 0.5f)), 1.f);
		outputs[5].position = float4((input[0].worldPosition.xyz + (float3(-1, 1, 1)  * 0.5f)), 1.f);
		outputs[6].position = float4((input[0].worldPosition.xyz + (float3(1, 1, -1)  * 0.5f)), 1.f);
		outputs[7].position = float4((input[0].worldPosition.xyz + (float3(1, 1, 1)   * 0.5f)), 1.f);

		[unroll]
		for (int i = 0; i < 8; i++)
		{
			outputs[i].position = mul(float4(outputs[i].position.xyz, 1.f), mWorldMatrix);
			outputs[i].position = mul(outputs[i].position, mViewMatrix);
			outputs[i].position = mul(outputs[i].position, mProjectionMatrix);
			outputs[i].worldPosition = input[0].worldPosition;
			outputs[i].colour = colour;
		}

		//face1
		outputStream.Append(outputs[4]);
		outputStream.Append(outputs[6]);
		outputStream.Append(outputs[0]);
		outputStream.Append(outputs[6]);
		outputStream.Append(outputs[2]);
		outputStream.RestartStrip();

		//face2
		outputStream.Append(outputs[6]);
		outputStream.Append(outputs[7]);
		outputStream.Append(outputs[2]);
		outputStream.Append(outputs[7]);
		outputStream.Append(outputs[3]);
		outputStream.RestartStrip();

		//face3
		outputStream.Append(outputs[7]);
		outputStream.Append(outputs[5]);
		outputStream.Append(outputs[3]);
		outputStream.Append(outputs[5]);
		outputStream.Append(outputs[1]);
		outputStream.RestartStrip();

		//face4
		outputStream.Append(outputs[5]);
		outputStream.Append(outputs[4]);
		outputStream.Append(outputs[1]);
		outputStream.Append(outputs[4]);
		outputStream.Append(outputs[0]);
		outputStream.RestartStrip();

		//face5
		outputStream.Append(outputs[5]);
		outputStream.Append(outputs[7]);
		outputStream.Append(outputs[4]);
		outputStream.Append(outputs[7]);
		outputStream.Append(outputs[6]);
		outputStream.RestartStrip();

		//face6
		outputStream.Append(outputs[0]);
		outputStream.Append(outputs[2]);
		outputStream.Append(outputs[1]);
		outputStream.Append(outputs[2]);
		outputStream.Append(outputs[3]);
		outputStream.RestartStrip();
	}

}


float4 PSMain(PixelInput input) : SV_TARGET
{
	return input.colour;	
}