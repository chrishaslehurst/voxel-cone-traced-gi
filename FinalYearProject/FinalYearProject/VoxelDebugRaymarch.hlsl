//Globals
cbuffer MatrixBuffer
{
	matrix mWorld;
	matrix mView;
	matrix mProjection;
	float3 eyePos;
	float padding;
};

cbuffer VoxelGridBuffer
{
	matrix mWorldToVoxelGrid;
	matrix mVoxelGridToWorld;
	float3 voxelGridSize; //The dimension in worldspace of the voxel volume...
	uint VoxelTextureSize; //the texture resolution of the voxel grid.
};

Texture3D<float4> gVoxelVolume       : register(t0);

RWTexture2D<float4> gOutputTexture : register(u0);

SamplerState gVolumeSampler        : register(s0);


float3 WorldToVolume(float3 world)
{
	return mul(mWorldToVoxelGrid, float4(world.xyz, 1.f));
	//return (world.xyz / float(15.0f).xxx + 1.0f) * 0.5f;
}

float4 convRGBA8ToVec4(uint val)
{
	return float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
}

[numthreads(16, 16, 1)]
void RaymarchCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int mipLevel = 0;

	uint2 globalCoords = dispatchThreadID.xy;

	uint2 outputDimensions;
	gOutputTexture.GetDimensions(outputDimensions.x, outputDimensions.y);

	uint3 volumeDimensions;
	gVoxelVolume.GetDimensions(volumeDimensions.x, volumeDimensions.y, volumeDimensions.z);

	if (globalCoords.x >= outputDimensions.x || globalCoords.y >= outputDimensions.y)
		return;

	//Find world space ray from pixel coordinates
	float2 sampleCoords = float2(globalCoords.xy) / float2(outputDimensions.xy);
	float2 clipCoords = sampleCoords * 2.0f - 1.0f;
	float2 uv = clipCoords;
	uv *= outputDimensions.x / outputDimensions.y;

	float aspectRatio = outputDimensions.x / outputDimensions.y;
	float2 pixelScreen;
	pixelScreen.x = (2 * ((globalCoords.x) / outputDimensions.x) - 1);
	pixelScreen.y =  (2 * ((globalCoords.y) / outputDimensions.y) - 1) * -1.f;
	pixelScreen.x = pixelScreen.x / mProjection[0][0];
	pixelScreen.y = pixelScreen.y / mProjection[1][1];

	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 forward = normalize(float3(mView[2][0], mView[2][1], mView[2][2]));

	float3 right = cross(forward, up);
	up = cross(forward, right);

	float projheight = mProjection[1][1];
	forward += (clipCoords.x * right / mProjection[0][0] - clipCoords.y * up / projheight);
	forward = normalize(forward);

	//Find amount to advance each sample
	const int sampleCount = 1024;
	const float maxDistance = 20.0f;
	forward *= maxDistance / sampleCount;

	//Setup ray
	float3 samplePosition = -eyePos;
	uint sampleColint = 0;
	float4 sampleColor;
	float4 finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float occlusion = 0.0f;

	[loop]
	for (int i = 0; i < sampleCount; i++)
	{
		//sampleColor = convRGBA8ToVec4(gVoxelVolume.SampleLevel(gVolumeSampler, WorldToVolume(samplePosition), 0));
		int3 volumeCoord = WorldToVolume(samplePosition);// *volumeDimensions / pow(2, mipLevel);
		//volumeCoord.x /= 6;
		//volumeCoord.x += volumeDimensions.y * 5;
		volumeCoord = int3(0, 0, 0);
		finalColor = gVoxelVolume.Load(int4(volumeCoord, mipLevel));
		occlusion = finalColor.a;
//		if (sampleColor.a > 0.0f)
//			sampleColor.a = 1.0f;

		//Average colors if this ray is partially occluded
//		if (occlusion < 1.0f)
//		{
//			finalColor.rgb += sampleColor.rgb;
//		}
//		else
////		{
//			break;
//		}

//		occlusion += sampleColor.a;
//		samplePosition += forward;
	}

	if (occlusion < 1)
	{
		finalColor.rgb = float3(1.f, 1.f, 1.f);
	}

	//finalColor.a = 1;
	gOutputTexture[globalCoords] = finalColor;
}