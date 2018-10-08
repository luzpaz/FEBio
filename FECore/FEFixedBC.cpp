#include "stdafx.h"
#include "FEFixedBC.h"
#include "FENodeSet.h"
#include "FEModel.h"

//-----------------------------------------------------------------------------
FEFixedBC::FEFixedBC(FEModel* pfem) : FEBoundaryCondition(FEBC_ID, pfem)
{
	m_dof = -1;
}

//-----------------------------------------------------------------------------
FEFixedBC::FEFixedBC(FEModel* pfem, int node, int dof) : FEBoundaryCondition(FEBC_ID, pfem)
{
	m_node.push_back(node);
	m_dof = dof;
}

//-----------------------------------------------------------------------------
void FEFixedBC::AddNode(int node)
{
	m_node.push_back(node);
}

//-----------------------------------------------------------------------------
void FEFixedBC::AddNodes(const FENodeSet& ns)
{
	int N = ns.size();
	for (int i = 0; i<N; ++i) AddNode(ns[i]);
}

//-----------------------------------------------------------------------------
void FEFixedBC::SetDOF(int dof)
{
	m_dof = dof;
}

//-----------------------------------------------------------------------------
void FEFixedBC::Serialize(DumpStream& ar)
{
	if (ar.IsShallow()) return;

	FEBoundaryCondition::Serialize(ar);
	if (ar.IsSaving())
	{
		ar << m_node << m_dof;
	}
	else
	{
		ar >> m_node >> m_dof;
	}
}

//-----------------------------------------------------------------------------
void FEFixedBC::Activate()
{
	FEBoundaryCondition::Activate();
	if (m_dof >= 0)
	{
		FEMesh& mesh = GetFEModel()->GetMesh();
		int n = (int)m_node.size();
		for (int i = 0; i<n; ++i)
		{
			// make sure we only activate open dof's
			vector<int>& BC = mesh.Node(m_node[i]).m_BC;
			if (BC[m_dof] == DOF_OPEN) BC[m_dof] = DOF_FIXED;
		}
	}
}

//-----------------------------------------------------------------------------
void FEFixedBC::Deactivate()
{
	FEBoundaryCondition::Deactivate();
	if (m_dof >= 0)
	{
		FEMesh& mesh = GetFEModel()->GetMesh();
		int n = (int)m_node.size();
		for (int i = 0; i<n; ++i)
		{
			vector<int>& BC = mesh.Node(m_node[i]).m_BC;
			BC[m_dof] = DOF_OPEN;
		}
	}
}