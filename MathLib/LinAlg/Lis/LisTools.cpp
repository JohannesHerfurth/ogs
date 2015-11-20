/**
 * \file
 * \author Norihiro Watanabe
 * \date   2013-04-16
 * \brief  Implementation of Lis utility functions.
 *
 * \copyright
 * Copyright (c) 2012-2015, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#include "LisTools.h"

#include <cassert>

#include "logog/include/logog.hpp"

#include "LisMatrix.h"
#include "LisVector.h"

#include "BaseLib/quicksort.h"
#include "MathLib/LinAlg/Sparse/CRSMatrix.h"
#include "MathLib/LinAlg/Sparse/CRSTools.h"

namespace MathLib
{

namespace detail
{
/// Converts the internal format of a lis matrix into the compressed row storage
/// format. Since the column entries of a row in the internal lis format aren't
/// sorted lis2crs not only transfers the indices and entries, it also
/// sorts the columns and values, accordingly.
MathLib::CRSMatrix<double, typename LisMatrix::IndexType>* lis2crs(LisMatrix &a)
{
	using IndexType = LisMatrix::IndexType;

	LIS_MATRIX &A = a.getRawMatrix();

	IndexType const n_rows(A->n); // number of rows
	IndexType *iA(new IndexType[n_rows+1]); // row ptr array
	iA[0] = 0;
	for (LIS_INT k=1; k<n_rows+1; ++k) {
		iA[k] = iA[k-1] + A->w_row[k-1 - A->is];
	}

	IndexType *jA(new IndexType[iA[n_rows]]); // column indices array
	double *entries(new double[iA[n_rows]]);
	for (IndexType r(0); r<n_rows; ++r) {
		IndexType const beg_idx(iA[r]);
		IndexType const end_idx(iA[r+1]);
		for (IndexType j(beg_idx); j<end_idx; ++j) {
			jA[j] = A->w_index[r-A->is][j-beg_idx];
			entries[j] = A->w_value[r-A->is][j-beg_idx];
		}
	}

	for (IndexType r(0); r<n_rows; ++r) {
		IndexType const beg_idx(iA[r]);
		IndexType const end_idx(iA[r+1]);
		// sort the column entries of the row
		BaseLib::quicksort(jA, beg_idx, end_idx, entries);
	}

	return new MathLib::CRSMatrix<double,IndexType>(A->n, iA, jA, entries);
}
} // end namespace detail

void applyKnownSolution(LisMatrix &eqsA, LisVector &eqsRHS,
	const std::vector<std::size_t> &input_rows,
	const std::vector<double> &input_vals)
{
	// unfortunatly the input is not sorted => copy and sort
	std::vector<std::size_t> rows(input_rows);
	std::vector<double> vals(input_vals);
	BaseLib::quicksort(rows, 0, rows.size(), vals);

	MathLib::CRSMatrix<double, typename LisMatrix::IndexType> *crs_mat(
		MathLib::detail::lis2crs(eqsA));

	applyKnownSolution(crs_mat, eqsRHS, input_rows, input_vals);

	LisMatrix::IndexType const*const jA(crs_mat->getColIdxArray());
	double * entries(const_cast<double*>(crs_mat->getEntryArray()));

	LIS_MATRIX &A = eqsA.getRawMatrix();
	// reset the entries in the lis matrix
	LisMatrix::IndexType cnt(0);
	for (LIS_INT row_i = 0; row_i < A->n; ++row_i) {
		for (LIS_INT j = 0; j < A->w_row[row_i - A->is]; ++j) {
			A->w_index[row_i-A->is][j] = jA[cnt];
			A->w_value[row_i-A->is][j] = entries[cnt];
			cnt++;
		}
	}
	delete crs_mat;
}

}  // MathLib
