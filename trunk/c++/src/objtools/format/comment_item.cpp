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
*   flat-file generator -- comment item implementation
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


bool CCommentItem::sm_FirstComment = true;
const string CCommentItem::kNsAreGaps = "The strings of n's in this record " \
"represent gaps between contigs, and the length of each string corresponds " \
"to the length of the gap.";




CCommentItem::CCommentItem(CFFContext& ctx) :
    CFlatItem(ctx), m_First(false)
{
    swap(m_First, sm_FirstComment);
}


CCommentItem::CCommentItem
(const string& comment,
 CFFContext& ctx,
 const CSerialObject* obj) :
    CFlatItem(ctx), m_Comment(comment)
{
    swap(m_First, sm_FirstComment);
    if ( obj != 0 ) {
        x_SetObject(*obj);
    }
}

    
CCommentItem::CCommentItem(const CSeqdesc&  desc, CFFContext& ctx) :
    CFlatItem(ctx)
{
    swap(m_First, sm_FirstComment);
    x_SetObject(desc);
    x_GatherInfo(ctx);
    if ( m_Comment.empty() ) {
        x_SetSkip();
    }
}


void CCommentItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatComment(*this, text_os);
}


string CCommentItem::GetStringForTPA
(const CUser_object& uo,
 CFFContext& ctx)
{
    static const string tpa_string = 
        "THIRD PARTY ANNOTATION DATABASE: This TPA record uses data from DDBJ/EMBL/GenBank ";

    if ( !ctx.IsTPA()  ||  ctx.IsRefSeq() ) {
        return kEmptyStr;
    }
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||  
         uo.GetType().GetStr() != "TpaAssembly" ) {
        return kEmptyStr;
    }
    const CSeq_inst& inst = ctx.GetActiveBioseq().GetInst();
    if ( inst.CanGetHist()  &&  inst.GetHist().CanGetAssembly() ) {
        return kEmptyStr;
    }

    string id;
    vector<string> accessions;
    ITERATE (CUser_object::TData, curr, uo.GetData()) {
        const CUser_field& uf = **curr;
        if ( !uf.CanGetData()  ||  !uf.GetData().IsFields() ) {
            continue;
        }

        ITERATE (CUser_field::C_Data::TFields, ufi, uf.GetData().GetFields()) {
            if( !(*ufi)->CanGetData()  ||  !(*ufi)->GetData().IsStr()  ||
                !(*ufi)->CanGetLabel() ) {
                continue;
            }
            const CObject_id& oid = (*ufi)->GetLabel();
            if ( oid.IsStr()  &&  
                 (NStr::CompareNocase(oid.GetStr(), "accession") == 0) ) {
                string acc = (*ufi)->GetData().GetStr();
                if ( !acc.empty() ) {
                    accessions.push_back(NStr::ToUpper(acc));
                }
            }
        }
    }
    if ( accessions.empty() ) {
        return kEmptyStr;
    }

    CNcbiOstrstream text;
    text << tpa_string << ((accessions.size() > 1) ? "entries " : "entry ");

    size_t size = accessions.size();
    size_t last = size - 1;

    for ( size_t i = 0; i < size; ) {
        text << accessions[i];
        ++i;
        if ( i < size ) {
            text << ((i == last) ? " and " : ", ");
        }
    }

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStringForBankIt(const CUser_object& uo)
{
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||
         uo.GetType().GetStr() != "Submission" ) {
        return kEmptyStr;
    }

    const string* uvc = 0, *bic = 0;
    if ( uo.HasField("UniVecComment") ) {
        const CUser_field& uf = uo.GetField("UniVecComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            uvc = &(uf.GetData().GetStr());
        } 
    }
    if ( uo.HasField("AdditionalComment") ) {
        const CUser_field& uf = uo.GetField("AdditionalComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            bic = &(uf.GetData().GetStr());
        } 
    }

    CNcbiOstrstream text;
    if ( uvc != 0  &&  bic != 0 ) {
        text << "Vector Explanation: " << *uvc << "~Bankit Comment: " << *bic;
    } else if ( uvc != 0 ) {
        text << "Vector Explanation: " << *uvc;
    } else if ( bic != 0 ) {
         text << "Bankit Comment: " << *bic;
    }

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStatusForRefTrack(const CUser_object& uo)
{
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||
         uo.GetType().GetStr() != "RefGeneTracking" ) {
        return kEmptyStr;
    }

    if ( uo.HasField("Status") ) {
        const CUser_field& uf = uo.GetField("Status");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            string str = uf.GetData().GetStr();
            if ( NStr::CompareNocase(str, "Inferred")    == 0  ||
                 NStr::CompareNocase(str, "Provisional") == 0  ||
                 NStr::CompareNocase(str, "Predicted")   == 0  ||
                 NStr::CompareNocase(str, "Validated")   == 0  ||
                 NStr::CompareNocase(str, "Reviewed")    == 0  ||
                 NStr::CompareNocase(str, "Model")       == 0  ||
                 NStr::CompareNocase(str, "WGS")         == 0 ) {
                return NStr::ToUpper(str);
            }
        }
    }
    return kEmptyStr;
}


bool CCommentItem::NsAreGaps(const CBioseq& seq, CFFContext& ctx)
{
    const CSeq_inst& inst = seq.GetInst();
    if ( !inst.CanGetExt() ) {
        return false;
    }

    if ( ctx.GetRepr() == CSeq_inst::eRepr_delta  &&  ctx.IsWGS()  &&
         inst.GetExt().IsDelta() ) {
        ITERATE (CDelta_ext::Tdata, iter, inst.GetExt().GetDelta().Get()) {
            const CDelta_seq& dseg = **iter;
            if ( dseg.IsLiteral() ) {
                const CSeq_literal& lit = dseg.GetLiteral();
                if ( !lit.CanGetSeq_data()  &&  lit.CanGetLength()  &&
                      lit.GetLength() > 0 ) {
                    return true;
                }
            }
        }
    }

    return false;
}


string CCommentItem::GetStringForWGS(CFFContext& ctx)
{
    static const string default_str = "?";

    if ( !ctx.IsWGSMaster()  ||  ctx.GetWGSMasterAccn().empty() ) {
        return kEmptyStr;
    }
    const string& wgsaccn = ctx.GetWGSMasterAccn();

    const string* taxname = 0;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source); it; ++it) {
        const CBioSource& src = it->GetSource();
        if ( src.CanGetOrg()  &&  src.GetOrg().CanGetTaxname() ) {
            taxname = &(src.GetOrg().GetTaxname());
        }
    }

    const string* first = 0, *last = 0;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User); it; ++it) {
        const CUser_object& uo = it->GetUser();
        if ( uo.CanGetType()  &&  uo.GetType().IsStr()  &&
             NStr::CompareNocase(uo.GetType().GetStr(), "WGSProjects") == 0 ) {
            if ( uo.HasField("WGS_accession_first") ) {
                const CUser_field& uf = uo.GetField("WGS_accession_first");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
                    first = &(uf.GetData().GetStr());
                }
            }
            if ( uo.HasField("WGS_accession_last") ) {
                const CUser_field& uf = uo.GetField("WGS_accession_last");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
                    last = &(uf.GetData().GetStr());
                }
            }
        }
    }

    if ( taxname == 0  ||  taxname->empty() ) {
        taxname = &default_str;
    }
    if ( first == 0  ||  first->empty() ) {
        first = &default_str;
    }
    if ( last == 0  ||  last->empty() ) {
        last = &default_str;
    }

    string version = (wgsaccn.length() == 15) ? wgsaccn.substr(4) :
                                                wgsaccn.substr(7);

    CNcbiOstrstream text;
    text << "The " << *taxname 
         << "whole genome shotgun (WGS) project has the project accession " 
         << wgsaccn << ".  This version of the project (" << version 
         << ") has the accession number " << ctx.GetWGSMasterName() << ",";
    if ( (*first != *last) ) {
        text << " and consists of sequences " << *first << "-" << *last;
    } else {
        text << " and consists of sequence " << *first;
    }

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStringForMolinfo(const CMolInfo& mi, CFFContext& ctx)
{
    _ASSERT(mi.CanGetCompleteness());

    bool is_prot = ctx.IsProt();

    switch ( mi.GetCompleteness() ) {
    case CMolInfo::eCompleteness_complete:
        return "COMPLETENESS: full length";

    case CMolInfo::eCompleteness_partial:
        return "COMPLETENESS: not full length";

    case CMolInfo::eCompleteness_no_left:
        return (is_prot ? "COMPLETENESS: incomplete on the amino end" :
                          "COMPLETENESS: incomplete on the 5' end");

    case CMolInfo::eCompleteness_no_right:
        return (is_prot ? "COMPLETENESS: incomplete on the carboxy end" :
                          "COMPLETENESS: incomplete on the 3' end");

    case CMolInfo::eCompleteness_no_ends:
        return "COMPLETENESS: incomplete on both ends";

    case CMolInfo::eCompleteness_has_left:
        return (is_prot ? "COMPLETENESS: complete on the amino end" :
                          "COMPLETENESS: complete on the 5' end");

    case CMolInfo::eCompleteness_has_right:
        return (is_prot ? "COMPLETENESS: complete on the carboxy end" :
                          "COMPLETENESS: complete on the 3' end");

    default:
        return "COMPLETENESS: unknown";
    }

    return kEmptyStr;
}


string CCommentItem::GetStringForHTGS(const CMolInfo& mi, CFFContext& ctx)
{
    SDeltaSeqSummary summary;
    if ( ctx.GetRepr() == CSeq_inst::eRepr_delta ) {
        GetDeltaSeqSummary(ctx.GetActiveBioseq(), ctx.GetScope(), summary);
    }

    CMolInfo::TTech tech = ctx.GetTech();

    CNcbiOstrstream text;

    if ( tech == CMolInfo::eTech_htgs_0 ) {
        if ( summary.num_segs > 0 ) {
            text << "* NOTE: This record contains " << (summary.num_gaps + 1) << " individual~"
                 << "* sequencing reads that have not been assembled into~"
                 << "* contigs. Runs of N are used to separate the reads~"
                 << "* and the order in which they appear is completely~"
                 << "* arbitrary. Low-pass sequence sampling is useful for~"
                 << "* identifying clones that may be gene-rich and allows~"
                 << "* overlap relationships among clones to be deduced.~"
                 << "* However, it should not be assumed that this clone~"
                 << "* will be sequenced to completion. In the event that~"
                 << "* the record is updated, the accession number will~"
                 << "* be preserved.";
        }
        text << "~";
        text << summary.text;
    } else if ( tech == CMolInfo::eTech_htgs_1 ) {
        text << "* NOTE: This is a \"working draft\" sequence.";
        if ( summary.num_segs > 0 ) {
            text << " It currently~"
                 << "* consists of " << (summary.num_gaps + 1) << " contigs. The true order of the pieces~"
                 << "* is not known and their order in this sequence record is~"
                 << "* arbitrary. Gaps between the contigs are represented as~"
                 << "* runs of N, but the exact sizes of the gaps are unknown.";
        }
        text << "~* This record will be updated with the finished sequence~"
             << "* as soon as it is available and the accession number will~"
             << "* be preserved."
             << "~"
             << summary.text;
    } else if ( tech == CMolInfo::eTech_htgs_2 ) {
        text << "* NOTE: This is a \"working draft\" sequence.";
        if ( summary.num_segs > 0 ) {
            text << " It currently~* consists of " << (summary.num_gaps + 1) 
                 << " contigs. Gaps between the contigs~"
                 << "* are represented as runs of N. The order of the pieces~"
                 << "* is believed to be correct as given, however the sizes~"
                 << "* of the gaps between them are based on estimates that have~"
                 << "* provided by the submittor.";
        }
        text << "~* This sequence will be replaced~"
             << "* by the finished sequence as soon as it is available and~"
             << "* the accession number will be preserved."
             << "~"
             << summary.text;
    } else if ( !GetTechString(tech).empty() ) {
        text << "Method: " << GetTechString(tech) << ".";
    }

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStringForModelEvidance(const SModelEvidance& me)
{
    CNcbiOstrstream text;

    text << "MODEL " << "REFSEQ" << ":  " << "This record is predicted by "
         << "automated computational analysis. This record is derived from"
         << "an annotated genomic sequence (" << me.name << ")";
    if ( !me.method.empty() ) {
        text << " using gene prediction method: " << me.method;
    }

    if ( me.mrnaEv  ||  me.estEv ) {
        text << ", supported by ";
        if ( me.mrnaEv  &&  me.estEv ) {
            text << "mRNA and EST ";
        } else if ( me.mrnaEv ) {
            text << "mRNA ";
        } else {
            text << "EST ";
        }
        // !!! for html we need much more !!!
        text << "evidence";
    }

    text << "." << ExpandTildes("~Also see:~    ", eTilde_newline) 
         << "Documentation" 
         << ExpandTildes(" of NCBI's Annotation Process~    ", eTilde_newline);

    return CNcbiOstrstreamToString(text);
}


/***************************************************************************/
/*                                 PROTECTED                               */
/***************************************************************************/


void CCommentItem::x_GatherInfo(CFFContext& ctx)
{
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(GetObject());
    if ( desc != 0 ) {
        x_GatherDescInfo(*desc, ctx);
        return;
    }

    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(GetObject());
    if ( feat != 0 ) {
        x_GatherFeatInfo(*feat, ctx);
    }
}


void CCommentItem::x_GatherDescInfo(const CSeqdesc& desc, CFFContext& ctx)
{
    string prefix, str, suffix;
    switch ( desc.Which() ) {
    case CSeqdesc::e_Comment:
        {{
            str = desc.GetComment();
        }}
        break;

    case CSeqdesc::e_Maploc:
        {{
            const CDbtag& dbtag = desc.GetMaploc();
            if ( dbtag.CanGetTag() ) {
                const CObject_id& oid = dbtag.GetTag();
                if ( oid.IsStr() ) {
                    prefix = "Map location: ";
                    str = oid.GetStr();
                } else if ( oid.IsId()  &&  dbtag.CanGetDb() ) {
                    prefix = "Map location: (Database ";
                    str = dbtag.GetDb();
                    suffix = "; id # " + NStr::IntToString(oid.GetId()) + ").";
                }
            }
        }}
        break;

    case CSeqdesc::e_Region:
        {{
            prefix = "Region: ";
            str = desc.GetRegion();
        }}
        break;
    default:
        break;
    }

    if ( str.empty() ) {
        return;
    }
    //StringCleanup(str);
    if ( NStr::EndsWith(str, "..")  && !NStr::EndsWith(str, "...") ) {
        str.erase(str.length() - 1);
    }
    if ( !NStr::EndsWith(str, ".") ) {
        str += ".";
    }
    x_SetCommentWithURLlinks(prefix, str, suffix);
}


void CCommentItem::x_GatherFeatInfo(const CSeq_feat& feat, CFFContext& ctx)
{
}


void CCommentItem::x_SetSkip(void)
{
    CFlatItem::x_SetSkip();
    swap(m_First, sm_FirstComment);
}


void CCommentItem::x_SetCommentWithURLlinks
(const string& prefix,
 const string& str,
 const string& suffix)
{
    // !!! test for html - find links within the comment string
    m_Comment = ExpandTildes(prefix + str + suffix, eTilde_newline);
}


/////////////////////////////////////////////////////////////////////////////
//
// Derived Classes

// CGenomeAnnotComment

CGenomeAnnotComment::CGenomeAnnotComment
(CFFContext& ctx,
 const string& build_num) :
    CCommentItem(ctx), m_GenomeBuildNumber(build_num)
{
    x_GatherInfo(ctx);
}


void CGenomeAnnotComment::x_GatherInfo(CFFContext& ctx)
{
    CNcbiOstrstream text;

    text << "GENOME ANNOTATION " << "REFSEQ" << ":  ";
    if ( !m_GenomeBuildNumber.empty() ) {
         text << "Features on this sequence have been produced for build "
              << m_GenomeBuildNumber << " of the NCBI's genome annotation"
              << " [see " << "documentation" << "].";
    } else {
        text << "NCBI contigs are derived from assembled genomic sequence data."
             << "~Also see:~    " << "Documentation" 
             << " of NCBI's Annotation Process~    ";
    }
    
    x_SetComment(ExpandTildes(CNcbiOstrstreamToString(text), eTilde_newline));
}


// CHistComment

CHistComment::CHistComment
(EType type,
 const CSeq_hist& hist,
 CFFContext& ctx) : 
    CCommentItem(ctx), m_Type(type), m_Hist(&hist)
{
    x_GatherInfo(ctx);
    m_Hist.Reset();
}


string s_CreateHistCommentString
(const string& prefix,
 const string& suffix,
 int gi,
 const CSeq_hist_rec& hist)
{
    _ASSERT(hist.CanGetDate());
    _ASSERT(hist.CanGetIds());

    string date;
    hist.GetDate().GetDate(&date, "%3N %2D, %4Y");

    vector<int> gis;
    ITERATE (CSeq_hist_rec::TIds, id, hist.GetIds()) {
        if ( (*id)->IsGi() ) {
            gis.push_back((*id)->GetGi());
        }
    }

    CNcbiOstrstream text;

    text << prefix << ((gis.size() > 1) ? " or before " : " ") << date 
         << ' ' << suffix;

    if ( gis.empty() ) {
        text << " gi:?";
        return CNcbiOstrstreamToString(text);
    }

    for ( size_t count = 0; count < gis.size(); ++count ) {
        if ( count != 0 ) {
            text << ",";
        }
        text << " gi:" << NStr::IntToString(gis[count]);
    }
    text << '.';

    return CNcbiOstrstreamToString(text);
}

void CHistComment::x_GatherInfo(CFFContext& ctx)
{
    _ASSERT(m_Hist);

    switch ( m_Type ) {
    case eReplaced_by:
        x_SetComment(s_CreateHistCommentString(
            "[WARNING] On",
            "this sequence was replaced by a newer version",
            ctx.GetGI(),
            m_Hist->GetReplaced_by()));
        break;
    case eReplaces:
        x_SetComment(s_CreateHistCommentString(
            "On",
            "this sequence version replaced",
            ctx.GetGI(),
            m_Hist->GetReplaces()));
        break;
    }
}


CGsdbComment::CGsdbComment(const CDbtag& dbtag, CFFContext& ctx) :
    CCommentItem(ctx), m_Dbtag(&dbtag)
{
    x_GatherInfo(ctx);
}


void CGsdbComment::x_GatherInfo(CFFContext& ctx)
{
    x_SetComment("GSDB:S:" + m_Dbtag->GetTag().GetId());
}


void CGsdbComment::AddPeriod(void)
{
    x_GetComment() += ".";
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
* Revision 1.1  2003/12/17 20:18:17  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/

