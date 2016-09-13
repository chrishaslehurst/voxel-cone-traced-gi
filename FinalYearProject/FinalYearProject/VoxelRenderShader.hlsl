
cbuffer MatrixBuffer
{
	matrix mWorldMatrix;
	matrix mViewMatrix;
	matrix mProjectionMatrix;
	float3 eyePos;
	float padding;
};

cbuffer PerCubeBuffer
{
	int3 VolumeCoord;
};

struct VertexInput
{
	float4 position : POSITION;
	float2 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 tex		: TEXCOORD0;
	float4 worldPosition : TEXCOORD1;
};

Texture3D<float4> VoxelVolume;

PixelInput VSMain(VertexInput input)
{
	PixelInput output;

	//Calculate the position of the vertex against the world, view and projection matrices.
	output.position = mul(input.position, mWorldMatrix);
	output.worldPosition = output.position;
	output.position = mul(output.position, mViewMatrix);
	output.position = mul(output.position, mProjectionMatrix);

	//Normal needs to be transformed to world space and then normalised before being sent to pixel shader
	output.normal = mul(input.normal, (float3x3)mWorldMatrix);
	output.normal = normalize(output.normal);

	output.tex = input.tex;

	return output;
}

float4 PSMain(PixelInput input) : SV_TARGET
{
	float4 colour = VoxelVolume.Load(int4(VolumeCoord,0));
	if (colour.a > 0.f)
	{
		if (input.tex.x < 0.01f || input.tex.x > 0.99f || input.tex.y < 0.01f || input.tex.y > 0.99f)
		{
			return float4(1.f, 0.f, 0.f, 1.f);
		}
	}
	else
	{
		discard;
	}
	return colour;	
}