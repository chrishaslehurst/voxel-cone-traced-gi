static const uint ShadowMapCount = 6;

//Globals
cbuffer MatrixBuffer
{
	matrix mWorldMatrix;
};

cbuffer LightBuffer
{
	float4 WorldSpaceLightPosition;
};

cbuffer LightRangeBuffer
{
	float range;
	float3 padding;
};

cbuffer LightProjectionsBuffer
{
	matrix LightViewProjMatrices[ShadowMapCount];
};

//Structs
struct VertexInput
{
	float4 position : POSITION;
	float3 normal	: NORMAL;
	float2 tex		: TEXCOORD0;
	float4 colour	: COLOR;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct GSInput
{
	float4	worldSpacePosition			: SV_Position;
	float3  worldSpaceLightDirection	: Attrib0;
};

struct PSInput
{
	float4	clipSpacePosition			: SV_Position;
	uint	shadowMapIndex				: SV_RenderTargetArrayIndex;
	float3  worldSpaceLightDirection	: Attrib0;
};

GSInput VSMain(VertexInput input)
{
	GSInput output;

	output.worldSpacePosition = mul(mWorldMatrix, float4(input.position.xyz, 1.0f));
	output.worldSpaceLightDirection = output.worldSpacePosition.xyz - WorldSpaceLightPosition.xyz;

	return output;
}


//Geometry shader creates 6 copies of the geometry and transforms it to the different viewpoints so it can be rendered on the 6 faces of the cubemap by the pixel shader
[instance(ShadowMapCount)]
[maxvertexcount(3)]
void GSMain(uint shadowMapId : SV_GSInstanceID, triangle GSInput input[3], inout TriangleStream<PSInput> outputStream)
{
	PSInput output;
	output.shadowMapIndex = shadowMapId;
	[unroll]
	for (uint vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
	{
		output.worldSpaceLightDirection = input[vertexIndex].worldSpaceLightDirection;
		output.clipSpacePosition = mul(LightViewProjMatrices[shadowMapId], input[vertexIndex].worldSpacePosition);

		outputStream.Append(output);
	}

	outputStream.RestartStrip();
}

float PSMain(PSInput input) : SV_Depth
{
	return dot(input.worldSpaceLightDirection, input.worldSpaceLightDirection) / (range * range);
}