# voxel-cone-traced-gi

Build Instructions
------------------
This project will only run on Windows 10 due to the use of DirectX11.3. The Volume Tiled Resources functionality is limited to a few GPUs at present also, so may be a barrier to building and running the renderer.

If you wish to download and build this project you will need the assets folder. This is a little large and uneccesary to track in a git repo, so you can grab it from dropbox if you want it.

https://www.dropbox.com/s/w2q72ptt9r7baha/RendererAssets.zip?dl=0

If this link goes stale for whatever reason, shoot me a message and I'll send over the assets folder.

Project Description
-------------------
For my Master's thesis I chose to investigate a method of reducing the impact of Voxel Cone Tracing on GPU memory occupancy.

Voxel Cone tracing is a method of computing global illumination. The scene is first voxelized and stored in a 3D texture. This simplified version of the scene is then sampled by approximate cones to calculate global illumination.

I proposed storing the voxelized scene in a 3D texture using volume tiled resources, a new feature in DirectX 11.3 and 12. Volume Tiled Resources separate the backing memory from the logical resource, meaning only the parts of the volume which contain data have memory mapped to them. 

In order to investigate this, I developed a renderer using the DirectX 11.3 API. The renderer needed to be representative of a modern video game engine in order to test in a realistic environment. As such it supports physically based shading, omnidirectional PCF shadows, normal, metallic and roughness maps as well as global illumination via Voxel Cone Tracing. The scene renders in real-time and dynamic objects both receive and contribute to global illumination.



