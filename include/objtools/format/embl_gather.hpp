#ifndef OBJTOOLS_FORMAT___EMBL_GATHER__HPP
#define OBJTOOLS_FORMAT___EMBL_GATHER__HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- base class for items
*   (which roughly correspond to blocks/paragraphs in the C version)
*
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/gather_items.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_FORMAT_EXPORT CEmblGatherer : public CFlatGatherer
{
public:
    CEmblGatherer(void);

    virtual void x_DoSingleSection(CBioseqContext& ctx) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2005/09/13 19:22:55  jcherry
* Added export specifiers
*
* Revision 1.3  2004/04/22 15:42:40  shomrat
* Changes in context
*
* Revision 1.2  2004/03/25 20:29:30  shomrat
* CBioseq -> CBioseq_Handle
*
* Revision 1.1  2003/12/17 19:51:23  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT___EMBL_GATHER__HPP */
