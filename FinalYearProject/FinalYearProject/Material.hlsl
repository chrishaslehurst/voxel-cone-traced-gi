///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_LIGHTS 4
#define PI 3.14159265359
//Globals
cbuffer MatrixBuffer
{
	matrix mWorldMatrix;
	matrix mViewMatrix;
	matrix mProjectionMatrix;
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
#if USE_PHYSICALLY_BASED_SHADING
Texture2D roughnessMapTexture;
Texture2D metallicMapTexture;
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
struct VertexInput
{
	float4 position : POSITION;
	float3 normal	: NORMAL;
	float2 tex		: TEXCOORD0;
	float4 colour	: COLOR;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct PixelInput
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


float3 CalculateDiffuseBRDF(float3 DiffuseColour, float Metallic)
{

	// Lerp with metallic value to find the good diffuse and specular.
	float3 RealDiffuse = DiffuseColour - DiffuseColour * Metallic;
	return (RealDiffuse * (1.f / PI));
}

//GGX/Trowbridge Reitz Normal Distribution Function (Disney/Epic: https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)
float NormalDistributionFunction(float NdotH, float Roughness)
{
	float a = Roughness * Roughness;
	float aSquared = a * a;
	float NdotHSquared = NdotH * NdotH;
	float DenomPart = (NdotHSquared * (aSquared - 1)) + 1;
	float Denom = PI * (DenomPart * DenomPart);

	return aSquared / Denom;
}

//Schlick Fresnel with spherical gaussian approximation ( same source as above)
float SchlickFresnel(float Metallic, float VdotH)
{
	float MetalClamped = clamp(Metallic, 0.2f, 0.99f);
	float SphericalGaussian = exp2((-5.55473f * VdotH - 6.98316f) * VdotH);
	return(MetalClamped + (1.f - MetalClamped) * SphericalGaussian);
}

//Schlick Geometric Attenuation with Disney modification ( same source as above)
float SchlickGeometricAttenuation(float Roughness, float NdotV, float NdotL)
{
	float k = ((Roughness + 1) * (Roughness + 1)) * 0.125f;
	float G1 = NdotV / ((NdotV * (1 - k)) + k);
	float G2 = NdotL / ((NdotL * (1 - k)) + k);

	return G1 * G2;
}


//This is the cook torrance microfacet BRDF for physically based rendering
float4 CookTorranceBRDF(float3 ToLight, float3 ToCamera, float3 SurfaceNormal, float Roughness, float Metallic, float3 DiffuseColour, float3 LightColour, float LightRange)
{
	float4 Colour = float4(1.f, 1.f, 1.f, 1.f);

	float3 ToC = normalize(ToCamera);
	float3 ToL = normalize(ToLight);
	float3 H = normalize(ToC + ToL);
	float NdotH = saturate(dot(SurfaceNormal, H));
	float VdotH = saturate(dot(ToCamera, H));
	float NdotL = saturate(dot(SurfaceNormal, ToLight));
	float NdotV = abs(dot(SurfaceNormal, ToCamera)) + 1e-5f; //Add this really small number so this doesn't go to 0.. just to ensure no divide by 0 occurs..
	float Denom = 4 * NdotL * NdotV;

	float D = NormalDistributionFunction(NdotH, Roughness);
	float F = SchlickFresnel(Metallic, VdotH);
	float G = SchlickGeometricAttenuation(Roughness, NdotV, NdotL);


	// 0.03 default specular value for dielectric.
	float3 realSpecularColor = lerp(0.03f, DiffuseColour, Metallic);


	Colour.rgb = ((CalculateDiffuseBRDF(DiffuseColour, Metallic) * NdotH) + realSpecularColor * ((D * F * G ) / Denom));
	return saturate(Colour);
}


//Calculate the attenuation based on the distance to the light, and the lights range.
float CalculateAttenuation(float3 ToLight, float LightRange)
{
	float DistToLight = length(ToLight);
	//Attenuation
	float DistToLightNorm = 1.f - saturate(DistToLight * LightRange); //Pointlightrangercp = 1/Range - can be sent through from the app as this..
	return DistToLightNorm * DistToLightNorm;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Vertex Shader

PixelInput VSMain(VertexInput input)
{
	PixelInput output;

	//change the position vector to be 4 units for proper matrix calculations
	input.position.w = 1.f;

	//Calculate the position of the vertex against the world, view and projection matrices.
	output.position = mul(input.position, mWorldMatrix);
	float4 worldPos = output.position;
	output.position = mul(output.position, mViewMatrix);
	output.position = mul(output.position, mProjectionMatrix);

	//Store the input col for the pixel shader to make use of
	output.colour = input.colour;
	output.tex = input.tex;

	//Normal needs to be transformed to world space and then normalised before being sent to pixel shader
	output.normal = mul(input.normal, (float3x3)mWorldMatrix);
	output.normal = normalize(output.normal);

	//transform tangent to world space then normalise
	output.tangent = mul(input.tangent, (float3x3)mWorldMatrix);
	output.tangent = normalize(output.tangent);

	//transform binormal to world space then normalise
	output.binormal = mul(input.binormal, (float3x3)mWorldMatrix);
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
float4 PSMain(PixelInput input) : SV_TARGET
{
	float4 FinalColour;
	float4 textureColour, normalMapCol;
	float3 SurfaceNormal;
	float4 specularIntensity;

	float4 specCol = float4(0.f, 0.f, 0.f, 0.f);

	//Sample the diffuse colour from the texture.
	textureColour = diffuseTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	textureColour = textureColour * input.colour;

	//Invert the light direction
	float3 lightDir = -lightDirection;
	FinalColour = ambientColour * textureColour;
	//Get the surface normal, either from the normal map or from the vertex shader output...
#if USE_NORMAL_MAPS
	normalMapCol = normalMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	//Expand range of normal map to -1 / +1
	normalMapCol = (normalMapCol * 2) - 1.f;
	// Calculate normal from the data in normal map.
	SurfaceNormal = (normalMapCol.x * input.tangent) + (normalMapCol.y * input.binormal) + (normalMapCol.z * input.normal);
	SurfaceNormal = normalize(SurfaceNormal);
#else
	SurfaceNormal = input.normal;
#endif
	
	//Find the specular intensity from the map if using them, otherwise from the material property
#if USE_SPECULAR_MAPS
	specularIntensity = specularMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
#else
	specularIntensity = specularColor;
#endif

#if USE_PHYSICALLY_BASED_SHADING

	float roughness = roughnessMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y)).r;
	float4 metallic = metallicMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));

	float4 col1 = CookTorranceBRDF(input.lightPos1, input.viewDirection, SurfaceNormal, roughness, metallic.r, textureColour.rgb, pointLights[0].colour, pointLights[0].range);
	col1 = col1 * CalculateAttenuation(input.lightPos1, pointLights[0].range) * pointLights[0].colour * pointLights[0].colour.w;
	float4 col2 = CookTorranceBRDF(input.lightPos2, input.viewDirection, SurfaceNormal, roughness, metallic.r, textureColour.rgb, pointLights[1].colour, pointLights[1].range);
	col2 = col2 * CalculateAttenuation(input.lightPos2, pointLights[1].range) * pointLights[1].colour;
	float4 col3 = CookTorranceBRDF(input.lightPos3, input.viewDirection, SurfaceNormal, roughness, metallic.r, textureColour.rgb, pointLights[2].colour, pointLights[2].range);
	col3 = col3 * CalculateAttenuation(input.lightPos3, pointLights[2].range) * pointLights[2].colour;
	float4 col4 = CookTorranceBRDF(input.lightPos4, input.viewDirection, SurfaceNormal, roughness, metallic.r, textureColour.rgb, pointLights[3].colour, pointLights[3].range);
	col4 = col4 * CalculateAttenuation(input.lightPos4, pointLights[3].range) * pointLights[3].colour;

	

	FinalColour += saturate((col1 + col2 + col3 + col4));
	
//	FinalColour = saturate(FinalColour);
#else
	//Use the Blinn Phong Model
	float4 col1 = BlinnPhongBRDF(input.lightPos1, input.viewDirection, SurfaceNormal, specularIntensity, specularPower, pointLights[0].colour, textureColour, specCol, pointLights[0].range);
	float4 col2 = BlinnPhongBRDF(input.lightPos2, input.viewDirection, SurfaceNormal, specularIntensity, specularPower, pointLights[1].colour, textureColour, specCol, pointLights[1].range);
	float4 col3 = BlinnPhongBRDF(input.lightPos3, input.viewDirection, SurfaceNormal, specularIntensity, specularPower, pointLights[2].colour, textureColour, specCol, pointLights[2].range);
	float4 col4 = BlinnPhongBRDF(input.lightPos4, input.viewDirection, SurfaceNormal, specularIntensity, specularPower, pointLights[3].colour, textureColour, specCol, pointLights[3].range);

	FinalColour += saturate((col1 + col2 + col3 + col4));
	FinalColour = saturate(FinalColour);
	FinalColour = saturate(FinalColour + specCol);
#endif

	//Perform any masking being used for transparency
#if USE_ALPHA_MASKS
	float4 alpha = alphaMaskTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	FinalColour.a = alpha.r;
#endif
	return FinalColour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

