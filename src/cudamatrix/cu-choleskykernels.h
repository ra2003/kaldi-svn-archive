#ifndef KALDI_CUDAMATRIX_CU_CHOLESKYKERNELS_H_
#define KALDI_CUDAMATRIX_CU_CHOLESKYKERNELS_H_

#if HAVE_CUDA==1

#include "base/kaldi-error.h"
#include "cudamatrix/cu-choleskykernels-ansi.h"

/*
 * In this file are C++ templated wrappers
 * of the ANSI-C CUDA kernels
 */

namespace kaldi {

/*********************************************************
* base templates
*/
template<typename Real> inline void cuda_factorize_diagonal_block(Real* A, int block_offset, MatrixDim d) { KALDI_ERR << __func__ << " Not implemented!"; }
template<typename Real> inline void cuda_strip_update(Real* A, int block_offset, int n_rows_padded, int n_remaining_blocks) { KALDI_ERR << __func__ << " Not implemented!"; }
template<typename Real> inline void cuda_diag_update(Real* A, int block_offset, int n_rows_padded, int n_remaining_blocks) { KALDI_ERR << __func__ << " Not implemented!"; }
template<typename Real> inline void cuda_lo_update(Real* A, int block_offset, int n_blocks, int n_rows_padded, int n_remaining_blocks) { KALDI_ERR << __func__ << " Not implemented!"; }
/*********************************************************
* float specialization
*/
template<> inline void cuda_factorize_diagonal_block<float>(float* A, int block_offset, MatrixDim d) { cudaF_factorize_diagonal_block(A,block_offset,d); }
template<> inline void cuda_strip_update<float>(float* A, int block_offset, int n_rows_padded, int n_remaining_blocks) { cudaF_strip_update(A,block_offset,n_rows_padded,n_remaining_blocks); }
template<> inline void cuda_diag_update<float>(float* A, int block_offset, int n_rows_padded, int n_remaining_blocks) { cudaF_diag_update(A,block_offset,n_rows_padded,n_remaining_blocks); }
template<> inline void cuda_lo_update<float>(float* A, int block_offset, int n_blocks, int n_rows_padded, int n_remaining_blocks) { cudaF_lo_update(A,block_offset,n_blocks,n_rows_padded,n_remaining_blocks); }
/*********************************************************
* double specialization
*/
template<> inline void cuda_factorize_diagonal_block<double>(double* A, int block_offset, MatrixDim d) { cudaD_factorize_diagonal_block(A,block_offset,d); }
template<> inline void cuda_strip_update<double>(double* A, int block_offset, int n_rows_padded, int n_remaining_blocks) { cudaD_strip_update(A,block_offset,n_rows_padded,n_remaining_blocks); }
template<> inline void cuda_diag_update<double>(double* A, int block_offset, int n_rows_padded, int n_remaining_blocks) { cudaD_diag_update(A,block_offset,n_rows_padded,n_remaining_blocks); }
template<> inline void cuda_lo_update<double>(double* A, int block_offset, int n_blocks, int n_rows_padded, int n_remaining_blocks) { cudaD_lo_update(A,block_offset,n_blocks,n_rows_padded,n_remaining_blocks); }

} // namespace

#endif // HAVE_CUDA

#endif
