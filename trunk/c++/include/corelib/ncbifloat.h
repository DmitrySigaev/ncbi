#ifndef NCBIFLOAT__H
#define NCBIFLOAT__H

/*  $Id$
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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *      Floating-point support routines
 *      
 */


/** @addtogroup Exception
 *
 * @{
 */


#include <ncbiconf.h>

#if defined(NCBI_OS_MSWIN)
#   include <float.h>
#endif


#if defined(NCBI_OS_MSWIN)
//Checks double-precision value for not a number (NaN)
#   define isnan _isnan
#elif defined(NCBI_OS_DARWIN)  &&  defined(MATH_ERRNO)  &&  !defined(isnan)
// <math.h> changed a lot between 10.1 and 10.2; the presence of
// MATH_ERRNO indicates 10.2, which needs this hack.
#   define isnan __isnan
#endif


/* @} *


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/04/01 14:19:42  siyan
 * Added doxygen support
 *
 * Revision 1.2  2003/02/14 15:48:44  ucko
 * Add a workaround for Mac OS X 10.2, where <cmath> clobbers <math.h>'s
 * #define of isnan but doesn't replace it with anything.
 *
 * Revision 1.1  2003/02/04 17:02:07  gouriano
 * initial revision
 *
 *
 * ==========================================================================
 */

#endif /* NCBIFLOAT__H */
