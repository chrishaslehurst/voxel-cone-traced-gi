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
RWTexture3D<uint> VoxelTex_Normals : register(u1);

RWTexture3D<bool> VoxelOccupancy : register(u2);

Texture2D diffuseTexture; 

////////////////////////////////////////////////////////////////////////////////
//Sample State
SamplerState SampleState;

//Functions

//Averages the color stored in a texel atomically
void imageAtomicRGBA8Avg(RWTexture3D<uint> img, uint3 coords, float4 val)
{
	val *= 255.0f;
	uint newVal = convVec4ToRGBA8(val);
	
	uint prevStoredVal = 0;
	uint curStoredVal = 0;

	InterlockedCompareExchange(img[coords], prevStoredVal, newVal, curStoredVal);
	// Loop as long as destination value gets changed by other threads
	[allow_uav_condition] do 
	{
		prevStoredVal = curStoredVal;

		float4 rval = convRGBA8ToVec4(curStoredVal); //Convert to float4s to compute average, will overflow rgba8's
		float4 curValF = rval + val;	 // Add new value
		float a = rval.w / 255.f;
		curValF.xyz /= (1 + a);			 //average the values
		curValF.w = 255.f;

		newVal = convVec4ToRGBA8(curValF); //convert back to rgba8 for comparison/storage
	
		InterlockedCompareExchange(img[coords], prevStoredVal, newVal, curStoredVal);
	} while (prevStoredVal != curStoredVal);
}

////////////////////////////////////////////////////////////////////////////////
//Shaders..

GSInput VSMain(VertexInput input)
{
	GSInput output;

	output.PosL = mul(input.PosL, World);
	output.Normal = normalize(input.Normal);
	output.Tex = input.Tex;

	return output;
}

////////////////////////////////////////////////////////////////////////////////

[maxvertexcount(3)]
void GSMain(triangle GSInput input[3], inout TriangleStream<PSInput> outputStream)
{
	PSInput output;
	
	float3 normal = cross(normalize(input[1].PosL - input[0].PosL), normalize(input[2].PosL - input[0].PosL)); //Get face normal

	
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
		//This will enlarge the tri slightly, conservative rasterisation
		float3 toCentre = normalize(input[i].PosL.xyz - triCentre);
		toCentre = normalize(toCentre);
		toCentre = toCentre * 15.f;
		//toCentre = float3(0, 0, 0);
		float4 inputPosL = float4(input[i].PosL + toCentre,1.f);

		[flatten]
		switch (index)
		{
		case 0:
			inputPosL = float4(inputPosL.yzx, 1.0f);
			break;
		case 1:
			inputPosL = float4(inputPosL.zxy, 1.0f);
			break;
		case 2:
			inputPosL = float4(inputPosL.xyz, 1.0f);
			break;
		}

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

	uint3 tileCoord = uint3(texCoord.x / 32, texCoord.y / 32, texCoord.z / 16);
	
	float4 Colour =  diffuseTexture.Sample(SampleState, input.Tex);
	VoxelOccupancy[uint3(0,0,0)] = true;
	if (all(texCoord < texDimensions.xyz) && all(texCoord >= 0)) 
	{
		VoxelOccupancy[tileCoord] = true;

		imageAtomicRGBA8Avg(VoxelTex_Colour, texCoord.xyz, Colour);
	}	
}

////////////////////////////////////////////////////////////////////////////////