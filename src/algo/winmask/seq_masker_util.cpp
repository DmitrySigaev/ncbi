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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   CSeqMaskerUtil class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <algo/winmask/seq_masker_util.hpp>

BEGIN_NCBI_SCOPE

//-------------------------------------------------------------------------
Uint1 CSeqMaskerUtil::BitCount( Uint4 mask, Uint1 bit_value )
{
    if( !bit_value ) return BitCount( ~mask, 1 );
    else 
    {
        Uint1 result = 0;

        for( Uint1 i = 0; i < 8*sizeof( mask ); ++i )
            if( (1<<i)&mask ) ++result;

        return result;
    }
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

