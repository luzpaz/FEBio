#include "stdafx.h"
#include "FEDiscreteSet.h"

//-----------------------------------------------------------------------------
FEDiscreteSet::FEDiscreteSet(FEMesh* pm) : m_pmesh(pm)
{

}

//-----------------------------------------------------------------------------
void FEDiscreteSet::create(int n)
{
	m_pair.resize(n);
}

//-----------------------------------------------------------------------------
void FEDiscreteSet::add(int n0, int n1)
{
	NodePair p = { n0, n1 };
	m_pair.push_back(p);
}

//-----------------------------------------------------------------------------
void FEDiscreteSet::SetName(const std::string& name)
{
	m_name = name;
}

//-----------------------------------------------------------------------------
const std::string& FEDiscreteSet::GetName() const
{
	return m_name;
}

//-----------------------------------------------------------------------------
void FEDiscreteSet::Serialize(DumpStream& ar)
{
	if (ar.IsSaving())
	{
		ar << m_name;
		ar << m_pair;
	}
	else
	{
		ar >> m_name;
		ar >> m_pair;
	}
}