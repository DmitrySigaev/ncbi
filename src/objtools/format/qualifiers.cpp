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
*   new (early 2003) flat-file generator -- qualifier types
*   (mainly of interest to implementors)
*
* ===========================================================================
*/
#include <corelib/ncbistd.hpp>

#include <serial/enumvalues.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// in Ncbistdaa order
static const char* kAANames[] = {
    "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
    "Lys", "Leu", "Met", "Asn", "Pro", "Glu", "Arg", "Ser", "Thr", "Val",
    "Trp", "Xaa", "Tyr", "Glx", "Sec", "TERM"
};


inline
static const char* s_AAName(unsigned char aa, bool is_ascii)
{
    if (is_ascii) {
        aa = CSeqportUtil::GetMapToIndex
            (CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa);
    }
    return (aa < sizeof(kAANames)/sizeof(*kAANames)) ? kAANames[aa] : "OTHER";
}


inline
static bool s_IsNote(CFFContext& ctx, IFlatQVal::TFlags flags)
{
    return (flags & IFlatQVal::fIsNote);
}


static bool s_StringIsJustQuotes(const string& str)
{
    ITERATE(string, it, str) {
        if ( (*it != '"')  ||  (*it != '\'') ) {
            return false;
        }
    }

    return true;
}


static string s_GetGOText(const CUser_field& field)
{
    string text_string, evidence, go_id;
    int pmid = 0;

    ITERATE (CUser_field::C_Data::TFields, it, field.GetData().GetFields()) {
        if ( !(*it)->CanGetLabel()  ||  !(*it)->GetLabel().IsStr() ) {
            continue;
        }
        
        const string& label = (*it)->GetLabel().GetStr();
        const CUser_field::C_Data& data = (*it)->GetData();
        
        if ( label == "text string"  &&  data.IsStr() ) {
            text_string = data.GetStr();
        }
        
        if ( label == "go id" ) {
            if ( data.IsStr() ) {
                go_id = data.GetStr();
            } else if ( data.IsInt() ) {
                go_id = NStr::IntToString(data.GetInt());
            }
        }
        
        if ( label == "evidence"  &&  data.IsStr() ) {
            evidence = data.GetStr();
        }
        
        if ( label == "pubmed id"  &&  data.IsInt() ) {
            pmid = data.GetInt();
        }
    }

    
    CNcbiOstrstream text;

    text << text_string;
    if ( !go_id.empty() ) {
        text << " [goid " << go_id << "]";
    }
    if ( !evidence.empty() ) {
        text << " [evidence " << evidence << "]";
    }

    if ( pmid != 0 ) {
        text << " [pmid " << pmid << "]";
    }

    return NStr::TruncateSpaces(CNcbiOstrstreamToString(text));
}


void CFlatStringQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    string sfx = m_Suffix;
    if ( sfx.empty()  &&  s_IsNote(ctx, flags) ) {
        sfx = "\n";
    }

    if ( s_IsNote(ctx, flags) ) {
        x_AddFQ(q, "note", m_Value, sfx, m_Style);
    } else {
        x_AddFQ(q, name, m_Value, sfx, m_Style);
    }
}


void CFlatStringListQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    if ( s_IsNote(ctx, flags) ) {
        x_AddFQ(q, "note", JoinNoRedund(m_Value, "; "), "; ", m_Style);
    } else {
        x_AddFQ(q, name, JoinNoRedund(m_Value, "; "), m_Style);
    }
}


void CFlatCodeBreakQVal::Format(TFlatQuals& q, const string& name,
                              CFFContext& ctx, IFlatQVal::TFlags) const
{
    ITERATE (CCdregion::TCode_break, it, m_Value) {
        string pos = CFlatSeqLoc((*it)->GetLoc(), ctx).GetString();
        string aa  = "OTHER";
        switch ((*it)->GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa = s_AAName((*it)->GetAa().GetNcbieaa(), true);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa = s_AAName((*it)->GetAa().GetNcbi8aa(), false);
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa = s_AAName((*it)->GetAa().GetNcbistdaa(), false);
            break;
        default:
            return;
        }
        x_AddFQ(q, name, "(pos:" + pos + ",aa:" + aa + ')', 
            CFlatQual::eUnquoted);
    }
}


CFlatCodonQVal::CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii)
    : m_Codon(CGen_code_table::IndexToCodon(codon)),
      m_AA(s_AAName(aa, is_ascii)), m_Checked(true)
{
}


void CFlatCodonQVal::Format(TFlatQuals& q, const string& name, CFFContext& ctx,
                          IFlatQVal::TFlags) const
{
    if ( !m_Checked ) {
        // ...
    }
    x_AddFQ(q, name, "(seq:\"" + m_Codon + "\",aa:" + m_AA + ')');
}


void CFlatExpEvQVal::Format(TFlatQuals& q, const string& name,
                          CFFContext&, IFlatQVal::TFlags) const
{
    const char* s = 0;
    switch (m_Value) {
    case CSeq_feat::eExp_ev_experimental:      s = "experimental";      break;
    case CSeq_feat::eExp_ev_not_experimental:  s = "not_experimental";  break;
    default:                                   break;
    }
    if (s) {
        x_AddFQ(q, name, s, CFlatQual::eUnquoted);
    }
}


void CFlatIllegalQVal::Format(TFlatQuals& q, const string&, CFFContext &ctx,
                            IFlatQVal::TFlags) const
{
    // XXX - return if too strict
    x_AddFQ(q, m_Value->GetQual(), m_Value->GetVal());
}


void CFlatMolTypeQVal::Format(TFlatQuals& q, const string& name,
                            CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    const char* s = 0;
    switch ( m_Biomol ) {
    case CMolInfo::eBiomol_unknown:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  s = "unassigned DNA"; break;
        case CSeq_inst::eMol_rna:  s = "unassigned RNA"; break;
        default:                   break;
        }
        break;
    case CMolInfo::eBiomol_genomic:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  s = "genomic DNA";  break;
        case CSeq_inst::eMol_rna:  s = "genomic RNA";  break;
        default:                   break;
        }
        break;
    case CMolInfo::eBiomol_pre_RNA:  s = "pre-mRNA";  break;
    case CMolInfo::eBiomol_mRNA:     s = "mRNA";      break;
    case CMolInfo::eBiomol_rRNA:     s = "rRNA";      break;
    case CMolInfo::eBiomol_tRNA:     s = "tRNA";      break;
    case CMolInfo::eBiomol_snRNA:    s = "snRNA";     break;
    case CMolInfo::eBiomol_scRNA:    s = "scRNA";     break;
    case CMolInfo::eBiomol_other_genetic:
    case CMolInfo::eBiomol_other:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  s = "other DNA";  break;
        case CSeq_inst::eMol_rna:  s = "other RNA";  break;
        default:                   break;
        }
        break;
    case CMolInfo::eBiomol_cRNA:
    case CMolInfo::eBiomol_transcribed_RNA:  s = "other RNA";  break;
    case CMolInfo::eBiomol_snoRNA:           s = "snoRNA";     break;
    }

    if ( s == 0 ) {
        switch ( m_Mol ) {
        case CSeq_inst::eMol_rna:
            s = "unassigned RNA";
            break;
        case CSeq_inst::eMol_aa:
            s = 0;
            break;
        case CSeq_inst::eMol_dna:
        default:
            s = "unassigned DNA";
            break;
        }
    }

    if ( s != 0 ) {
        x_AddFQ(q, name, s);
    }
}


void CFlatOrgModQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    string subname = m_Value->GetSubname();
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    if ( subname.empty() ) {
        return;
    }

    if ( s_IsNote(ctx, flags) ) {
        string delim;
        if ( m_Value->GetSubtype() == COrgMod::eSubtype_other ) {
            delim = "\n";
        } else {
            delim = "; ";
        }

        x_AddFQ(q, "note", name + ": " + subname, delim);
    } else {
        x_AddFQ(q, name, subname);
    }
}


void CFlatOrganelleQVal::Format(TFlatQuals& q, const string& name,
                              CFFContext&, IFlatQVal::TFlags) const
{
    const string& organelle
        = CBioSource::GetTypeInfo_enum_EGenome()->FindName(m_Value, true);

    switch (m_Value) {
    case CBioSource::eGenome_chloroplast: case CBioSource::eGenome_chromoplast:
    case CBioSource::eGenome_cyanelle:    case CBioSource::eGenome_apicoplast:
    case CBioSource::eGenome_leucoplast:  case CBioSource::eGenome_proplastid:
        x_AddFQ(q, name, "plastid:" + organelle);
        break;

    case CBioSource::eGenome_kinetoplast:
        x_AddFQ(q, name, "mitochondrion:kinetoplast");
        break;

    case CBioSource::eGenome_mitochondrion: case CBioSource::eGenome_plastid:
    case CBioSource::eGenome_nucleomorph:
        x_AddFQ(q, name, organelle);
        break;

    case CBioSource::eGenome_macronuclear: case CBioSource::eGenome_proviral:
    case CBioSource::eGenome_virion:
        x_AddFQ(q, organelle, kEmptyStr, CFlatQual::eEmpty);
        break;

    case CBioSource::eGenome_plasmid: case CBioSource::eGenome_transposon:
        x_AddFQ(q, organelle, kEmptyStr);
        break;

    case CBioSource::eGenome_insertion_seq:
        x_AddFQ(q, "insertion_seq", kEmptyStr);
        break;

    default:
        break;
    }
    
}


void CFlatPubSetQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags) const
{
    /* !!!
    bool found = false;
    ITERATE (vector<CRef<CFlatReference> >, it, ctx.GetReferences()) {
        if ((*it)->Matches(*m_Value)) {
            x_AddFQ(q, name, '[' + NStr::IntToString((*it)->GetSerial()) + ']',
                    CFlatQual::eUnquoted);
            found = true;
        }
    }
    // complain if not found?
    */
}

/*
void CFlatTranslationQVal::Format(TFlatQuals& q, const string& name,
                            CFFContext& ctx, IFlatQVal::TFlags) const
{
    string s;
    CSeqVector v = ctx.GetHandle().GetScope().GetBioseqHandle(*m_Value)
        .GetSequenceView(*m_Value, CBioseq_Handle::eViewConstructed,
                         CBioseq_Handle::eCoding_Iupac);
    v.GetSeqData(0, v.size(), s);
    x_AddFQ(q, name, s);
}
*/

void CFlatSeqIdQVal::Format(TFlatQuals& q, const string& name,
                          CFFContext& ctx, IFlatQVal::TFlags) const
{
    // XXX - add link in HTML mode
    string id_str;
    if ( m_Value->IsGi() ) {
        id_str = "GI:";
        m_Value->GetLabel(&id_str, CSeq_id::eContent);
    } else {
        id_str = m_Value->GetSeqIdString(true);
    }
    x_AddFQ(q, name, id_str);
}


void CFlatSubSourceQVal::Format(TFlatQuals& q, const string& name,
                              CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    if ( !m_Value->CanGetName()  ||  m_Value->GetName().empty() ) {
        return;
    }

    string subname = m_Value->GetName();
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }

    if ( s_IsNote(ctx, flags) ) {
        string delim;
        bool note = false;
        if ( m_Value->GetSubtype() == CSubSource::eSubtype_other ) {
            delim = "\n";
            note = true;
        } else {
            delim = "; ";
        }

        x_AddFQ(q, "note", note ? subname : name + ": " + subname, delim);
    } else {
        CSubSource::TSubtype subtype = m_Value->GetSubtype();
        if ( subtype == CSubSource::eSubtype_germline            ||
             subtype == CSubSource::eSubtype_rearranged          ||
             subtype == CSubSource::eSubtype_transgenic          ||
             subtype == CSubSource::eSubtype_environmental_sample ) {
            x_AddFQ(q, name, kEmptyStr, CFlatQual::eEmpty);
        } else {
            x_AddFQ(q, name, subname);
        }
    }
}


void CFlatXrefQVal::Format(TFlatQuals& q, const string& name,
                         CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    // XXX - add link in HTML mode?
    ITERATE (TXref, it, m_Value) {
        string s((*it)->GetDb());
        const CObject_id& id = (*it)->GetTag();
        switch (id.Which()) {
        case CObject_id::e_Id: s += ':' + NStr::IntToString(id.GetId()); break;
        case CObject_id::e_Str: s += ':' + id.GetStr(); break;
        default: break; // complain?
        }
        x_AddFQ(q, name, s);
    }
}


void CFlatModelEvQVal::Format
(TFlatQuals& q,
 const string& name,
 CFFContext& ctx,
 IFlatQVal::TFlags flags) const
{
    const string* method = 0;
    if ( m_Value->HasField("Method") ) {
        const CUser_field& uf = m_Value->GetField("Method");
        if ( uf.GetData().IsStr() ) {
            method = &uf.GetData().GetStr();
        }
    }

    size_t nm = 0;
    if ( m_Value->HasField("mRNA") ) {
        const CUser_field& uf = m_Value->GetField("mRNA");
        if ( uf.GetData().IsFields() ) {
            ITERATE (CUser_field::C_Data::TFields, it, uf.GetData().GetFields()) {
                if ( (*it)->CanGetLabel()  &&  (*it)->GetData().IsStr()  &&
                     (*it)->GetData().GetStr() == "accession" ) {
                    ++nm;
                }
            }
        }
    }

    CNcbiOstrstream text;
    text << "Derived by automated computational analysis";
    if ( method != 0  &&  !method->empty() ) {
         text << " using gene prediction method: " << method;
    }
    text << ".";

    if ( nm > 0 ) {
        text << " Supporting evidence includes similarity to: " << nm << " mRNA";
        if ( nm > 1 ) {
            text << "s";
        }
    }

    x_AddFQ(q, name, CNcbiOstrstreamToString(text));
}


void CFlatGoQVal::Format
(TFlatQuals& q,
 const string& name,
 CFFContext& ctx,
 IFlatQVal::TFlags flags) const
{
    _ASSERT(m_Value->GetData().IsFields());
    x_AddFQ(q, name, s_GetGOText(*m_Value));
}


void CFlatAnticodonQVal::Format
(TFlatQuals& q,
 const string& name,
 CFFContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if ( !m_Anticodon->IsInt()  ||  !m_Aa.empty() ) {
        return;
    }

    CSeq_loc::TRange range = m_Anticodon->GetTotalRange();
    CNcbiOstrstream text;
    text << "(pos:" << range.GetFrom() + 1 << ".." << range.GetTo() + 1
         << ",aa:" << m_Aa << ")";

    x_AddFQ(q, name, CNcbiOstrstreamToString(text), CFlatQual::eUnquoted);
}


void CFlatTrnaCodonsQVal::Format
(TFlatQuals& q,
 const string& name,
 CFFContext& ctx,
 IFlatQVal::TFlags flags) const
{
    // !!!
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/03/05 18:47:32  shomrat
* Added qualifier classes
*
* Revision 1.4  2004/02/11 16:55:38  shomrat
* changes in formatting of OrgMod and SubSource quals
*
* Revision 1.3  2004/01/14 16:18:07  shomrat
* uncomment code
*
* Revision 1.2  2003/12/18 17:43:35  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:24:04  shomrat
* Initial Revision (adapted from flat lib)
*
* Revision 1.4  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.3  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
