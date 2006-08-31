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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/sgml_entity.hpp>
#include <serial/enumvalues.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
//#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const string IFlatQVal::kSpace     = " ";
const string IFlatQVal::kSemicolon = ";";
const string IFlatQVal::kComma     = ",";
const string IFlatQVal::kEOL       = "\n";


static bool s_IsNote(IFlatQVal::TFlags flags, CBioseqContext& ctx)
{
    return (flags & IFlatQVal::fIsNote)  &&  !ctx.Config().IsModeDump();
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


static string s_GetGOText(const CUser_field& field, bool is_ftable)
{
    const string* text_string = NULL,
                * evidence = NULL,
                * go_id = NULL;
    string temp;
    int pmid = 0;
    
    

    ITERATE (CUser_field::C_Data::TFields, it, field.GetData().GetFields()) {
        if ( !(*it)->IsSetLabel()  ||  !(*it)->GetLabel().IsStr() ) {
            continue;
        }
        
        const string& label = (*it)->GetLabel().GetStr();
        const CUser_field::C_Data& data = (*it)->GetData();
        
        if (data.IsStr()) {
            if (label == "text string") {
                text_string = &data.GetStr();
            } else if (label == "go id") {
                go_id = &data.GetStr();
            } else if (label == "evidence") {
                evidence = &data.GetStr();
            }
        } else if (data.IsInt()) {
            if (label == "go id") {
                NStr::IntToString(temp, data.GetInt());
                go_id = &temp;
            } else if (label == "pubmed id") {
                pmid = data.GetInt();
            }
        }
    }
    
    string go_text;
    
    if (text_string != NULL) {
        go_text = *text_string;
    }
    if ( is_ftable ) {
        go_text += '|';
        if (go_id != NULL) {
            go_text += *go_id;
        }
        go_text += '|';
        if ( pmid != 0 ) {
            go_text +=  NStr::IntToString(pmid);
        }
        if (evidence != NULL) {
            go_text += '|';
            go_text += *evidence;
        }
    } else { 
        if (go_id != NULL) {
            go_text += " [goid ";
            go_text += *go_id;
            go_text += ']';
            if (evidence != NULL) {
                go_text += " [evidence ";
                go_text += *evidence;
                go_text += ']';
            }
            if ( pmid != 0 ) {
                go_text += " [pmid ";
                go_text += NStr::IntToString(pmid);
                go_text += ']';
            }
        }
    }
    NStr::TruncateSpacesInPlace(go_text);
    return go_text;
}


static void s_ReplaceUforT(string& codon)
{
    NON_CONST_ITERATE (string, base, codon) {
        if (*base == 'T') {
            *base = 'U';
        }
    }
}


static char s_MakeDegenerateBase(const string &str1, const string& str2)
{
    static const char kIdxToSymbol[] = "?ACMGRSVUWYHKDBN";
    
    vector<char> symbol_to_idx(256, '\0');
    for (size_t i = 0; i < sizeof(kIdxToSymbol) - 1; ++i) {
        symbol_to_idx[kIdxToSymbol[i]] = i;
    }

    size_t idx = symbol_to_idx[str1[2]] | symbol_to_idx[str2[2]];
    return kIdxToSymbol[idx];
}


static size_t s_ComposeCodonRecognizedStr(const CTrna_ext& trna, string& recognized)
{
    recognized.erase();

    if (!trna.IsSetCodon()) {
        return 0;
    }

    list<string> codons;
    
    ITERATE (CTrna_ext::TCodon, it, trna.GetCodon()) {
        string codon = CGen_code_table::IndexToCodon(*it);
        s_ReplaceUforT(codon);
        if (!codon.empty()) {
            codons.push_back(codon);
        }
    }
    if (codons.empty()) {
        return 0;
    }
    size_t size = codons.size();
    if (size > 1) {
        codons.sort();

        list<string>::iterator it = codons.begin();
        list<string>::iterator prev = it++;
        while (it != codons.end()) {
            string& codon1 = *prev;
            string& codon2 = *it;
            if (codon1[0] == codon2[0]  &&  codon1[1] == codon2[1]) {
                codon1[2] = s_MakeDegenerateBase(codon1, codon2);
                it = codons.erase(it);
            } else {
                prev = it;
                ++it;
            }
        }
    }

    recognized = NStr::Join(codons, ", ");
    return size;
}


/////////////////////////////////////////////////////////////////////////////
// CFormatQual - low-level formatted qualifier

CFormatQual::CFormatQual
(const string& name,
 const string& value, 
 const string& prefix,
 const string& suffix,
 TStyle style) :
    m_Name(name), m_Value(value), m_Prefix(prefix), m_Suffix(suffix),
    m_Style(style), m_AddPeriod(false)
{
    NStr::TruncateSpacesInPlace(m_Value, NStr::eTrunc_End);
}


CFormatQual::CFormatQual(const string& name, const string& value, TStyle style) :
    m_Name(name), m_Value(value), m_Prefix(" "), m_Suffix(kEmptyStr),
    m_Style(style), m_AddPeriod(false)
{
    NStr::TruncateSpacesInPlace(m_Value, NStr::eTrunc_End);
}


// === CFlatStringQVal ======================================================

CFlatStringQVal::CFlatStringQVal(const string& value, TStyle style)
    :  IFlatQVal(&kSpace, &kSemicolon),
       m_Value(value), m_Style(style), m_AddPeriod(0)
{
	MakeLegalFlatFileString( m_Value );
    NStr::TruncateSpacesInPlace(m_Value);
}


CFlatStringQVal::CFlatStringQVal
(const string& value,
 const string& pfx,
 const string& sfx,
 TStyle style)
    :   IFlatQVal(&pfx, &sfx),
        m_Value(value),
        m_Style(style), m_AddPeriod(0)
{
	MakeLegalFlatFileString( m_Value );
    NStr::TruncateSpacesInPlace(m_Value);
}


void CFlatStringQVal::Format(TFlatQuals& q, const string& name,
                           CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    flags |= m_AddPeriod;

    ETildeStyle tilde_style = (name == "seqfeat_note" ? eTilde_newline : eTilde_space);
    ExpandTildes(m_Value, tilde_style);
                
    TFlatQual qual = x_AddFQ(q, (s_IsNote(flags, ctx) ? "note" : name), m_Value, m_Style);
    if ((flags & fAddPeriod)  &&  qual) {
        qual->SetAddPeriod();
    }
}


// === CFlatNumberQVal ======================================================


void CFlatNumberQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (ctx.Config().CheckQualSyntax()) {
        if (NStr::IsBlank(m_Value)) {
            return;
        }
        bool has_space = false;
        ITERATE (string, it, m_Value) {
            if (isspace((unsigned char)(*it))) {
                has_space = true;
            } else if (has_space) {
                // non-space after space
                return;
            }
        }
    }

    CFlatStringQVal::Format(quals, name, ctx, flags);
}


// === CFlatBondQVal ========================================================

void CFlatBondQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    string value = m_Value;
    if (s_IsNote(flags, ctx)) {
        value += " bond";
    }
    x_AddFQ(quals, (s_IsNote(flags, ctx) ? "note" : name), value, m_Style);
}


// === CFlatGeneQVal ========================================================

void CFlatGeneQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (ctx.IsJournalScan()) {
        Sgml2Ascii(m_Value);
    }
    CFlatStringQVal::Format(quals, name, ctx, flags);
}


// === CFlatSiteQVal ========================================================

void CFlatSiteQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (s_IsNote(flags, ctx)) {
        m_Value += " site";
    }
    CFlatStringQVal::Format(quals, name, ctx, flags);
}


// === CFlatStringListQVal ==================================================


void CFlatStringListQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if (m_Value.empty()) {
        return;
    }

    if ( s_IsNote(flags, ctx) ) {
        m_Suffix = &kSemicolon;
    }

    x_AddFQ(q, 
            (s_IsNote(flags, ctx) ? "note" : name),
            JoinString(m_Value, "; "),
            m_Style);
}


// === CFlatGeneSynonymsQVal ================================================

void CFlatGeneSynonymsQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if (GetValue().empty()) {
        return;
    }
    string qual, syns;
    if ( ! ctx.Config().GeneSynsToNote() ) {
        qual = "synonym";
        syns += NStr::Join(GetValue(), ", ");
    }
    else {
        qual = "note";
        syns = ( GetValue().size() == 1 ? "synonym: " : "synonyms: " );
        syns += NStr::Join(GetValue(), ", ");
    }
    x_AddFQ(q, qual, syns, m_Style);
    m_Suffix = &kSemicolon;
}


// === CFlatCodeBreakQVal ===================================================

void CFlatCodeBreakQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    static const char* kOTHER = "OTHER";

    ITERATE (CCdregion::TCode_break, it, m_Value) {
        string pos = CFlatSeqLoc((*it)->GetLoc(), ctx).GetString();
        const char* aa  = kOTHER;
        switch ((*it)->GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa = GetAAName((*it)->GetAa().GetNcbieaa(), true);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa = GetAAName((*it)->GetAa().GetNcbi8aa(), false);
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa = GetAAName((*it)->GetAa().GetNcbistdaa(), false);
            break;
        default:
            return;
        }
        x_AddFQ(q, name, "(pos:" + pos + ",aa:" + aa + ')', 
            CFormatQual::eUnquoted);
    }
}


CFlatCodonQVal::CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii)
    : m_Codon(CGen_code_table::IndexToCodon(codon)),
      m_AA(GetAAName(aa, is_ascii)), m_Checked(true)
{
}


void CFlatCodonQVal::Format(TFlatQuals& q, const string& name, CBioseqContext& ctx,
                          IFlatQVal::TFlags) const
{
    if ( !m_Checked ) {
        // ...
    }
    x_AddFQ(q, name, "(seq:\"" + m_Codon + "\",aa:" + m_AA + ')');
}


void CFlatExperimentQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext&, IFlatQVal::TFlags) const
{
    const char* s = "experimental evidence, no additional details recorded";
    x_AddFQ(q, name, s, CFormatQual::eQuoted);
}


CFlatInferenceQVal::CFlatInferenceQVal( const string& gbValue ) :
    m_str( "non-experimental evidence, no additional details recorded" )
{
    //
    //  the initial "non-experimental ..." is just a default value which may be
    //  overridden through an additional "inference" Gb-qual in the ASN.1.
    //  However, it can't be overriden to be just anything, only certain strings
    //  are allowed.
    //  The following code will change m_str from its default if gbValue is a
    //  legal replacement for "non-experimental ...", and leave it alone 
    //  otherwise.
    //
    const char* legalPrefixes[] = { 
        "similar to sequence",
        "similar to AA sequence",
        "similar to DNA sequence",
        "similar to RNA sequence",
        "similar to RNA sequence, mRNA",
        "similar to RNA sequence, EST",
        "similar to RNA sequence, other RNA",
        "profile",
        "nucleotide motif",
        "protein motif",
        "ab initio prediction",
        0
    };
    for ( size_t i=0; legalPrefixes[i] != 0; ++i ) {
        if ( NStr::StartsWith( gbValue, legalPrefixes[i] ) ) {
            m_str = gbValue;
        }
    }
}


void CFlatInferenceQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext&, IFlatQVal::TFlags) const
{
    x_AddFQ(q, name, m_str, CFormatQual::eQuoted);
}


void CFlatIllegalQVal::Format(TFlatQuals& q, const string&, CBioseqContext &ctx,
                            IFlatQVal::TFlags) const
{
    // XXX - return if too strict
    x_AddFQ(q, m_Value->GetQual(), m_Value->GetVal());
}


void CFlatMolTypeQVal::Format(TFlatQuals& q, const string& name,
                            CBioseqContext& ctx, IFlatQVal::TFlags flags) const
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
    case CMolInfo::eBiomol_pre_RNA:  s = "pre-RNA";   break;
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
                           CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    TFlatQual qual;

    string subname = m_Value->GetSubname();
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    if ( subname.empty() ) {
        return;
    }
    ConvertQuotes(subname);
    
    if (s_IsNote(flags, ctx)) {
        bool add_period = RemovePeriodFromEnd(subname, true);
        if (!subname.empty()) {
            bool is_src_orgmod_note = 
                (flags & IFlatQVal::fIsSource)  &&  (name == "orgmod_note");
            if (is_src_orgmod_note) {
                if (add_period) {
                    AddPeriod(subname);
                }
                m_Suffix = &kEOL;
                qual = x_AddFQ(q, "note", subname);
            } else {
                qual = x_AddFQ(q, "note", name + ": " + subname);
            }
            if (add_period  &&  qual) {
                qual->SetAddPeriod();
            }
        }
    } else {
        x_AddFQ(q, name, subname);
    }
}


void CFlatOrganelleQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext&, IFlatQVal::TFlags) const
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
        x_AddFQ(q, organelle, kEmptyStr, CFormatQual::eEmpty);
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
                           CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    ITERATE (vector< CRef<CReferenceItem> >, it, ctx.GetReferences()) {
        if ((*it)->Matches(*m_Value)) {
            x_AddFQ(q, name, '[' + NStr::IntToString((*it)->GetSerial()) + ']',
                    CFormatQual::eUnquoted);
        }
    }
}


void CFlatSeqIdQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    // XXX - add link in HTML mode
    string id_str;
    if ( m_Value->IsGi() ) {
        if ( m_GiPrefix ) {
            id_str = "GI:";
        }
        m_Value->GetLabel(&id_str, CSeq_id::eContent);
    } else {
        id_str = m_Value->GetSeqIdString(true);
    }
    x_AddFQ(q, name, id_str);
}


void s_ConvertGtLt(string& subname)
{
    SIZE_TYPE pos;
    for (pos = subname.find('<'); pos != NPOS; pos = subname.find('<', pos)) {
        subname.replace(pos, 1, "&lt");
    }
    for (pos = subname.find('>'); pos != NPOS; pos = subname.find('>', pos)) {
        subname.replace(pos, 1, "&gt");
    }
}


void CFlatSubSourceQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    TFlatQual qual;
    string subname = m_Value->CanGetName() ? m_Value->GetName() : kEmptyStr;
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    ConvertQuotes(subname);
    if (ctx.Config().DoHTML()) {
        s_ConvertGtLt(subname);
    }

    if ( s_IsNote(flags, ctx) ) {
        bool add_period = RemovePeriodFromEnd(subname, true);
        if (!subname.empty()) {
            bool is_subsource_note =
                m_Value->GetSubtype() == CSubSource::eSubtype_other;
            if (is_subsource_note) {
                if (add_period) {
                    AddPeriod(subname);
                }
                m_Suffix = &kEOL;
                qual = x_AddFQ(q, "note", subname);
            } else {
                qual = x_AddFQ(q, "note", name + ": " + subname);        
            }
            if (add_period  &&  qual) {
                qual->SetAddPeriod();
            }
        }
    } else {
        CSubSource::TSubtype subtype = m_Value->GetSubtype();
        if ( subtype == CSubSource::eSubtype_germline            ||
             subtype == CSubSource::eSubtype_rearranged          ||
             subtype == CSubSource::eSubtype_transgenic          ||
             subtype == CSubSource::eSubtype_environmental_sample ) {
            x_AddFQ(q, name, kEmptyStr, CFormatQual::eEmpty);
        } else if (!subname.empty()) {
            ExpandTildes(subname, eTilde_space);
            x_AddFQ(q, name, subname);
        }
    }
}


void CFlatXrefQVal::Format(TFlatQuals& q, const string& name,
                         CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    ITERATE (TXref, it, m_Value) {
        const CDbtag& dbt = **it;
        if (!m_Quals.Empty()  &&  x_XrefInGeneXref(dbt)) {
            continue;
        }

        CDbtag::TDb db = dbt.GetDb();
        if (db == "PID"  ||  db == "GI") {
            continue;
        } else if (db == "cdd") {
            db = "CDD"; // canonicalize
        }

        if (ctx.Config().DropBadDbxref()) {
            if (!dbt.IsApproved(ctx.IsRefSeq())) {
                continue;
            }
        }

        const CDbtag::TTag& tag = (*it)->GetTag();
        string id;
        if (tag.IsId()) {
            id = NStr::IntToString(tag.GetId());
        } else if (tag.IsStr()) {        
            id = tag.GetStr();
            if (NStr::EqualNocase(db, "MGI")  ||  NStr::EqualNocase(db, "MGD")) {
                if (NStr::StartsWith(id, "MGI:", NStr::eNocase)  ||
                    NStr::StartsWith(id, "MGD:", NStr::eNocase)) {
                    db = "MGI";
                    id.erase(0, 4);
                }
            }
        }
        if (NStr::IsBlank(id)) {
            continue;
        }

        CNcbiOstrstream db_xref;
        db_xref << db << ':';
        if (ctx.Config().DoHTML()) {
            string url = dbt.GetUrl();
            if (!NStr::IsBlank(url)) {
                db_xref <<  "<a href=" <<  url << '>' << id << "</a>";
            } else {
                db_xref << id;
            }
        } else {
            db_xref << id;
        }

        x_AddFQ(q, name, CNcbiOstrstreamToString(db_xref));
    }
}


bool CFlatXrefQVal::x_XrefInGeneXref(const CDbtag& dbtag) const
{
    if ( !m_Quals->HasQual(eFQ_gene_xref) ) {
        return false;
    }

    typedef TQuals::const_iterator TQCI;
    TQCI gxref = m_Quals->LowerBound(eFQ_gene_xref);
    TQCI end = m_Quals->end();
    while (gxref != end  &&  gxref->first == eFQ_gene_xref) {
        const CFlatXrefQVal* xrefqv =
            dynamic_cast<const CFlatXrefQVal*>(gxref->second.GetPointerOrNull());
        if (xrefqv != NULL) {
            ITERATE (TXref, dbt, xrefqv->m_Value) {
                if (dbtag.Match(**dbt)) {
                    return true;
                }
            }
        }
        ++gxref;
    }
    /*pair<TQCI, TQCI> gxrefs = m_Quals->GetQuals(eFQ_gene_xref);
    for ( TQCI it = gxrefs.first; it != gxrefs.second; ++it ) {
        const CFlatXrefQVal* xrefqv =
            dynamic_cast<const CFlatXrefQVal*>(it->second.GetPointerOrNull());
        if ( xrefqv != 0 ) {
            ITERATE (TXref, dbt, xrefqv->m_Value) {
                if ( dbtag.Match(**dbt) ) {
                    return true;
                }
            }
        }
    }*/
    return false;
}


static size_t s_CountAccessions(const CUser_field& field)
{
    size_t count = 0;

    if (!field.IsSetData()  ||  !field.GetData().IsFields()) {
        return 0;
    }
    
    //
    //  Each accession consists of yet another block of "Fields" one of which carries
    //  a label named "accession":
    //
    ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
        const CUser_field& uf = **it;
        if ( uf.CanGetData()  &&  uf.GetData().IsFields() ) {
        
            ITERATE( CUser_field::TData::TFields, it2, uf.GetData().GetFields() ) {
                const CUser_field& inner = **it2;
                if ( inner.IsSetLabel() && inner.GetLabel().IsStr() ) {
                    if ( inner.GetLabel().GetStr() == "accession" ) {
                        ++count;
                    }
                }
            }
        }
    }
    return count;
}


void CFlatModelEvQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    size_t num_mrna = 0, num_prot = 0, num_est = 0;
    const string* method = 0;

    ITERATE (CUser_object::TData, it, m_Value->GetData()) {
        const CUser_field& field = **it;
        if (!field.IsSetLabel()  &&  field.GetLabel().IsStr()) {
            continue;
        }
        const string& label = field.GetLabel().GetStr();
        if (label == "Method") {
            method = &label;
            if ( field.CanGetData() && field.GetData().IsStr() ) {
                method = &( field.GetData().GetStr() );
            }
        } else if (label == "mRNA") {
            num_mrna = s_CountAccessions(field);
        } else if (label == "EST") {
            num_est  = s_CountAccessions(field);
        } else if (label == "Protein") {
            num_prot = s_CountAccessions(field);
        }
    }

    CNcbiOstrstream text;
    text << "Derived by automated computational analysis";
    if (method != NULL  &&  !NStr::IsBlank(*method)) {
         text << " using gene prediction method: " << *method;
    }
    text << ".";

    if (num_mrna > 0  ||  num_est > 0  ||  num_prot > 0) {
        text << " Supporting evidence includes similarity to:";
    }
    string prefix = " ";
    if (num_mrna > 0) {
        text << prefix << num_mrna << " mRNA";
        if (num_mrna > 1) {
            text << 's';
        }
        prefix = ", ";
    }
    if (num_est > 0) {
        text << prefix << num_est << " EST";
        if (num_est > 1) {
            text << 's';
        }
        prefix = ", ";
    }
    if (num_prot > 0) {
        text << prefix << num_prot << " Protein";
        if (num_prot > 1) {
            text << 's';
        }
    }

    x_AddFQ(q, name, CNcbiOstrstreamToString(text));
}


void CFlatGoQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    _ASSERT(m_Value->GetData().IsFields());
    bool is_ftable = ctx.Config().IsFormatFTable();

    if ( s_IsNote(flags, ctx) ) {
        static const string sfx = ";";
        m_Prefix = &kEOL;
        m_Suffix = &sfx;
        x_AddFQ(q, "note", name + ": " + s_GetGOText(*m_Value, is_ftable));
    } else {
        x_AddFQ(q, name, s_GetGOText(*m_Value, is_ftable));
    }
}


void CFlatAnticodonQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if ( !m_Anticodon->IsInt()  ||  m_Aa.empty() ) {
        return;
    }

    CNcbiOstrstream text;
    text << "(pos:" ;
    if (ctx.Config().IsModeRelease()) {
        CSeq_loc::TRange range = m_Anticodon->GetTotalRange();
        text << range.GetFrom() + 1 << ".." << range.GetTo() + 1;
    } else {
        text << CFlatSeqLoc(*m_Anticodon, ctx).GetString();
    }
    text << ",aa:" << m_Aa << ')';

    x_AddFQ(q, name, CNcbiOstrstreamToString(text), CFormatQual::eUnquoted);
}


void CFlatTrnaCodonsQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if (!m_Value  ||  !m_Value->IsSetCodon()) {
        return;
    }

    string recognized;
    size_t num = s_ComposeCodonRecognizedStr(*m_Value, recognized);
    if (num > 0) {
        if (num == 1) {
            string str = "codon recognized: " + recognized;
            if (NStr::Find(m_Seqfeat_note, str) == NPOS) {
                x_AddFQ(q, name, str);
            }
        } else {
            x_AddFQ(q, name, "codons recognized: " + recognized);
        }
    }
}


void CFlatProductNamesQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (m_Value.size() < 2) {
        return;
    }
    bool note = s_IsNote(flags, ctx);
    
    CProt_ref::TName::const_iterator it = m_Value.begin();
    ++it;  // first is used for /product
    while (it != m_Value.end()  &&  !NStr::IsBlank(*it)) {
        if (*it != m_Gene) {
            x_AddFQ(quals, (note ? "note" : name), *it);
        }
        ++it;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.38  2006/08/31 13:54:06  ludwigf
* CHANGED: In relaxed modes, turn object qualifier "syn" into a flat file
*  qualifier "/synonym" rather than a component of the "/note" qualifier.
*
* Revision 1.37  2006/08/30 13:28:50  ludwigf
* FIXED: Handling of "exp_ev" qualifier and "inference" gb-qualifier. The
*  second may be used to modify the value of the first.
*
* Revision 1.36  2006/07/24 14:34:41  ludwigf
* CHANGED: In flat file string values, embedded double quotes are now turned
*   into single quotes.
*
* Revision 1.35  2006/06/06 20:39:59  ucko
* CFlatXrefQVal::Format: canonicalize the database name "cdd" to "CDD".
*
* Revision 1.34  2006/01/26 19:54:37  ludwigf
* FIXED: User object methods would always be displayed as "Method" regardless
*  of what the ASN.1 said.
* FIXED: "Supporting evidence" in user objects would not get the count of si-
*  milar mRNAs, ESTs, and Proteins right.
* FIXED: Even with correct counts, appending a plural 's' for similarity
*  counts greater than one wasn't working right.
*
* Revision 1.33  2005/11/23 16:26:21  ludwigf
* CHANGED: Replaced calls to function "JoinNoRedund()" to call function
* "JoinString()" instead.
*
* CHANGED: Policy when turning qualifiers into /note comments:
* To paraphrase Jonathan, "If the contribution to /note comes from anything
* other than go_ qualifiers then we don't want to bother the reader with
* unnecessary redundancy. If the contribution does come from go_ qualifiers
* then we want to keep any redundancy to educate submitters about the error
* of their ways.".
*
* Revision 1.32  2005/10/26 13:30:18  ludwigf
* Removed qualifier "evidence".
* Added qualifiers "experiment" and "inference".
*
* Revision 1.31  2005/08/16 16:50:06  shomrat
* Do not convert quals to note in dump mode; Chages to ModelEv and ProductNames
*
* Revision 1.30  2005/06/03 17:00:44  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.29  2005/04/27 17:13:18  shomrat
* Changed anticodon formatting
*
* Revision 1.28  2005/04/15 12:29:41  shomrat
* Handle anticodon location on minus strand
*
* Revision 1.27  2005/03/31 14:55:14  shomrat
* Bug fix - formatting pmid in go qual
*
* Revision 1.26  2005/03/28 17:23:36  shomrat
* Changes to synonym formatting
*
* Revision 1.25  2005/02/09 14:59:39  shomrat
* Initial support for HTML output
*
* Revision 1.24  2005/01/12 17:24:26  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.23  2004/12/09 14:47:17  shomrat
* Added CFlatSiteQVal
*
* Revision 1.22  2004/11/15 20:10:49  shomrat
* Fixed formatting of string, OrgMod and SubSource quals
*
* Revision 1.21  2004/10/18 18:46:22  shomrat
* Added Gene and Bond qual classes
*
* Revision 1.20  2004/10/05 20:44:23  ucko
* s_ComposeCodonRecognizedStr: tweak to avoid string::compare, whose
* syntax has varied over time.
*
* Revision 1.19  2004/10/05 20:25:47  ucko
* s_MakeDegenerateBase: tweak initializer for symbol_to_idx to avoid
* triggering an inappropriate template with some compilers.
*
* Revision 1.18  2004/10/05 15:51:05  shomrat
* Fixed codon and orgmod formatting
*
* Revision 1.17  2004/08/20 16:28:27  shomrat
* fixed mol type for pre_RNA biomol
*
* Revision 1.16  2004/08/19 16:37:52  shomrat
* + CFlatNumberQVal and CFlatGeneSynonymsQVal
*
* Revision 1.15  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.14  2004/05/06 17:58:52  shomrat
* CFlatQual -> CFormatQual
*
* Revision 1.13  2004/04/27 15:13:16  shomrat
* fixed SubSource formatting
*
* Revision 1.12  2004/04/22 15:57:06  shomrat
* Changes in context
*
* Revision 1.11  2004/04/07 14:28:44  shomrat
* Fixed GO quals formatting for FTable format
*
* Revision 1.10  2004/03/30 20:32:53  shomrat
* Fixed go and modelev qual formatting
*
* Revision 1.9  2004/03/25 20:46:19  shomrat
* implement CFlatPubSetQVal formatting
*
* Revision 1.8  2004/03/18 15:43:27  shomrat
* Fixes to quals formatting
*
* Revision 1.7  2004/03/08 21:01:44  shomrat
* GI prefix flag for Seq-id quals
*
* Revision 1.6  2004/03/08 15:24:27  dicuccio
* FIxed dereference of string pointer
*
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
