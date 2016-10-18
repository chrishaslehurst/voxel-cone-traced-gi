struct OctreeNode
{
	bool bNeedsDividing;
	int	 iChildNodeIndex;
	int3 iChildDataIndex;
};

RWStructuredBuffer<OctreeNode> Octree;
RWTexture3D<uint> VoxelFragmentList;

[numthreads(1, 1, 1)]
void CSDivideOctreeNodes( uint3 id : SV_DispatchThreadID )
{
	uint numOctreeNodes, stride;
	Octree.GetDimensions(numOctreeNodes, stride);
	if (id < numOctreeNodes)
	{
		if (Octree[id].bNeedsDividing)
		{
			Octree[id].iChildNodeIndex = (Octree.IncrementCounter() * 8) + 1;
			int subId = Octree[id].iChildNodeIndex;
			for (int i = 0; i < 8; i++)
			{
				Octree[subId + i].bNeedsDividing = false;
				Octree[subId + i].iChildNodeIndex = 0;
			}
		}
	}
}

//the last parameter of OMSetRenderTargetsAndUnorderedAccessViews() is used to initialize the counter value.

[numthreads(NUM_THREADS, NUM_THREADS, 1)]
void CSFindNodesToDivide(uint3 id: SV_DispatchThreadID)
{
	uint3 size = 0;
	VoxelFragmentList.GetDimensions(size.x, size.y, size.z);

	uint threadNumber = id.x + (id.y * NUM_THREADS * NUM_GROUPS) + (id.z * NUM_THREADS * NUM_THREADS * NUM_GROUPS);

	uint index = threadNumber * NUM_TEXELS_PER_THREAD;

	float3 texCoord;

	if (Octree[0].iChildNodeIndex == 0)
	{
		Octree[0].bNeedsDividing = true;
		return;
	}

	for (int i = 0; i < NUM_TEXELS_PER_THREAD; i++)
	{
		texCoord.x = index / (size * size);
		texCoord.y = (index / size.x) % size.x;
		texCoord.z = index % size.x;
		
		int levelRes = 1;
		int level = 0;
		int childIndex = Octree[0].iChildNodeIndex;
		int xOffset = 0;
		int yOffset = 0; 
		int zOffset = 0;
		
		while (levelRes <= size.x)
		{
			int sizeOfDivision = size.x * pow(0.5, level+1);
			divSubIndex = 0;
			if (texCoord.x < xOffset + sizeOfDivision)
			{
				//its in the lower x half.. 
			}
			else
			{
				//its in the upper x half
				divSubIndex += 1;
				xOffset += sizeOfDivision; //add this to the xoffset for future calcs..
			}
			
			if (texCoord.y < yOffset + sizeOfDivision)
			{
				//its in the lower y half.. 
			}
			else
			{
				//its in the upper y half
				divSubIndex += 2;
				yOffset += sizeOfDivision; //add this to the xoffset for future calcs..
			}
			if (texCoord.z < zOffset + sizeOfDivision)
			{
				//its in the lower z half.. 
			}
			else
			{
				//its in the upper z half
				divSubIndex += 4;
				zOffset += sizeOfDivision; //add this to the xoffset for future calcs..
			}

			if (Octree[childIndex + divSubIndex].iChildNodeIndex == 0)
			{
				Octree[childIndex + divSubIndex].bNeedsDividing = true;
				break;
			}
			else
			{
				childIndex = Octree[childIndex + divSubIndex].iChildNodeIndex;
			}
			level++;
			levelRes *= 2;
		}

		index++;
	}
}