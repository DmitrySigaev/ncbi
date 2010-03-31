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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of bioseq_set 
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include "validatorp.hpp"
#include "utilities.hpp"

#include <objmgr/util/sequence.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_bioseqset::CValidError_bioseqset(CValidError_imp& imp) :
    CValidError_base(imp) , m_AnnotValidator(imp) , m_DescrValidator(imp) , m_BioseqValidator(imp)
{
}


CValidError_bioseqset::~CValidError_bioseqset(void)
{
}


void CValidError_bioseqset::ValidateBioseqSet(const CBioseq_set& seqset)
{
    int protcnt = 0;
    int nuccnt  = 0;
    int segcnt  = 0;
    
    // Validate Set Contents
    FOR_EACH_SEQENTRY_ON_SEQSET (se_list_it, seqset) {
        if ( (*se_list_it)->IsSet() ) {
            const CBioseq_set& set = (*se_list_it)->GetSet();
            // validate member set
            ValidateBioseqSet (set);

            // look for internal genbank sets
            if ( set.IsSetClass() 
                 && set.GetClass() == CBioseq_set::eClass_genbank ) {

                PostErr(eDiag_Warning, eErr_SEQ_PKG_InternalGenBankSet,
                         "Bioseq-set contains internal GenBank Bioseq-set",
                         seqset);
            }
        } else if ((*se_list_it)->IsSeq()) {
            const CBioseq& seq = (*se_list_it)->GetSeq();

            // Validate Member Seq
            m_BioseqValidator.ValidateBioseq(seq);
        }
    }

    // note - need to do this with an iterator, so that we count sequences in subsets
    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    for (; seqit; ++seqit) {

        if ( seqit->IsAa() ) {
            protcnt++;
        } else if ( seqit->IsNa() ) {
            nuccnt++;
        }

        if (seqit->GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
            segcnt++;
        }
    }

    switch ( seqset.GetClass() ) {
	case CBioseq_set::eClass_not_set:
        PostErr(eDiag_Warning, eErr_SEQ_PKG_BioseqSetClassNotSet, 
			    "Bioseq_set class not set", seqset);
		break;
    case CBioseq_set::eClass_nuc_prot:
        ValidateNucProtSet(seqset, nuccnt, protcnt, segcnt);
        break;
        
    case CBioseq_set::eClass_segset:
        ValidateSegSet(seqset, segcnt);
        break;
        
    case CBioseq_set::eClass_parts:
        ValidatePartsSet(seqset);
        break;
        
    case CBioseq_set::eClass_genbank:
        ValidateGenbankSet(seqset);
        break;
        
    case CBioseq_set::eClass_pop_set:
        ValidatePopSet(seqset);
        break;
        
    case CBioseq_set::eClass_mut_set:
    case CBioseq_set::eClass_phy_set:
    case CBioseq_set::eClass_eco_set:
    case CBioseq_set::eClass_wgs_set:
        ValidatePhyMutEcoWgsSet(seqset);
        break;
        
    case CBioseq_set::eClass_gen_prod_set:
        ValidateGenProdSet(seqset);
        break;
    case CBioseq_set::eClass_conset:
        if (!m_Imp.IsRefSeq()) {
            PostErr (eDiag_Error, eErr_SEQ_PKG_ConSetProblem, 
                     "Set class should not be conset", seqset);
        }
        break;
    /*
    case CBioseq_set::eClass_other:
        PostErr(eDiag_Critical, eErr_SEQ_PKG_GenomicProductPackagingProblem, 
            "Genomic product set class incorrectly set to other", seqset);
        break;
    */
    default:
        if ( nuccnt == 0  &&  protcnt == 0 )  {
            PostErr(eDiag_Warning, eErr_SEQ_PKG_EmptySet, 
                "No Bioseqs in this set", seqset);
        }
        break;
    }

    // validate annots
    FOR_EACH_SEQANNOT_ON_SEQSET (annot_it, seqset) {
        m_AnnotValidator.ValidateSeqAnnot (**annot_it);
        m_AnnotValidator.ValidateSeqAnnotContext (**annot_it, seqset);
    }
    if (seqset.IsSetDescr()) {
        CBioseq_set_Handle bsh = m_Scope->GetBioseq_setHandle(seqset);
        if (bsh) {
            CSeq_entry_Handle ctx = bsh.GetParentEntry();
            if (ctx) {
                m_DescrValidator.ValidateSeqDescr (seqset.GetDescr(), *(ctx.GetCompleteSeq_entry()));
            }
        }
    }
}


// =============================================================================
//                                     Private
// =============================================================================


bool CValidError_bioseqset::IsMrnaProductInGPS(const CBioseq& seq)
{
    if ( m_Imp.IsGPS() ) {
        CFeat_CI mrna(
            m_Scope->GetBioseqHandle(seq), 
            SAnnotSelector(CSeqFeatData::e_Rna)
            .SetByProduct());
        return (bool)mrna;
    }
    return true;
}


bool CValidError_bioseqset::IsCDSProductInGPS(const CBioseq& seq, const CBioseq_set& gps)
{
	if (gps.IsSetSeq_set() && gps.GetSeq_set().size() > 0
		&& gps.GetSeq_set().front()->IsSeq()) {
		FOR_EACH_SEQANNOT_ON_SEQENTRY (annot_it, *(gps.GetSeq_set().front())) {
			FOR_EACH_SEQFEAT_ON_SEQANNOT (feat_it, **annot_it) {
				if ((*feat_it)->IsSetData()
					&& (*feat_it)->GetData().IsCdregion()
					&& (*feat_it)->IsSetProduct()) {
					const CSeq_id *id = (*feat_it)->GetProduct().GetId();
					if (id) {
						FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
							if ((*id_it)->Compare(*id) == CSeq_id::e_YES) {
								return true;
							}
						}
					}
				}
			}
		}
	}
    return false;
}


void CValidError_bioseqset::ValidateNucProtSet
(const CBioseq_set& seqset,
 int nuccnt, 
 int protcnt,
 int segcnt)
{
    if ( nuccnt == 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                 "No nucleotides in nuc-prot set", seqset);
    } else if ( nuccnt > 1 && segcnt != 1) {
        PostErr(eDiag_Critical, eErr_SEQ_PKG_NucProtProblem,
                 "Multiple unsegmented nucleotides in nuc-prot set", seqset);
    }
    if ( protcnt == 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                 "No proteins in nuc-prot set", seqset);
    }

    int prot_biosource = 0;

    FOR_EACH_SEQENTRY_ON_SEQSET (se_list_it, seqset) {
        if ( (*se_list_it)->IsSeq() ) {
            const CBioseq& seq = (*se_list_it)->GetSeq();
            CBioseq_set_Handle gps = GetGenProdSetParent(m_Scope->GetBioseqHandle(seq));
			if (seq.IsNa()) {
                if (gps  &&  !IsMrnaProductInGPS(seq) ) {
                    PostErr(eDiag_Warning,
                        eErr_SEQ_PKG_GenomicProductPackagingProblem,
                        "Nucleotide bioseq should be product of mRNA "
                        "feature on contig, but is not",
                        seq);
				}
            } else if ( seq.IsAa() ) {
                if (gps && !IsCDSProductInGPS(seq, *(gps.GetCompleteBioseq_set())) ) {
                    PostErr(eDiag_Warning,
                        eErr_SEQ_PKG_GenomicProductPackagingProblem,
                        "Protein bioseq should be product of CDS "
                        "feature on contig, but is not",
                        seq);
                }
                FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
                    if ((*it)->IsSource()) {
                        prot_biosource++;
                    }
                }
            }
        }

        if ( !(*se_list_it)->IsSet() )
            continue;

        const CBioseq_set& set = (*se_list_it)->GetSet();
        if ( set.GetClass() != CBioseq_set::eClass_segset ) {

            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class = tv->FindName(set.GetClass(), true);

            PostErr(eDiag_Critical, eErr_SEQ_PKG_NucProtNotSegSet,
                     "Nuc-prot Bioseq-set contains wrong Bioseq-set, "
                     "its class is \"" + set_class + "\".", set);
            break;
        }
    }
    if (prot_biosource > 1) {
        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceOnProtein,
                 "Nuc-prot set has " + NStr::IntToString (prot_biosource)
                 + " proteins with a BioSource descriptor", seqset);
    } else if (prot_biosource > 0) {
        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceOnProtein,
                 "Nuc-prot set has 1 protein with a BioSource descriptor", seqset);
    }

    bool has_source = false;
    bool has_title = false;
    FOR_EACH_DESCRIPTOR_ON_SEQSET (it, seqset) {
        if ((*it)->IsSource()
            && (*it)->GetSource().IsSetOrg()
            && (*it)->GetSource().GetOrg().IsSetTaxname()
            && !NStr::IsBlank ((*it)->GetSource().GetOrg().GetTaxname())) {
            has_source = true;
        } else if ((*it)->IsTitle()) {
            has_title = true;
        }
        if (has_title && has_source) {
            break;
        }
    }

    if (!has_source) {
        // error if does not have source and is not genprodset
        CBioseq_set_Handle gps = GetGenProdSetParent (m_Scope->GetBioseq_setHandle (seqset));
        if (!gps) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceMissing,
                     "Nuc-prot set does not contain expected BioSource descriptor", seqset);
        }
    }

    if (has_title) {
        PostErr (eDiag_Warning, eErr_SEQ_PKG_NucProtSetHasTitle,
                 "Nuc-prot set should not have title descriptor", seqset);
    }
}


void CValidError_bioseqset::CheckForInconsistentBiomols (const CBioseq_set& seqset)
{
    if (!seqset.IsSetClass()) {
        return;
    }            
    
    CTypeConstIterator<CMolInfo> miit(ConstBegin(seqset));
    const CMolInfo* mol_info = 0;
    
    for (; miit; ++miit) {
        if (!miit->IsSetBiomol() || miit->GetBiomol() == CMolInfo::eBiomol_peptide) {
            continue;
        }
        if (mol_info == 0) {
            mol_info = &(*miit);
        } else if (mol_info->GetBiomol() != miit->GetBiomol() ) {
            if (seqset.GetClass() == CBioseq_set::eClass_segset) {
                PostErr(eDiag_Error, eErr_SEQ_PKG_InconsistentMolInfoBiomols,
                    "Segmented set contains inconsistent MolInfo biomols",
                    seqset);
            } else if (seqset.GetClass() == CBioseq_set::eClass_pop_set
                       || seqset.GetClass() == CBioseq_set::eClass_eco_set
                       || seqset.GetClass() == CBioseq_set::eClass_mut_set
                       || seqset.GetClass() == CBioseq_set::eClass_phy_set
                       || seqset.GetClass() == CBioseq_set::eClass_wgs_set) {
                PostErr(eDiag_Warning, eErr_SEQ_PKG_InconsistentMolInfoBiomols,
                    "Pop/phy/mut/eco set contains inconsistent MolInfo biomols",
                    seqset);
            }
            break;
        }
    } // for

}


void CValidError_bioseqset::ValidateSegSet(const CBioseq_set& seqset, int segcnt)
{
    if ( segcnt == 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_SegSetProblem,
            "No segmented Bioseq in segset", seqset);
    }

    CSeq_inst::EMol     mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol     seq_inst_mol;

    FOR_EACH_SEQENTRY_ON_SEQSET (se_list_it, seqset) {
        if ( (*se_list_it)->IsSeq() ) {
            const CSeq_inst& seq_inst = (*se_list_it)->GetSeq().GetInst();
            
            if ( mol == CSeq_inst::eMol_not_set ||
                 mol == CSeq_inst::eMol_other ) {
                mol = seq_inst.GetMol();
            } else if ( (seq_inst_mol = seq_inst.GetMol()) != CSeq_inst::eMol_other) {
                if ( seq_inst.IsNa() != CSeq_inst::IsNa(mol) ) {
                    PostErr(eDiag_Critical, eErr_SEQ_PKG_SegSetMixedBioseqs,
                        "Segmented set contains mixture of nucleotides"
                        " and proteins", seqset);
                    break;
                }
            }
        } else if ( (*se_list_it)->IsSet() ) {
            const CBioseq_set& set = (*se_list_it)->GetSet();

            if ( set.IsSetClass()  &&  
                 set.GetClass() != CBioseq_set::eClass_parts ) {
                const CEnumeratedTypeValues* tv = 
                    CBioseq_set::GetTypeInfo_enum_EClass();
                const string& set_class_str = 
                    tv->FindName(set.GetClass(), true);
                
                PostErr(eDiag_Critical, eErr_SEQ_PKG_SegSetNotParts,
                    "Segmented set contains wrong Bioseq-set, "
                    "its class is \"" + set_class_str + "\".", set);
                break;
            }
        } // else if
    } // iterate
    
    CheckForInconsistentBiomols (seqset);
}


void CValidError_bioseqset::ValidatePartsSet(const CBioseq_set& seqset)
{
    CSeq_inst::EMol     mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol     seq_inst_mol;

    FOR_EACH_SEQENTRY_ON_SEQSET (se_list_it, seqset) {
        if ( (*se_list_it)->IsSeq() ) {
            const CSeq_inst& seq_inst = (*se_list_it)->GetSeq().GetInst();

            if ( mol == CSeq_inst::eMol_not_set  ||
                 mol == CSeq_inst::eMol_other ) {
                mol = seq_inst.GetMol();
            } else  {
                seq_inst_mol = seq_inst.GetMol();
                if ( seq_inst_mol != CSeq_inst::eMol_other) {
                    if ( seq_inst.IsNa() != CSeq_inst::IsNa(mol) ) {
                        PostErr(eDiag_Critical, eErr_SEQ_PKG_PartsSetMixedBioseqs,
                                 "Parts set contains mixture of nucleotides "
                                 "and proteins", seqset);
                    }
                }
            }
        } else if ( (*se_list_it)->IsSet() ) {
            const CBioseq_set& set = (*se_list_it)->GetSet();
            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class_str = 
                tv->FindName(set.GetClass(), true);

            PostErr(eDiag_Critical, eErr_SEQ_PKG_PartsSetHasSets,
                    "Parts set contains unwanted Bioseq-set, "
                    "its class is \"" + set_class_str + "\".", set);
        } // else if
    } // for
}


void CValidError_bioseqset::ValidateGenbankSet(const CBioseq_set& seqset)
{
}


void CValidError_bioseqset::ValidatePopSet(const CBioseq_set& seqset)
{
    const CBioSource*   biosrc  = 0;
    static const string sp = " sp. ";

	if (m_Imp.IsRefSeq()) {
		PostErr (eDiag_Critical, eErr_SEQ_PKG_RefSeqPopSet,
				"RefSeq record should not be a Pop-set", seqset);
	}

    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    string first_taxname = "";
    bool is_first = true;
    for (; seqit; ++seqit) {
        
        biosrc = 0;
        
        // Will get the first biosource either from the descriptor
        // or feature.
        CTypeConstIterator<CBioSource> biosrc_it(ConstBegin(*seqit));
        if (biosrc_it) {
            biosrc = &(*biosrc_it);
        } 
        
        if (biosrc == 0)
            continue;
        
        string taxname = "";
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetTaxname()) {
            taxname = biosrc->GetOrg().GetTaxname();
        }
        if (is_first) {
            first_taxname = taxname;
            is_first = false;
            continue;
        }
        
        // Make sure all the taxnames in the set are the same.
        if ( NStr::CompareNocase(first_taxname, taxname) == 0 ) {
            continue;
        }
        
        // drops severity if first mismatch is same up to sp.
        EDiagSev sev = eDiag_Error;
        SIZE_TYPE pos = NStr::Find(taxname, sp);
        if ( pos != NPOS ) {
            SIZE_TYPE len = pos + sp.length();
            if ( NStr::strncasecmp(first_taxname.c_str(), 
                                   taxname.c_str(),
                                   len) == 0 ) {
                sev = eDiag_Warning;
            }
        }
        // drops severity if one name is subset of the other
        SIZE_TYPE comp_len = min (taxname.length(), first_taxname.length());
        if (NStr::EqualCase(taxname, 0, comp_len, first_taxname)) {
            sev = eDiag_Warning;
        }

        PostErr(sev, eErr_SEQ_DESCR_InconsistentBioSources,
            "Population set contains inconsistent organisms.",
            seqset);
        break;
    }
    CheckForInconsistentBiomols (seqset);
    bool has_title = false;
    FOR_EACH_DESCRIPTOR_ON_SEQSET (it, seqset) {
        if ((*it)->IsTitle()) {
            has_title = true;
            break;
        }
    }
    if (!has_title) {
        PostErr(eDiag_Warning, eErr_SEQ_PKG_MissingSetTitle,
            "Pop/Phy/Mut/Eco set does not have title",
            seqset);
    }
}


void CValidError_bioseqset::ValidatePhyMutEcoWgsSet(const CBioseq_set& seqset)
{
    CheckForInconsistentBiomols (seqset);
    if (seqset.GetClass() != CBioseq_set::eClass_wgs_set) {
        bool has_title = false;
        FOR_EACH_DESCRIPTOR_ON_SEQSET (it, seqset) {
            if ((*it)->IsTitle()) {
                has_title = true;
                break;
            }
        }
        if (!has_title) {
            PostErr(eDiag_Warning, eErr_SEQ_PKG_MissingSetTitle,
                "Pop/Phy/Mut/Eco set does not have title",
                seqset);
        }
    }
}


void CValidError_bioseqset::ValidateGenProdSet(const CBioseq_set& seqset)
{
    bool                id_no_good = false;
    CSeq_id::E_Choice   id_type = CSeq_id::e_not_set;
    
    // genprodset should not have annotations directly on set
    if (seqset.IsSetAnnot()) {
        PostErr(eDiag_Critical,
            eErr_SEQ_PKG_GenomicProductPackagingProblem,
            "Seq-annot packaged directly on genomic product set", seqset);
    }

    CBioseq_set::TSeq_set::const_iterator se_list_it =
        seqset.GetSeq_set().begin();
    
    if ( !(**se_list_it).IsSeq() ) {
        return;
    }
    
    const CBioseq& seq = (*se_list_it)->GetSeq();
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CFeat_CI fi(bsh, CSeqFeatData::e_Rna);
    for (; fi; ++fi) {
        if ( fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA ) {
            if ( fi->IsSetProduct() ) {
                CBioseq_Handle cdna = 
                    m_Scope->GetBioseqHandle(fi->GetProduct());
                if ( !cdna ) {
                    try {
                        const CSeq_id& id = GetId(fi->GetProduct(), m_Scope);
                        id_type = id.Which();
					} catch (CException ) {
                        id_no_good = true;
					} catch (std::exception ) {
                        id_no_good = true;
                    }
                    
                    // okay to have far RefSeq product
                    if ( id_no_good  ||  (id_type != CSeq_id::e_Other) ) {
                        string loc_label;
                        fi->GetProduct().GetLabel(&loc_label);
                        
                        if (loc_label.empty()) {
                            loc_label = "?";
                        }
                        
                        PostErr(eDiag_Warning,
                            eErr_SEQ_PKG_GenomicProductPackagingProblem,
                            "Product of mRNA feature (" + loc_label +
                            ") not packaged in genomic product set", seq);
                        
                    }
                } // if (cdna == 0)
            } else {
                PostErr(eDiag_Error,
                    eErr_SEQ_PKG_GenomicProductPackagingProblem,
                    "Product of mRNA feature (?) not packaged in"
                    "genomic product set", seq);
            }
        }
    } // for 
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
