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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator -- accession item implementation
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAccessionItem::CAccessionItem(CFFContext& ctx) :
    CFlatItem(ctx)
{
    x_GatherInfo(ctx);
}


void CAccessionItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatAccession(*this, text_os);
}


const string& CAccessionItem::GetAccession(void) const
{
    return m_Accession;
}


const list<string>& CAccessionItem::GetExtraAccessions(void) const
{
    return m_ExtraAccessions;
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CAccessionItem::x_GatherInfo(CFFContext& ctx)
{
    if ( ctx.GetPrimaryId() == 0 ) {
        x_SetSkip();
        return;
    }

    m_Accession = ctx.GetPrimaryId()->GetSeqIdString();

    CSeqdesc_CI gb_desc(ctx.GetHandle(), CSeqdesc::e_Genbank);
    if ( gb_desc ) {
        x_SetObject(*gb_desc);
        m_ExtraAccessions = gb_desc->GetGenbank().GetExtra_accessions();
    }

    CSeqdesc_CI embl_desc(ctx.GetHandle(), CSeqdesc::e_Embl);
    if ( embl_desc ) {
        x_SetObject(*embl_desc);
        m_ExtraAccessions = embl_desc->GetEmbl().GetExtra_acc();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/12/18 17:43:31  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:17:45  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
