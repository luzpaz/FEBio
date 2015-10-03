// FECoordSysMap.h: interface for the FECoordSysMap class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FECOORDSYSMAP_H__5BEAB9FF_6AAE_4CCE_876C_2A2866A8165C__INCLUDED_)
#define AFX_FECOORDSYSMAP_H__5BEAB9FF_6AAE_4CCE_876C_2A2866A8165C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "mat3d.h"
#include "FEElement.h"
#include "DumpFile.h"
#include "FECoreBase.h"

class FEModel;
class FEMesh;

#define FE_MAP_NONE		0
#define FE_MAP_LOCAL	1
#define FE_MAP_SPHERE	2
#define FE_MAP_VECTOR	3
#define FE_MAP_CYLINDER	4
#define FE_MAP_ANGLES	5
#define FE_MAP_POLAR	6

//-----------------------------------------------------------------------------
//! The FECoordSysMap class is used to create local coordinate systems.

class FECoordSysMap : public FECoreBase
{
public:
	FECoordSysMap(int ntype) : FECoreBase(FECOORDSYSMAP_ID) { m_ntype = ntype; }
	virtual ~FECoordSysMap() {}

	//! initialization
	virtual void Init() {}

	//! return the local coordinate system at an element's gauss point
	virtual mat3d LocalElementCoord(FEElement& el, int n) = 0;

	//! serialization
	virtual void Serialize(DumpFile& ar) = 0;

public:
	int	m_ntype;
};

//-----------------------------------------------------------------------------

class FELocalMap : public FECoordSysMap
{
public:
	FELocalMap(FEModel* pfem);

	void Init();

	void SetLocalNodes(int n1, int n2, int n3);

	mat3d LocalElementCoord(FEElement& el, int n);

	virtual void Serialize(DumpFile& ar);

public:
	int			m_n[3];	// local element nodes

protected:
	FEMesh&		m_mesh;

	DECLARE_PARAMETER_LIST();
};

//-----------------------------------------------------------------------------

class FESphericalMap : public FECoordSysMap
{
public:
	FESphericalMap(FEModel* pfem);

	void Init();

	void SetSphereCenter(vec3d c) { m_c = c; }

	mat3d LocalElementCoord(FEElement& el, int n);

	virtual void Serialize(DumpFile& ar);

public:
	vec3d		m_c;	// center of map

protected:
	FEMesh&		m_mesh;

	DECLARE_PARAMETER_LIST();
};


//-----------------------------------------------------------------------------

class FECylindricalMap : public FECoordSysMap
{
public:
	FECylindricalMap(FEModel* pfem);

	void Init();

	void SetCylinderCenter(vec3d c) { m_c = c; }

	void SetCylinderAxis(vec3d a) { m_a = a; m_a.unit(); }

	void SetCylinderRef(vec3d r) { m_r = r; m_r.unit(); }

	mat3d LocalElementCoord(FEElement& el, int n);

	virtual void Serialize(DumpFile& ar);

public:
	vec3d		m_c;	// center of map
	vec3d		m_a;	// axis
	vec3d		m_r;	// reference direction

protected:
	FEMesh&		m_mesh;

	DECLARE_PARAMETER_LIST();
};

//-----------------------------------------------------------------------------

class FEPolarMap : public FECoordSysMap
{
public:
	FEPolarMap(FEModel* pfem);

	void Init();

	void SetCenter(vec3d c) { m_c = c; }

	void SetAxis(vec3d a) { m_a = a; m_a.unit(); }

	void SetVector0(vec3d r) { m_d0 = r; m_d0.unit(); }
	void SetVector1(vec3d r) { m_d1 = r; m_d1.unit(); }

	void SetRadius0(double r) { m_R0 = r; }
	void SetRadius1(double r) { m_R1 = r; }

	mat3d LocalElementCoord(FEElement& el, int n);

	virtual void Serialize(DumpFile& ar);

public:
	vec3d		m_c;		// center of map
	vec3d		m_a;		// axis
	vec3d		m_d0, m_d1;	// reference direction
	double		m_R0, m_R1;

protected:
	FEMesh&		m_mesh;

	DECLARE_PARAMETER_LIST();
};


//-----------------------------------------------------------------------------

class FEVectorMap : public FECoordSysMap
{
public:
	FEVectorMap(FEModel* pfem) : FECoordSysMap(FE_MAP_VECTOR) {}

	void Init();

	void SetVectors(vec3d a, vec3d d) { m_a = a; m_d = d; }

	mat3d LocalElementCoord(FEElement& el, int n);

	virtual void Serialize(DumpFile& ar);

public:
	vec3d	m_a, m_d;

	DECLARE_PARAMETER_LIST();
};

//-----------------------------------------------------------------------------
class FESphericalAngleMap : public FECoordSysMap
{
public:
	FESphericalAngleMap(FEModel* pfem) : FECoordSysMap(FE_MAP_ANGLES){}

	void Init();

	void SetAngles(double theta, double phi) { m_theta = theta; m_phi = phi; }

	mat3d LocalElementCoord(FEElement& el, int n);

	virtual void Serialize(DumpFile& ar);

public:
	double	m_theta;
	double	m_phi;

	DECLARE_PARAMETER_LIST();
};

#endif // !defined(AFX_FECOORDSYSMAP_H__5BEAB9FF_6AAE_4CCE_876C_2A2866A8165C__INCLUDED_)
