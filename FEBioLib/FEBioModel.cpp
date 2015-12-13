#include "stdafx.h"
#include "FEBioModel.h"
#include "FEBioPlot/FEBioPlotFile.h"
#include "FEBioXML/FEBioImport.h"
#include "FEBioXML/FERestartImport.h"
#include "FECore/NodeDataRecord.h"
#include "FECore/ElementDataRecord.h"
#include "FECore/ObjectDataRecord.h"
#include "FECore/FERigidBody.h"
#include "FECore/log.h"
#include "FECore/FECoreKernel.h"
#include "FECore/DOFS.h"
#include "version.h"

//-----------------------------------------------------------------------------
// echo the input data to the log file
extern void echo_input(FEBioModel& fem);

//-----------------------------------------------------------------------------
// Constructor of FEBioModel class.
FEBioModel::FEBioModel()
{
	// --- I/O-Data ---
	m_szfile_title = 0;
	m_szfile[0] = 0;
	m_szplot[0] = 0;
	m_szlog[0] = 0;
	m_szdump[0] = 0;
	m_debug = false;
	m_becho = true;
	m_plot = 0;
}

//-----------------------------------------------------------------------------
FEBioModel::~FEBioModel()
{
	// close the plot file
	if (m_plot) { delete m_plot; m_plot = 0; }
}

//-----------------------------------------------------------------------------
Timer& FEBioModel::GetSolveTimer()
{
	return m_SolveTime;
}

//=============================================================================
//
//		FEBioModel: I-O Functions
//
//=============================================================================

//-----------------------------------------------------------------------------
//! Return the data store
DataStore& FEBioModel::GetDataStore()
{
	return m_Data;
}

//-----------------------------------------------------------------------------
//! Add a data record to the data store
void FEBioModel::AddDataRecord(DataRecord* pd)
{
	m_Data.AddRecord(pd); 
}

//-----------------------------------------------------------------------------
//! Get the plot file
PlotFile* FEBioModel::GetPlotFile()
{
	return m_plot;
}

//-----------------------------------------------------------------------------
//! Sets the name of the FEBio input file
void FEBioModel::SetInputFilename(const char* szfile)
{ 
	strcpy(m_szfile, szfile); 
	m_szfile_title = strrchr(m_szfile, '/');
	if (m_szfile_title == 0) 
	{
		m_szfile_title = strrchr(m_szfile, '\\'); 
		if (m_szfile_title == 0) m_szfile_title = m_szfile; else ++m_szfile_title;
	}
	else ++m_szfile_title;
}

//-----------------------------------------------------------------------------
//! Set the name of the log file
void FEBioModel::SetLogFilename(const char* szfile) 
{ 
	strcpy(m_szlog , szfile); 
}

//-----------------------------------------------------------------------------
//! Set the name of the plot file
void FEBioModel::SetPlotFilename(const char* szfile) 
{ 
	strcpy(m_szplot, szfile); 
}

//-----------------------------------------------------------------------------
//! Set the name of the restart archive (i.e. the dump file)
void FEBioModel::SetDumpFilename (const char* szfile) 
{ 
	strcpy(m_szdump, szfile); 
}

//-----------------------------------------------------------------------------
//! Return the name of the input file
const char* FEBioModel::GetInputFileName()
{ 
	return m_szfile; 
}

//-----------------------------------------------------------------------------
//! Return the name of the log file
const char* FEBioModel::GetLogfileName()
{ 
	return m_szlog;  
}

//-----------------------------------------------------------------------------
//! Return the name of the plot file
const char* FEBioModel::GetPlotFileName()
{ 
	return m_szplot; 
}

//-----------------------------------------------------------------------------
//! Return the dump file name.
const char* FEBioModel::GetDumpFileName()
{
	return	m_szdump;
}

//-----------------------------------------------------------------------------
//! get the file title (i.e. name of input file without the path)
//! \todo Do I actually need to store this?
const char* FEBioModel::GetFileTitle()
{ 
	return m_szfile_title; 
}

//=============================================================================
//    I N P U T
//=============================================================================

//-----------------------------------------------------------------------------
//! This routine reads in an input file and performs some initialization stuff.
//! The rest of the initialization is done in Init

bool FEBioModel::Input(const char* szfile)
{
	// start the timer
	TimerTracker t(m_InputTime);

	// create file reader
	FEBioImport fim;

	// Load the file
	if (fim.Load(*this, szfile) == false)
	{
		char szerr[256];
		fim.GetErrorMessage(szerr);
		fprintf(stderr, szerr);

		return false;
	}

	// set the input file name
	SetInputFilename(szfile);

	// see if user redefined output filenames
	if (fim.m_szdmp[0]) SetDumpFilename(fim.m_szdmp);
	if (fim.m_szlog[0]) SetLogFilename (fim.m_szlog);
	if (fim.m_szplt[0]) SetPlotFilename(fim.m_szplt);

	// set the plot file
	FEBioPlotFile* pplt = new FEBioPlotFile(*this);
	m_plot = pplt;

	// set compression
	pplt->SetCompression(fim.m_nplot_compression);

	// define the plot file variables
	FEMesh& mesh = GetMesh();
	int NP = (int) fim.m_plot.size();
	for (int i=0; i<NP; ++i)
	{
		FEBioImport::PlotVariable& var = fim.m_plot[i];

		vector<int> item = var.m_item;
		if (item.empty() == false)
		{
			// TODO: currently, this is only supported for domain variables, where
			//       the list is a list of materials
			vector<int> lmat = var.m_item;

			// convert the material list to a domain list
			mesh.DomainListFromMaterial(lmat, item);
		}

		// add the plot output variable
		if (pplt->AddVariable(var.m_szvar, item, var.m_szdom) == false)
		{
			felog.printf("FATAL ERROR: Output variable \"%s\" is not defined\n", var.m_szvar);
			return false;
		}
	}

	// add the data records
	int ND = fim.m_data.size();
	for (int i=0; i<ND; ++i) AddDataRecord(fim.m_data[i]);

	// we're done reading
	return true;
}

//=============================================================================
//    O U T P U T
//=============================================================================

//-----------------------------------------------------------------------------
//! Export state to plot file.
void FEBioModel::Write(FE_OUTPUT_HINT hint)
{
	TimerTracker t(m_IOTimer);

	// get the current step
	FEAnalysis* pstep = GetCurrentStep();

	if (m_plot)
	{
		// get the plot level
		int nplt = pstep->GetPlotLevel();

		// if we don't want to plot anything we return
		if (nplt != FE_PLOT_NEVER)
		{
			// try to open the plot file
			if (hint == FE_STEP_INITIALIZED)
			{
				if (m_plot->IsValid() == false)
				{
					if (m_plot->Open(*this, m_szplot) == false)
					{
						felog.printf("ERROR : Failed creating PLOT database\n");
						delete m_plot;
						m_plot = 0;
					}

					// Since it is assumed that for the first timestep
					// there are no loads or initial displacements, the case n=0 is skipped.
					// Therefor we can output those results here.
					// TODO: Offcourse we should actually check if this is indeed
					//       the case, otherwise we should also solve for t=0
					m_plot->Write(*this);
				}
			}
			else
			{
				// assume we won't be writing anything
				bool bout = false;

				// see if we need to output something
				bool bdebug = GetDebugFlag();

				// when debugging we always output
				// (this coule mean we may end up writing the same state multiple times)
				if (bdebug) bout = true;
				else
				{
					switch (hint)
					{
					case FE_UNKNOWN    : bout = true; break;
					case FE_UNCONVERGED: if (nplt == FE_PLOT_MINOR_ITRS   ) bout = true; break;
					case FE_CONVERGED  : 
						if ((nplt == FE_PLOT_MAJOR_ITRS ) && (pstep->m_ntimesteps % pstep->m_nplot_stride == 0)) bout = true; 
						if ((nplt == FE_PLOT_MUST_POINTS) && (pstep->m_nmust >= 0)) bout = true;
						break;
					case FE_AUGMENT    : if (nplt == FE_PLOT_AUGMENTATIONS) bout = true; break;
					case FE_STEP_SOLVED: if (nplt == FE_PLOT_FINAL) bout = true;
					}
				}

				// output the state if requested
				if (bout) 
				{
					m_plot->Write(*this);
				}
			}
		}
	}

	// Dump converged state to the archive
	int ndump = pstep->GetDumpLevel();
	if (ndump != FE_DUMP_NEVER)
	{
		bool bdump = false;
		if ((hint == FE_STEP_SOLVED) && (ndump == FE_DUMP_STEP      )) bdump = true;
		if ((hint == FE_CONVERGED  ) && (ndump == FE_DUMP_MAJOR_ITRS)) bdump = true;
		if (bdump) DumpData();
	}

	// write the output data
	int nout = pstep->GetOutputLevel();
	if (nout != FE_OUTPUT_NEVER)
	{
		bool bout = false;
		switch (hint)
		{
		case FE_UNCONVERGED: if (nout == FE_OUTPUT_MINOR_ITRS) bout = true; break;
		case FE_CONVERGED:
			if (nout == FE_OUTPUT_MAJOR_ITRS) bout = true;
			if ((nout == FE_OUTPUT_MUST_POINTS) && (pstep->m_nmust >= 0)) bout = true;
			break;
		case FE_STEP_SOLVED:
			if (nout == FE_OUTPUT_FINAL) bout = true;
			break;
		}

		if (bout) WriteData();
	}
}

//-----------------------------------------------------------------------------
//! Write user data to the logfile
void FEBioModel::WriteData()
{
	m_Data.Write();
}

//-----------------------------------------------------------------------------
//! Dump state to archive for restarts
void FEBioModel::DumpData()
{
	DumpFile ar(this);
	if (ar.Create(m_szdump) == false)
	{
		felog.printf("WARNING: Failed creating restart file (%s).\n", m_szdump);
	}
	else 
	{
		Serialize(ar);
		felog.printf("\nRestart point created. Archive name is %s\n", m_szdump);
	}
}

//=============================================================================
//    R E S T A R T
//=============================================================================

//-----------------------------------------------------------------------------
//!  Reads or writes the current state to/from a binary file
//!  This is used to restart the solution from a saved position
//!  or to create a restart point.
//!  A version number is written to file to make sure the same
//!  format is used for reading and writing.
//! \param[in] ar the archive to which the data is serialized
//! \sa DumpFile
bool FEBioModel::Serialize(DumpFile &ar)
{
	if (ar.IsSaving())
	{
		// --- version number ---
		ar << (int) RSTRTVERSION;
	}
	else
	{
		// --- version ---
		int nversion;
		ar >> nversion;

		// make sure it is the right version
		if (nversion != RSTRTVERSION) return false;
	}

	// --- Load Data ---
	SerializeLoadData(ar);

	// --- Global Data ---
	SerializeGlobals(ar);

	// --- Material Data ---
	SerializeMaterials(ar);

	// --- Geometry Data ---
	SerializeGeometry(ar);

	// --- Contact Data ---
	SerializeContactData(ar);

	// --- Boundary Condition Data ---
	SerializeBoundaryData(ar);

	// --- Analysis data ---
	SerializeAnalysisData(ar);

	// --- Save IO Data
	SerializeIOData(ar);

	return true;
}


//-----------------------------------------------------------------------------
//! Serialize load curves
void FEBioModel::SerializeLoadData(DumpFile& ar)
{
	if (ar.IsSaving())
	{
		// save curve data
		ar << LoadCurves();
		for (int i=0; i<LoadCurves(); ++i) GetLoadCurve(i)->Serialize(ar);
	}
	else
	{
		// loadcurve data
		int nlc;
		ar >> nlc;
		m_LC.clear();
		for (int i=0; i<nlc; ++i)
		{
			FELoadCurve* plc = new FELoadCurve();
			plc->Serialize(ar);
			AddLoadCurve(plc);
		}
	}
}

//-----------------------------------------------------------------------------
//! Serialize global data
void FEBioModel::SerializeGlobals(DumpFile& ar)
{
	if (ar.IsSaving())
	{
		int NC = (int) m_Const.size();
		ar << NC;
		if (NC > 0)
		{
			char sz[256] = {0};
			map<string, double>::iterator it;
			for (it = m_Const.begin(); it != m_Const.end(); ++it)
			{
				strcpy(sz, it->first.c_str());
				ar << sz;
				ar << it->second;
			}
		}
		int nGD = GlobalDataItems();
		ar << nGD;
		for (int i=0; i<nGD; i++) 
		{
			FEGlobalData* pgd = GetGlobalData(i);
			ar << pgd->GetTypeStr();
			pgd->Serialize(ar);
		}
	}
	else
	{
		char sz[256] = {0};
		double v;
		int NC;
		ar >> NC;
		m_Const.clear();
		for (int i=0; i<NC; ++i)
		{
			ar >> sz >> v;
			SetGlobalConstant(string(sz), v);
		}
		int nGD;
		ar >> nGD;
		if (nGD) 
		{
			char sztype[256];
			for (int i=0; i<nGD; ++i)
			{
				ar >> sztype;
				FEGlobalData* pgd = fecore_new<FEGlobalData>(FEGLOBALDATA_ID, sztype, this);
				pgd->Serialize(ar);
				FEModel::AddGlobalData(pgd);
			}
		}
	}
}

//-----------------------------------------------------------------------------
//! Serialize analysis data
void FEBioModel::SerializeAnalysisData(DumpFile &ar)
{
	if (ar.IsSaving())
	{
		// analysis steps
		ar << (int) m_Step.size();
		for (int i=0; i<(int) m_Step.size(); ++i) 
		{
			m_Step[i]->Serialize(ar);
		}

		ar << m_nStep;
		ar << m_ftime << m_ftime0;
		ar << m_nplane_strain;

		// direct solver data
		ar << m_nsolver;
		ar << m_bwopt;

		// body loads
		ar << (int) m_BL.size();
		for (int i=0; i<(int) m_BL.size(); ++i)
		{
			FEBodyLoad* pbl = m_BL[i];
			ar << pbl->GetTypeStr();
			pbl->Serialize(ar);
		}
	}
	else
	{
		m_Step.clear();
		FEModel* pfem = ar.GetFEModel();

		char sztype[256] = {0};

		// analysis steps
		int nsteps;
		ar >> nsteps;
		for (int i=0; i<nsteps; ++i)
		{
			FEAnalysis* pstep = new FEAnalysis(this); assert(pstep);
			pstep->Serialize(ar);
			m_Step.push_back(pstep);
		}
		ar >> m_nStep;
		ar >> m_ftime >> m_ftime0;
		ar >> m_nplane_strain;

		// direct solver data
		ar >> m_nsolver;
		ar >> m_bwopt;

		// body loads
		int nbl;
		ar >> nbl;
		m_BL.clear();
		char szbl[256] = {0};
		for (int i=0; i<nbl; ++i)
		{
			ar >> szbl;
			FEBodyLoad* pbl = fecore_new<FEBodyLoad>(FEBODYLOAD_ID, szbl, this);
			assert(pbl);

			pbl->Serialize(ar);
			m_BL.push_back(pbl);
		}

		// set the correct step
		m_pStep = m_Step[m_nStep];
	}
}

//-----------------------------------------------------------------------------
//! serialize material data
void FEBioModel::SerializeMaterials(DumpFile& ar)
{
	FECoreKernel& febio = FECoreKernel::GetInstance();

	if (ar.IsSaving())
	{
		// store the nr of materials
		ar << Materials();

		// store the materials
		for (int i=0; i<Materials(); ++i)
		{
			FEMaterial* pmat = GetMaterial(i);

			// store the type string
			ar << pmat->GetTypeStr();

			// store the name
			ar << pmat->GetName();

			// store material parameters
			pmat->Serialize(ar);
		}
	}
	else
	{
		// read the number of materials
		int nmat;
		ar >> nmat;

		// read the material data
		char szmat[256] = {0}, szvar[256] = {0};
		for (int i=0; i<nmat; ++i)
		{
			// read the type string
			ar >> szmat;

			// create a material
			FEMaterial* pmat = fecore_new<FEMaterial>(FEMATERIAL_ID, szmat, this);
			assert(pmat);

			// read the name
			ar >> szmat;
			pmat->SetName(szmat);

			// read all parameters
			pmat->Serialize(ar);

			// Add material and parameter list to FEM
			AddMaterial(pmat);

			// initialize the rigid bodies
			if (m_prs) m_prs->Init();

			// call init in case this function initializes other data
			pmat->Init();
		}
	}
}

//-----------------------------------------------------------------------------
//! \todo Serialize nonlinear constraints
void FEBioModel::SerializeGeometry(DumpFile &ar)
{
	// serialize the mesh first 
	SerializeMesh(ar);
	FERigidSystem& rigid = *GetRigidSystem();

	// serialize the other geometry data
	if (ar.IsSaving())
	{
		// FE objects
		int nrb = rigid.Objects();
		ar << nrb;
		for (int i=0; i<nrb; ++i) rigid.Object(i)->Serialize(ar);
	}
	else
	{
		// rigid bodies
		int nrb = 0;
		ar >> nrb;
		rigid.Clear();
		for (int i=0; i<nrb; ++i)
		{
			FERigidBody* prb = new FERigidBody(this);
			prb->Serialize(ar);
			rigid.AddRigidBody(prb);
		}
	}
}

//-----------------------------------------------------------------------------
//! This function is used by the restart feature and reads or writes
//! the mesh data to or from the binary archive
//! \param[in] ar the archive to which the data is serialized
//! \sa FEM::Serialize()
//! \sa DumpFile
//! \todo serialize nodesets

void FEBioModel::SerializeMesh(DumpFile& ar)
{
    DOFS& fedofs = GetDOFS();
	FEMesh& m = m_mesh;

	if (ar.IsSaving())
	{
		int i;

		// write DOFS
		ar << fedofs.GetNDOFS() << fedofs.GetCDOFS();
		
		// write nodal data
		int nn = m.Nodes();
		ar << nn;
		for (i=0; i<nn; ++i)
		{
			FENode& node = m.Node(i);
			ar << node.m_ap;
			ar << node.m_at;
			ar << node.m_bshell;
			ar << node.m_bexclude;
			ar << node.m_Fr;
			ar << node.m_ID;
			ar << node.m_r0;
			ar << node.m_rid;
			ar << node.m_rp;
			ar << node.m_rt;
			ar << node.m_vp;
			ar << node.m_val;
		}

		// write domain data
		int ND = m.Domains();
		ar << ND;
		for (i=0; i<ND; ++i)
		{
			FEDomain& d = m.Domain(i);

			ar << d.GetMaterial()->GetID();
			ar << d.GetTypeStr() << d.Elements();
			d.Serialize(ar);
		}
	}
	else
	{
		FECoreKernel& febio = FECoreKernel::GetInstance();

		// read DOFS
		int MAX_NDOFS, MAX_CDOFS;
		ar >> MAX_NDOFS >> MAX_CDOFS;
		fedofs.SetNDOFS(MAX_NDOFS);
		fedofs.SetCDOFS(MAX_CDOFS);
		
		// read nodal data
		int nn;
		ar >> nn;
		m.CreateNodes(nn);
		for (int i=0; i<nn; ++i)
		{
			FENode& node = m.Node(i);
			ar >> node.m_ap;
			ar >> node.m_at;
			ar >> node.m_bshell;
			ar >> node.m_bexclude;
			ar >> node.m_Fr;
			ar >> node.m_ID;
			ar >> node.m_r0;
			ar >> node.m_rid;
			ar >> node.m_rp;
			ar >> node.m_rt;
			ar >> node.m_vp;
			ar >> node.m_val;
		}

		// read domain data
		int ND, ne;
		ar >> ND;
		char sz[256] = {0};
		for (int i=0; i<ND; ++i)
		{
			int nmat;
			ar >> nmat;
			FEMaterial* pm = FindMaterial(nmat);
			assert(pm);

			ar >> sz >> ne;
			FEDomain* pd = fecore_new<FEDomain>(FEDOMAIN_ID, sz, this);
			assert(pd);
			pd->SetMaterial(pm);
			pd->create(ne);
			pd->Serialize(ar);

			m.AddDomain(pd);
		}

		m.UpdateBox();
	}
}

//-----------------------------------------------------------------------------
//! serialize contact data
void FEBioModel::SerializeContactData(DumpFile &ar)
{
	FECoreKernel& febio = FECoreKernel::GetInstance();

	if (ar.IsSaving())
	{
		ar << SurfacePairInteractions();
		for (int i=0; i<SurfacePairInteractions(); ++i)
		{
			FESurfacePairInteraction* pci = SurfacePairInteraction(i);

			// store the type string
			ar << pci->GetTypeStr();

			pci->Serialize(ar);
		}
	}
	else
	{
		int numci;
		ar >> numci;

		char szci[256] = {0};
		for (int i=0; i<numci; ++i)
		{
			// get the interface type
			ar >> szci;

			// create a new interface
			FESurfacePairInteraction* pci = fecore_new<FESurfacePairInteraction>(FESURFACEPAIRINTERACTION_ID, szci, this);

			// serialize interface data from archive
			pci->Serialize(ar);

			// add interface to list
			AddSurfacePairInteraction(pci);

			// add surfaces to mesh
			FEMesh& m = m_mesh;
			if (pci->GetMasterSurface()) m.AddSurface(pci->GetMasterSurface());
			m.AddSurface(pci->GetSlaveSurface());
		}	
	}
}

//-----------------------------------------------------------------------------
//! \todo Do we need to store the m_bActive flag of the boundary conditions?
void FEBioModel::SerializeBoundaryData(DumpFile& ar)
{
	FECoreKernel& febio = FECoreKernel::GetInstance();

	if (ar.IsSaving())
	{
		// fixed bc's
		ar << (int) m_BC.size();
		for (int i=0; i<(int) m_BC.size(); ++i) 
		{
			FEFixedBC& bc = *m_BC[i];
			bc.Serialize(ar);
		}

		// displacements
		ar << (int) m_DC.size();
		for (int i=0; i<(int) m_DC.size(); ++i) 
		{
			FEPrescribedBC& dc = *m_DC[i];
			dc.Serialize(ar);
		}

		// initial conditions
		ar << (int) m_IC.size();
		for (int i=0; i<(int) m_IC.size(); ++i) 
		{
			FEInitialCondition& ic = *m_IC[i];
			ar << ic.GetTypeStr();
			ic.Serialize(ar);
		}

		// nodal loads
		ar << (int) m_FC.size();
		for (int i=0; i<(int) m_FC.size(); ++i)
		{
			FENodalLoad& fc = *m_FC[i];
			fc.Serialize(ar);
		}

		// surface loads
		ar << (int) m_SL.size();
		for (int i=0; i<(int) m_SL.size(); ++i)
		{
			FESurfaceLoad* psl = m_SL[i];

			// get the surface
			FESurface& s = psl->Surface();
			s.Serialize(ar);

			// save the load data
			ar << psl->GetTypeStr();
			psl->Serialize(ar);
		}

		// fixed rigid body dofs
		ar << (int) m_RBC.size();
		for (int i=0; i<(int) m_RBC.size(); ++i)
		{
			FERigidBodyFixedBC& bc = *m_RBC[i];
			bc.Serialize(ar);
		}

		// rigid body displacements
		ar << (int) m_RDC.size();
		for (int i=0; i<(int) m_RDC.size(); ++i)
		{
			FERigidBodyDisplacement& dc = *m_RDC[i];
			dc.Serialize(ar);
		}

		// model loads
		ar << (int) m_ML.size();
		for (int i=0; i<(int) m_ML.size(); ++i)
		{
			FEModelLoad& ml = *m_ML[i];
			ar << ml.GetTypeStr();
			ml.Serialize(ar);
		}

		// rigid nodes
		ar << (int) m_RN.size();
		for (int i=0; i<(int) m_RN.size(); ++i)
		{
			FERigidNode& rn = *m_RN[i];
			rn.Serialize(ar);
		}

		// linear constraints
		ar << (int) m_LinC.size();
		list<FELinearConstraint>::iterator it = m_LinC.begin();
		for (int i=0; i<(int) m_LinC.size(); ++i, ++it) it->Serialize(ar);

		ar << m_LCT;

		// aug lag linear constraints
/*		n = (int) m_LCSet.size();
		ar << n;
		if (m_LCSet.empty() == false)
		{
			for (i=0; i<n; ++i) m_LCSet[i]->Serialize(ar);
		}
*/
		int n = m_NLC.size();
		ar << n;
		if (n) 
		{
			for (int i=0; i<n; ++i) 
			{
//				ar << m_NLC[i]->Type();
//				m_NLC[i]->Serialize(ar);
			}
		}
	}
	else
	{
		int n;
		char sz[256] = {0};

		// fixed bc's
		// NOTE: I think this may create a memory leak if m_BC is not empty
		ar >> n;
		m_BC.clear();
		for (int i=0; i<n; ++i) 
		{
			FEFixedBC* pbc = new FEFixedBC(this);
			pbc->Serialize(ar);
			if (pbc->IsActive()) pbc->Activate(); else pbc->Deactivate();
			m_BC.push_back(pbc);
		}

		// displacements
		ar >> n;
		m_DC.clear();
		for (int i=0; i<n; ++i) 
		{
			FEPrescribedBC* pdc = new FEPrescribedBC(this);
			pdc->Serialize(ar);
			if (pdc->IsActive()) pdc->Activate(); else pdc->Deactivate();
			m_DC.push_back(pdc);
		}

		// initial conditions
		ar >> n;
		m_IC.clear();
		for (int i=0; i<n; ++i) 
		{
			ar >> sz;
			FEInitialCondition* pic = fecore_new<FEInitialCondition>(FEIC_ID, sz, this);
			assert(pic);
			pic->Serialize(ar);
			if (pic->IsActive()) pic->Activate(); else pic->Deactivate();
			m_IC.push_back(pic);
		}

		// nodal loads
		ar >> n;
		m_FC.clear();
		for (int i=0; i<n; ++i)
		{
			FENodalLoad* pfc = new FENodalLoad(this);
			pfc->Serialize(ar);
			if (pfc->IsActive()) pfc->Activate(); else pfc->Deactivate();
			m_FC.push_back(pfc);
		}

		// surface loads
		ar >> n;
		m_SL.clear();
		for (int i=0; i<n; ++i)
		{
			// create a new surface
			FESurface* psurf = new FESurface(&m_mesh);
			psurf->Serialize(ar);

			// read load data
			char sztype[256] = {0};
			ar >> sztype;
			FESurfaceLoad* ps = fecore_new<FESurfaceLoad>(FESURFACELOAD_ID, sztype, this);
			assert(ps);
			ps->SetSurface(psurf);

			ps->Serialize(ar);
			if (ps->IsActive()) ps->Activate(); else ps->Deactivate();

			m_SL.push_back(ps);
			m_mesh.AddSurface(psurf);
		}

		// fixed rigid body dofs
		ar >> n;
		m_RBC.clear();
		for (int i=0; i<n; ++i)
		{
			FERigidBodyFixedBC* pbc = new FERigidBodyFixedBC(this);
			pbc->Serialize(ar);
			if (pbc->IsActive()) pbc->Activate(); else pbc->Deactivate();
			m_RBC.push_back(pbc);
		}

		// rigid body displacements
		ar >> n;
		m_RDC.clear();
		for (int i=0; i<n; ++i)
		{
			FERigidBodyDisplacement* pdc = new FERigidBodyDisplacement(this);
			pdc->Serialize(ar);
			if (pdc->IsActive()) pdc->Activate(); else pdc->Deactivate();
			m_RDC.push_back(pdc);
		}

		// model loads
		ar >> n;
		m_ML.clear();
		for (int i=0; i<n; ++i)
		{
			// read load data
			char sztype[256] = {0};
			ar >> sztype;
			FEModelLoad* pml = fecore_new<FEModelLoad>(FEBC_ID, sztype, this);
			assert(pml);

			pml->Serialize(ar);
			if (pml->IsActive()) pml->Activate(); else pml->Deactivate();
			m_ML.push_back(pml);
		}

		// rigid nodes
		ar >> n;
		m_RN.clear();
		for (int i=0; i<n; ++i)
		{
			FERigidNode* prn = new FERigidNode(this);
			prn->Serialize(ar);
			if (prn->IsActive()) prn->Activate(); else prn->Deactivate();
			m_RN.push_back(prn);
		}

		// linear constraints
		ar >> n;
		FELinearConstraint LC(this);
		for (int i=0; i<n; ++i)
		{
			LC.Serialize(ar);
			m_LinC.push_back(LC);
		}

		ar >> m_LCT;

		// reset the pointer table
		int nlin = m_LinC.size();
		m_LCA.resize(nlin);
		list<FELinearConstraint>::iterator ic = m_LinC.begin();
		for (int i=0; i<nlin; ++i, ++ic) m_LCA[i] = &(*ic);

		// aug lag linear constraints
		ar >> n;
//		int ntype;
		m_NLC.clear();
		for (int i=0; i<n; ++i)
		{
/*			ar >> ntype;
			FENLConstraint* plc = 0;
			switch (ntype)
			{
			case FE_POINT_CONSTRAINT : plc = new FEPointConstraint    (this); break;
			case FE_LINEAR_CONSTRAINT: plc = new FELinearConstraintSet(this); break;
			default:
				assert(false);
			}
			assert(plc);
			plc->Serialize(ar);
			m_NLC.push_back(plc);
*/		}
	}
}

//-----------------------------------------------------------------------------
//! Serialization of FEBioModel data
void FEBioModel::SerializeIOData(DumpFile &ar)
{
	if (ar.IsSaving())
	{
		// file names
		ar << m_szfile << m_szplot << m_szlog << m_szdump;
		ar << m_sztitle;

		// plot file
		int npltfmt = 2;
		ar << npltfmt;

		// data records
		SerializeDataStore(ar);
	}
	else
	{
		// file names
		ar >> m_szfile >> m_szplot >> m_szlog >> m_szdump;
		ar >> m_sztitle;

		// don't forget to call store the input file name so
		// that m_szfile_title gets initialized
		SetInputFilename(m_szfile);

		// get the plot file format (should be 2)
		int npltfmt = 0;
		ar >> npltfmt;
		assert(m_plot == 0);
		assert(npltfmt == 2);

		// create the plot file and open it for appending
		m_plot = new FEBioPlotFile(*this);
		if (m_plot->Append(*this, m_szplot) == false)
		{
			printf("FATAL ERROR: Failed reopening plot database %s\n", m_szplot);
			throw "FATAL ERROR";
		}

		// data records
		SerializeDataStore(ar);
	}
}

//-----------------------------------------------------------------------------
void FEBioModel::SerializeDataStore(DumpFile& ar)
{
	if (ar.IsSaving())
	{
		int N = m_Data.Size();
		ar << N;
		for (int i=0; i<N; ++i)
		{
			DataRecord* pd = m_Data.GetDataRecord(i);

			int ntype = -1;
			if (dynamic_cast<NodeDataRecord*>(pd)) ntype = FE_DATA_NODE;
			if (dynamic_cast<ElementDataRecord*>(pd)) ntype = FE_DATA_ELEM;
			if (dynamic_cast<ObjectDataRecord*>(pd)) ntype = FE_DATA_RB;
			assert(ntype != -1);
			ar << ntype;
			pd->Serialize(ar);
		}
	}
	else
	{
		int N;
		m_Data.Clear();
		ar >> N;
		for (int i=0; i<N; ++i)
		{
			int ntype;
			ar >> ntype;

			DataRecord* pd = 0;
			switch(ntype)
			{
			case FE_DATA_NODE: pd = new NodeDataRecord(this, 0); break;
			case FE_DATA_ELEM: pd = new ElementDataRecord(this, 0); break;
			case FE_DATA_RB  : pd = new ObjectDataRecord(this, 0); break;
			}
			assert(pd);
			pd->Serialize(ar);
			m_Data.AddRecord(pd);
		}
	}
}

//=============================================================================
//    I N I T I A L I Z A T I O N
//=============================================================================

//-----------------------------------------------------------------------------
// Forward declarations
int Hello();

//-----------------------------------------------------------------------------
//! This function performs one-time-initialization stuff. All the different 
//! modules are initialized here as well. This routine also performs some
//! data checks

bool FEBioModel::Init()
{
	TimerTracker t(m_InitTime);

	// Open the logfile
	if (!felog.is_valid()) 
	{
		// see if a valid log file name is defined.
		const char* szlog = GetLogfileName();
		if (szlog[0] == 0)
		{
			// if not, we take the input file name and set the extension to .log
			char sz[1024] = {0};
			strcpy(sz, GetInputFileName());
			char *ch = strrchr(sz, '.');
			if (ch) *ch = 0;
			strcat(sz, ".log");
			SetLogFilename(sz);
		}
		
		if (felog.open(m_szlog) == false)
		{
			felog.printbox("FATAL ERROR", "Failed creating log file");
			return false;
		}

		// make sure we have a step
		if (m_pStep == 0)
		{
			felog.printf("FATAL ERROR: No step defined\n\n");
			return false;
		}

		// if we don't want to output anything we only output to the logfile
		if (m_pStep->GetPrintLevel() == FE_PRINT_NEVER) felog.SetMode(Logfile::FILE_ONLY);

		// print welcome message to file
		Logfile::MODE m = felog.SetMode(Logfile::FILE_ONLY);
		Hello();
		felog.SetMode(m);
	}

	// open plot database file
	if (m_pStep->GetPlotLevel() != FE_PLOT_NEVER)
	{
		if (m_plot == 0) 
		{
			m_plot = new FEBioPlotFile(*this);
		}

		// see if a valid plot file name is defined.
		const char* szplt = GetPlotFileName();
		if (szplt[0] == 0)
		{
			// if not, we take the input file name and set the extension to .xplt
			char sz[1024] = {0};
			strcpy(sz, GetInputFileName());
			char *ch = strrchr(sz, '.');
			if (ch) *ch = 0;
			strcat(sz, ".xplt");
			SetPlotFilename(sz);
		}
	}

	// initialize model data
	if (FEModel::Init() == false) 
	{
		felog.printf("FATAL ERROR: Model initialization failed\n\n");
		return false;
	}

	// see if a valid dump file name is defined.
	const char* szdmp = this->GetDumpFileName();
	if (szdmp[0] == 0)
	{
		// if not, we take the input file name and set the extension to .dmp
		char sz[1024] = {0};
		strcpy(sz, GetInputFileName());
		char *ch = strrchr(sz, '.');
		if (ch) *ch = 0;
		strcat(sz, ".dmp");
		SetDumpFilename(sz);
	}

	// Alright, all initialization is done, so let's get busy !
	return true;
}

//-----------------------------------------------------------------------------
//! This function resets the FEM data so that a new run can be done.
//! This routine is called from the optimization routine.

bool FEBioModel::Reset()
{
	// Reset model data
	FEModel::Reset();
/*
	// open plot database file
	if (m_pStep->GetPlotLevel() != FE_PLOT_NEVER)
	{
		if (m_plot == 0) 
		{
			m_plot = new FEBioPlotFile(*this);
		}

		if (m_plot->Open(*this, m_szplot) == false)
		{
			felog.printf("ERROR : Failed creating PLOT database\n");
			return false;
		}
	}

	// Since it is assumed that for the first timestep
	// there are no loads or initial displacements, the case n=0 is skipped.
	// Therefor we can output those results here.
	// Offcourse we should actually check if this is indeed
	// the case, otherwise we should also solve for t=0
	if (m_pStep->GetPlotLevel() != FE_PLOT_NEVER) m_plot->Write(*this);
*/
/*
	// reset the log file
	if (!log.is_valid())
	{
		log.open(m_szlog);

		// if we don't want to output anything we only output to the logfile
		if (m_pStep->GetPrintLevel() == FE_PRINT_NEVER) log.SetMode(Logfile::FILE_ONLY);

		// print welcome message to file
		Hello();
	}
*/
	// do the callback
	DoCallback(CB_INIT);

	// All data is reset successfully
	return true;
}

//=============================================================================
//                               S O L V E
//=============================================================================

//-----------------------------------------------------------------------------
//! This is the main solve method. This function loops over all analysis steps
//! and solves each one in turn. 
//! \sa FEAnalysis

bool FEBioModel::Solve()
{
	// echo fem data to the logfile
	// we do this here (and not e.g. directly after input)
	// since the data can be changed after input, which is the case,
	// for instance, in the parameter optimization module
	if (m_becho) echo_input(*this);

	// start the total time tracker
	m_SolveTime.start();

	// solve the FE model
	bool bconv = FEModel::Solve();

	// stop total time tracker
	m_SolveTime.stop();

	// get and print elapsed time
	char sztime[64];
	Logfile::MODE old_mode = felog.SetMode(Logfile::SCREEN_ONLY);
	m_SolveTime.time_str(sztime);
	felog.printf("\n Elapsed time : %s\n\n", sztime);

	// print more detailed timing info to the log file
	felog.SetMode(Logfile::FILE_ONLY);

	// sum up all the times spend in the linear solvers
	double total_time = 0.0;
	double input_time   = m_InputTime.GetTime(); total_time += input_time;
	double init_time    = m_InitTime.GetTime (); total_time += init_time;
	double solve_time   = m_SolveTime.GetTime(); total_time += solve_time;
	double io_time      = m_IOTimer.GetTime  ();
	double total_linsol = 0.0;
	double total_reform = 0.0;
	double total_stiff  = 0.0;
	double total_rhs    = 0.0;
	double total_update = 0.0;
	int NS = Steps();
	for (int i = 0; i<NS; ++i)
	{
		FEAnalysis* pstep = GetStep(i);
		FESolver* psolve = pstep->GetFESolver();
		if (psolve) 
		{
			total_linsol += psolve->m_SolverTime.GetTime();
			total_reform += psolve->m_ReformTime.GetTime();
			total_stiff  += psolve->m_StiffnessTime.GetTime();
			total_rhs    += psolve->m_RHSTime.GetTime();
			total_update += psolve->m_UpdateTime.GetTime();
		}
	}


	felog.printf(" T I M I N G   I N F O R M A T I O N\n\n");
	Timer::time_str(input_time  , sztime); felog.printf("\tInput time ...................... : %s (%lg sec)\n\n", sztime, input_time  );
	Timer::time_str(init_time   , sztime); felog.printf("\tInitialization time ............. : %s (%lg sec)\n\n", sztime, init_time   );
	Timer::time_str(solve_time  , sztime); felog.printf("\tSolve time ...................... : %s (%lg sec)\n\n", sztime, solve_time  );
	Timer::time_str(io_time     , sztime); felog.printf("\t   IO-time (plot, dmp, data) .... : %s (%lg sec)\n\n", sztime, io_time     );
	Timer::time_str(total_reform, sztime); felog.printf("\t   reforming stiffness .......... : %s (%lg sec)\n\n", sztime, total_reform);
	Timer::time_str(total_stiff , sztime); felog.printf("\t   evaluating stiffness ......... : %s (%lg sec)\n\n", sztime, total_stiff );
	Timer::time_str(total_rhs   , sztime); felog.printf("\t   evaluating residual .......... : %s (%lg sec)\n\n", sztime, total_rhs   );
	Timer::time_str(total_update, sztime); felog.printf("\t   model update ................. : %s (%lg sec)\n\n", sztime, total_update);
	Timer::time_str(total_linsol, sztime); felog.printf("\t   time in linear solver ........ : %s (%lg sec)\n\n", sztime, total_linsol);
	Timer::time_str(total_time  , sztime); felog.printf("\tTotal elapsed time .............. : %s (%lg sec)\n\n", sztime, total_time  );


	felog.SetMode(old_mode);

	if (bconv)
	{
		felog.printf("\n N O R M A L   T E R M I N A T I O N\n\n");
	}
	else
	{
		felog.printf("\n E R R O R   T E R M I N A T I O N\n\n");
	}

	// flush the log file
	felog.flush();

	// We're done !
	return bconv;
}
