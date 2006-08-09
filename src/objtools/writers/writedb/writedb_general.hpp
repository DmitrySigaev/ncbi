#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GENERAL_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GENERAL_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_general.hpp
/// Defines implementation class of WriteDB.
///
/// Defines classes:
///     CWriteDBHeader
///
/// Implemented for: UNIX, MS-Windows

#include <objects/seq/seq__.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// Divide by a number, rounding up to a whole integer.
inline int s_DivideRoundUp(int value, int blocksize)
{
    return ((value + (blocksize - 1)) / blocksize);
}

/// Round up to the next multiple of some number.
inline int s_RoundUp(int value, int blocksize)
{
    return s_DivideRoundUp(value, blocksize)*blocksize;
}

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GENERAL_HPP


