#include "PointLight.h"

PointLight::PointLight()
{

}

PointLight::~PointLight()
{

}

void PointLight::SetDiffuseColour(float r, float g, float b, float a)
{
	m_vDiffuseColour.x = r;
	m_vDiffuseColour.y = g;
	m_vDiffuseColour.z = b;
	m_vDiffuseColour.w = a;
}

void PointLight::SetPosition(float x, float y, float z)
{
	m_vPosition.x = x;
	m_vPosition.y = y;
	m_vPosition.z = z;
}
