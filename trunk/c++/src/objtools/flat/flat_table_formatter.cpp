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
*
* File Description:
*   new (early 2003) flat-file generator -- 5-column tabular output
*
* ===========================================================================
*/

#include <objects/flat/flat_table_formatter.hpp>
#include <objects/flat/flat_items.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CFlatTableFormatter::BeginSequence(CFlatContext& context)
{
    IFlatFormatter::BeginSequence(context);
    m_Stream->NewSequence();
}


void CFlatTableFormatter::FormatHead(const CFlatHead& head)
{
    list<string> l;
    l.push_back(">Features " + head.GetLocus());
    m_Stream->AddParagraph(l, &head, &m_Context->GetPrimaryID());
}


void CFlatTableFormatter::FormatFeature(const IFlattishFeature& f)
{
    const CFlatFeature& feat     = *f.Format();
    list<string>        l;
    string              line;
    bool                need_key = true;
    ITERATE (CFlatLoc::TIntervals, it, feat.GetLoc().GetIntervals()) {
        if (it->m_Accession != m_Context->GetAccession()) {
            continue; // or should we turn it into a prefix?
        }
        string left, right;
        if (it->IsPartialLeft()) {
            left = '<';
        }
        left += NStr::IntToString(it->m_Range.GetFrom());
        if (it->IsPartialRight()) {
            right = '>';
        }
        right += NStr::IntToString(it->m_Range.GetTo());
        if (it->IsReversed()) {
            line = right + '\t' + left;
        } else {
            line = left + '\t' + right;
        }
        if (need_key) {
            line += '\t' + feat.GetKey();
            need_key = false;
        }
        l.push_back(line);
    }

    ITERATE (CFlatFeature::TQuals, it, feat.GetQuals()) {
        // drop redundant quals?
        line = "\t\t\t" + (*it)->GetName();
        if ((*it)->GetStyle() != CFlatQual::eEmpty) {
            line += '\t' + (*it)->GetValue();
        }
    }
    m_Stream->AddParagraph(l, &f, &feat.GetFeat());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.1  2003/03/28 19:04:33  ucko
* Add a formatter for 5-column tabular output.
*
*
* ===========================================================================
*/
