//Globals..

//#include "VoxelGICommon.hlsl"
float4 convRGBA8ToVec4(uint val)
{
	return float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
}

uint convVec4ToRGBA8(float4 val)
{
	return (uint (val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

cbuffer VoxeliseVertexShaderBuffer
{
	matrix World;
	matrix mWorldToVoxelGrid;
	matrix WorldViewProj;
	matrix WorldInverseTranspose;
	matrix mAxisProjections[3];
};

////////////////////////////////////////////////////////////////////////////////
//Structs

struct VertexInput
{
	float4 PosL : POSITION;
	float2 Tex : TEXCOORD0;
	float3 Normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct GSInput
{
	float4 PosL : POSITION;
	float3 Normal : NORMAL;
	float2 Tex : TEXCOORD0;
};

struct PSInput
{
	int proj : ATTRIB0;
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 Normal : NORMAL;
	float2 Tex : TEXCOORD1;
};

////////////////////////////////////////////////////////////////////////////////
//Resources

RWTexture3D<uint> VoxelTex_Colour : register(u0); //This is where we will write the voxel colours to..


Texture2D diffuseTexture; 

////////////////////////////////////////////////////////////////////////////////
//Sample State
SamplerState SampleState;

//Functions
//Averages the color stored in a specific texel
void imageAtomicRGBA8Avg(RWTexture3D<uint> imgUI, uint3 coords, float4 val)
{
	val.rgb *= 255.0f; // Optimize following calculations
	uint newVal = convVec4ToRGBA8(val);
	uint prevStoredVal = 0;
	uint curStoredVal = 0;

	// Loop as long as destination value gets changed by other threads

	[allow_uav_condition] while (true)
	{
		InterlockedCompareExchange(imgUI[coords], prevStoredVal, newVal, curStoredVal);

		if (curStoredVal == prevStoredVal)
			break;

		prevStoredVal = curStoredVal;
		float4 rval = convRGBA8ToVec4(curStoredVal);
		rval.xyz = (rval.xyz* rval.w); // Denormalize
		float4 curValF = rval + val; // Add new value
		curValF.xyz /= (curValF.w); // Renormalize
		newVal = convVec4ToRGBA8(curValF);
	}
}

////////////////////////////////////////////////////////////////////////////////
//Shaders..

GSInput VSMain(VertexInput input)
{
	GSInput output;

	output.PosL = input.PosL;

	output.Normal = mul(input.Normal, (float3x3)WorldInverseTranspose);
	output.Normal = normalize(output.Normal);

	output.Tex = input.Tex;

	return output;
}

////////////////////////////////////////////////////////////////////////////////

[maxvertexcount(3)]
void GSMain(triangle GSInput input[3], inout TriangleStream<PSInput> outputStream)
{
	PSInput output;
	
	float3 normal = cross(normalize(input[1].PosL - input[0].PosL), normalize(input[2].PosL - input[0].PosL)); //Get face normal

	const float halfVoxelSize = (1.f / 64.f) * 0.5f;
	float3 triCentre = ((input[0].PosL + input[1].PosL + input[2].PosL) / 3.f);

	//First we find the dominant axis of the triangles normal, in order to maximise the number of fragments that will be generated (Conservative rasterisation)
	float axis[] = {
		abs(dot(normal, float3(1.0f, 0.0f, 0.0f))),
		abs(dot(normal, float3(0.0f, 1.0f, 0.0f))),
		abs(dot(normal, float3(0.0f, 0.0f, 1.0f))),
	};

	//Find dominant axis
	int index = 0;
	int i = 0;

	[unroll]
	for (i = 1; i < 3; i++)
	{
		[flatten]
		if (axis[i] > axis[i - 1])
			index = i;
	}

	[unroll]
	for (i = 0; i < 3; i++)
	{	
		float4 inputPosL;

		[flatten]
		switch (index)
		{
		case 0:
			inputPosL = float4(input[i].PosL.yzx, 1.0f);
			break;
		case 1:
			inputPosL = float4(input[i].PosL.zxy, 1.0f);
			break;
		case 2:
			inputPosL = float4(input[i].PosL.xyz, 1.0f);
			break;
		}
		//This will enlarge the tri slightly, conservative rasterisation
		float3 toCentre = normalize(input[i].PosL.xyz - triCentre);
		toCentre = toCentre * halfVoxelSize;
		//toCentre = float3(0, 0, 0);

		output.Tex = input[i].Tex;
		output.PosW = mul(input[i].PosL, mWorldToVoxelGrid);
		output.PosH = mul(inputPosL, WorldViewProj);
		output.Normal = input[i].Normal;
		output.proj = index;

		outputStream.Append(output);
	}
}

////////////////////////////////////////////////////////////////////////////////

void PSMain(PSInput input)
{
	int3 texDimensions;
	VoxelTex_Colour.GetDimensions(texDimensions.x, texDimensions.y, texDimensions.z);

	uint3 texCoord = uint3((((input.PosW.x * 0.5) + 0.5f) * texDimensions.x),
						   (((input.PosW.y * 0.5) + 0.5f) * texDimensions.x),
						   (((input.PosW.z * 0.5) + 0.5f) * texDimensions.x));

	float4 Colour =  diffuseTexture.Sample(SampleState, input.Tex);
	
	if (all(texCoord < texDimensions.xyz) && all(texCoord >= 0)) // this is needed or things outside the range seem to get in?
	{
		VoxelTex_Colour[texCoord] = convVec4ToRGBA8(Colour * 255.f);
	//	imageAtomicRGBA8Avg(VoxelTex_Colour, texCoord.xyz, Colour);
	}	
}

////////////////////////////////////////////////////////////////////////////////