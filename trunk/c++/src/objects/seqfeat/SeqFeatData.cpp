/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqfeat.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2002/03/22 16:13:42  ucko
 * CSeqFeatData::GetKey: return "Prot" rather than "Protein" in default
 * vocabulary.  (Keep "Protein" for Genbank).
 *
 * Revision 6.2  2002/03/06 21:59:48  ucko
 * CSeqFeatData::GetKey: return (misc_)RNA rather than extended name for
 * RNA of type "other."
 *
 * Revision 6.1  2001/10/30 20:25:58  ucko
 * Implement feature labels/keys, subtypes, and sorting
 *
 *
 * ===========================================================================
 */

// standard includes

// generated includes
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeqFeatData::~CSeqFeatData(void)
{
}


// ASCII representation of subtype (GenBank feature key, e.g.)
string CSeqFeatData::GetKey(EVocabulary vocab) const
{
    bool genbank = (vocab == eVocabulary_genbank);
    if (genbank) {
        switch (Which()) {
        case e_Gene:
            return "gene";
        case e_Org:
        case e_Biosrc:
            return "source";
        case e_Prot:
            return "Protein";
        case e_Site: // Is this correct, or are these encoded as Imp?
            switch (GetSite()) {
            case CSeqFeatData::eSite_binding:
            case CSeqFeatData::eSite_metal_binding:
            case CSeqFeatData::eSite_lipid_binding:
                return "misc_binding";
            case CSeqFeatData::eSite_np_binding:
                return "protein_bind";
            case CSeqFeatData::eSite_dna_binding:
                return "primer_bind"; // ?
            case CSeqFeatData::eSite_signal_peptide:
                return "sig_peptide";
            case CSeqFeatData::eSite_transit_peptide:
                return "transit_peptide";
            default:
                return "misc_feature";
            }
        case e_Txinit:
            return "promoter"; // ?
        case e_Het:
            return "misc_binding"; // ?
        case e_Cdregion:
        case e_Rna:
        case e_Imp:
            break;
        default:
            return "misc_feature"; // ???
        }
    }

    switch (Which()) {
    case e_Gene:            return "Gene";
    case e_Org:             return "Org";
    case e_Cdregion:        return "CDS";
    case e_Prot:            return "Prot";
    case e_Rna:
        switch (GetRna().GetType()) {
        case CRNA_ref::eType_premsg:  return "precursor_RNA";
        case CRNA_ref::eType_mRNA:    return "mRNA";
        case CRNA_ref::eType_tRNA:    return "tRNA";
        case CRNA_ref::eType_rRNA:    return "rRNA";
        case CRNA_ref::eType_snRNA:   return "snRNA";
        case CRNA_ref::eType_scRNA:   return "scRNA";
        case CRNA_ref::eType_snoRNA:  return "sno_RNA"; // ok for GenBank?
        case CRNA_ref::eType_other:
            // return GetRna().GetExt().GetName();
            return genbank ? "misc_RNA" : "RNA";
        default:                      return "misc_RNA";
        }
    case e_Imp:             return GetImp().GetKey(); // "Imp"?
    case e_Region:          return "Region";
    case e_Comment:         return "Comment";
    case e_Bond:            return "Bond";
    case e_Site:            return "Site";
    case e_Rsite:           return "Rsite";
    case e_User:            return "User";
    case e_Txinit:          return "TxInit";
    case e_Num:             return "Num";
    case e_Psec_str:        return "SecStr";
    case e_Non_std_residue: return "NonStdRes";
    case e_Het:             return "Het";
    case e_Biosrc:          return "Src";
    default:                return "???";
    }
}


CSeqFeatData::ESubtype CSeqFeatData::GetSubtype(void) const
{
    if (m_Subtype == eSubtype_any) { // unknown
        switch (Which()) {
        case e_Gene:       m_Subtype = eSubtype_gene;     break;
        case e_Org:        m_Subtype = eSubtype_org;      break;
        case e_Cdregion:   m_Subtype = eSubtype_cdregion; break;
        case e_Prot:       m_Subtype = eSubtype_prot;     break;
        case e_Rna:
            switch (GetRna().GetType()) {
            case CRNA_ref::eType_premsg: m_Subtype = eSubtype_preRNA;   break;
            case CRNA_ref::eType_mRNA:   m_Subtype = eSubtype_mRNA;     break;
            case CRNA_ref::eType_tRNA:   m_Subtype = eSubtype_tRNA;     break;
            case CRNA_ref::eType_rRNA:   m_Subtype = eSubtype_rRNA;     break;
            case CRNA_ref::eType_snRNA:  m_Subtype = eSubtype_snRNA;    break;
            case CRNA_ref::eType_scRNA:  m_Subtype = eSubtype_scRNA;    break;
            default:                     m_Subtype = eSubtype_otherRNA; break;
            }
            break;
        case e_Pub:        m_Subtype = eSubtype_pub;      break;
        case e_Seq:        m_Subtype = eSubtype_seq;      break;
        case e_Imp: // XXX -- linear search isn't very efficient
        {
            const string& key = GetImp().GetKey();
            if      (key == "allele") {
                m_Subtype = eSubtype_allele;
            }
            else if (key == "attenuator") {
                m_Subtype = eSubtype_attenuator;
            }
            else if (key == "C_region") {
                m_Subtype = eSubtype_C_region;
            }
            else if (key == "CAAT_signal") {
                m_Subtype = eSubtype_CAAT_signal;
            }
            else if (key == "Imp_CDS") {
                m_Subtype = eSubtype_Imp_CDS;
            }
            else if (key == "conflict") {
                m_Subtype = eSubtype_conflict;
            }
            else if (key == "D_loop") {
                m_Subtype = eSubtype_D_loop;
            }
            else if (key == "D_segment") {
                m_Subtype = eSubtype_D_segment;
            }
            else if (key == "enhancer") {
                m_Subtype = eSubtype_enhancer;
            }
            else if (key == "exon") {
                m_Subtype = eSubtype_exon;
            }
            else if (key == "GC_signal") {
                m_Subtype = eSubtype_GC_signal;
            }
            else if (key == "iDNA") {
                m_Subtype = eSubtype_iDNA;
            }
            else if (key == "intron") {
                m_Subtype = eSubtype_intron;
            }
            else if (key == "J_segment") {
                m_Subtype = eSubtype_J_segment;
            }
            else if (key == "LTR") {
                m_Subtype = eSubtype_LTR;
            }
            else if (key == "mat_peptide") {
                m_Subtype = eSubtype_mat_peptide;
            }
            else if (key == "misc_binding") {
                m_Subtype = eSubtype_misc_binding;
            }
            else if (key == "misc_difference") {
                m_Subtype = eSubtype_misc_difference;
            }
            else if (key == "misc_feature") {
                m_Subtype = eSubtype_misc_feature;
            }
            else if (key == "misc_recomb") {
                m_Subtype = eSubtype_misc_recomb;
            }
            else if (key == "misc_RNA") {
                m_Subtype = eSubtype_misc_RNA;
            }
            else if (key == "misc_signal") {
                m_Subtype = eSubtype_misc_signal;
            }
            else if (key == "misc_structure") {
                m_Subtype = eSubtype_misc_structure;
            }
            else if (key == "modified_base") {
                m_Subtype = eSubtype_modified_base;
            }
            else if (key == "mutation") {
                m_Subtype = eSubtype_mutation;
            }
            else if (key == "N_region") {
                m_Subtype = eSubtype_N_region;
            }
            else if (key == "old_sequence") {
                m_Subtype = eSubtype_old_sequence;
            }
            else if (key == "polyA_signal") {
                m_Subtype = eSubtype_polyA_signal;
            }
            else if (key == "polyA_site") {
                m_Subtype = eSubtype_polyA_site;
            }
            else if (key == "precursor_RNA") {
                m_Subtype = eSubtype_precursor_RNA;
            }
            else if (key == "prim_transcript") {
                m_Subtype = eSubtype_prim_transcript;
            }
            else if (key == "primer_bind") {
                m_Subtype = eSubtype_primer_bind;
            }
            else if (key == "promoter") {
                m_Subtype = eSubtype_promoter;
            }
            else if (key == "protein_bind") {
                m_Subtype = eSubtype_protein_bind;
            }
            else if (key == "RBS") {
                m_Subtype = eSubtype_RBS;
            }
            else if (key == "repeat_region") {
                m_Subtype = eSubtype_repeat_region;
            }
            else if (key == "repeat_unit") {
                m_Subtype = eSubtype_repeat_unit;
            }
            else if (key == "rep_origin") {
                m_Subtype = eSubtype_rep_origin;
            }
            else if (key == "S_region") {
                m_Subtype = eSubtype_S_region;
            }
            else if (key == "satellite") {
                m_Subtype = eSubtype_satellite;
            }
            else if (key == "sig_peptide") {
                m_Subtype = eSubtype_sig_peptide;
            }
            else if (key == "source") {
                m_Subtype = eSubtype_source;
            }
            else if (key == "stem_loop") {
                m_Subtype = eSubtype_stem_loop;
            }
            else if (key == "STS") {
                m_Subtype = eSubtype_STS;
            }
            else if (key == "TATA_signal") {
                m_Subtype = eSubtype_TATA_signal;
            }
            else if (key == "terminator") {
                m_Subtype = eSubtype_terminator;
            }
            else if (key == "transit_peptide") {
                m_Subtype = eSubtype_transit_peptide;
            }
            else if (key == "unsure") {
                m_Subtype = eSubtype_unsure;
            }
            else if (key == "V_region") {
                m_Subtype = eSubtype_V_region;
            }
            else if (key == "V_segment") {
                m_Subtype = eSubtype_V_segment;
            }
            else if (key == "variation") {
                m_Subtype = eSubtype_variation;
            }
            else if (key == "virion") {
                m_Subtype = eSubtype_virion;
            }
            else if (key == "3clip") {
                m_Subtype = eSubtype_3clip;
            }
            else if (key == "3UTR") {
                m_Subtype = eSubtype_3UTR;
            }
            else if (key == "5clip") {
                m_Subtype = eSubtype_5clip;
            }
            else if (key == "5UTR") {
                m_Subtype = eSubtype_5UTR;
            }
            else if (key == "10_signal") {
                m_Subtype = eSubtype_10_signal;
            }
            else if (key == "35_signal") {
                m_Subtype = eSubtype_35_signal;
            }
            else if (key == "site_ref") {
                m_Subtype = eSubtype_site_ref;
            }
            else if (key == "preprotein") {
                m_Subtype = eSubtype_preprotein;
            }
            else if (key == "mat_peptide_aa") {
                m_Subtype = eSubtype_mat_peptide_aa;
            }
            else if (key == "sig_peptide_aa") {
                m_Subtype = eSubtype_sig_peptide_aa;
            }
            else if (key == "transit_peptide_aa") {
                m_Subtype = eSubtype_transit_peptide_aa;
            }
            else {
                m_Subtype = eSubtype_imp;
            }
            break;
        }
        case e_Region:          m_Subtype = eSubtype_region;          break;
        case e_Comment:         m_Subtype = eSubtype_comment;         break;
        case e_Bond:            m_Subtype = eSubtype_bond;            break;
        case e_Site:            m_Subtype = eSubtype_site;            break;
        case e_Rsite:           m_Subtype = eSubtype_rsite;           break;
        case e_User:            m_Subtype = eSubtype_user;            break;
        case e_Txinit:          m_Subtype = eSubtype_txinit;          break;
        case e_Num:             m_Subtype = eSubtype_num;             break;
        case e_Psec_str:        m_Subtype = eSubtype_psec_str;        break;
        case e_Non_std_residue: m_Subtype = eSubtype_non_std_residue; break;
        case e_Het:             m_Subtype = eSubtype_het;             break;
        case e_Biosrc:          m_Subtype = eSubtype_biosrc;          break;
        default:                m_Subtype = eSubtype_bad;             break;
        }
    } 
    return m_Subtype;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1894, CRC32: 86fb976 */
