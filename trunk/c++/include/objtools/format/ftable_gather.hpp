#ifndef OBJTOOLS_FORMAT___FTABLE_GATHER__HPP
#define OBJTOOLS_FORMAT___FTABLE_GATHER__HPP

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
* Author:  Mati Shomrat
*
* File Description:
*   5-Column feature table data gathering.
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/gather_items.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CFtableGatherer : public CFlatGatherer
{
public:
    CFtableGatherer(void);

    virtual void x_DoSingleSection(CBioseqContext& ctx) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/04/22 15:44:07  shomrat
* Changes in context
*
* Revision 1.2  2004/04/07 14:51:46  shomrat
* Fixed typo
*
* Revision 1.1  2004/04/07 14:25:46  shomrat
* Initial Revision
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT___FTABLE_GATHER__HPP */
