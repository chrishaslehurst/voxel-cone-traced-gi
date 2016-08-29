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

cbuffer MatrixBuffer
{
	matrix mWorld;
};

cbuffer ProjectionMatrixBuffer
{
	matrix viewProjs[3];
	float3 voxelGridSize; //The dimension in worldspace of the voxel volume...
	uint VoxelTextureSize; //the texture resolution of the voxel grid.
};

cbuffer VoxelGridBuffer
{
	matrix mWorldToVoxelGrid;
	matrix mVoxelGridToWorld;
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

struct GSInput
{
	float4 position : SV_Position;
	float3 normal : TEXCOORD0;
	float2 tex : TEXCOORD1;
};

struct PSInput
{
	float4 Position : SV_Position;
	float3 normal : TEXCOORD0;
	float2 tex : TEXCOORD1;
};

////////////////////////////////////////////////////////////////////////////////
//Resources

RWTexture3D<uint> VoxelTex_Colour : register(u0); //This is where we will write the voxel colours to..

//TODO: Get this working
//Texture2D diffuseTexture; //TODO: Set up texture and sampler..

////////////////////////////////////////////////////////////////////////////////
//Sample State
//SamplerState SampleType;

////////////////////////////////////////////////////////////////////////////////
//Shaders..

GSInput VSMain(VertexInput input)
{
	GSInput output;

	output.position = mul(mWorld, float4(input.position.xyz, 1.0f));

	output.normal = mul(input.normal, (float3x3)mWorld);
	output.normal = normalize(output.normal);

	output.tex = input.tex;

	return output;
}

////////////////////////////////////////////////////////////////////////////////

[maxvertexcount(3)]
void GSMain(triangle GSInput input[3], inout TriangleStream<PSInput> outputStream)
{
	PSInput output;
	//First we find the dominant axis of the triangles normal, in order to maximise the number of fragments that will be generated (Conservative rasterisation)
	const float3 worldAxes[3] = { float3(1.0, 0.0, 0.0),
		float3(0.0, 1.0, 0.0),
		float3(0.0, 0.0, 1.0) };

	float3 voxelSize = voxelGridSize / float3(VoxelTextureSize, VoxelTextureSize, VoxelTextureSize);

	uint projectionAxis = 0;
	float maxArea = 0.f;

	for (uint i = 0; i < 3; ++i)
	{
		//assuming per triangle normals for simplicity..
		float area = abs(dot(input[0].normal, worldAxes[i]));
		{
			if (area > maxArea)
			{
				maxArea = area;
				projectionAxis = i;
			}
		}
	}

	float3 middle = mul(viewProjs[projectionAxis], float4(((input[0].position + input[1].position + input[2].position) / 3.f).xyz, 1.f));

	for (uint j = 0; j < 3; j++)
	{
		output.Position = mul(viewProjs[projectionAxis], input[j].position);
		output.Position += float4(normalize(output.Position.xyz - middle) * (voxelSize.x / 2.f), 1.f);

		output.normal = input[j].normal;
		output.tex = input[j].tex;

		outputStream.Append(output);
		
	}
}

////////////////////////////////////////////////////////////////////////////////

//Averages the color stored in a specific texel
void imageAtomicRGBA8Avg(RWTexture3D<uint> imgUI, uint3 coords, float4 val)
{
	val.rgb *= 255.0f; // Optimize following calculations
	uint newVal = convVec4ToRGBA8(val);
	uint prevStoredVal = 0;
	uint curStoredVal = 0;

	// Loop as long as destination value gets changed by other threads

	[allow_uav_condition] do //While loop does not work and crashes the graphics driver, but do while loop that does the same works; compiler error?
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


	} while (true);
}

void PSMain(PSInput input)
{
	float4 texCoord = mul(mWorldToVoxelGrid, input.Position);
	//float4 Colour = SampleGrad(SampleType, input.tex, ddx(input.tex.x), ddy(input.tex.y));
	float4 Colour = float4(1.f, 1.f, 1.f, 1.f);

	if (all(texCoord < 64) && all(texCoord >= 0)) // this is needed or things outside the range seem to get in?
	{
		VoxelTex_Colour[texCoord.xyz] = convVec4ToRGBA8(Colour);
		imageAtomicRGBA8Avg(VoxelTex_Colour, texCoord.xyz, Colour);
	}
}

////////////////////////////////////////////////////////////////////////////////