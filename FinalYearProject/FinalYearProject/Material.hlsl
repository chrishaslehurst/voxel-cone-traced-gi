///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Globals
cbuffer MatrixBuffer
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
};

Texture2D diffuseTexture;
SamplerState SampleType;

cbuffer LightBuffer
{
	float4 diffuseColour;
	float3 lightDirection;
	float padding; //needs to be a multiple of 16 for CreateBuffer Requirements
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Structs
struct VertexInputType
{
	float4 position : POSITION;
	float3 normal	: NORMAL;
	float2 tex		: TEXCOORD0;
	float4 colour	: COLOR;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 normal	: NORMAL;
	float2 tex		: TEXCOORD0;
	float4 colour	: COLOR;
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
	output.tex = input.tex;

	//Normal needs to be transformed to world space and then normalised before being sent to pixel shader
	output.normal = mul(input.normal, (float3x3)worldMatrix);
	output.normal = normalize(output.normal);

	return output;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Pixel Shader

float4 PSMain(PixelInputType input) : SV_TARGET
{
	float4 finalColour;
	float4 textureColour;
	textureColour = diffuseTexture.Sample(SampleType, input.tex);

	textureColour = textureColour * input.colour;

	//Invert the light direction
	float3 lightDir = -lightDirection;
	float lightIntensity = saturate(dot(input.normal, lightDir));

	finalColour = saturate(diffuseColour * lightIntensity);

	//Multiply surface colour by light colour to get final colour
	finalColour = finalColour * textureColour;


	return finalColour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////