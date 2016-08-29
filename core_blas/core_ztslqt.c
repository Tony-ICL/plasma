/**
 *
 * @file
 *
 *  PLASMA is a software package provided by:
 *  University of Tennessee, US,
 *  University of Manchester, UK.
 *
 * @precisions normal z -> c d s
 *
 **/

#include "core_blas.h"
#include "plasma_types.h"
#include "plasma_internal.h"

#ifdef PLASMA_WITH_MKL
    #include <mkl.h>
#else
    #include <cblas.h>
    #include <lapacke.h>
#endif

// this will be swapped during the automatic code generation
#undef REAL
#define COMPLEX

/***************************************************************************//**
 *
 * @ingroup core_tslqt
 *
 *  Computes an LQ factorization of a rectangular matrix
 *  formed by coupling side-by-side a complex m-by-m
 *  lower triangular tile A1 and a complex m-by-n tile A2:
 *
 *    | A1 A2 | = L * Q
 *
 *  The tile Q is represented as a product of elementary reflectors
 *
 *    Q = H(k)^H . . . H(2)^H H(1)^H, where k = min(m,n).
 *
 *  Each H(i) has the form
 *
 *    H(i) = I - tau * v * v^H
 *
 *  where tau is a complex scalar, and v is a complex vector with
 *  v(1:i-1) = 0 and v(i) = 1; v(i+1:n)^H is stored on exit in
 *  A2(i,1:n), and tau in TAU(i).
 *
 *******************************************************************************
 *
 * @param[in] m
 *         The number of rows of the tile A1 and A2. m >= 0.
 *         The number of columns of the tile A1.
 *
 * @param[in] n
 *         The number of columns of the tile A2. n >= 0.
 *
 * @param[in] ib
 *         The inner-blocking size.  ib >= 0.
 *
 * @param[in,out] A1
 *         On entry, the m-by-m tile A1.
 *         On exit, the elements on and below the diagonal of the array
 *         contain the m-by-m lower trapezoidal tile L;
 *         the elements above the diagonal are not referenced.
 *
 * @param[in] lda1
 *         The leading dimension of the array A1. lda1 >= max(1,m).
 *
 * @param[in,out] A2
 *         On entry, the m-by-n tile A2.
 *         On exit, all the elements with the array TAU, represent
 *         the unitary tile Q as a product of elementary reflectors
 *         (see Further Details).
 *
 * @param[in] lda2
 *         The leading dimension of the tile A2. lda2 >= max(1,m).
 *
 * @param[out] T
 *         The ib-by-n triangular factor T of the block reflector.
 *         T is upper triangular by block (economic storage);
 *         The rest of the array is not referenced.
 *
 * @param[in] ldt
 *         The leading dimension of the array T. ldt >= ib.
 *
 * @param TAU
 *         Auxiliarry workspace array of length m.
 *
 * @param WORK
 *         Auxiliary workspace array of length ib*m.
 *
 ******************************************************************************/
void CORE_ztslqt(int m, int n, int ib,
                 PLASMA_Complex64_t *A1, int lda1,
                 PLASMA_Complex64_t *A2, int lda2,
                 PLASMA_Complex64_t *T, int ldt,
                 PLASMA_Complex64_t *TAU, PLASMA_Complex64_t *WORK)
{
    static PLASMA_Complex64_t zone  = 1.0;
    static PLASMA_Complex64_t zzero = 0.0;

    PLASMA_Complex64_t alpha;
    int i, ii, sb;

    // Check input arguments
    if (m < 0) {
        plasma_error("Illegal value of m");
        return;
    }
    if (n < 0) {
        plasma_error("Illegal value of n");
        return;
    }
    if (ib < 0) {
        plasma_error("Illegal value of ib");
        return;
    }
    if ((lda2 < imax(1,m)) && (m > 0)) {
        plasma_error("Illegal value of lda2");
        return;
    }

    // Quick return
    if ((m == 0) || (n == 0) || (ib == 0))
        return;

    for (ii = 0; ii < m; ii += ib) {
        sb = imin(m-ii, ib);
        for (i = 0; i < sb; i++) {
            // Generate elementary reflector H(ii*ib+i) to annihilate
            // A(ii*ib+i,ii*ib+i:n).
#ifdef COMPLEX
            LAPACKE_zlacgv_work(n, &A2[ii+i], lda2);
            LAPACKE_zlacgv_work(1, &A1[lda1*(ii+i)+ii+i], lda1);
#endif
            LAPACKE_zlarfg_work(n+1, &A1[lda1*(ii+i)+ii+i], &A2[ii+i], lda2,
                                &TAU[ii+i]);

            alpha = -(TAU[ii+i]);
            if (ii+i+1 < m) {
                // Apply H(ii+i-1) to A(ii+i:ii+ib-1, ii+i-1:n) from the right.
                cblas_zcopy(
                    sb-i-1,
                    &A1[lda1*(ii+i)+(ii+i+1)], 1,
                    WORK, 1);

                cblas_zgemv(
                    CblasColMajor, (CBLAS_TRANSPOSE)PlasmaNoTrans,
                    sb-i-1, n,
                    CBLAS_SADDR(zone), &A2[ii+i+1], lda2,
                    &A2[ii+i], lda2,
                    CBLAS_SADDR(zone), WORK, 1);

                cblas_zaxpy(
                    sb-i-1, CBLAS_SADDR(alpha),
                    WORK, 1,
                    &A1[lda1*(ii+i)+ii+i+1], 1);

                cblas_zgerc(
                    CblasColMajor, sb-i-1, n,
                    CBLAS_SADDR(alpha), WORK, 1,
                    &A2[ii+i], lda2,
                    &A2[ii+i+1], lda2);
            }
            // Calculate T.
            cblas_zgemv(
                CblasColMajor, (CBLAS_TRANSPOSE)PlasmaNoTrans, i, n,
                CBLAS_SADDR(alpha), &A2[ii], lda2,
                &A2[ii+i], lda2,
                CBLAS_SADDR(zzero), &T[ldt*(ii+i)], 1);
#ifdef COMPLEX
            LAPACKE_zlacgv_work(n, &A2[ii+i], lda2 );
            LAPACKE_zlacgv_work(1, &A1[lda1*(ii+i)+ii+i], lda1 );
#endif
            cblas_ztrmv(
                CblasColMajor, (CBLAS_UPLO)PlasmaUpper,
                (CBLAS_TRANSPOSE)PlasmaNoTrans, (CBLAS_DIAG)PlasmaNonUnit, i,
                &T[ldt*ii], ldt,
                &T[ldt*(ii+i)], 1);

            T[ldt*(ii+i)+i] = TAU[ii+i];
        }
        if (m > ii+sb) {
            // Plasma_ConjTrans will be converted to PlasmaTrans in
            // automatic datatype conversion, which is what we want here.
            // PlasmaConjTrans is protected from this conversion.
            CORE_ztsmlq(
                PlasmaRight, Plasma_ConjTrans,
                m-(ii+sb), sb, m-(ii+sb), n, ib, ib,
                &A1[lda1*ii+ii+sb], lda1,
                &A2[ii+sb], lda2,
                &A2[ii], lda2,
                &T[ldt*ii], ldt,
                WORK, lda1);
        }
    }
}

/******************************************************************************/
void CORE_OMP_ztslqt(int m, int n, int ib, int nb,
                     PLASMA_Complex64_t *A1, int lda1,
                     PLASMA_Complex64_t *A2, int lda2,
                     PLASMA_Complex64_t *T,  int ldt)
{
    // assuming m == nb, n == nb
    #pragma omp task depend(inout:A1[0:nb*nb]) \
                     depend(inout:A2[0:nb*nb]) \
                     depend(out:T[0:ib*nb])
    {
        // prepare memory for auxiliary arrays
        PLASMA_Complex64_t *TAU  =
            (PLASMA_Complex64_t *) malloc((size_t)nb *
                                          sizeof(PLASMA_Complex64_t));
        if (TAU == NULL) {
            plasma_error("malloc() failed");
        }
        PLASMA_Complex64_t *WORK =
            (PLASMA_Complex64_t *) malloc((size_t)ib*nb *
                                          sizeof(PLASMA_Complex64_t));
        if (WORK == NULL) {
            plasma_error("malloc() failed");
        }

        CORE_ztslqt(m, n, ib,
                    A1, lda1,
                    A2, lda2,
                    T,  ldt,
                    TAU,
                    WORK);

        // deallocate auxiliary arrays
        free(TAU);
        free(WORK);
    }
}