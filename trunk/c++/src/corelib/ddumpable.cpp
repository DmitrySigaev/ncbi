
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
 *  Please say you saw it at NCBI
 *
 * ===========================================================================
 *
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Abstract base class: defines DebugDump() functionality
 *
 */

#include <corelib/ddumpable.hpp>
#include <corelib/ddump_formatter_text.hpp>


BEGIN_NCBI_SCOPE

bool CDebugDumpable::sm_DumpEnabled = true;

CDebugDumpable::~CDebugDumpable(void)
{
    return;
}

void CDebugDumpable::EnableDebugDump( bool on)
{
    sm_DumpEnabled = on;
}


void CDebugDumpable::DebugDumpText(ostream& out,
     const string& bundle, unsigned int depth) const
{
    if (sm_DumpEnabled) {
        CDebugDumpFormatterText ddf(out);
        DebugDumpFormat( ddf, bundle, depth);
    }
}

void CDebugDumpable::DebugDumpFormat(CDebugDumpFormatter& ddf,
     const string& bundle, unsigned int depth) const
{
    if (sm_DumpEnabled) {
        CDebugDumpContext ddc(ddf, bundle);
        DebugDump( ddc, depth);
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/05/17 14:27:11  gouriano
 * added DebugDump base class and function to CObject
 *
 *
 *
 * ===========================================================================
 */
