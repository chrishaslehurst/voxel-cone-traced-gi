///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Globals
cbuffer MatrixBuffer
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Structs
struct VertexInputType
{
	float4 position : POSITION;
	float4 colour : COLOR;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 colour : COLOR;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Vertex Shader

PixelInputType VSMain(VertexInputType input)
{
	PixelInputType output;

	//change the position vector to be 4 units for proper matrix calculations
	input.position.w = 1.f;

	//Calculate the position of the vertex against the world, view and projection matrices.
	output.position = mul(input.position, worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);

	//Store the input col for the pixel shader to make use of
	output.colour = input.colour;

	return output;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Pixel Shader

float4 PSMain(PixelInputType input) : SV_TARGET
{
	return input.colour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////