//! This class implements the Pardiso solver.

//! The Pardiso solver is included in the Intel Math Kernel Library (MKL).
//! It can also be installed as a shared object library from
//!		http://www.pardiso-project.org

#pragma once

#include <FECore/LinearSolver.h>
#include <FECore/CompactUnSymmMatrix.h>
#include <FECore/CompactSymmMatrix.h>

class PardisoSolver : public LinearSolver
{
public:
	PardisoSolver(FEModel* fem);
	bool PreProcess() override;
	bool Factor() override;
	bool BackSolve(double* x, double* y) override;
	void Destroy() override;

	SparseMatrix* CreateSparseMatrix(Matrix_Type ntype) override;
	bool SetSparseMatrix(SparseMatrix* pA) override;

	void PrintConditionNumber(bool b);

	double condition_number();

protected:

	CompactMatrix*	m_pA;
	int				m_mtype; // matrix type

	// Pardiso control parameters
	int m_iparm[64];
	int m_maxfct, m_mnum, m_msglvl;
	double m_dparm[64];

	// Matrix data
	int m_n, m_nnz, m_nrhs;

	bool	m_print_cn;	// estimate and print the condition number

	void* m_pt[64]; // Internal solver memory pointer
};
