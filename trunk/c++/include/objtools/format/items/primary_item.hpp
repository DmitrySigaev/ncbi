#ifndef OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP

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
*   Primary item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <list>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFFContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// PRIMARY

class CPrimaryItem : public CFlatItem
{
public:
    CPrimaryItem(CFFContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    typedef CRange<TSignedSeqPos>   TRange;

    struct SPiece {
        TRange             m_Span;
        CConstRef<CSeq_id> m_PrimaryID;
        TRange             m_PrimarySpan;
        bool               m_Complemented;

        // for usual tabular format (even incorporated in GBSet!)
        string& Format(string &s) const;
    };
    typedef list<SPiece> TPieces;

    // for usual tabular format (even incorporated in GBSet!)
    const char*    GetHeader(void) const;
    const TPieces& GetPieces(void) const { return m_Pieces; }

private:
    void x_GatherInfo(CFFContext& ctx);

    bool    m_IsRefSeq;
    TPieces m_Pieces;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/02/12 20:21:38  shomrat
* added include directive
*
* Revision 1.1  2003/12/17 19:49:08  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP */
