//Globals
cbuffer MatrixBuffer
{
	matrix mWorld;
	matrix mView;
	matrix mProjection;
};

////////////////////////////////////////////////////////////////////////////////
//Structs
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
	float4 worldPosition : TEXCOORD0;
	float2 tex : TEXCOORD1;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct PixelOutput
{
	float4 position : SV_Target0;
	float4 colour : SV_Target1; //metallic is just one float so store in final element of colour
	float4 normal : SV_Target2; //roughness is just a float value so stored in the final element of the normal
//	float4 metallic : SV_Target3;
};

////////////////////////////////////////////////////////////////////////////////
//Resources


Texture2D diffuseTexture;

Texture2D normalMapTexture;

Texture2D roughnessMapTexture;
Texture2D metallicMapTexture;


////////////////////////////////////////////////////////////////////////////////
//Sample State
SamplerState SampleType;

////////////////////////////////////////////////////////////////////////////////
//Vertex Shader

PixelInput VSMain(VertexInput input)
{
	PixelInput output;

	input.position.w = 1.f;
	
	output.position = mul(input.position, mWorld);
	output.worldPosition = output.position;
	output.position = mul(output.position, mView);
	output.position = mul(output.position, mProjection);

	output.tex = input.tex;

	//Normal needs to be transformed to world space and then normalised before being sent to pixel shader
	output.normal = mul(input.normal, (float3x3)mWorld);
	output.normal = normalize(output.normal);

	//transform tangent to world space then normalise
	output.tangent = mul(input.tangent, (float3x3)mWorld);
	output.tangent = normalize(output.tangent);

	//transform binormal to world space then normalise
	output.binormal = mul(input.binormal, (float3x3)mWorld);
	output.binormal = normalize(output.binormal);

	return output;
}

////////////////////////////////////////////////////////////////////////////////
//Pixel Shader
PixelOutput PSMain(PixelInput input) : SV_TARGET
{
	PixelOutput output;

	float4 normalMapCol;
	
	output.position = input.worldPosition;


	output.colour = diffuseTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));

	normalMapCol = normalMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	//Expand range of normal map to -1 / +1
	normalMapCol = (normalMapCol * 2) - 1.f;
	// Calculate normal from the data in normal map.
	output.normal.rgb = (normalMapCol.x * input.tangent) + (normalMapCol.y * input.binormal) + (normalMapCol.z * input.normal);
	output.normal.rgb = normalize(output.normal.rgb);

	//sample the roughness and metallic maps
	output.normal.w = roughnessMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y)).r;
	output.colour.w = metallicMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y)).r;


	return output;
}