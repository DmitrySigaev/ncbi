#ifndef OBJTOOLS_FORMAT_ITEMS___KEYWORDS_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___KEYWORDS_ITEM__HPP

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
*   Keywords item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>

#include <vector>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;



///////////////////////////////////////////////////////////////////////////
//
// Keywords

class CKeywordsItem : public CFlatItem
{
public:
    // types
    typedef vector<string> TKeywords;

    CKeywordsItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const TKeywords& GetKeywords(void) const;

private:
    void x_GatherInfo(CBioseqContext& ctx);

    void x_AddKeyword(const string& keyword);

    // data
    TKeywords m_Keywords;
};


//===============================  inline methods  ==========================

inline
const CKeywordsItem::TKeywords& CKeywordsItem::GetKeywords(void) const 
{ 
    return m_Keywords; 
}

//===========================  end of inline methods  =======================



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/10/05 15:29:56  shomrat
* list -> vector; changes to x_AddKeyword
*
* Revision 1.2  2004/04/22 15:37:17  shomrat
* Changes in context
*
* Revision 1.1  2003/12/17 19:48:45  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___KEYWORDS_ITEM__HPP */
