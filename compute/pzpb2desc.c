/**
 *
 * @file
 *
 *  PLASMA is a software package provided by:
 *  University of Tennessee, US,
 *  University of Manchester, UK.
 *
 * @precisions normal z -> s d c
 *
 **/

#include "plasma_async.h"
#include "plasma_context.h"
#include "plasma_descriptor.h"
#include "plasma_internal.h"
#include "plasma_types.h"
#include "plasma_workspace.h"
#include "core_blas.h"

#define tileA(m, n) ((plasma_complex64_t*)plasma_tile_addr(A, (m), (n)))
#define bandA(m, n) (&(pA[lda*(A.nb*(n)) + (A.uplo == PlasmaUpper ? A.ku : 0)+A.mb*((m)-(n))]))

/******************************************************************************/
void plasma_pzpb2desc(plasma_complex64_t *pA, int lda,
                      plasma_desc_t A,
                      plasma_sequence_t *sequence,
                      plasma_request_t *request)
{
    int n, m;

    // Check sequence status.
    if (sequence->status != PlasmaSuccess) {
        plasma_request_fail(sequence, request, PlasmaErrorSequence);
        return;
    }

    for (n = 0; n < A.nt; n++)
    {
        int m_start, m_end;
        if (A.uplo == PlasmaGeneral) {
            m_start = (imax(0, n*A.nb-A.ku-A.kl)) / A.nb;
            m_end = (imin(A.m-1, (n+1)*A.nb+A.kl-1)) / A.nb;
        }
        else if (A.uplo == PlasmaUpper) {
            m_start = (imax(0, n*A.nb-A.ku)) / A.nb;
            m_end = (imin(A.m-1, (n+1)*A.nb-1)) / A.nb;
        }
        else {
            m_start = (imax(0, n*A.nb)) / A.nb;
            m_end = (imin(A.m-1, (n+1)*A.nb+A.kl-1)) / A.nb;
        }
        for (m = m_start; m <= m_end; m++)
        {
            int mb = imin(A.mb, A.m-m*A.mb);
            int nb = imin(A.nb, A.n-n*A.nb);
            core_omp_zlacpy_lapack2tile_band(
                   A.uplo, m, n,
                   mb, nb, A.mb, A.kl, A.ku,
                   bandA(m, n), lda-1,
                   tileA(m, n), BLKLDD_BAND(A.uplo, A, m, n));
                   //tileA(i_start,n), nb*nb, INOUT | GATHERV);
        }
    }
}