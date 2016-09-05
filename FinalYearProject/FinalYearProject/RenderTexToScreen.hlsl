
//Globals
cbuffer MatrixBuffer
{
	matrix mWorld;
	matrix mView;
	matrix mProjection;

};

//Structs
struct VertexInput
{
	float4 Position : POSITION;
	float2 tex		: TEXCOORD0;
};

struct PixelInput
{
	float4 Position : SV_POSITION;
	float2 tex		: TEXCOORD0;
};


//Resources
Texture2D	Texture;

//Sample State
SamplerState SampleTypePoint;


//Vertex Shader
PixelInput VSMain(VertexInput input)
{
	PixelInput output;

	input.Position.w = 1.0f;

	output.Position = mul(input.Position, mWorld);
	output.Position = mul(output.Position, mView);
	output.Position = mul(output.Position, mProjection);

	output.tex = input.tex;

	return output;
}


//Pixel Shader
float4 PSMain(PixelInput input) : SV_TARGET
{
	return Texture.Sample(SampleTypePoint, input.tex);
}