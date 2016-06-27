///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_LIGHTS 4

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
#if USE_ALPHA_MASKS
Texture2D alphaMaskTexture;
#endif


SamplerState SampleType;

cbuffer DirectionLightBuffer
{
	float4 ambientColour;
	float4 diffuseColour;
	float3 lightDirection;
	float specularPower;
	float4 specularColor;
};

cbuffer PointLightPositionsBuffer
{
	float4 lightPositions[NUM_LIGHTS];
};

struct PointLight
{
	float4 colour;
	float range;
	float3 padding;

};

cbuffer PointLightPixelBuffer
{
	PointLight pointLights[NUM_LIGHTS];
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
	float3 lightPos1 : TEXCOORD2;
	float3 lightPos2 : TEXCOORD3;
	float3 lightPos3 : TEXCOORD4;
	float3 lightPos4 : TEXCOORD5;
};

//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float4 BlinnPhongBRDF(float3 toLight, float3 viewDir, float3 surfaceNormal, float4 specularIntensity, float specularPower, float4 lightColour, float4 diffuseTexture, inout float4 specularColour, float lightRange)
{
	float4 colour = float4(0.f,0.f,0.f, 1.f);
	float4 specCol = float4(0.f, 0.f, 0.f, 1.f);
	float3 toLightNorm = normalize(toLight);
	float lightIntensity = saturate(dot(surfaceNormal, toLightNorm));
	float DistToLight = length(toLight);
	

	if (lightIntensity > 0.0f)
	{
		// Determine the final diffuse color based on the diffuse color and the amount of light intensity.
		colour = lightIntensity * lightColour;

		colour = saturate(colour * diffuseTexture);

		//Calculate reflection vector based on light and surface normal direction
		float3 reflectionVector = normalize(2 * lightIntensity * surfaceNormal - toLightNorm);

		//Calculate amount of specular light based on viewing pos
		specCol = pow(saturate(dot(reflectionVector, viewDir)), specularPower);

		specCol = specCol * specularIntensity;
	}

	//Attenuation
	float DistToLightNorm = 1.f - saturate(DistToLight * lightRange); //Pointlightrangercp = 1/Range - can be sent through from the app as this..
	float Attn = DistToLightNorm * DistToLightNorm;
	colour *= Attn;
	specCol *= Attn;

	specularColour += specCol;

	return colour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Vertex Shader

PixelInputType VSMain(VertexInputType input)
{
	PixelInputType output;

	//change the position vector to be 4 units for proper matrix calculations
	input.position.w = 1.f;

	//Calculate the position of the vertex against the world, view and projection matrices.
	output.position = mul(input.position, worldMatrix);
	float4 worldPos = output.position;
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
	output.viewDirection = cameraPosition.xyz - worldPos.xyz;
	output.viewDirection = normalize(output.viewDirection);

	
	//Turn the light positions into 'ToLight' vectors
	output.lightPos1.xyz = lightPositions[0].xyz - worldPos.xyz;
	output.lightPos2.xyz = lightPositions[1].xyz - worldPos.xyz;
	output.lightPos3.xyz = lightPositions[2].xyz - worldPos.xyz;
	output.lightPos4.xyz = lightPositions[3].xyz - worldPos.xyz;	


	return output;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Pixel Shader

float4 PSMain(PixelInputType input) : SV_TARGET
{
	float4 finalColour;
	float4 textureColour, specCol, normalMapCol;
	float3 bumpNormal;

	specCol = float4(0.f, 0.f, 0.f, 0.f);
	textureColour = diffuseTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	textureColour = textureColour * input.colour;

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
	

	//Invert the light direction
	float3 lightDir = -lightDirection;
	float4 specularIntensity;
#if USE_SPECULAR_MAPS
	specularIntensity = specularMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
#else
	specularIntensity = specularColor;
#endif

	float4 col1 = BlinnPhongBRDF(input.lightPos1, input.viewDirection, bumpNormal, specularIntensity, specularPower, pointLights[0].colour, textureColour, specCol, pointLights[0].range);
	float4 col2 = BlinnPhongBRDF(input.lightPos2, input.viewDirection, bumpNormal, specularIntensity, specularPower, pointLights[1].colour, textureColour, specCol, pointLights[1].range);
	float4 col3 = BlinnPhongBRDF(input.lightPos3, input.viewDirection, bumpNormal, specularIntensity, specularPower, pointLights[2].colour, textureColour, specCol, pointLights[2].range);
	float4 col4 = BlinnPhongBRDF(input.lightPos4, input.viewDirection, bumpNormal, specularIntensity, specularPower, pointLights[3].colour, textureColour, specCol, pointLights[3].range);

	finalColour = ambientColour * textureColour;

	finalColour += ((col1 +col2 + col3 + col4));
	finalColour = saturate(finalColour);
	finalColour = saturate(finalColour + specCol);


#if USE_ALPHA_MASKS
	float4 alpha = alphaMaskTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	finalColour.a = alpha.r;
#endif
	return finalColour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

