#include "MaterialLibrary.h"
#include "Debugging.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MaterialLibrary::MaterialLibrary()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MaterialLibrary::~MaterialLibrary()
{
	for (map<string, Material*>::iterator it = m_MaterialMap.begin(); it != m_MaterialMap.end(); it++)
	{
		delete it->second;
		it->second = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MaterialLibrary::LoadMaterialLibrary(ID3D11Device* pDevice, HWND hwnd, const char* filename)
{
	fstream fin;
	fin.open(filename);
	if (fin.fail())
	{
		VS_LOG_VERBOSE("Failed to open material file");
		return false;
	}
	char inputChar;
	std::string s;
	fin >> s;
	while (fin)
	{	
		if (s == "#")
		{
			fin.get(inputChar);
			while (inputChar != '\n')
			{
				fin.get(inputChar);
			}
			fin >> s;
		}
		else if (fin && s == "newmtl")
		{
			//We've found a new material so parse the values
			std::string sMaterialName("");
			std::string trashBuffer;
			float Ns(0.f), Ni(0.f), d, Tr(0.f), illum(0.f);
			float TfX(0.f), TfY(0.f), TfZ(0.f);
			float KaR(0.f), KaG(0.f), KaB(0.f);
			float KdR(0.f), KdG(0.f), KdB(0.f);
			float KsR(0.f), KsG(0.f), KsB(0.f);
			float KeR(0.f), KeG(0.f), KeB(0.f);
			std::string map_Ka (""), map_Kd(""), map_d(""), map_Ks(""), map_bump(""), bump("");

			fin >> sMaterialName;
			fin >> trashBuffer;
			fin >> Ns;
			fin >> trashBuffer;
			fin >> Ni;
			fin >> trashBuffer;
			fin >> d;
			fin >> trashBuffer;
			fin >> Tr;
			fin >> trashBuffer;
			fin >> TfX >> TfY >> TfZ;
			fin >> trashBuffer;
			fin >> illum;
			fin >> trashBuffer;
			fin >> KaR >> KaG >> KaB;
			fin >> trashBuffer;
			fin >> KdR >> KdG >> KdB;
			fin >> trashBuffer;
			fin >> KsR >> KsG >> KsB;
			fin >> trashBuffer;
			fin >> KeR >> KeG >> KeB;
			
			fin >> s;
			while (fin && s != "newmtl")
			{
				if (s == "map_Ka")
				{
					fin >> map_Ka;
				}
				if (s == "map_Kd")
				{
					fin >> map_Kd;
				}
				if (s == "map_d")
				{
					fin >> map_d;
				}
				if (s == "map_bump")
				{
					fin >> map_bump;
				}
				if (s == "bump")
				{
					fin >> bump;
				}
				if (s == "map_Ks")
				{
					fin >> map_Ks;
				}
				
				fin >> s;
			}
			//Create a material from the parameters..
			Material* pMat = new Material;
			std::string AssetFolderString = "../Assets/";
			

			pMat->SetSpecularProperties(KsR, KsG, KsB, Ns);
			
			if (map_bump != "")
			{
				map_bump = AssetFolderString + map_bump;
				wstring wideBumpName = wstring(map_bump.begin(), map_bump.end());
				pMat->SetNormalMap(pDevice, &wideBumpName[0]);
				pMat->SetHasNormal(true);
			}
			else
			{
				pMat->SetHasNormal(false);
			}

			if (map_Ks != "")
			{
				map_Ks = AssetFolderString + map_Ks;
				wstring wideSpecName = wstring(map_Ks.begin(), map_Ks.end());
				pMat->SetSpecularMap(pDevice, &wideSpecName[0]);
				pMat->SetHasSpecular(true);
			}
			else
			{
				pMat->SetHasSpecular(false);
			}


			//Do this stage last as it compiles the shader and the defines need to be set before this.
			if (map_Ka != "")
			{
				map_Ka = AssetFolderString + map_Ka;
				wstring wideDiffuseName = wstring(map_Ka.begin(), map_Ka.end());
				pMat->Initialise(pDevice, hwnd, &wideDiffuseName[0]);
			}
			else
			{
				//TODO: FIGURE OUT WHY SOME DONT HAVE A TEX
				pMat->Initialise(pDevice, hwnd, L"textures\vase_hanging.tga");
			}

			m_MaterialMap[sMaterialName] = pMat;
		}
		
	}

	return true;
}

Material* MaterialLibrary::GetMaterial(string sMaterialName)
{
	return m_MaterialMap[sMaterialName];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


