///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Globals
cbuffer MatrixBuffer
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
};

Texture2D diffuseTexture;
#if USE_NORMAL_MAPS
Texture2D normalMapTexture;
#endif
#if USE_SPECULAR_MAPS
Texture2D specularMapTexture;
#endif


SamplerState SampleType;

cbuffer LightBuffer
{
	float4 ambientColour;
	float4 diffuseColour;
	float3 lightDirection;
	float specularPower;
	float4 specularColor;
};

cbuffer CameraBuffer
{
	float3 cameraPosition;
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
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 normal	: NORMAL;
	float2 tex		: TEXCOORD0;
	float4 colour	: COLOR;
	float3 viewDirection : TEXCOORD1;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
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

	//transform tangent to world space then normalise
	output.tangent = mul(input.tangent, (float3x3)worldMatrix);
	output.tangent = normalize(output.tangent);

	//transform binormal to world space then normalise
	output.binormal = mul(input.binormal, (float3x3)worldMatrix);
	output.binormal = normalize(output.binormal);


	//Viewing direction based on position of the camera and position of the vertex
	output.viewDirection = cameraPosition.xyz - mul(input.position, worldMatrix).xyz;
	output.viewDirection = normalize(output.viewDirection);

	return output;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Pixel Shader

float4 PSMain(PixelInputType input) : SV_TARGET
{
	float4 finalColour = ambientColour;
	float4 textureColour, specCol, normalMapCol;
	float3 reflectionVector, bumpNormal;

	textureColour = diffuseTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));

#if USE_NORMAL_MAPS
	normalMapCol = normalMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));

	//Expand range of normal map to -1 / +1
	normalMapCol = (normalMapCol * 2) - 1.f;
	// Calculate normal from the data in normal map.

	bumpNormal = (normalMapCol.x * input.tangent) + (normalMapCol.y * input.binormal) + (normalMapCol.z * input.normal);
	bumpNormal = normalize(bumpNormal);
#else
	bumpNormal = input.normal;
#endif

	textureColour = textureColour * input.colour;

	specCol = float4(0.f, 0.f, 0.f, 0.f);

	//Invert the light direction
	float3 lightDir = -lightDirection;
	float lightIntensity = saturate(dot(bumpNormal, lightDir));

	if (lightIntensity > 0.0f)
	{
		// Determine the final diffuse color based on the diffuse color and the amount of light intensity.
		finalColour += (textureColour * lightIntensity);

		finalColour = saturate(finalColour);

		//Calculate reflection vector based on light and surface normal direction
		reflectionVector = normalize(2 * lightIntensity * bumpNormal - lightDir);

		//Calculate amount of specular light based on viewing pos
		specCol = pow(saturate(dot(reflectionVector, input.viewDirection)), specularPower);
#if USE_SPECULAR_MAPS
		float4 specularIntensity = specularMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
		specCol = specCol * specularIntensity;
#else
		specCol = specCol * specularColor;
#endif
	}

	//Multiply surface colour by light colour to get final colour
	finalColour = finalColour * textureColour;

	finalColour = saturate(finalColour + specCol);

	return finalColour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////