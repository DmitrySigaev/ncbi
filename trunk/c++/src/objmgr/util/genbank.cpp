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
* Author:  Aaron Ucko
*
* File Description:
*   Code to write Genbank/Genpept flat-file records.
*/

#include <objects/util/genbank.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/seq_vector.hpp>

#include <algorithm>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/serialimpl.hpp>
#include <serial/stltypes.hpp>

#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Heterogen.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/gencode.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Txinit.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include "gbqual.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFeature
{
public:
    CFeature(const CSeq_feat& asn) : m_ASN(asn) {}

    typedef set<CGBQual> TQuals;
    const TQuals& GetQuals(void);
    void          AddQual(CGBQual::EType type, string value = kEmptyStr);
private:
    const CSeq_feat& m_ASN;
    TQuals           m_Quals;
};

inline
const CFeature::TQuals& CFeature::GetQuals(void)
{
    return m_Quals;
}


void CFeature::AddQual(CGBQual::EType type, string value)
{
    // Tweak qualifier type according to feature type to ensure proper
    // ordering.  Could also check appropriateness.

    CSeqFeatData::E_Choice asn_type = m_ASN.GetData().Which();

    switch (type) {
    case CGBQual::eType_product:
        if (asn_type == CSeqFeatData::e_Cdregion) {
            type = CGBQual::eType_cds_product;
#if 0
        } else if (asn_type == CSeqFeatData::e_Prot) {
            type = CGBQual::eType_prot_name;
#endif
        }
        break;

    case CGBQual::eType_allele:
        if (asn_type == CSeqFeatData::e_Gene) {
            type = CGBQual::eType_gene_allele;
        }
        break;

    case CGBQual::eType_map:
        if (asn_type == CSeqFeatData::e_Gene) {
            type = CGBQual::eType_gene_map;
        }
        break;

    case CGBQual::eType_db_xref:
        if (asn_type == CSeqFeatData::e_Gene) {
            type = CGBQual::eType_gene_xref;
        }
        break;

    case CGBQual::eType_EC_number:
        if (asn_type == CSeqFeatData::e_Prot) {
            type = CGBQual::eType_prot_EC_number;
        }
        break;

    default: // make compiler happy
        break;
    }

    m_Quals.insert(CGBQual(type, value));
}


typedef set<CGBSQual> TGBSQuals;

static void s_AddProteinQualifiers(CFeature &feature, const CProt_ref& prot);
static TGBSQuals s_SourceQualifiers(const COrg_ref& org);
static TGBSQuals s_SourceQualifiers(const CBioSource& source);

static string s_Pad(const string& s, SIZE_TYPE width)
{
    if (s.size() >= width) {
        return s;
    } else {
        return s + string(width - s.size(), ' ');
    }
}


const unsigned int CGenbankWriter::sm_KeywordWidth = 12;
const unsigned int CGenbankWriter::sm_LineWidth = 80;
const unsigned int CGenbankWriter::sm_DataWidth
= CGenbankWriter::sm_LineWidth - CGenbankWriter::sm_KeywordWidth;
const unsigned int CGenbankWriter::sm_FeatureNameIndent = 5;
const unsigned int CGenbankWriter::sm_FeatureNameWidth = 16;


bool CGenbankWriter::Write(const CSeq_entry& entry)
{
    typedef bool (CGenbankWriter::*TFun)(const CBioseq_Handle&);
    static const TFun funlist[]
        = { &CGenbankWriter::WriteLocus,
            &CGenbankWriter::WriteDefinition,
            &CGenbankWriter::WriteAccession,
            &CGenbankWriter::WriteID,
            &CGenbankWriter::WriteVersion,
            &CGenbankWriter::WriteDBSource,
            &CGenbankWriter::WriteKeywords,
            &CGenbankWriter::WriteSegment,
            &CGenbankWriter::WriteSource,
            &CGenbankWriter::WriteReference,
            &CGenbankWriter::WriteComment,
            &CGenbankWriter::WriteFeatures,
            &CGenbankWriter::WriteSequence,
            NULL};

    if (entry.IsSet()) {
        const CBioseq_set& s = entry.GetSet();
        iterate(CBioseq_set::TSeq_set, it, s.GetSeq_set()) {
            if (!Write(**it)) {
                return false;
            }
        }
        return true;
    } else {
        const CBioseq& seq = entry.GetSeq();
        if (seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
            return true; // don't bother with dummy entry for whole sequence
        }

        {{
            bool type_okay = false;
            switch (seq.GetInst().GetMol()) {
            case CSeq_inst::eMol_dna:
            case CSeq_inst::eMol_rna:
            case CSeq_inst::eMol_na:
                if (m_Format == eFormat_Genbank)
                    type_okay = true;
                break;
            case CSeq_inst::eMol_aa:
                if (m_Format == eFormat_Genpept)
                    type_okay = true;
                break;
            default:
                break; // stays false
            }
            if (!type_okay) {
                return true; // succeeded, albeit with no output
            }
        }}

        CBioseq_Handle handle = m_Scope.GetBioseqHandle(*seq.GetId().front());
        for (unsigned int i = 0;  funlist[i];  ++i) {
            if (!(this->*funlist[i])(handle)) {
                return false;
            }
        }
        m_Stream << "//" << NcbiEndl;
        return true;
    }
}


void CGenbankWriter::SetParameters(void)
{
    switch (m_Version) {
    case eVersion_pre127:
        m_LocusNameWidth = 9;
        m_SequenceLenWidth = 7;
        m_MoleculeTypeWidth = 5;
        m_TopologyWidth = 9;
        m_DivisionWidth = 9;
        m_GenpeptBlanks = 20; // 33-52
        break;
    case eVersion_127:
        m_LocusNameWidth = 16;
        m_SequenceLenWidth = 11;
        m_MoleculeTypeWidth = 7;
        m_TopologyWidth = 8;
        m_DivisionWidth = 3;
        m_GenpeptBlanks = 21; // 44-64
        break;
    }
    switch (m_Format) {
    case eFormat_Genbank:
        m_LengthUnit = "bases";
        break;
    case eFormat_Genpept:
        m_LengthUnit = "residues";
        break;
    }
}


static string s_FormatDate(const CDate& date) {
    if (date.IsStr()) {
        return date.GetStr();
    }

    // otherwise standard
    const CDate_std& std = date.GetStd();

    static const char kMonthNames[13][4]
        = {"000", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG",
           "SEP", "OCT", "NOV", "DEC" };
    CNcbiOstrstream oss;

    oss << setfill('0') << setw(2) << std.GetDay() << '-'
        << kMonthNames[std.GetMonth()] << '-' << setw(4) << std.GetYear();

    return CNcbiOstrstreamToString(oss);
}


static string s_StripDot(const string& s) {
    // removes a trailing period if present
    if (s.empty()  ||  s[s.size() - 1] != '.')
        return s;
    else
        return s.substr(0, s.size() - 1);
}


bool CGenbankWriter::WriteLocus(const CBioseq_Handle& handle)
{
    const CBioseq& seq = handle.GetBioseq();
    const CSeq_inst& inst = seq.GetInst();

    {{
        string name;
        CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);
        for ( ; it;  ++it) {
            if (it->IsSetName()) {
                name = it->GetName();
            } else {
                name = it->GetAccession();
            }
            BREAK(it);
        }
        m_Stream << s_Pad("LOCUS", sm_KeywordWidth)
                 << s_Pad(name, m_LocusNameWidth) << ' '
                 << setw(m_SequenceLenWidth) << inst.GetLength() << ' ';
    }}
    switch (m_Format) {
    case eFormat_Genbank:
        m_Stream << "bp ";
        {{
            CSeq_inst::TStrand strand = CSeq_inst::eStrand_not_set;
            if (inst.IsSetStrand()) {
                strand = inst.GetStrand();
            }
#ifdef SUPPLY_DEFAULT_STRAND_TYPE
            if (strand == CSeq_inst::eStrand_not_set) {
                if (inst.GetMol() == CSeq_inst::eMol_dna) {
                    strand = CSeq_inst::eStrand_ds;
                } else {
                    strand = CSeq_inst::eStrand_ss;
                }
            }
#endif
            switch (strand) {
            case CSeq_inst::eStrand_ss:     m_Stream << "ss-";  break;
            case CSeq_inst::eStrand_ds:     m_Stream << "ds-";  break;
            case CSeq_inst::eStrand_mixed:  m_Stream << "ms-";  break;
            default:                        m_Stream << "   ";  break;
            }
        }}                

        {{
            const char* moltype = "";
            switch (inst.GetMol()) {
            case CSeq_inst::eMol_dna:
                moltype = "DNA";
                break;
            case CSeq_inst::eMol_rna:
                for (CTypeConstIterator<CMolInfo> it=ConstBegin(seq);
                     it; ++it) {
                    switch (it->GetBiomol()) {
                    case CMolInfo::eBiomol_tRNA:
                        moltype = "tRNA";
                        break;
                    case CMolInfo::eBiomol_rRNA:
                        moltype = "rRNA";
                        break;
                    case CMolInfo::eBiomol_mRNA:
                    case CMolInfo::eBiomol_genomic_mRNA: // ?
                        moltype = "mRNA";
                        break;
                    case CMolInfo::eBiomol_snRNA:
                        moltype = "snRNA";
                        break;
                    case CMolInfo::eBiomol_scRNA:
                        moltype = "scRNA";
                        break;
                    case CMolInfo::eBiomol_snoRNA:
                        if (m_Version >= eVersion_127) {
                            moltype = "snoRNA";
                            break;
                        } // otherwise fall through
                    default: // Why do we never yield "uRNA"?
                        moltype = "RNA";
                        break;
                    }
                    break;
                }
                break;
            default: // stays blank
                break;
            }
            m_Stream << s_Pad(moltype, m_MoleculeTypeWidth) << ' ';
        }}

        {{
            const char* topology = "";
            switch (inst.GetTopology()) {
            case CSeq_inst::eTopology_circular:
                topology = "circular";
                break;
            case CSeq_inst::eTopology_linear:
                if (m_Version >= eVersion_127) {
                    topology = "linear";
                    break;
                } // otherwise fall through
            default: // stays blank; what about tandem?
                break;
            }
            m_Stream << s_Pad(topology, m_TopologyWidth) << ' ';
        }}
        break;

    case eFormat_Genpept:
        m_Stream << s_Pad("aa", m_GenpeptBlanks + 2);
        break;
    }

    {{
        string division;
        // Deal with translating EMBL divisions?

        CSeqdesc_CI it(handle);
        for (;  it;  ++it) {
            if (it->IsOrg()  &&  it->GetOrg().IsSetOrgname()) {
                const COrgName& info = it->GetOrg().GetOrgname();
                if (info.IsSetDiv()) {
                    division = info.GetDiv();
                    continue; // in hopes of finding a GB_block
                }
            } else if (it->IsSource()
                       &&  it->GetSource().GetOrg().IsSetOrgname()) {
                const COrgName& info = it->GetSource().GetOrg().GetOrgname();
                if (info.IsSetDiv()) {
                    division = info.GetDiv();
                    continue; // in hopes of finding a GB_block
                }
            } else if (it->IsGenbank()) {
                const CGB_block& info = it->GetGenbank();
                if (info.IsSetDiv()) {
                    division = info.GetDiv();
                    BREAK(it);
                }
            }
        }
        m_Stream << s_Pad(division, m_DivisionWidth) << ' ';
    }}

    {{
        const CDate* date = NULL;
        CSeqdesc_CI it(handle, CSeqdesc::e_Update_date);
        for (; it;  ++it) {
            date = &it->GetUpdate_date();
            BREAK(it);
        }
        if (date) {
            m_Stream << s_FormatDate(*date);
        }
    }}
    m_Stream << NcbiEndl; // Finally, the end of the LOCUS line!

    return true;
}


bool CGenbankWriter::WriteDefinition(const CBioseq_Handle& handle)
{
    string definition = handle.GetTitle();
    if (definition.empty()  ||  definition[definition.size()-1] != '.') {
        definition += '.';
    }
    Wrap("DEFINITION", definition);
    return true;
}


bool CGenbankWriter::WriteAccession(const CBioseq_Handle& handle)
{
    const CBioseq& seq = handle.GetBioseq();
    string accessions;
    {
        CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);
        for (;  it;  ++it) {
            accessions = it->GetAccession();
            BREAK(it);
        }
    }

    CTypesConstIterator it;
    Type<CEMBL_block>::AddTo(it);
    Type<CSP_block>::AddTo(it);
    Type<CGB_block>::AddTo(it);
    for (it = ConstBegin(seq);  it;  ++it) {
        if (Type<CEMBL_block>::Match(it)) {
            const CEMBL_block::TExtra_acc& accs
                = Type<CEMBL_block>::Get(it)->GetExtra_acc();
            iterate (CEMBL_block::TExtra_acc, acc, accs) {
                accessions += ' ' + *acc;
            }
        } else if (Type<CSP_block>::Match(it)) {
            const CSP_block::TExtra_acc& accs
                = Type<CSP_block>::Get(it)->GetExtra_acc();
            iterate (CSP_block::TExtra_acc, acc, accs) {
                accessions += ' ' + *acc;
            }
        } else if (Type<CGB_block>::Match(it)) {
            const CGB_block::TExtra_accessions& accs
                = Type<CGB_block>::Get(it)->GetExtra_accessions();
            iterate (CGB_block::TExtra_accessions, acc, accs) {
                accessions += ' ' + *acc;
            }
        }
    }
    Wrap("ACCESSION", accessions);
    return true;
}


bool CGenbankWriter::WriteID(const CBioseq_Handle& handle)
{
    if (m_Format == eFormat_Genbank) {
        return true;
    }

    iterate (CBioseq::TId, it, handle.GetBioseq().GetId()) {
        if ((*it)->IsGi()) {
            m_Stream << s_Pad("PID", sm_KeywordWidth) << 'g' << (*it)->GetGi()
                     << NcbiEndl;
            break;
        }
    }
    return true;
}


bool CGenbankWriter::WriteVersion(const CBioseq_Handle& handle)
{
    const CBioseq& seq = handle.GetBioseq();
    string accession;
    int version;
    {
        CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);
        for (;  it;  ++it) {
            accession = it->GetAccession();
            version = it->GetVersion();
            BREAK(it);
        }
    }

    int gi = 0;
    {
        CTypeConstIterator<CSeq_id> it = ConstBegin(seq);
        for (;  it;  ++it) {
            if (it->IsGi()) {
                gi = it->GetGi();
                BREAK(it);
            }
        }
    }
    m_Stream << s_Pad("VERSION", sm_KeywordWidth) << accession
             << '.' << version << "  GI:" << gi << NcbiEndl;
    return true;
}


bool CGenbankWriter::WriteDBSource(const CBioseq_Handle& handle)
{
    if (m_Format == eFormat_Genbank) {
        return true;
    }

    // Implement!
    return true;
}


bool CGenbankWriter::WriteKeywords(const CBioseq_Handle& handle)
{
    vector<string> keywords;

    for (CSeqdesc_CI it(handle);  it;  ++it) {
        switch (it->Which()) {
        case CSeqdesc::e_Genbank:
        {
            const CGB_block::TKeywords& kw = it->GetGenbank().GetKeywords();
            iterate (CGB_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
            break;
        }

        case CSeqdesc::e_Embl:
        {
            const CEMBL_block::TKeywords& kw = it->GetEmbl().GetKeywords();
            iterate (CEMBL_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
            break;
        }
        
        case CSeqdesc::e_Pir:
        {
            const CPIR_block::TKeywords& kw = it->GetPir().GetKeywords();
            iterate (CPIR_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
            break;
        }
        
        case CSeqdesc::e_Sp:
        {
            const CSP_block::TKeywords& kw = it->GetSp().GetKeywords();
            iterate (CSP_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
            break;
        }
        
        case CSeqdesc::e_Prf:
        {
            const CPRF_block::TKeywords& kw = it->GetPrf().GetKeywords();
            iterate (CPRF_block::TKeywords, k, kw) {
                keywords.push_back(*k);
            }
            break;
        }
        
        default:
            break;
        }
    }

    // Wrap manually, because splitting within a keyword is bad.
    SIZE_TYPE space_left = sm_DataWidth;
    bool at_start = true;
    for (unsigned int n = 0;  n < keywords.size();  ++n) {
        if (n == 0) {
            m_Stream << s_Pad("KEYWORDS", sm_KeywordWidth);
        } else if (keywords[n].size() + 1 >= space_left  &&  !at_start) {
            // Don't let long keywords cause infinite loops.
            m_Stream << NcbiEndl << s_Pad("", sm_KeywordWidth);
            space_left = sm_DataWidth;
            at_start = true;
        } else {
            m_Stream << ' ';
        }
        m_Stream << keywords[n];
        if (n == keywords.size() - 1) {
            m_Stream << '.' << NcbiEndl;
        } else {
            m_Stream << ';';
            space_left -= keywords[n].size() + 2; // 2 for "; "
            at_start = false;
        }
    }
    return true;
}


bool CGenbankWriter::WriteSegment(const CBioseq_Handle& handle)
{
    const CBioseq& seq = handle.GetBioseq();
    const CSeq_entry* parent_entry = seq.GetParentEntry()->GetParentEntry();
    if (parent_entry == NULL  ||  !parent_entry->IsSet()) {
        return true; // nothing to do
    }
    const CBioseq_set& parent = parent_entry->GetSet();
    if (parent.GetClass() != CBioseq_set::eClass_parts
        ||  (parent_entry->GetParentEntry()->GetSet().GetClass()
             != CBioseq_set::eClass_segset)) {
        // Got enough levels here?
        return true; // nothing to do
    }

    unsigned int n = 1;
    iterate (CBioseq_set::TSeq_set, it, parent.GetSeq_set()) {
        if ((*it)->IsSeq()  &&  &(*it)->GetSeq() == &seq) {
            m_Stream << s_Pad("SEGMENT", sm_KeywordWidth) << n
                     << " of " << parent.GetSeq_set().size() << NcbiEndl;
            break;
        }
        n++;
    }

    return true;
}


bool CGenbankWriter::WriteSource(const CBioseq_Handle& handle)
{
    string source, taxname, lineage;
    for (CSeqdesc_CI it(handle, CSeqdesc::e_Genbank);
         it;  ++it) {
        const CGB_block& gb = it->GetGenbank();
        if (gb.IsSetSource()  &&  source.empty()) {
            source = gb.GetSource();
        }
        if (gb.IsSetTaxonomy()  &&  lineage.empty()) {
            lineage = gb.GetTaxonomy();
        }
    }            

    for (CDesc_CI desc_it(handle);  desc_it;  ++desc_it) {
        for (CTypeConstIterator<COrg_ref> it = ConstBegin(*desc_it);
             it;  ++it) {
            if (it->IsSetCommon()  &&  source.empty()) {
                source = it->GetCommon();
            }
            if (it->IsSetTaxname()  &&  taxname.empty()) {
                taxname = it->GetTaxname();
            }
            if (it->IsSetOrgname()  &&  it->GetOrgname().IsSetLineage()
                &&  lineage.empty()) {
                lineage = it->GetOrgname().GetLineage();
            }
        }
    }

    if (taxname.empty()) {
        taxname = "Unknown";
    }

    if (lineage.empty()) {
        lineage = "Unclassified.";
    }
    
    if (source.empty()) {
        source = taxname;
    }
    
    if (source[source.size()-1] != '.') {
        source += '.';
    }
    if (lineage[lineage.size()-1] != '.') {
        lineage += '.';
    }

    Wrap("SOURCE", source);
    m_Stream << s_Pad("  ORGANISM", sm_KeywordWidth) << taxname << NcbiEndl;
    Wrap("", lineage);

    return true;
}


static string s_FormatTitle(const CTitle& title) {
    string result;
    iterate (CTitle::Tdata, it, title.Get()) {
        switch ((*it)->Which()) {
        case CTitle::C_E::e_Name:    result = (*it)->GetName();     break;
        case CTitle::C_E::e_Trans:   result = (*it)->GetTrans();    break;
        case CTitle::C_E::e_Jta:     result = (*it)->GetJta();      break;
        case CTitle::C_E::e_Iso_jta: result = (*it)->GetIso_jta();  break;
        case CTitle::C_E::e_Ml_jta:  result = (*it)->GetMl_jta();   break;
        case CTitle::C_E::e_Abr:     result = (*it)->GetAbr();      break;
        default: // tsub, coden, issn, isbn
            break; // not useful
        }
        if (!result.empty()) {
            break;
        }
    }
    return result;
}


// works for CImprint and CCit_gen
template <class T>
string FormatImprint(const T& imprint) {
    CNcbiOstrstream oss;
    if (imprint.IsSetVolume()) {
        oss << ' ' << imprint.GetVolume();
        if (imprint.IsSetIssue()) {
            oss << " (" << imprint.GetIssue() << ')';
        }
    }
    if (imprint.IsSetPages()) {
        oss << ", " << imprint.GetPages();
    }
    if (imprint.GetDate().IsStd()) {
        oss << " (" << imprint.GetDate().GetStd().GetYear() << ')';
    }

    return CNcbiOstrstreamToString(oss);
}


static string s_FormatAffil(const CAffil& affil)
{
    if (affil.IsStr()) {
        return affil.GetStr();
    } else {
        string result;
        const CAffil::C_Std& std = affil.GetStd();
        if (std.IsSetDiv()) {
            result = std.GetDiv();
        }
        if (std.IsSetAffil()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetAffil();
        }
        if (std.IsSetStreet()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetStreet();
        }
        if (std.IsSetCity()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCity();
        }
        if (std.IsSetSub()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetSub();
        }
        if (std.IsSetPostal_code()) {
            if (!result.empty()) {
                result += " ";
            }
            result += std.GetPostal_code();
        }
        if (std.IsSetCountry()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCountry();
        }
        if (std.IsSetFax()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "Fax: " + std.GetFax();
        }
        if (std.IsSetPhone()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "Phone: " + std.GetPhone();
        }
        if (std.IsSetEmail()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "E-mail: " + std.GetEmail();
        }
        return result;
    }
}


string FormatArticleSource(const CCit_art& art)
{
    switch (art.GetFrom().Which()) {
    case CCit_art::C_From::e_Journal:
    {
        const CCit_jour& jour = art.GetFrom().GetJournal();
        return s_FormatTitle(jour.GetTitle()) + FormatImprint(jour.GetImp());
    }
    case CCit_art::C_From::e_Book:
    {
        const CCit_book& book = art.GetFrom().GetBook();
        return s_FormatTitle(book.GetTitle()) + FormatImprint(book.GetImp());
    }
    case CCit_art::C_From::e_Proc:
    {
        const CCit_book& book = art.GetFrom().GetProc().GetBook();
        return s_FormatTitle(book.GetTitle()) + FormatImprint(book.GetImp());
    }
    default:
        return string();
    }
}


class CReference : public CObject
{
public:
    enum ECategory {
        eCategory_unknown,
        eCategory_published,
        eCategory_unpublished,
        eCategory_submission
    };

    CReference(const CPubdesc& pub, const CSeq_loc& loc);

    const CPubdesc& GetPub(void) const { return *m_Pub; }
    string          GetLocString(const string& length_unit) const;
    int             GetPmid(void) const { return m_Pmid; }
    int             GetMuid(void) const { return m_Muid; }
    ECategory       GetCategory(void) const { return m_Category; }
    const string&   GetAuthors(void) const { return m_Authors; }

    friend class CRefLess;

private:
    CConstRef<CPubdesc>  m_Pub;
    CConstRef<CSeq_loc>  m_Loc;

    // Everything that follows is technically redundant but very
    // useful when sorting.
    int                  m_Serial;
    int                  m_Pmid;
    int                  m_Muid;
    CConstRef<CDate_std> m_Date;
    ECategory            m_Category;
    string               m_Authors;
};


CReference::CReference(const CPubdesc& pub, const CSeq_loc& loc)
    : m_Pub(&pub), m_Loc(&loc), m_Serial(0), m_Pmid(0), m_Muid(0),
      m_Category(eCategory_unknown)
{
    // Extract IDs
    iterate (CPub_equiv::Tdata, it, pub.GetPub().Get()) {
        switch ((*it)->Which()) {
        case CPub::e_Gen:
        {
            const CCit_gen& gen = (*it)->GetGen();
            if (gen.IsSetSerial_number()  &&  m_Serial == 0) {
                m_Serial = gen.GetSerial_number();
            } else if (gen.IsSetPmid()  &&  m_Pmid == 0) {
                m_Serial = gen.GetPmid().Get();
            } else if (gen.IsSetMuid()  &&  m_Muid == 0) {
                m_Serial = gen.GetMuid();
            }
            break;
        }

        case CPub::e_Sub:
            m_Category = eCategory_submission;
            break;

        case CPub::e_Medline:
        {
            const CMedline_entry& medline = (*it)->GetMedline();
            if (medline.IsSetUid()  &&  !m_Muid) {
                m_Muid = medline.GetUid();
            }
            if (medline.IsSetPmid()  &&  !m_Pmid) {
                m_Pmid = medline.GetPmid().Get();
            }
            break;
        }
        
        case CPub::e_Muid:
            if (!m_Muid) {
                m_Muid = (*it)->GetMuid();
            }
            break;

        case CPub::e_Pmid:
            if (!m_Pmid) {
                m_Pmid = (*it)->GetPmid().Get();
            }
            break;

        default:
            break;
        }
    }
        

    {
        CTypeConstIterator<CDate_std> it = ConstBegin(pub);
        for (;  it;  ++it) {
            m_Date = &*it;
            BREAK(it);
        }
    }

    {{
        list<string> author_list;
        for (CTypeConstIterator<CAuth_list> it = ConstBegin(pub);  it;  ++it) {
            switch (it->GetNames().Which()) {
            case CAuth_list::C_Names::e_Ml:
                author_list = it->GetNames().GetMl();
                break;
            case CAuth_list::C_Names::e_Str:
                author_list = it->GetNames().GetStr();
                break;
            case CAuth_list::C_Names::e_Std:
                for (CTypeConstIterator<CName_std> it2
                         = ConstBegin(it->GetNames());
                     it2;  ++it2) {
                    string author = it2->GetLast();
                    if (it2->IsSetInitials()) {
                        author += ',' + it2->GetInitials();
                    } else if (it2->IsSetFirst()) {
                        author += ',';
                        author += it2->GetFirst()[0];
                        author += '.';
                        if (it2->IsSetMiddle()) {
                            author += it2->GetMiddle()[0];
                            author += '.';
                        }
                    }
                    author_list.push_back(author);
                }
                break;
            default:
                // skip, for lack of better information
                break;
            }
        }
            
        SIZE_TYPE count = author_list.size();
        iterate (list<string>, it, author_list) {
            m_Authors += *it;
            switch (--count) {
            case 0:
                break; // last entry
            case 1:
                m_Authors += " and "; // next-to-last
                break;
            default:
                m_Authors += ", ";
                break;
            }
        }
    }}
}


string CReference::GetLocString(const string& length_unit) const
{
    switch (m_Pub->GetReftype()) {
    case CPubdesc::eReftype_sites:
        return "sites";
        break;

    default:
        switch (m_Loc->Which()) {
        case CSeq_loc::e_Mix:
        {
            string result = length_unit + ' ';
            iterate (CSeq_loc_mix::Tdata, it, m_Loc->GetMix().Get()) {
                if (it != m_Loc->GetMix().Get().begin()) {
                    result += "; ";
                }
                CSeq_loc::TRange range = (*it)->GetTotalRange();
                result += (NStr::IntToString(range.GetFrom() + 1) + " to "
                             + NStr::IntToString(range.GetTo() + 1));
            }
            return result;
        }

        default:
            CSeq_loc::TRange range = m_Loc->GetTotalRange();
            return (length_unit + ' ' + NStr::IntToString(range.GetFrom() + 1)
                    + " to " + NStr::IntToString(range.GetTo() + 1));
            break;
        }
        break;
    }
}


class CRefLess
  : public binary_function<CConstRef<CReference>, CConstRef<CReference>, bool>
{
public:
    explicit CRefLess(bool serial_first) : m_SerialFirst(serial_first) {}
    bool operator()(CConstRef<CReference> ref1, CConstRef<CReference> ref2);

private:
    bool m_SerialFirst;
};


// Modeled after SortReferences from the C toolkit.
// Does not yet cover everything the C version does!

bool CRefLess::operator()(CConstRef<CReference> ref1,
                          CConstRef<CReference> ref2)
{
    if (m_SerialFirst  &&  ref1->m_Serial != ref2->m_Serial) {
        return ref1->m_Serial < ref2->m_Serial;
    }

    if (ref1->m_Category != ref2->m_Category) {
        return ref1->m_Category < ref2->m_Category;
    }

    if (ref1->m_Date.NotEmpty()  &&  ref2->m_Date.NotEmpty()) {
        switch (ref1->m_Date->Compare(*ref2->m_Date)) {
        case CDate::eCompare_before: return true;
        case CDate::eCompare_after:  return false;
        default:                     break;
        }
    }

    {{
        bool used_uids;
        if (ref1->m_Pmid  &&  ref2->m_Pmid) {
            used_uids = true;
            if (ref1->m_Pmid != ref2->m_Pmid) {
                return ref1->m_Pmid < ref2->m_Pmid;
            }
        }
        if (ref1->m_Muid  &&  ref2->m_Muid) {
            used_uids = true;
            if (ref1->m_Muid != ref2->m_Muid) {
                return ref1->m_Muid < ref2->m_Muid;
            }
        }
        if (used_uids
            &&  ref1->m_Authors.empty() != ref2->m_Authors.empty()) {
            return ref2->m_Authors.empty();
        }
    }}

    {{
        bool sites1 = (ref1->m_Pub->GetReftype() == CPubdesc::eReftype_sites);
        bool sites2 = (ref2->m_Pub->GetReftype() == CPubdesc::eReftype_sites);
        if (sites1 != sites2) {
            return sites2;
        }
    }}

    // No explore indices to compare.
    
    // ...

    return false;
}


bool CGenbankWriter::WriteReference(const CBioseq_Handle& handle)
{
    const CBioseq& seq = handle.GetBioseq();
    CSeq_loc everywhere;
    vector< CConstRef<CReference> > v;

    {{
        CRef<CSeq_id> id = seq.GetId().front();
        
        // everywhere.SetWhole(id);
        {{
            CSeq_interval& si = everywhere.SetInt();
            si.SetFrom(0);
            si.SetTo(seq.GetInst().GetLength() - 1);
            SerialAssign<CSeq_id>(si.SetId(), *id);
        }}

        for (CSeqdesc_CI it(handle, CSeqdesc::e_Pub);
             it;  ++it) {
            v.push_back(new CReference(it->GetPub(), everywhere));
        }

        // get references from features
        for (CFeat_CI pub(m_Scope, everywhere, CSeqFeatData::e_Pub);
             pub;  ++pub) {
            v.push_back(new CReference(pub->GetData().GetPub(),
                                       pub->GetLocation()));
        }
    }}

    sort(v.begin(), v.end(), CRefLess(false));

    // prune dups

    sort(v.begin(), v.end(), CRefLess(true));

    for (unsigned int n = 0;  n < v.size();  ++n) {
        m_Stream << s_Pad("REFERENCE", sm_KeywordWidth) << n + 1 << "  ("
                 << v[n]->GetLocString(m_LengthUnit) << ')' << NcbiEndl;
        Wrap("  AUTHORS", v[n]->GetAuthors());
        
        {{
            bool seen_main = false;
            iterate (CPub_equiv::Tdata, it, v[n]->GetPub().GetPub().Get()) {
                switch ((*it)->Which()) {
                case CPub::e_Gen:
                {
                    const CCit_gen& gen = (*it)->GetGen();
                    if (seen_main) {
                        break;
                    }
                    if (gen.IsSetTitle()) {
                        seen_main = true;
                        Wrap("  TITLE", s_StripDot(gen.GetTitle()));
                        if (gen.IsSetJournal()) {
                            Wrap("  JOURNAL",
                                 s_FormatTitle(gen.GetJournal())
                                 + FormatImprint(gen));
                        } else if (gen.IsSetCit()
                                   &&  (gen.GetCit() == "unpublished"
                                        ||  gen.GetCit() == "Unpublished")) {
                            m_Stream << s_Pad("  JOURNAL", sm_KeywordWidth)
                                     << "Unpublished";
                            if (gen.IsSetDate() && gen.GetDate().IsStd()) {
                                m_Stream << " ("
                                         << gen.GetDate().GetStd().GetYear()
                                         << ')';
                            }
                            m_Stream << NcbiEndl;
                        }
                    }
                    break;
                }

                case CPub::e_Sub:
                {
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    const CCit_sub& sub = (*it)->GetSub();
                    Wrap("  TITLE", "Direct Submission");
                    string details = "Submitted";
                    if (sub.IsSetDate()) {
                        details += " (" + s_FormatDate(sub.GetDate()) + ')';
                    } else if (sub.IsSetImp()) {
                        details += " (" + s_FormatDate(sub.GetImp().GetDate())
                            + ')';
                    } if (sub.GetAuthors().IsSetAffil()) {
                        details += ' '
                            + s_FormatAffil(sub.GetAuthors().GetAffil());
                    }
                    Wrap("  JOURNAL", details);
                    break;
                }

                case CPub::e_Medline:
                {
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    const CCit_art& art = (*it)->GetMedline().GetCit();
                    Wrap("  TITLE", s_StripDot(s_FormatTitle(art.GetTitle())));
                    Wrap("  JOURNAL", FormatArticleSource(art));
                    break;
                }

                case CPub::e_Article:
                {
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    const CCit_art& art = (*it)->GetArticle();
                    Wrap("  TITLE", s_StripDot(s_FormatTitle(art.GetTitle())));
                    Wrap("  JOURNAL", FormatArticleSource(art));
                    break;
                }

                case CPub::e_Journal:
                {
                    const CCit_jour& jour = (*it)->GetJournal();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  JOURNAL",
                         s_FormatTitle(jour.GetTitle())
                         + FormatImprint(jour.GetImp()));
                    break;
                }

                case CPub::e_Book:
                {
                    const CCit_book& book = (*it)->GetBook();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  JOURNAL",
                         s_FormatTitle(book.GetTitle())
                         + FormatImprint(book.GetImp()));
                    break;
                }

                case CPub::e_Proc:
                {
                    const CCit_book& book = (*it)->GetProc().GetBook();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  JOURNAL",
                         s_FormatTitle(book.GetTitle())
                         + FormatImprint(book.GetImp()));
                    break;
                }

                case CPub::e_Patent:
                {
                    const CCit_pat& pat = (*it)->GetPatent();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    Wrap("  TITLE", s_StripDot(pat.GetTitle()));
                    string journal = "Patent: " + pat.GetCountry() + ' ';
                    if (pat.IsSetNumber()) {
                        journal += pat.GetNumber() + '-';
                    }
                    journal += pat.GetDoc_type();
                    for (CTypeConstIterator<CPatent_seq_id> psid
                             = ConstBegin(seq);
                         psid;  ++psid) {
                        journal += ' ' + NStr::IntToString(psid->GetSeqid());
                    }
                    if (pat.IsSetDate_issue()) {
                        journal += ' ' + s_FormatDate(pat.GetDate_issue());
                    }
                    Wrap("  JOURNAL", journal + ';');
                    break;
                }

                case CPub::e_Pat_id:
                {
                    const CId_pat& id = (*it)->GetPat_id();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    string doc_type("patent");
                    if (id.IsSetDoc_type()) {
                        doc_type = id.GetDoc_type();
                    }
                    string number;
                    if (id.GetId().IsNumber()) {
                        number = id.GetId().GetNumber();
                    } else {
                        number = "application " + id.GetId().GetApp_number();
                    }
                    Wrap("  JOURNAL",
                         id.GetCountry() + ' ' + doc_type + ' ' + number);
                    break;
                }
                
                case CPub::e_Man:
                {
                    const CCit_let& let = (*it)->GetMan();
                    if (seen_main) {
                        break;
                    }
                    seen_main = true;
                    if (let.IsSetMan_id()) {
                        Wrap("  TITLE", s_StripDot(let.GetMan_id()));
                    }
                    const CCit_book& book = let.GetCit();
                    Wrap("  JOURNAL",
                         s_FormatTitle(book.GetTitle())
                         + FormatImprint(book.GetImp()));
                    break;
                }
                    
                default:
                    break;
                }
            }

            if (v[n]->GetMuid()) {
                m_Stream << s_Pad("  MEDLINE", sm_KeywordWidth)
                         << v[n]->GetMuid() << NcbiEndl;
            }
            if (v[n]->GetPmid()) {
                m_Stream << s_Pad("   PUBMED", sm_KeywordWidth)
                         << v[n]->GetPmid() << NcbiEndl;
            }
        }}
        
        if (v[n]->GetPub().IsSetComment()) {
            Wrap("  REMARK", v[n]->GetPub().GetComment());
        }
    }

    return true;
}


bool CGenbankWriter::WriteComment(const CBioseq_Handle& handle)
{
    string comments;
    for (CSeqdesc_CI it(handle, CSeqdesc::e_Comment);
         it;  ++it) {
        if (!comments.empty()) {
            comments += "~~"; // blank line between comments
        }
        comments += it->GetComment();
        if (comments[comments.size() - 1] != '.') {
            comments += '.';
        }
    }
    Wrap("COMMENT", comments);
    return true;
}


static string s_FormatDbtag(const CDbtag& dbtag)
{
    string result(dbtag.GetDb());
    result += ':';
    switch (dbtag.GetTag().Which()) {
    case CObject_id::e_Str:
        result += dbtag.GetTag().GetStr();
        break;
    case CObject_id::e_Id:
        result += NStr::IntToString(dbtag.GetTag().GetId());
        break;
    default:
        break;
    }
    return result;
}


static const CSeq_id& s_GetID(const CSeq_loc& loc)
{
    static CSeq_id unknown;
    switch (loc.Which()) {
    case CSeq_loc::e_Whole:
        return loc.GetWhole();
    case CSeq_loc::e_Int:
        return loc.GetInt().GetId();
    case CSeq_loc::e_Packed_int:
        return loc.GetPacked_int().Get().front()->GetId();
    case CSeq_loc::e_Pnt:
        return loc.GetPnt().GetId();
    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().GetId();
    case CSeq_loc::e_Mix:
        return s_GetID(*loc.GetMix().Get().front());
    case CSeq_loc::e_Equiv:
        return s_GetID(*loc.GetEquiv().Get().front());
    case CSeq_loc::e_Bond:
        return loc.GetBond().GetA().GetId();
    default:
        return unknown;
    }
}


static bool s_CompareFeats(CConstRef<CSeq_feat> f1, CConstRef<CSeq_feat> f2)
{
    return *f1 < *f2;
}


// Amino acid abbreviations for feature table.  Differ from Iupac3aa
// in that unknown is Xaa or OTHER instead of Xxx and termination is TERM
// rather than Ter.

static const char* s_RawToAbbrev(unsigned char aa)
{ // takes Ncbistdaa or Ncbi8aa
    static const char* const kAbbrev[] = {
        "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
        "Lys", "Leu", "Met", "Asn", "Pro", "Gln", "Arg", "Ser", "Thr", "Val",
        "Trp", "Xaa", "Tyr", "Glx", "Sec", "TERM" };
    if (aa < sizeof(kAbbrev)/sizeof(*kAbbrev)) {
        return kAbbrev[aa];
    } else {
        return "OTHER";
    }
}


static const char* s_ASCIIToAbbrev(char aa)
{
    if (aa == '*') {
        return "TERM";
    } else if (aa == '-') {
        return "---";
    } else if (aa >= 'A'  &&  aa <= 'I') {
        return s_RawToAbbrev(aa - 'A' + 1);
    } else if (aa >= 'K'  &&  aa <= 'N') {
        return s_RawToAbbrev(aa - 'K' + 10);
    } else if (aa >= 'P'  &&  aa <= 'T') {
        return s_RawToAbbrev(aa - 'P' + 14);
    } else if (aa == 'U') {
        return "Sec";
    } else if (aa >= 'V'  &&  aa <= 'Z') {
        return s_RawToAbbrev(aa - 'V' + 19);
    } else if (islower(aa)) {
        return s_ASCIIToAbbrev(toupper(aa));
    } else {
        return "OTHER";
    }
}


bool CGenbankWriter::WriteFeatures(const CBioseq_Handle& handle)
{
    const CBioseq& seq = handle.GetBioseq();
    CSeq_loc everywhere;
    everywhere.SetWhole(*seq.GetId().front());

    m_Stream << s_Pad("FEATURES", sm_FeatureNameIndent + sm_FeatureNameWidth)
             << "Location/Qualifiers" << NcbiEndl;

    {{
        WriteFeatureLocation("source", everywhere, seq);
        bool found_source = false;
        CSeqdesc_CI it(handle, CSeqdesc::e_Source);
        for (; it;  ++it) {
            TGBSQuals quals = s_SourceQualifiers(it->GetSource());
            iterate (TGBSQuals, qual, quals) {
                WriteFeatureQualifier(qual->ToString());
            }
            found_source = true;
            BREAK(it);
        }
        if (!found_source) {
            WriteFeatureQualifier("/organism=\"unknown\"");
        }
    }}

    typedef vector< CConstRef<CSeq_feat> > TFeatVect;
    TFeatVect v;
    for (CFeat_CI feat(m_Scope,everywhere, CSeqFeatData::e_not_set);
         feat;  ++feat) {
        if ((feat->GetData().Which() == CSeqFeatData::e_Prot
             &&  m_Format != eFormat_Genpept)
            ||  feat->GetData().Which() == CSeqFeatData::e_Pub) {
            continue;
        }
        v.push_back(CConstRef<CSeq_feat>(&*feat));
    }
    sort(v.begin(), v.end(), s_CompareFeats);

    iterate (TFeatVect, feat, v) {
        CFeature gbfeat(**feat);

        string name
            = (*feat)->GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
        if ((*feat)->IsSetProduct()) {
            CNcbiOstrstream loc_stream, prod_stream;
            FormatFeatureLocation((*feat)->GetLocation(), seq, loc_stream);
            FormatFeatureLocation((*feat)->GetProduct(),  seq, prod_stream);
            string location = CNcbiOstrstreamToString(loc_stream);
            string product  = CNcbiOstrstreamToString(prod_stream);
            if (location.find(':') != NPOS  &&  product.find(':') == NPOS) {
                WriteFeatureLocation(name, (*feat)->GetProduct(), seq);
                gbfeat.AddQual(CGBQual::eType_coded_by, location);
            } else {
                // What if both contain colons (because both are segmented)?
                WriteFeatureLocation(name, location);
            }
        } else {
            WriteFeatureLocation(name, (*feat)->GetLocation(), seq);
        }

        switch ((*feat)->GetData().Which()) {
        case CSeqFeatData::e_Gene: // gene
        {
            const CGene_ref& gene = (*feat)->GetData().GetGene();
            if (gene.IsSetAllele()) {
                gbfeat.AddQual(CGBQual::eType_allele, gene.GetAllele());
            }
            if (gene.IsSetDesc()) {
                gbfeat.AddQual(CGBQual::eType_function, gene.GetDesc());
            }
            if (gene.IsSetMaploc()) {
                gbfeat.AddQual(CGBQual::eType_map, gene.GetMaploc());
            }
            if (gene.GetPseudo()) {
                gbfeat.AddQual(CGBQual::eType_pseudo);
            }
            if (gene.IsSetDb()) {
                iterate (CGene_ref::TDb, it, gene.GetDb()) {
                    gbfeat.AddQual(CGBQual::eType_db_xref, s_FormatDbtag(**it));
                }
            }
            if (gene.IsSetSyn()) {
                iterate (CGene_ref::TSyn, it, gene.GetSyn()) {
                    gbfeat.AddQual(CGBQual::eType_standard_name, *it);
                }
            }
            break;
        }

        case CSeqFeatData::e_Org:
            // WriteSourceQualifiers((*feat)->GetData().GetOrg());
            break;

        case CSeqFeatData::e_Cdregion: // CDS
        {
            if (m_Format != eFormat_Genbank) {
                break;
            }
            const CCdregion& region = (*feat)->GetData().GetCdregion();
            if (region.IsSetFrame()) {
                gbfeat.AddQual(CGBQual::eType_codon_start,
                               NStr::IntToString(region.GetFrame()));
            }
            if (region.IsSetCode()) {
                iterate (CGenetic_code::Tdata, it, region.GetCode().Get()) {
                    switch ((*it)->Which()) {
                    case CGenetic_code::C_E::e_Id:
                        if ((*it)->GetId() != 1) {
                            gbfeat.AddQual(CGBQual::eType_transl_table,
                                           NStr::IntToString((*it)->GetId()));
                        }
                        break;

                    case CGenetic_code::C_E::e_Ncbieaa:
                    case CGenetic_code::C_E::e_Ncbi8aa:
                    case CGenetic_code::C_E::e_Ncbistdaa:
                    {
                        static const string kUniversalCode
                            = "FFLLSSSSYY**CC*WLLLLPPPPHHQQRRRR"
                              "IIIMTTTTNNKKSSRRVVVVAAAADDEEGGGG";
                        string custom_code;
                        if ((*it)->IsNcbieaa()) {
                            custom_code = (*it)->GetNcbieaa();
                        } else if ((*it)->IsNcbi8aa()) {
                            CSeq_data in, out;
                            in.SetNcbi8aa().Set() = (*it)->GetNcbi8aa();
                            CSeqportUtil::Convert(in, &out,
                                                  CSeq_data::e_Ncbieaa);
                            custom_code = out.GetNcbieaa().Get();
                        } else { // Ncbistdaa
                            CSeq_data in, out;
                            in.SetNcbistdaa().Set() = (*it)->GetNcbistdaa();
                            CSeqportUtil::Convert(in, &out,
                                                  CSeq_data::e_Ncbieaa);
                            custom_code = out.GetNcbieaa().Get();
                        }
                        for (unsigned int i = 0;  i < 4 * 4 * 4;  ++i) {
                            if (custom_code[i] != kUniversalCode[i]) {
                                string codon = CGencode::Idx2Codon(i);
                                NStr::ToLower(codon);
                                gbfeat.AddQual(CGBQual::eType_codon,
                                               "(seq:\"" + codon + "\",aa:" +
                                               s_ASCIIToAbbrev(custom_code[i])
                                               + ")");
                            }
                        }
                        break;
                    }

                    default:
                        break;
                    }
                }
            }
            if (region.IsSetCode_break()) {
                iterate (CCdregion::TCode_break, it, region.GetCode_break()) {
                    const char* abbrev;
                    switch ((*it)->GetAa().Which()) {
                    case CCode_break::C_Aa::e_Ncbieaa:
                        abbrev = s_ASCIIToAbbrev((*it)->GetAa().GetNcbieaa());
                        break;
                    case CCode_break::C_Aa::e_Ncbi8aa:
                        abbrev = s_RawToAbbrev((*it)->GetAa().GetNcbi8aa());
                        break;
                    case CCode_break::C_Aa::e_Ncbistdaa:
                        abbrev = s_RawToAbbrev((*it)->GetAa().GetNcbistdaa());
                        break;
                    default:
                        abbrev = "";
                        break;
                    }
                    CNcbiOstrstream oss;
                    oss << "(pos:";
                    FormatFeatureLocation((*it)->GetLoc(), seq, oss);
                    oss << ",aa:" << abbrev << ')';
                    gbfeat.AddQual(CGBQual::eType_transl_except,
                                   CNcbiOstrstreamToString(oss));
                }
            }
            break;
        }

        case CSeqFeatData::e_Prot: // Protein
            if (m_Format == eFormat_Genpept) {
                s_AddProteinQualifiers(gbfeat, (*feat)->GetData().GetProt());
            }
            break;

        case CSeqFeatData::e_Rna: // *RNA
        {
            const CRNA_ref& rna = (*feat)->GetData().GetRna();
            if (rna.IsSetExt()  &&  rna.GetExt().IsName()) {
                gbfeat.AddQual(CGBQual::eType_note, rna.GetExt().GetName());
            }

            switch (rna.GetType()) {
            case CRNA_ref::eType_tRNA:
            {
                if (!rna.IsSetExt()  ||  !rna.GetExt().IsTRNA()
                    ||  !rna.GetExt().GetTRNA().IsSetAa()
                    ||  !rna.GetExt().GetTRNA().IsSetAnticodon()) {
                    break;
                }
                const CTrna_ext& trna = rna.GetExt().GetTRNA();
                const char* abbrev;
                switch (trna.GetAa().Which()) {
                case CTrna_ext::C_Aa::e_Iupacaa:
                    abbrev = s_ASCIIToAbbrev(trna.GetAa().GetIupacaa());
                    break;
                case CTrna_ext::C_Aa::e_Ncbieaa:
                    abbrev = s_ASCIIToAbbrev(trna.GetAa().GetNcbieaa());
                    break;
                case CTrna_ext::C_Aa::e_Ncbi8aa:
                    abbrev = s_RawToAbbrev(trna.GetAa().GetNcbi8aa());
                    break;
                case CTrna_ext::C_Aa::e_Ncbistdaa:
                    abbrev = s_RawToAbbrev(trna.GetAa().GetNcbistdaa());
                    break;
                default:
                    abbrev = "";
                    break;
                }
                CNcbiOstrstream oss;
                oss << "(pos:";
                FormatFeatureLocation(trna.GetAnticodon(), seq, oss);
                oss << ",aa:" << abbrev << ')';
                gbfeat.AddQual(CGBQual::eType_anticodon,
                               CNcbiOstrstreamToString(oss));
                break;
            }
            case CRNA_ref::eType_snoRNA:
                gbfeat.AddQual(CGBQual::eType_note, "snoRNA");
                break;
            default:
                break;
            }
            if (rna.IsSetPseudo()  &&  rna.GetPseudo()) {
                gbfeat.AddQual(CGBQual::eType_pseudo);
            }
            break;
        }

        case CSeqFeatData::e_Pub:
            for (CTypeConstIterator<CCit_gen> it
                     = ConstBegin((*feat)->GetData().GetPub());
                 it;  ++it) {
                if (it->IsSetSerial_number()) {
                    gbfeat.AddQual(CGBQual::eType_citation,
                                   '[' +
                                   (NStr::IntToString(it->GetSerial_number()))
                                   + ']');
                }
                // XXX - try to find citation if no serial number
            }
            break;

        case CSeqFeatData::e_Seq:
        {
            CNcbiOstrstream oss;
            oss << "origin: ";
            FormatFeatureLocation((*feat)->GetData().GetSeq(), seq, oss);
            gbfeat.AddQual(CGBQual::eType_note, CNcbiOstrstreamToString(oss));
            break;
        }
        
        case CSeqFeatData::e_Imp:
            if ((*feat)->GetData().GetImp().IsSetDescr()) {
                gbfeat.AddQual(CGBQual::eType_note,
                               (*feat)->GetData().GetImp().GetDescr());
            }
            break;

        case CSeqFeatData::e_Region:
            // XXX -- some of these (LTR) should be feature names
            gbfeat.AddQual(CGBQual::eType_note, (*feat)->GetData().GetRegion());
            break;

        case CSeqFeatData::e_Bond:
        {
            string type = CSeqFeatData::GetTypeInfo_enum_EBond()
                ->FindName((*feat)->GetData().GetBond(), true);
            gbfeat.AddQual(CGBQual::eType_note, type + " bond");
            break;
        }

        case CSeqFeatData::e_Site:
            switch ((*feat)->GetData().GetSite()) {
            case CSeqFeatData::eSite_binding:
            case CSeqFeatData::eSite_np_binding:
            case CSeqFeatData::eSite_dna_binding:
            case CSeqFeatData::eSite_signal_peptide:
            case CSeqFeatData::eSite_transit_peptide:
                break;
            case CSeqFeatData::eSite_metal_binding:
                gbfeat.AddQual(CGBQual::eType_bound_moiety, "metal");
                break;
            case CSeqFeatData::eSite_lipid_binding:
                gbfeat.AddQual(CGBQual::eType_bound_moiety, "lipid");
                break;
            default:
            {
                string type = CSeqFeatData::GetTypeInfo_enum_ESite()
                    ->FindName((*feat)->GetData().GetSite(), true);
                gbfeat.AddQual(CGBQual::eType_note, type + " site");
                break;
            }
            }
            break;

        case CSeqFeatData::e_Rsite:
        {
            const CRsite_ref& rsite = (*feat)->GetData().GetRsite();
            switch (rsite.Which()) {
            case CRsite_ref::e_Str:
                gbfeat.AddQual(CGBQual::eType_note,
                               "Restriction site: " + rsite.GetStr());
                break;
            case CRsite_ref::e_Db:
                gbfeat.AddQual(CGBQual::eType_note, "Restriction site");
                gbfeat.AddQual(CGBQual::eType_db_xref,
                               s_FormatDbtag(rsite.GetDb()));
                break;
            default:
                gbfeat.AddQual(CGBQual::eType_note, "Restriction site");
            }
            break;
        }

        case CSeqFeatData::e_Txinit: // promoter
        {
            const CTxinit& txinit = (*feat)->GetData().GetTxinit();
            gbfeat.AddQual(CGBQual::eType_function, txinit.GetName());
            // XXX - should turn rest of data into notes
            break;
        }

        case CSeqFeatData::e_Num:
            // XXX - turn into notes?
            break;

        case CSeqFeatData::e_Psec_str:
        {
            string type = CSeqFeatData::GetTypeInfo_enum_EPsec_str()
                ->FindName((*feat)->GetData().GetPsec_str(), true);
            gbfeat.AddQual(CGBQual::eType_note,
                           "secondary structure: " + type);
            break;
        }

        case CSeqFeatData::e_Non_std_residue:
            gbfeat.AddQual(CGBQual::eType_note,
                           "non-standard residue: "
                           + (*feat)->GetData().GetNon_std_residue());
            break;

        case CSeqFeatData::e_Het:
            gbfeat.AddQual(CGBQual::eType_bound_moiety,
                           (*feat)->GetData().GetHet().Get());
            break;

        case CSeqFeatData::e_Biosrc:
            // WriteSourceQualifiers((*feat)->GetData().GetBiosrc());
            break;

        default:
            break;
        }

        // Standard qualifiers.
        if ((*feat)->GetData().Which() != CSeqFeatData::e_Biosrc) {
            try {
                // handle "CAnnot_CI::CAnnot_CI() -- unsupported location type"
                for (CFeat_CI gene(m_Scope,(*feat)->GetLocation(),
                                                       CSeqFeatData::e_Gene);
                     gene;  ++gene) {
                    if (gene->GetData().GetGene().IsSetLocus()) {
                        gbfeat.AddQual(CGBQual::eType_gene,
                                       gene->GetData().GetGene().GetLocus());
                    }
                }
            } catch (exception& e) {
                ERR_POST(Warning << e.what());
            }
        }

        if ((*feat)->IsSetPartial()  &&  (*feat)->GetPartial()) {
            bool fuzzy = false;
            CTypeConstIterator<CInt_fuzz> it
                     = ConstBegin((*feat)->GetLocation());
            for (; it;  ++it) {
                fuzzy = true;
                BREAK(it);
            }
            if (!fuzzy) {
                gbfeat.AddQual(CGBQual::eType_partial);
            }
        }

        if ((*feat)->IsSetComment()) {
            gbfeat.AddQual(CGBQual::eType_note, (*feat)->GetComment());
        }

        if ((*feat)->IsSetProduct()  &&  m_Format != eFormat_Genpept) {
            try {
                // handle "CAnnot_CI::CAnnot_CI() -- unsupported location type"
                for (CFeat_CI prot(m_Scope,(*feat)->GetProduct(),
                                                       CSeqFeatData::e_Prot);
                     prot;  ++prot) {
                    s_AddProteinQualifiers(gbfeat, prot->GetData().GetProt());
                }
            } catch (exception& e) {
                ERR_POST(Warning << e.what());
            } 
            {{
                const CBioseq_Handle& prot_handle
                    = m_Scope.GetBioseqHandle(s_GetID((*feat)->GetProduct()));
                const CBioseq& prot_seq = prot_handle.GetBioseq();
                const CTextseq_id* tsid;
                iterate(CBioseq::TId, it, prot_seq.GetId()) {
                    if ((tsid = (*it)->GetTextseq_Id()) != NULL) {
                        gbfeat.AddQual(CGBQual::eType_protein_id,
                                       tsid->GetAccession() + '.' +
                                       (NStr::IntToString(tsid->GetVersion())));
                    } else if ((*it)->IsGi()) {
                        gbfeat.AddQual(CGBQual::eType_db_xref,
                                       "GI:" +
                                       (NStr::IntToString((*it)->GetGi())));
                    }
                }
                {{
                    CSeqVector vec = prot_handle.GetSeqVector();
                    vec.SetIupacCoding();
                    string data;
                    data.resize(vec.size());
                    for (SIZE_TYPE n = 0; n < vec.size(); n++) {
                        data[n] = vec[n];
                    }
                    gbfeat.AddQual(CGBQual::eType_translation, data);
                }}
            }}
        }

        if ((*feat)->IsSetQual()) {
            iterate (CSeq_feat::TQual, it, (*feat)->GetQual()) {
                gbfeat.AddQual(static_cast<CGBQual::EType>
                               (CGBQual::GetTypeInfo_enum_EType()
                                ->FindValue((*it)->GetQual())),
                               (*it)->GetVal());
            }
        }

        if ((*feat)->IsSetTitle()) {
            gbfeat.AddQual(CGBQual::eType_label, (*feat)->GetTitle());
        }

        if ((*feat)->IsSetCit()) {
            for (CTypeConstIterator<CCit_gen> it
                     = ConstBegin((*feat)->GetCit());
                 it;  ++it) {
                if (it->IsSetSerial_number()) {
                    gbfeat.AddQual(CGBQual::eType_citation,
                                  '[' +
                                   (NStr::IntToString(it->GetSerial_number()))
                                   + ']');
                }
                // otherwise, track down...could be in descs OR
                // elsewhere in the feature table. :-/
            }
        }

        if ((*feat)->IsSetExp_ev()) {
            switch ((*feat)->GetExp_ev()) {
            case CSeq_feat::eExp_ev_experimental:
                gbfeat.AddQual(CGBQual::eType_evidence, "experimental");
                break;
            case CSeq_feat::eExp_ev_not_experimental:
                gbfeat.AddQual(CGBQual::eType_evidence, "not_experimental");
                break;
            }
        }

        if ((*feat)->IsSetDbxref()) {
            iterate (CSeq_feat::TDbxref, it, (*feat)->GetDbxref()) {
                gbfeat.AddQual(CGBQual::eType_db_xref, s_FormatDbtag(**it));
            }
        }

        if ((*feat)->IsSetPseudo()  &&  (*feat)->GetPseudo()) {
            gbfeat.AddQual(CGBQual::eType_pseudo);
        }

        if ((*feat)->IsSetExcept_text()) {
            gbfeat.AddQual(CGBQual::eType_exception, (*feat)->GetExcept_text());
        }

        iterate (CFeature::TQuals, qual, gbfeat.GetQuals()) {
            WriteFeatureQualifier(qual->ToString());
        }
    }

    return true;
}


static void s_AddProteinQualifiers(CFeature& gbfeat, const CProt_ref& prot)
{
    if (prot.IsSetName()) {
        iterate (CProt_ref::TName, it, prot.GetName()) {
            gbfeat.AddQual(CGBQual::eType_product, *it);
        }
    }
    
    if (prot.IsSetDesc()) {
        gbfeat.AddQual(CGBQual::eType_function, prot.GetDesc());
    }
    
    if (prot.IsSetEc()) {
        iterate (CProt_ref::TEc, it, prot.GetEc()) {
            gbfeat.AddQual(CGBQual::eType_EC_number, *it);
        }
    }

    if (prot.IsSetActivity()) {
        iterate (CProt_ref::TActivity, it, prot.GetActivity()) {
            gbfeat.AddQual(CGBQual::eType_prot_activity, *it);
        }
    }

    if (prot.IsSetDb()) {
        iterate (CProt_ref::TDb, it, prot.GetDb()) {
            gbfeat.AddQual(CGBQual::eType_db_xref, s_FormatDbtag(**it));
        }
    }
}


static TGBSQuals s_SourceQualifiers(const COrg_ref& org)
{
    TGBSQuals quals;

    if (org.IsSetTaxname()) {
        quals.insert(CGBSQual(CGBSQual::eType_organism, org.GetTaxname()));
    } else if (org.IsSetCommon()) {
        quals.insert(CGBSQual(CGBSQual::eType_organism, org.GetCommon()));
    }
    

    if (org.IsSetMod()) {
        iterate (COrg_ref::TMod, it, org.GetMod()) {
            quals.insert(CGBSQual(CGBSQual::eType_note, *it));
        }
    }

    if (org.IsSetDb()) {
        iterate (COrg_ref::TDb, it, org.GetDb()) {
            quals.insert(CGBSQual(CGBSQual::eType_db_xref,
                                  s_FormatDbtag(**it)));
        }
    }

    if (org.IsSetOrgname() && org.GetOrgname().IsSetMod()) {
        iterate (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            CGBSQual::EType qual_type;
            string tag;
            switch ((*it)->GetSubtype()) {
            case COrgMod::eSubtype_strain:
                qual_type = CGBSQual::eType_strain;
                break;
            case COrgMod::eSubtype_substrain:
                qual_type = CGBSQual::eType_sub_strain;
                break;
            case COrgMod::eSubtype_variety:
                qual_type = CGBSQual::eType_variety;
                break;
            case COrgMod::eSubtype_serotype:
                qual_type = CGBSQual::eType_serotype;
                break;
            case COrgMod::eSubtype_cultivar:
                qual_type = CGBSQual::eType_cultivar;
                break;
            case COrgMod::eSubtype_isolate:
                qual_type = CGBSQual::eType_isolate;
                break;
            case COrgMod::eSubtype_nat_host:
                qual_type = CGBSQual::eType_specific_host;
                break;
            case COrgMod::eSubtype_sub_species:
                qual_type = CGBSQual::eType_sub_species;
                break;
            case COrgMod::eSubtype_specimen_voucher:
                qual_type = CGBSQual::eType_specimen_voucher;
                break;
            case COrgMod::eSubtype_other:
                qual_type = CGBSQual::eType_note;
                break;
            default:
                qual_type = CGBSQual::eType_note;
                tag = (COrgMod::GetTypeInfo_enum_ESubtype()
                       ->FindName((*it)->GetSubtype(), true)) + ": ";
            }
            quals.insert(CGBSQual(qual_type, tag + (*it)->GetSubname()));
        }
    }
    return quals;
}


static TGBSQuals s_SourceQualifiers(const CBioSource& source)
{
    TGBSQuals quals = s_SourceQualifiers(source.GetOrg());

    if (source.IsSetGenome()) {
        switch (source.GetGenome()) {
        case CBioSource::eGenome_chloroplast:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "plastid:chloroplast"));
            break;
        case CBioSource::eGenome_chromoplast:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "plastid:chromoplast"));
            break;
        case CBioSource::eGenome_kinetoplast:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "mitochondrion:kinetoplast"));
            break;
        case CBioSource::eGenome_mitochondrion:
            quals.insert(CGBSQual(CGBSQual::eType_organelle, "mitochondrion"));
            break;
        case CBioSource::eGenome_plastid:
            quals.insert(CGBSQual(CGBSQual::eType_organelle, "plastid"));
            break;
        case CBioSource::eGenome_macronuclear:
            quals.insert(CGBSQual(CGBSQual::eType_macronuclear));
            break;
        case CBioSource::eGenome_cyanelle:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "plastid:cyanelle"));
            break;
        case CBioSource::eGenome_proviral:
            quals.insert(CGBSQual(CGBSQual::eType_proviral));
            break;
        case CBioSource::eGenome_virion:
            quals.insert(CGBSQual(CGBSQual::eType_virion));
            break;
        case CBioSource::eGenome_nucleomorph:
            quals.insert(CGBSQual(CGBSQual::eType_organelle, "nucleomorph"));
            break;
        case CBioSource::eGenome_apicoplast:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "plastid:apicoplast"));
            break;
        case CBioSource::eGenome_leucoplast:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "plastid:leucoplast"));
            break;
        case CBioSource::eGenome_proplastid:
            quals.insert(CGBSQual(CGBSQual::eType_organelle,
                                  "plastid:proplastid"));
            break;
        case CBioSource::eGenome_plasmid:
        case CBioSource::eGenome_transposon:
        case CBioSource::eGenome_insertion_seq:
            break;
            // tag requires a value; hope for corresponding subsource.
        default:
            string type = CBioSource::GetTypeInfo_enum_EGenome()
                ->FindName(source.GetGenome(), true);
            quals.insert(CGBSQual(CGBSQual::eType_note, "genome: " + type));
            break;
        }
    }
    
    if (source.IsSetSubtype()) {
        iterate (CBioSource::TSubtype, it, source.GetSubtype()) {
            CGBSQual::EType qual_type;
            string tag;
            switch ((*it)->GetSubtype()) {
            case CSubSource::eSubtype_chromosome:
                qual_type = CGBSQual::eType_chromosome;
                break;
            case CSubSource::eSubtype_map:
                qual_type = CGBSQual::eType_map;
                break;
            case CSubSource::eSubtype_clone:
                qual_type = CGBSQual::eType_clone;
                break;
            case CSubSource::eSubtype_subclone:
                qual_type = CGBSQual::eType_sub_clone;
                break;
            case CSubSource::eSubtype_haplotype:
                qual_type = CGBSQual::eType_haplotype;
                break;
            case CSubSource::eSubtype_sex:
                qual_type = CGBSQual::eType_sex;
                break;
            case CSubSource::eSubtype_cell_line:
                qual_type = CGBSQual::eType_cell_line;
                break;
            case CSubSource::eSubtype_cell_type:
                qual_type = CGBSQual::eType_cell_type;
                break;
            case CSubSource::eSubtype_tissue_type:
                qual_type = CGBSQual::eType_tissue_type;
                break;
            case CSubSource::eSubtype_clone_lib:
                qual_type = CGBSQual::eType_clone_lib;
                break;
            case CSubSource::eSubtype_dev_stage:
                qual_type = CGBSQual::eType_dev_stage;
                break;
            case CSubSource::eSubtype_frequency:
                qual_type = CGBSQual::eType_frequency;
                break;
            case CSubSource::eSubtype_germline:
                quals.insert(CGBSQual(CGBSQual::eType_germline));
                continue;
            case CSubSource::eSubtype_rearranged:
                quals.insert(CGBSQual(CGBSQual::eType_rearranged));
                continue;
            case CSubSource::eSubtype_lab_host:
                qual_type = CGBSQual::eType_lab_host;
                break;
            case CSubSource::eSubtype_pop_variant:
                qual_type = CGBSQual::eType_pop_variant;
                break;
            case CSubSource::eSubtype_tissue_lib:
                qual_type = CGBSQual::eType_tissue_lib;
                break;
            case CSubSource::eSubtype_plasmid_name:
                qual_type = CGBSQual::eType_plasmid;
                break;
            case CSubSource::eSubtype_transposon_name:
                qual_type = CGBSQual::eType_transposon;
                break;
            case CSubSource::eSubtype_insertion_seq_name:
                qual_type = CGBSQual::eType_insertion_seq;
                break;
            case CSubSource::eSubtype_country:
                qual_type = CGBSQual::eType_country;
                break;
            case COrgMod::eSubtype_other:
                qual_type = CGBSQual::eType_note;
                break;
            default:
                qual_type = CGBSQual::eType_note;
                tag = (CSubSource::GetTypeInfo_enum_ESubtype()
                       ->FindName((*it)->GetSubtype(), true)) + ": ";
            }
            quals.insert(CGBSQual(qual_type, tag + (*it)->GetName()));
        }
    }

    if (source.IsSetIs_focus()) {
        quals.insert(CGBSQual(CGBSQual::eType_focus));
    }

    return quals;
}


bool CGenbankWriter::WriteSequence(const CBioseq_Handle& handle)
{
    CSeqVector vec = handle.GetSeqVector();
    vec.SetIupacCoding();
    if (m_Format == eFormat_Genbank) {
        TSeqPos a = 0, c = 0, g = 0, t = 0, other = 0;
        for (TSeqPos pos = 0;  pos < vec.size();  ++pos) {
            switch (vec[pos]) {
            case 'A': ++a;     break;
            case 'C': ++c;     break;
            case 'G': ++g;     break;
            case 'T': ++t;     break;
            default:  ++other; break;
            }
        }
        m_Stream << s_Pad("BASE COUNT", sm_KeywordWidth) << ' '
                 << setw(6) << a << " a " << setw(6) << c << " c "
                 << setw(6) << g << " g " << setw(6) << t << " t";
        if (other) {
            m_Stream << ' ' << setw(6) << other << " other";
            if (other > 1) {
                m_Stream << 's';
            }
        }
        m_Stream << NcbiEndl;
    }
    m_Stream << s_Pad("ORIGIN", sm_KeywordWidth);
    for (TSeqPos n = 0;  n < vec.size();  ++n) {
        if (n % 60 == 0) {
            m_Stream << NcbiEndl << setw(9) << n + 1 << ' ';
        } else if (n % 10 == 0) {
            m_Stream << ' ';
        }
        m_Stream << static_cast<char>(tolower(vec[n]));
    }
    m_Stream << NcbiEndl;
    return true;
}


void CGenbankWriter::Wrap(const string& keyword, const string& contents,
                          unsigned int indent)
{
    unsigned int data_width = sm_LineWidth - indent;

    if (contents.empty()) {
        return;
    }

    m_Stream << s_Pad(keyword, indent);
    const string newline = s_Pad("\n", indent + 1);

    SIZE_TYPE pos = 0, tilde_pos;
    while ((tilde_pos = contents.find('~', pos)) != NPOS
           ||  contents.size() - pos > data_width) {
        if (tilde_pos <= pos + data_width - 1) {
            m_Stream << contents.substr(pos, tilde_pos - pos) << newline;
            pos = tilde_pos + 1;
        } else {
            SIZE_TYPE split_pos = contents.rfind(' ', pos + data_width - 1);
            if (split_pos <= pos  ||  split_pos == NPOS) {
                split_pos = contents.rfind(',', pos + data_width - 1) + 1;
            }
            if (split_pos <= pos  ||  split_pos == NPOS + 1) {
                split_pos = pos + data_width - 1;
            }
            while (split_pos > 0  &&  contents[split_pos-1] == ' ') {
                --split_pos;
            }
            m_Stream << contents.substr(pos, split_pos - pos) << newline;
            while (split_pos < contents.size() - 1
                   &&  contents[split_pos] == ' ') {
                ++split_pos;
            }
            pos = split_pos;
        }
    }
    m_Stream << contents.substr(pos) << NcbiEndl;
}


void CGenbankWriter::FormatIDPrefix(const CSeq_id& id,
                                    const CBioseq& default_seq,
                                    CNcbiOstream& dest)
{
    iterate (CBioseq::TId, it, default_seq.GetId()) {
        if ((*it)->Match(id)) {
            return;
        }
    }

    const CTextseq_id* tsid = id.GetTextseq_Id();
    if (tsid == NULL) {
        try {
            const CBioseq& seq
                = m_Scope.GetBioseqHandle(id).GetBioseq();
            CTypeConstIterator<CTextseq_id> it = ConstBegin(seq);
            for (; it;  ++it) {
                tsid = &*it;
                BREAK(it);
            }
        } catch (exception& e) {
            ERR_POST(Warning << e.what());
        }
    }
    if (tsid) {
        dest << tsid->GetAccession() << '.' << tsid->GetVersion() << ':';
    } else {
        dest << "EXTERN:";
    }
}


static void s_FormatFuzzyPoint(const CInt_fuzz& fuzz, TSeqPos n,
                               CNcbiOstream& dest)
{
    switch (fuzz.Which()) {
    case CInt_fuzz::e_P_m:
    {
        TSeqPos delta = fuzz.GetP_m();
        dest << '(' << (n - delta) << '.' << (n + delta) << ')';
        break;
    }

    case CInt_fuzz::e_Range:
    {
        const CInt_fuzz::C_Range& range = fuzz.GetRange();
        dest << '(' << range.GetMin() << '.' << range.GetMax() << ')';
        break;
    }

    case CInt_fuzz::e_Pct:
    {
        TSeqPos delta = n * fuzz.GetPct() / 1000;
        dest << '(' << (n - delta) << '.' << (n + delta) << ')';
        break;
    }

    case CInt_fuzz::e_Lim:
        switch (fuzz.GetLim()) {
        case CInt_fuzz::eLim_gt:
            dest << '>' << n;
            break;
        case CInt_fuzz::eLim_lt:
            dest << '<' << n;
            break;
        case CInt_fuzz::eLim_tr:
            dest << n << '^' << (n + 1);
            break;
        case CInt_fuzz::eLim_tl:
            dest << (n - 1) << '^' << n;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}


template <class T>
const char* StrandPrefix(const T& location) {
    if (location.IsSetStrand()  &&  location.GetStrand() == eNa_strand_minus) {
        return "complement(";
    } else {
        return "";
    }
}


template <class T>
const char* StrandSuffix(const T& location) {
    if (location.IsSetStrand()  &&  location.GetStrand() == eNa_strand_minus) {
        return ")";
    } else {
        return "";
    }
}


void CGenbankWriter::FormatFeatureInterval(const CSeq_interval& interval,
                                           const CBioseq& default_seq,
                                           CNcbiOstream& dest)
{
    dest << StrandPrefix(interval);
    FormatIDPrefix(interval.GetId(), default_seq, dest);
    if (interval.IsSetFuzz_from()) {
        s_FormatFuzzyPoint(interval.GetFuzz_from(), interval.GetFrom() + 1,
                           dest);
    } else {
        dest << (interval.GetFrom() + 1);
    }
    dest << "..";
    if (interval.IsSetFuzz_to()) {
        s_FormatFuzzyPoint(interval.GetFuzz_to(), interval.GetTo() + 1, dest);
    } else {
        dest << (interval.GetTo() + 1);
    }
    dest << StrandSuffix(interval);
}


void CGenbankWriter::FormatFeatureLocation(const CSeq_loc& location,
                                           const CBioseq& default_seq,
                                           CNcbiOstream& dest)
{
    // Add 1 to all numbers because genbank indexes from 1 but the raw
    // data indexes from 0
    switch (location.Which()) {
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& id = location.GetWhole();
        FormatIDPrefix(id, default_seq, dest);
        try {
            const CBioseq_Handle& handle = m_Scope.GetBioseqHandle(id);
            const CBioseq_Handle::TBioseqCore core = handle.GetBioseqCore();
            if (core) {
                dest << "1.." << (core->GetInst().GetLength());
            } else {
                dest << "1..N";
            }
        } catch (exception& e) {
            ERR_POST(Warning << e.what());
            dest << "1..N";
        }
        break;
    }

    case CSeq_loc::e_Int:
        FormatFeatureInterval(location.GetInt(), default_seq, dest);
        break;

    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint& pi = location.GetPacked_int();
        dest << "join("; // should this be order?
        bool first = true;
        iterate (CPacked_seqint::Tdata, it, pi.Get()) {
            if (!first) {
                dest << ',';
            }
            FormatFeatureInterval(**it, default_seq, dest);
            first = false;
        }
        dest << ')'; // closes join(...);
        break;
    }

    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& pnt = location.GetPnt();
        dest << StrandPrefix(pnt);
        FormatIDPrefix(pnt.GetId(), default_seq, dest);
        if (pnt.IsSetFuzz()) {
            s_FormatFuzzyPoint(pnt.GetFuzz(), pnt.GetPoint() + 1, dest);
        } else {
            dest << pnt.GetPoint();
        }
        dest << StrandSuffix(pnt);
        break;
    }

    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& pp = location.GetPacked_pnt();
        dest << StrandPrefix(pp) << "join("; // should this be order?
        bool first = true;
        iterate (CPacked_seqpnt::TPoints, it, pp.GetPoints()) {
            if (!first) {
                dest << ',';
            }
            if (pp.IsSetFuzz()) {
                s_FormatFuzzyPoint(pp.GetFuzz(), *it + 1, dest);
            } else {
                dest << *it;
            }
            first = false;
        }
        dest << ')' /* closes join(...); */ << StrandSuffix(pp);
        break;
    }

    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix& mix = location.GetMix();
        bool first = true;
        dest << "join(";
        iterate (CSeq_loc_mix::Tdata, it, mix.Get()) {
            if (!first) {
                dest << ',';
            }
            FormatFeatureLocation(**it, default_seq, dest);
            first = false;
        }
        dest << ')'; // closes join(...);
        break;
    }

    case CSeq_loc::e_Equiv: // just take the first one
        FormatFeatureLocation(**location.GetEquiv().Get().begin(), default_seq,
                              dest);
        break;

    case CSeq_loc::e_Bond:
    {
        if (location.GetBond().IsSetB()) {
            dest << "order(";
        }
        const CSeq_point& a = location.GetBond().GetA();
        dest << StrandPrefix(a);
        FormatIDPrefix(a.GetId(), default_seq, dest);
        if (a.IsSetFuzz()) {
            s_FormatFuzzyPoint(a.GetFuzz(), a.GetPoint() + 1, dest);
        } else {
            dest << a.GetPoint();
        }
        dest << StrandSuffix(a);
        if (location.GetBond().IsSetB()) {
            const CSeq_point& b = location.GetBond().GetB();
            dest << ',' << StrandPrefix(b);
            FormatIDPrefix(b.GetId(), default_seq, dest);
            if (b.IsSetFuzz()) {
                s_FormatFuzzyPoint(b.GetFuzz(), b.GetPoint() + 1, dest);
            } else {
                dest << b.GetPoint();
            }
            dest << StrandSuffix(b) << ')'; // closes order(...);
        }
    }

    case CSeq_loc::e_Feat:
    {
        const CSeq_feat& feat = FindFeature(location.GetFeat(), default_seq);
        FormatFeatureLocation(feat.GetLocation(), default_seq, dest);
        break;
    }

    default:
        break;
    }
}


const CSeq_feat& CGenbankWriter::FindFeature(const CFeat_id& id,
                                             const CBioseq& default_seq)
{
    static CSeq_feat dummy;
    const CSeq_entry& tse =
        m_Scope.GetBioseqHandle(*default_seq.GetId().front()).GetTopLevelSeqEntry();
    CTypeConstIterator<CSeq_feat> it = ConstBegin(tse);
    // declared outside loop to avoid WorkShop bug. :-/
    for (;  it;  ++it) {
        const CFeat_id& id2 = it->GetId();
        if (id.Which() != id2.Which()) {
            continue;
        }
        switch (id.Which()) {
        case CFeat_id::e_Gibb:
            if (id.GetGibb() == id2.GetGibb()) {
                return *it;
            }
            break;

        case CFeat_id::e_Giim:
        { 
            const CGiimport_id& giim  = id.GetGiim();
            const CGiimport_id& giim2 = id2.GetGiim();
            if (giim.GetId() == giim2.GetId()
                &&  (!giim.IsSetDb()
                     ||  (giim2.IsSetDb()  &&  giim.GetDb() == giim2.GetDb()))
                &&  (!giim.IsSetRelease()
                     ||  (giim2.IsSetRelease()
                          &&  giim.GetRelease() == giim2.GetRelease()))) {
                return *it;
            }
            break;        
        }

        case CFeat_id::e_Local:
        {
            const CObject_id& local  = id.GetLocal();
            const CObject_id& local2 = id2.GetLocal();
            if (local.Which() != local2.Which()) {
                break; // out of switch; continue the for loop
            }
            switch (local.Which()) {
            case CObject_id::e_Id:
                if (local.GetId() == local2.GetId()) {
                    return *it;
                }
                break;

            case CObject_id::e_Str:
                if (local.GetStr() == local2.GetStr()) {
                    return *it;
                }
                break;

            default:
                break;
            }
            break;
        }

        case CFeat_id::e_General:
        {
            if (id.GetGeneral().GetDb() != id2.GetGeneral().GetDb()) {
                break;
            }
            const CObject_id& tag  = id.GetGeneral().GetTag();
            const CObject_id& tag2 = id2.GetGeneral().GetTag();
            if (tag.Which() != tag2.Which()) {
                break; // out of switch; continue the for loop
            }
            switch (tag.Which()) {
            case CObject_id::e_Id:
                if (tag.GetId() == tag2.GetId()) {
                    return *it;
                }
                break;

            case CObject_id::e_Str:
                if (tag.GetStr() == tag2.GetStr()) {
                    return *it;
                }
                break;

            default:
                break;
            }
            break;
        }

        default:
            break;
        }
    }
    return dummy; // didn't find anything :-(
}


void CGenbankWriter::WriteFeatureLocation(const string& name,
                                          const string& location)
{
    static const string indent(sm_FeatureNameIndent, ' ');
    Wrap(indent + name, location, sm_FeatureNameIndent + sm_FeatureNameWidth);
}


void CGenbankWriter::WriteFeatureLocation(const string& name,
                                          const CSeq_loc& location,
                                          const CBioseq& default_seq)
{
    CNcbiOstrstream oss;
    FormatFeatureLocation(location, default_seq, oss);
    WriteFeatureLocation(name, CNcbiOstrstreamToString(oss));
}


void CGenbankWriter::WriteFeatureQualifier(const string& qual)
{
    Wrap("", qual, sm_FeatureNameIndent + sm_FeatureNameWidth);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.19  2002/05/06 16:11:01  ucko
* Merge in Andrei Gourianov's changes to use the new OM (thanks!)
* Move CVS log to end.
*
*
* *** These four entries are from src/app/id1_fetch1/genbank1.cpp ***
* Revision 1.4  2002/05/06 03:31:52  vakatov
* OM/OM1 renaming
*
* Revision 1.3  2002/05/03 21:28:21  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.2  2002/04/10 21:02:22  gouriano
* moved construction of iterators out of "for" loop initialization:
* Sun Workshop compiler does not call destructors of such objects
* in case we use break to exit the loop
*
* Revision 1.1  2002/04/04 16:31:36  gouriano
* id1_fetch1 - modified version of id1_fetch, which uses objmgr1
*
* Revision 1.18  2002/03/07 16:45:22  ucko
* Add missing call to CPubMedId::Get needed on Windows.
*
* Revision 1.17  2002/03/06 22:18:25  ucko
* Whoops, accidentally committed old unfinished publication-ordering
* changes; clarify their status in a comment.
*
* Revision 1.16  2002/03/06 22:08:40  ucko
* Add code to calculate protein weights.
*
* Revision 1.15  2002/01/16 18:56:33  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.14  2001/11/13 15:40:34  ucko
* Use Idx2Codon to report genetic code variations.
*
* Revision 1.13  2001/11/02 20:54:51  ucko
* Make gbqual.hpp private; clean up cruft from genbank.hpp.
*
* Revision 1.12  2001/11/02 20:32:28  ucko
* Cope better with references to unavailable sequences.
*
* Revision 1.11  2001/11/01 16:32:24  ucko
* Rework qualifier handling to support appropriate reordering
*
* Revision 1.10  2001/10/30 20:27:04  ucko
* Force ASCII from Seq_vectors.
* Take advantage of new seqfeat functionality.
* Take advantage of CSeq_loc::GetTotalRange.
*
* Revision 1.9  2001/10/18 18:25:38  ucko
* Fix off-by-one keyword-formatting error.
* Remove unnecessary code to force use of GI IDs.
* Take advantage of SerialAssign<CSeq_id>.
*
* Revision 1.8  2001/10/17 21:17:49  ucko
* Seq_vector now properly starts from zero rather than one; adjust code
* that uses it accordingly.
*
* Revision 1.7  2001/10/12 19:49:08  ucko
* Whoops, use break rather than BREAK for STL iterators.
*
* Revision 1.6  2001/10/12 19:32:58  ucko
* move BREAK to a central location; move CBioseq::GetTitle to object manager
*
* Revision 1.5  2001/10/12 15:34:12  ucko
* Edit in-source version of CVS log to avoid end-of-comment marker.  (Oops.)
*
* Revision 1.4  2001/10/12 15:29:07  ucko
* Drop {src,include}/objects/util/asciiseqdata.* in favor of CSeq_vector.
* Rewrite GenBank output code to take fuller advantage of the object manager.
*
* Revision 1.3  2001/10/04 19:11:55  ucko
* Centralize (rudimentary) code to get a sequence's title.
*
* Revision 1.2  2001/10/02 19:23:30  ucko
* Avoid dereferencing NULL pointers.
*
* Revision 1.1  2001/09/25 20:12:06  ucko
* More cleanups from Denis.
* Put utility code in the objects namespace.
* Moved utility code to {src,include}/objects/util (to become libxobjutil).
* Moved static members of CGenbankWriter to above their first use.
*
* ---------------------------------------------------------------------------
* old log:
* Revision 1.4  2001/09/24 14:37:55  ucko
* Comment out names of unused args to WriteXxx.
*
* Revision 1.3  2001/09/21 22:39:07  ucko
* Fix MSVC build.
*
* Revision 1.2  2001/09/05 14:44:59  ucko
* Use NStr::IntToString instead of Stringify.
*
* Revision 1.1  2001/09/04 16:20:53  ucko
* Dramatically fleshed out id1_fetch
*
* ===========================================================================
*/
