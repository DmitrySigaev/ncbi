/*   ncbi_math.c
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* File Name:  ncbi_math.c
*
* Author:  Gish, Kans, Ostell, Schuler
*
* Version Creation Date:   10/23/91
*
* $Revision$
*
* File Description:
*   	portable math functions
*
* Modifications:
* --------------------------------------------------------------------------
* Date     Name        Description of modification
* -------  ----------  -----------------------------------------------------
* 04-15-93 Schuler     Changed _cdecl to LIBCALL
* 12-22-93 Schuler     Converted ERRPOST((...)) to ErrPostEx(...)
*
* $Log$
* Revision 1.2  2003/08/11 15:02:00  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.1  2003/08/02 16:31:48  camacho
* Moved ncbimath.c -> ncbi_math.c
*
* Revision 1.1  2003/08/01 21:03:46  madden
* Cleaned up version of file for C++ toolkit
*
* Revision 6.3  1999/11/24 17:29:16  sicotte
* Added LnFactorial function
*
* Revision 6.2  1997/11/26 21:26:18  vakatov
* Fixed errors and warnings issued by C and C++ (GNU and Sun) compilers
*
* Revision 6.1  1997/10/31 16:22:49  madden
* Limited the loop in Nlm_Log1p to 500 iterations
*
* Revision 6.0  1997/08/25 18:16:35  madden
* Revision changed to 6.0
*
* Revision 5.4  1997/01/31 22:21:40  kans
* had to remove <fp.h> and define HUGE_VAL inline, because of a conflict
* with <math.h> in 68K CodeWarrior 11
*
 * Revision 5.3  1997/01/28  22:57:57  kans
 * include <fp.h> for CodeWarrior to get HUGE_VAL
 *
 * Revision 5.2  1996/12/03  21:48:33  vakatov
 * Adopted for 32-bit MS-Windows DLLs
 *
 * Revision 5.1  1996/06/20  14:08:00  madden
 * Changed int to Int4, double to Nlm_FloatHi
 *
 * Revision 5.0  1996/05/28  13:18:57  ostell
 * Set to revision 5.0
 *
 * Revision 4.1  1996/03/06  19:47:15  epstein
 * fix problem observed by Epstein & fixed by Spouge in log calculation
 *
 * Revision 4.0  1995/07/26  13:46:50  ostell
 * force revision to 4.0
 *
 * Revision 2.11  1995/05/15  18:45:58  ostell
 * added Log line
 *
*
*
* ==========================================================================
*/

#define THIS_MODULE g_corelib
#define THIS_FILE _this_file

#include <algo/blast/core/ncbi_math.h>

extern char * g_corelib;
static char * _this_file = __FILE__;

/*
    Nlm_Expm1(x)
    Return values accurate to approx. 16 digits for the quantity exp(x)-1
    for all x.
*/
extern double Nlm_Expm1(double	x)
{
  double	absx;

  if ((absx = ABS(x)) > .33)
    return exp(x) - 1.;

  if (absx < 1.e-16)
    return x;

  return x * (1. + x *
              (0.5 + x * (1./6. + x *
                          (1./24. + x * (1./120. + x *
                                         (1./720. + x * (1./5040. + x *
                                                         (1./40320. + x * (1./362880. + x *
                                                                           (1./3628800. + x * (1./39916800. + x *
                                                                                               (1./479001600. + x/6227020800.)
                                                                                               ))
                                                                           ))
                                                         ))
                                         ))
                          )));
}

#define DBL_EPSILON 2.2204460492503131e-16
/*
    Nlm_Log1p(x)
    Return accurate values for the quantity log(x+1) for all x > -1.
*/
extern double Nlm_Log1p(double x)
{
	Int4	i;
	double	sum, y;

	if (ABS(x) >= 0.2)
		return log(x+1.);

	/* Limit the loop to 500 terms. */
	for (i=0, sum=0., y=x; i<500 ; ) {
		sum += y/++i;
		if (ABS(y) < DBL_EPSILON)
			break;
		y *= x;
		sum -= y/++i;
		if (y < DBL_EPSILON)
			break;
		y *= x;
	}
	return sum;
}

static double Nlm_LogDerivative(Int4 order, double* u) /* nth derivative of ln(u) */
        /* order is order of the derivative */
        /* u is values of u, u', u", etc. */
{
  Int4          i;
  double   y[LOGDERIV_ORDER_MAX+1];
  double  value, tmp;

  if (order < 0 || order > LOGDERIV_ORDER_MAX) {
InvalidOrder:
/*
    ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"LogDerivative: unsupported derivative order");
*/
    /**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "unsupported derivative order"));**/
    return HUGE_VAL;
  }

  if (order > 0 && u[0] == 0.) {
/*
    ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"LogDerivative: divide by 0");
*/
    /**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "divide by 0"));**/
    return HUGE_VAL;
  }
  for (i = 1; i <= order; i++)
    y[i] = u[i] / u[0];

  switch (order) {
  case 0:
    if (u[0] > 0.)
      value = log(u[0]);
    else {
/*
      ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"LogDerivative: log(x<=0)");
*/
      /**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "log(x<=0)"));**/
      return HUGE_VAL;
    }
    break;
  case 1:
    value = y[1];
    break;
  case 2:
    value = y[2] - y[1] * y[1];
    break;
  case 3:
    value = y[3] - 3. * y[2] * y[1] + 2. * y[1] * y[1] * y[1];
    break;
  case 4:
    value = y[4] - 4. * y[3] * y[1] - 3. * y[2] * y[2]
      + 12. * y[2] * (tmp = y[1] * y[1]);
    value -= 6. * tmp * tmp;
    break;
  default:
    goto InvalidOrder;
  }
  return value;
}

static double _default_gamma_coef [] = {
         4.694580336184385e+04,
        -1.560605207784446e+05,
         2.065049568014106e+05,
        -1.388934775095388e+05,
         5.031796415085709e+04,
        -9.601592329182778e+03,
         8.785855930895250e+02,
        -3.155153906098611e+01,
         2.908143421162229e-01,
        -2.319827630494973e-04,
         1.251639670050933e-10
         };

static int      xgamma_dim = DIM(_default_gamma_coef);

static double
general_lngamma(double x, Int4 order)      /* nth derivative of ln[gamma(x)] */
        /* x is 10-digit accuracy achieved only for 1 <= x */
        /* order is order of the derivative, 0..POLYGAMMA_ORDER_MAX */
{
        Int4            i;
        double     xx, tx;
        double     y[POLYGAMMA_ORDER_MAX+1];
        double    tmp, value;
	double    *coef;

        xx = x - 1.;  /* normalize from gamma(x + 1) to xx! */
        tx = xx + xgamma_dim;
        for (i = 0; i <= order; ++i) {
                tmp = tx;
                /* sum the least significant terms first */
                coef = &_default_gamma_coef[xgamma_dim];
                if (i == 0) {
                        value = *--coef / tmp;
                        while (coef > _default_gamma_coef)
                                value += *--coef / --tmp;
                }
                else {
                        value = *--coef / Nlm_Powi(tmp, i + 1);
                        while (coef > _default_gamma_coef)
                                value += *--coef / Nlm_Powi(--tmp, i + 1);
                        tmp = Nlm_Factorial(i);
                        value *= (i%2 == 0 ? tmp : -tmp);
                }
                y[i] = value;
        }
        ++y[0];
        value = Nlm_LogDerivative(order, y);
        tmp = tx + 0.5;
        switch (order) {
        case 0:
                value += ((NCBIMATH_LNPI+NCBIMATH_LN2) / 2.)
                                        + (xx + 0.5) * log(tmp) - tmp;
                break;
        case 1:
                value += log(tmp) - xgamma_dim / tmp;
                break;
        case 2:
                value += (tmp + xgamma_dim) / (tmp * tmp);
                break;
        case 3:
                value -= (1. + 2.*xgamma_dim / tmp) / (tmp * tmp);
                break;
        case 4:
                value += 2. * (1. + 3.*xgamma_dim / tmp) / (tmp * tmp * tmp);
                break;
        default:
                tmp = Nlm_Factorial(order - 2) * Nlm_Powi(tmp, 1 - order)
                                * (1. + (order - 1) * xgamma_dim / tmp);
                if (order % 2 == 0)
                        value += tmp;
                else
                        value -= tmp;
                break;
        }
        return value;
}


extern double Nlm_PolyGamma(double x, Int4 order) /* ln(ABS[gamma(x)]) - 10 digits of accuracy */
	/* x is and derivatives */
	/* order is order of the derivative */
/* order = 0, 1, 2, ...  ln(gamma), digamma, trigamma, ... */
/* CAUTION:  the value of order is one less than the suggested "di" and
"tri" prefixes of digamma, trigamma, etc.  In other words, the value
of order is truly the order of the derivative.  */
{
	Int4		k;
	double	value, tmp;
	double	y[POLYGAMMA_ORDER_MAX+1], sx;

	if (order < 0 || order > POLYGAMMA_ORDER_MAX) {
/*
		ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"PolyGamma: unsupported derivative order");
*/
		/**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "unsupported derivative order"));**/
		return HUGE_VAL;
	}

	if (x >= 1.)
		return general_lngamma(x, order);

	if (x < 0.) {
		value = general_lngamma(1. - x, order);
		value = ((order - 1) % 2 == 0 ? value : -value);
		if (order == 0) {
			sx = sin(NCBIMATH_PI * x);
			sx = ABS(sx);
			if ( (x < -0.1 && (ceil(x) == x || sx < 2.*DBL_EPSILON))
					|| sx == 0.) {
/*
				ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"PolyGamma: log(0)");
*/
				/**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "log(0)"));**/
				return HUGE_VAL;
			}
			value += NCBIMATH_LNPI - log(sx);
		}
		else {
			y[0] = sin(x *= NCBIMATH_PI);
			tmp = 1.;
			for (k = 1; k <= order; k++) {
				tmp *= NCBIMATH_PI;
				y[k] = tmp * sin(x += (NCBIMATH_PI/2.));
			}
			value -= Nlm_LogDerivative(order, y);
		}
	}
	else {
		value = general_lngamma(1. + x, order);
		if (order == 0) {
			if (x == 0.) {
/*
				ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"PolyGamma: log(0)");
*/
				/**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "log(0)"));**/
				return HUGE_VAL;
			}
			value -= log(x);
		}
		else {
			tmp = Nlm_Factorial(order - 1) * Nlm_Powi(x,  -order);
			value += (order % 2 == 0 ? tmp : - tmp);
		}
	}
	return value;
}

extern double Nlm_LnGamma(double x)               /* ln(ABS[gamma(x)]) - 10 dig
its of accuracy */
{
        return Nlm_PolyGamma(x, 0);
}


#define FACTORIAL_PRECOMPUTED   36

extern double Nlm_Factorial(Int4 n)
{
        static double      precomputed[FACTORIAL_PRECOMPUTED]
                = { 1., 1., 2., 6., 24., 120., 720., 5040., 40320., 362880., 3628800.};
        static Int4     nlim = 10;
        Int4   m;
        double    x;

        if (n >= 0) {
                if (n <= nlim)
                        return precomputed[n];
                if (n < DIM(precomputed)) {
                        for (x = precomputed[m = nlim]; m < n; ) {
                                ++m;
                                precomputed[m] = (x *= m);
                        }
                        nlim = m;
                        return x;
                }
                return exp(Nlm_LnGamma((double)(n+1)));
        }
        return 0.0; /* Undefined! */
}


/* Nlm_LnGammaInt(n) -- return log(Gamma(n)) for integral n */
extern double Nlm_LnGammaInt(Int4 n)
{
	static double	precomputed[FACTORIAL_PRECOMPUTED];
	static Int4	nlim = 1; /* first two entries are 0 */
	Int4 m;

	if (n >= 0) {
		if (n <= nlim)
			return precomputed[n];
		if (n < DIM(precomputed)) {
			for (m = nlim; m < n; ++m) {
				precomputed[m+1] = log(Nlm_Factorial(m));
			}
			return precomputed[nlim = m];
		}
	}
	return Nlm_LnGamma((double)n);
}




/*
	Romberg numerical integrator

	Author:  Dr. John Spouge, NCBI
	Received:  July 17, 1992
	Reference:
		Francis Scheid (1968)
		Schaum's Outline Series
		Numerical Analysis, p. 115
		McGraw-Hill Book Company, New York
*/
#define F(x)  ((*f)((x), fargs))
#define ROMBERG_ITMAX 20

extern double Nlm_RombergIntegrate(double (*f) (double,void*), void* fargs, double p, double q, double eps, Int4 epsit, Int4 itmin)

{
	double	romb[ROMBERG_ITMAX];	/* present list of Romberg values */
	double	h;	/* mesh-size */
	Int4		i, j, k, npts;
	long	n;	/* 4^(error order in romb[i]) */
	Int4		epsit_cnt = 0, epsck;
	double	y;
	double	x;
	double	sum;

	/* itmin = min. no. of iterations to perform */
	itmin = MAX(1, itmin);
	itmin = MIN(itmin, ROMBERG_ITMAX-1);

	/* epsit = min. no. of consecutive iterations that must satisfy epsilon */
	epsit = MAX(epsit, 1); /* default = 1 */
	epsit = MIN(epsit, 3); /* if > 3, the problem needs more prior analysis */

	epsck = itmin - epsit;

	npts = 1;
	h = q - p;
	x = F(p);
	if (ABS(x) == HUGE_VAL)
		return x;
	y = F(q);
	if (ABS(y) == HUGE_VAL)
		return y;
	romb[0] = 0.5 * h * (x + y);	/* trapezoidal rule */
	for (i = 1; i < ROMBERG_ITMAX; ++i, npts *= 2, h *= 0.5) {
		sum = 0.;	/* sum of ordinates for */
		/* x = p+0.5*h, p+1.5*h, ..., q-0.5*h */
		for (k = 0, x = p+0.5*h; k < npts; k++, x += h) {
			y = F(x);
			if (ABS(y) == HUGE_VAL)
				return y;
			sum += y;
		}
		romb[i] = 0.5 * (romb[i-1] + h*sum); /* new trapezoidal estimate */

		/* Update Romberg array with new column */
		for (n = 4, j = i-1; j >= 0; n *= 4, --j)
			romb[j] = (n*romb[j+1] - romb[j]) / (n-1);

		if (i > epsck) {
			if (ABS(romb[1] - romb[0]) > eps * ABS(romb[0])) {
				epsit_cnt = 0;
				continue;
			}
			++epsit_cnt;
			if (i >= itmin && epsit_cnt >= epsit)
				return romb[0];
		}
	}
/*
	ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_ITER,"RombergIntegrate: iterations > ROMBERG_ITMAX");
*/
	/**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_ITER, "iterations > ROMBERG_ITMAX"));**/
	return HUGE_VAL;
}

/*
	Nlm_Gcd(a, b)

	Return the greatest common divisor of a and b.

	Adapted 8-15-90 by WRG from code by S. Altschul.
*/
long Nlm_Gcd(long a, long b)
{
	long	c;

	b = ABS(b);
	if (b > a)
		c=a, a=b, b=c;

	while (b != 0) {
		c = a%b;
		a = b;
		b = c;
	}
	return a;
}

/* Round a floating point number to the nearest integer */
long Nlm_Nint(double x)	/* argument */
{
	x += (x >= 0. ? 0.5 : -0.5);
	return (long)x;
}

/*
integer power function

Original submission by John Spouge, 6/25/90
Added to shared library by WRG
*/
extern double Nlm_Powi(double x, Int4 n)	/* power */
{
	double	y;

	if (n == 0)
		return 1.;

	if (x == 0.) {
		if (n < 0) {
/*
			ErrPostEx(SEV_WARNING,E_Math,ERR_NCBIMATH_DOMAIN,"Powi: divide by 0");
*/
			/**ERRPOST((CTX_NCBIMATH, ERR_NCBIMATH_DOMAIN, "divide by 0"));**/
			return HUGE_VAL;
		}
		return 0.;
	}

	if (n < 0) {
		x = 1./x;
		n = -n;
	}

	while (n > 1) {
		if (n&1) {
			y = x;
			goto Loop2;
		}
		n /= 2;
		x *= x;
	}
	return x;

Loop2:
	n /= 2;
	x *= x;
	while (n > 1) {
		if (n&1)
			y *= x;
		n /= 2;
		x *= x;
	}
	return y * x;
}

extern double Nlm_LnFactorial (double x) {

    if(x<=0.0)
        return 0.0;
    else
        return Nlm_LnGamma(x+1.0);
        
}
