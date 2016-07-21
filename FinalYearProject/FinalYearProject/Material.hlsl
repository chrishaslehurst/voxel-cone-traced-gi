///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_LIGHTS 4
#define PI 3.14159265359


static const float DepthBias = 0.001f;
//Globals
cbuffer MatrixBuffer
{
	matrix mWorldMatrix;
	matrix mViewMatrix;
	matrix mProjectionMatrix;
};


#if USE_TEXTURE
Texture2D diffuseTexture;
#endif

#if USE_NORMAL_MAPS
Texture2D normalMapTexture;
#endif

#if USE_ALPHA_MASKS
Texture2D alphaMaskTexture;
#endif
#if USE_PHYSICALLY_BASED_SHADING
Texture2D roughnessMapTexture;
Texture2D metallicMapTexture;
#endif

TextureCube ShadowMap[NUM_LIGHTS];

SamplerState SampleType;
SamplerComparisonState ShadowMapSampler;

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
	float3 lightPos[4] : TEXCOORD2;
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


float3 CalculateLambertDiffuseBRDF(float3 DiffuseColour, float Metallic)
{
	// Lerp with metallic value to find the good diffuse and specular.
	float3 RealDiffuse = DiffuseColour - DiffuseColour * Metallic;
	return saturate(RealDiffuse * (1.f / PI));
}

//GGX/Trowbridge Reitz Normal Distribution Function (Disney/Epic: https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)
float NormalDistributionFunction(float NdotH, float Roughness)
{
	float a = Roughness * Roughness;
	float aSquared = a * a;
	float NdotHSquared = NdotH * NdotH;
	float DenomPart = (NdotHSquared * (aSquared - 1)) + 1;
	float Denom = PI * (DenomPart * DenomPart);

	return abs(aSquared / Denom);
}

//Schlick Fresnel with spherical gaussian approximation ( same source as above)
float SchlickFresnel(float Specular, float VdotH)
{
	float SpecularClamped = clamp(Specular, 0.02f, 0.99f);
	float SphericalGaussian = exp2((-5.55473f * VdotH - 6.98316f) * VdotH);
	return saturate(SpecularClamped + (1.f - SpecularClamped) * SphericalGaussian);
}

//Schlick Geometric Attenuation with Disney modification ( same source as above)
float SchlickGeometricAttenuation(float Roughness, float NdotV, float NdotL)
{
	float k = ((Roughness + 1) * (Roughness + 1)) * 0.125f;
	float G1 = NdotV / ((NdotV * (1 - k)) + k);
	float G2 = NdotL / ((NdotL * (1 - k)) + k);

	return saturate(G1 * G2);
}


//This is the cook torrance microfacet BRDF for physically based rendering
float4 CookTorranceBRDF(float3 ToLight, float3 ToCamera, float3 SurfaceNormal, float Roughness, float Metallic, float3 DiffuseColour)
{
	float4 Colour = float4(1.f, 1.f, 1.f, 1.f);
	
	float3 ToL = normalize(ToLight);
	float3 H = normalize(ToCamera + ToL);
	float NdotH = saturate(dot(SurfaceNormal, H));
	float VdotH = saturate(dot(ToCamera, H));
	float NdotL = saturate(dot(SurfaceNormal, ToL));
	float NdotV = abs(dot(SurfaceNormal, ToCamera)) + 1e-5f; //Add this really small number so this doesn't go to 0.. just to ensure no divide by 0 occurs..
	float Denom = 4 * NdotL * NdotV;

	// 0.03 default specular value for dielectric.]
	float3 realSpecularColour = lerp(0.03f, DiffuseColour, Metallic);

	float D = NormalDistributionFunction(NdotH, Roughness);
	float3 F = SchlickFresnel(1.f - Roughness, VdotH) * realSpecularColour;
	float G = SchlickGeometricAttenuation(Roughness, NdotV, NdotL);

	Colour.rgb = (CalculateLambertDiffuseBRDF(DiffuseColour, Metallic) * NdotH * (1.f - F)) + (((D * F * G ) / Denom ) );
	return saturate(Colour);
}


//Calculate the attenuation based on the distance to the light, and the lights range.
float CalculateAttenuation(float3 ToLight, float LightRange)
{
	//Attenuation
	float DistToLight = length(ToLight);
	float DistToLightNorm = 1.f - saturate(DistToLight * LightRange); //Pointlightrangercp = 1/Range - can be sent through from the app as this..
	return DistToLightNorm * DistToLightNorm;
}

float CalculateShadowFactor(int lightIdx, float3 ToLight, float ReciprocalRange)
{
	float LightDistanceSq = dot(ToLight, ToLight);
	float shadowFactor, x, y, z;

	for (z = -1.5; z <= 1.5; z += 1.5)
	{
		for (y = -1.5; y <= 1.5; y += 1.5)
		{
			for (x = -1.5; x <= 1.5; x += 1.5)
			{
				shadowFactor += saturate(ShadowMap[lightIdx].SampleCmp(ShadowMapSampler, -ToLight + float3(x, y, z), (LightDistanceSq * (ReciprocalRange * ReciprocalRange)) - DepthBias));
			}
		}
	}
	return (shadowFactor / 27.0);
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
	output.lightPos[0].xyz = lightPositions[0].xyz - worldPos.xyz;
	output.lightPos[1].xyz = lightPositions[1].xyz - worldPos.xyz;
	output.lightPos[2].xyz = lightPositions[2].xyz - worldPos.xyz;
	output.lightPos[3].xyz = lightPositions[3].xyz - worldPos.xyz;	


	return output;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Pixel Shader
float4 PSMain(PixelInput input) : SV_TARGET
{
	float4 FinalColour;
	float4 textureColour, normalMapCol;
	float3 SurfaceNormal;

	//Sample the diffuse colour from the texture.
#if USE_TEXTURE
	textureColour = diffuseTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	textureColour = textureColour * input.colour;
#else
	textureColour = input.colour;
#endif
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

#if USE_PHYSICALLY_BASED_SHADING
	//sample the roughness and metallic maps
	float roughness = roughnessMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y)).r;
	float4 metallic = metallicMapTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));

	//Compute lighting/shadow for all spotlights
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		float4 Light = CookTorranceBRDF(input.lightPos[i], input.viewDirection, SurfaceNormal, roughness, metallic.r, textureColour.rgb);
		FinalColour += Light * CalculateAttenuation(input.lightPos[i], pointLights[i].range) * pointLights[i].colour * pointLights[i].colour.w * CalculateShadowFactor(i, input.lightPos[i], pointLights[i].range);
	}
	
	//compute the direction light
	float4 DirectionalLight = CookTorranceBRDF(-lightDirection, input.viewDirection, SurfaceNormal, roughness, metallic.r, textureColour.rgb) * diffuseColour.w;
	FinalColour += DirectionalLight;
#endif
	FinalColour = saturate(FinalColour);

	//Perform any masking being used for transparency
#if USE_ALPHA_MASKS
	FinalColour.a = alphaMaskTexture.SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y)).r;
#endif
	return FinalColour;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

