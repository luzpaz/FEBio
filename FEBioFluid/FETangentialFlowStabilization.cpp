//
//  FETangentialFlowStabilization.cpp
//  FEBioFluid
//
//  Created by Gerard Ateshian on 3/2/17.
//  Copyright © 2017 febio.org. All rights reserved.
//

#include "FETangentialFlowStabilization.h"
#include "FECore/FEModel.h"
#include "FEFluid.h"

//-----------------------------------------------------------------------------
// Parameter block for pressure loads
BEGIN_PARAMETER_LIST(FETangentialFlowStabilization, FESurfaceLoad)
    ADD_PARAMETER(m_beta, FE_PARAM_DOUBLE, "beta");
END_PARAMETER_LIST()

//-----------------------------------------------------------------------------
//! constructor
FETangentialFlowStabilization::FETangentialFlowStabilization(FEModel* pfem) : FESurfaceLoad(pfem)
{
    m_beta = 1.0;
    m_rho = 1.0;
    
    // get the degrees of freedom
    m_dofVX = pfem->GetDOFIndex("vx");
    m_dofVY = pfem->GetDOFIndex("vy");
    m_dofVZ = pfem->GetDOFIndex("vz");
    m_dofE = pfem->GetDOFIndex("e");
}

//-----------------------------------------------------------------------------
//! allocate storage
void FETangentialFlowStabilization::SetSurface(FESurface* ps)
{
    FESurfaceLoad::SetSurface(ps);
}

//-----------------------------------------------------------------------------
//! initialize
bool FETangentialFlowStabilization::Init()
{
    FEModelComponent::Init();
    
    FESurface* ps = &GetSurface();
    ps->Init();
    // get fluid density from first surface element
    // assuming the entire surface bounds the same fluid
    FESurfaceElement& el = ps->Element(0);
    FEMesh* mesh = ps->GetMesh();
    FEElement* pe = mesh->FindElementFromID(el.m_elem[0]);
    if (pe == nullptr) return false;
    // get the material
    FEMaterial* pm = GetFEModel()->GetMaterial(pe->GetMatID());
    FEFluid* fluid = dynamic_cast<FEFluid*> (pm);
    if (fluid == nullptr) return false;
    // get the density
    m_rho = fluid->m_rhor;
    
    return true;
}

//-----------------------------------------------------------------------------
//! calculates the stiffness contribution due to hydrostatic pressure

void FETangentialFlowStabilization::ElementStiffness(FESurfaceElement& el, matrix& ke)
{
    int nint = el.GaussPoints();
    int neln = el.Nodes();
    
    mat3dd I(1);
    
    // gauss weights
    double* w = el.GaussWeights();
    
    // nodal coordinates
    FEMesh& mesh = *m_psurf->GetMesh();
    vec3d rt[FEElement::MAX_NODES], vt[FEElement::MAX_NODES];
    for (int j=0; j<neln; ++j) {
        rt[j] = mesh.Node(el.m_node[j]).m_rt;
        vt[j] = mesh.Node(el.m_node[j]).get_vec3d(m_dofVX, m_dofVY, m_dofVZ);
    }
    
    // repeat over integration points
    ke.zero();
    for (int k=0; k<nint; ++k)
    {
        double* N = el.H(k);
        double* Gr = el.Gr(k);
        double* Gs = el.Gs(k);
        
        vec3d v(0,0,0);
        vec3d dxr(0,0,0), dxs(0,0,0);
        for (int i=0; i<neln; ++i)
        {
            v += vt[i]*N[i];
            dxr += rt[i]*Gr[i];
            dxs += rt[i]*Gs[i];
        }
        
        vec3d n = dxr ^ dxs;
        double da = n.unit();
        
        vec3d vtau = (I - dyad(n))*v;
        double vmag = vtau.unit();
        mat3d K = (I - dyad(n) + dyad(vtau))*(-m_beta*m_rho*vmag*da*w[k]);
        
        // calculate stiffness component
        for (int i=0; i<neln; ++i)
            for (int j=0; j<neln; ++j)
            {
                mat3d Kab = K*(N[i]*N[j]);
                ke[3*i  ][3*j  ] -= Kab(0,0); ke[3*i  ][3*j+1] -= Kab(0,1); ke[3*i  ][3*j+2] -= Kab(0,2);
                ke[3*i+1][3*j  ] -= Kab(1,0); ke[3*i+1][3*j+1] -= Kab(1,1); ke[3*i+1][3*j+2] -= Kab(1,2);
                ke[3*i+2][3*j  ] -= Kab(2,0); ke[3*i+2][3*j+1] -= Kab(2,1); ke[3*i+2][3*j+2] -= Kab(2,2);
            }
    }
}

//-----------------------------------------------------------------------------
//! calculates the element force

void FETangentialFlowStabilization::ElementForce(FESurfaceElement& el, vector<double>& fe)
{
    // nr integration points
    int nint = el.GaussPoints();
    
    // nr of element nodes
    int neln = el.Nodes();
    
    mat3dd I(1);
    
    // nodal coordinates
    FEMesh& mesh = *m_psurf->GetMesh();
    vec3d rt[FEElement::MAX_NODES], vt[FEElement::MAX_NODES];
    for (int j=0; j<neln; ++j) {
        rt[j] = mesh.Node(el.m_node[j]).m_rt;
        vt[j] = mesh.Node(el.m_node[j]).get_vec3d(m_dofVX, m_dofVY, m_dofVZ);
    }
    
    // repeat over integration points
    zero(fe);
    double* w  = el.GaussWeights();
    for (int j=0; j<nint; ++j)
    {
        double* N  = el.H(j);
        double* Gr = el.Gr(j);
        double* Gs = el.Gs(j);
        
        // traction at integration points
        vec3d v(0,0,0);
        vec3d dxr(0,0,0), dxs(0,0,0);
        for (int i=0; i<neln; ++i)
        {
            v += vt[i]*N[i];
            dxr += rt[i]*Gr[i];
            dxs += rt[i]*Gs[i];
        }
        
        vec3d n = dxr ^ dxs;
        double da = n.unit();
        
        // tangential traction = -beta*density*|tangential velocity|*(tangential velocity)
        vec3d vtau = (I - dyad(n))*v;
        double vmag = vtau.norm();
        // force vector (change sign for inflow vs outflow)
        vec3d f = vtau*(-m_beta*m_rho*vmag*da*w[j]);
        
        for (int i=0; i<neln; ++i)
        {
            fe[3*i  ] += N[i]*f.x;
            fe[3*i+1] += N[i]*f.y;
            fe[3*i+2] += N[i]*f.z;
        }
    }
}

//-----------------------------------------------------------------------------

void FETangentialFlowStabilization::Serialize(DumpStream& ar)
{
    FESurfaceLoad::Serialize(ar);
}

//-----------------------------------------------------------------------------
void FETangentialFlowStabilization::UnpackLM(FEElement& el, vector<int>& lm)
{
    FEMesh& mesh = *GetSurface().GetMesh();
    int N = el.Nodes();
    lm.resize(N*4);
    for (int i=0; i<N; ++i)
    {
        int n = el.m_node[i];
        FENode& node = mesh.Node(n);
        vector<int>& id = node.m_ID;
        
        lm[4*i  ] = id[m_dofVX];
        lm[4*i+1] = id[m_dofVY];
        lm[4*i+2] = id[m_dofVZ];
        lm[4*i+3] = id[m_dofE];
    }
}

//-----------------------------------------------------------------------------
void FETangentialFlowStabilization::StiffnessMatrix(const FETimeInfo& tp, FESolver* psolver)
{
    matrix ke;
    vector<int> lm;
    
    FESurface& surf = GetSurface();
    int npr = surf.Elements();
    for (int m=0; m<npr; ++m)
    {
        // get the surface element
        FESurfaceElement& el = m_psurf->Element(m);
        
        // calculate nodal normal tractions
        int neln = el.Nodes();
        vector<double> tn(neln);
        
        // get the element stiffness matrix
        int ndof = 3*neln;
        ke.resize(ndof, ndof);
        
        // calculate pressure stiffness
        ElementStiffness(el, ke);
        
        // get the element's LM vector
        UnpackLM(el, lm);
        
        // assemble element matrix in global stiffness matrix
        psolver->AssembleStiffness(el.m_node, lm, ke);
    }
}

//-----------------------------------------------------------------------------
void FETangentialFlowStabilization::Residual(const FETimeInfo& tp, FEGlobalVector& R)
{
    vector<double> fe;
    vector<int> lm;
    
    FESurface& surf = GetSurface();
    int npr = surf.Elements();
    for (int i=0; i<npr; ++i)
    {
        FESurfaceElement& el = m_psurf->Element(i);
        
        // calculate nodal normal tractions
        int neln = el.Nodes();
        vector<double> tn(neln);
        
        int ndof = 3*neln;
        fe.resize(ndof);
        
        ElementForce(el, fe);
        
        // get the element's LM vector
        UnpackLM(el, lm);
        
        // add element force vector to global force vector
        R.Assemble(el.m_node, lm, fe);
    }
}