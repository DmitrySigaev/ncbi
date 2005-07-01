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
 *   'seqset.asn'.
 *
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>
#include <objects/general/cleanup_utils.hpp>

// generated includes
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp> // to make KCC happy
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CBioseq_set::~CBioseq_set(void)
{
}


static bool s_is_na(const CBioseq& seq)
{
    switch (seq.GetInst().GetMol()) {
    case CSeq_inst::eMol_dna:
    case CSeq_inst::eMol_rna:
    case CSeq_inst::eMol_na:
        return true;
    default:
        return false;
    }
}


static bool s_has_gb(const CSeq_id& id)
{
    switch (id.Which()) {
    case CSeq_id::e_Genbank:
    case CSeq_id::e_Embl:
    case CSeq_id::e_Ddbj:
    case CSeq_id::e_Other:
    case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:
    case CSeq_id::e_Tpd:
        return true;
    default:
        return false;
    }
}


static bool s_has_accession(const CSeq_id& id)
{
    if (!id.GetTextseq_Id()) {
        return false;
    } else if (id.GetTextseq_Id()->IsSetAccession()) {
        return true;
    } else {
        return false;
    }
}

void CBioseq_set::GetLabel(string* label, ELabelType type) const
{
    // If no label, just return
    if (!label) {
        return;
    }

    // Get type label
    if (IsSetClass()  &&  type != eContent) {
        const CEnumeratedTypeValues* tv =
            CBioseq_set::GetTypeInfo_enum_EClass();
        const string& cn = tv->FindName(GetClass(), true);
        *label += cn;

        if (type != eType) {
            *label += ": ";
        }
    }

    if (type == eType) {
        return;
    }

    // Loop through CBioseqs looking for the best one to use for a label
    bool best_is_na = false, best_has_gb = false, best_has_accession = false;
    const CBioseq* best = 0;
    for (CTypeConstIterator<CBioseq> si(ConstBegin(*this)); si; ++si) {
        bool takeit = false, is_na, has_gb = false, has_accession = false;
        is_na = s_is_na(*si);
        for (CTypeConstIterator<CSeq_id> ii(ConstBegin(*si)); ii; ++ii) {
            has_gb = has_gb ? true : s_has_gb(*ii);
            has_accession = has_accession ? true : s_has_accession(*ii);
        }

        if (!best) {
            takeit = true;
        } else {
            bool longer = false;
            if (si->GetInst().GetLength() > best->GetInst().GetLength()) {
                longer = true;
            }
            if(best_has_accession) {
                if (has_accession) {
                    if(longer) {
                        takeit = true;
                    }
                }
            } else if (has_accession) {
                takeit = true;
            } else if (best_has_gb) {
                if (has_gb) {
                    if (longer) {
                        takeit = true;
                    }
                }
            } else if (has_gb) {
                takeit = true;
            } else if (best_is_na) {
                if (is_na) {
                    if (longer) {
                        takeit = true;
                    }
                }
            } else if (is_na) {
                takeit = true;
            } else if (longer) {
                takeit = true;
            }
        }

        if (takeit) {
            best = &(*si);
            best_has_accession = has_accession;
            best_has_gb = has_gb;
            best_is_na = is_na;
        }
    }

    // Add content to label.
    if (!best) {
        *label += "(No Bioseqs)";
    } else {
        CNcbiOstrstream os;
        if (best->GetFirstId()) {
            os << best->GetFirstId()->DumpAsFasta();
            *label += CNcbiOstrstreamToString(os);
        }
    }
}


static ECleanupMode s_GetCleanupMode(const CBioseq_set& bsst)
{
    CTypeConstIterator<CBioseq> seq(ConstBegin(bsst));
    if (seq) {
        ITERATE (CBioseq::TId, it, seq->GetId()) {
            const CSeq_id& id = **it;
            if (id.IsEmbl()  ||  id.IsTpe()) {
                return eCleanup_EMBL;
            } else if (id.IsDdbj()  ||  id.IsTpd()) {
                return eCleanup_DDBJ;
            } else if (id.IsSwissprot()) {
                return eCleanup_SwissProt;
            }
        }
    }
    return eCleanup_GenBank;
}


void CBioseq_set::BasicCleanup(void)
{
    ECleanupMode mode = s_GetCleanupMode(*this);

    if (IsSetAnnot()) {
        NON_CONST_ITERATE (TAnnot, it, SetAnnot()) {
            (*it)->BasicCleanup(mode);
        }
    }
    if (IsSetDescr()) {
        SetDescr().BasicCleanup(mode);
    }
    if (IsSetSeq_set()) {
        NON_CONST_ITERATE (TSeq_set, it, SetSeq_set()) {
            (*it)->BasicCleanup();
        }
    }
}


const CBioseq& CBioseq_set::GetNucFromNucProtSet(void) const
{
    if (GetClass() != eClass_nuc_prot) {
        NCBI_THROW(CException, eUnknown,
            "CBioseq_set::GetNucFromNucProtSet() : incompatible class (" +
            ENUM_METHOD_NAME(EClass)()->FindName(GetClass(), true) + ")");
    }

    ITERATE (TSeq_set, it, GetSeq_set()) {
        const CSeq_entry& se = **it;
        if (se.IsSeq()  &&  se.GetSeq().IsNa()) {
            return se.GetSeq();
        } else if (se.IsSet()  &&
            se.GetSet().GetClass() == CBioseq_set::eClass_segset) {
            return se.GetSet().GetMasterFromSegSet();
        }
    }

    NCBI_THROW(CException, eUnknown, 
        "CBioseq_set::GetNucFromNucProtSet() : \
        nuc-prot set doesn't contain the nucleotide bioseq");
}


const CBioseq& CBioseq_set::GetGenomicFromGenProdSet(void) const
{
    if (GetClass() != eClass_gen_prod_set) {
         NCBI_THROW(CException, eUnknown,
            "CBioseq_set::GetGenomicFromGenProdSet() : incompatible class (" +
            ENUM_METHOD_NAME(EClass)()->FindName(GetClass(), true) + ")");
    }

    ITERATE (TSeq_set, it, GetSeq_set()) {
        if ((*it)->IsSeq()) {
            const CBioseq& seq = (*it)->GetSeq();
            if (seq.GetInst().IsSetMol()  &&
                seq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
                return seq;
            }
        }
    }

    NCBI_THROW(CException, eUnknown, 
        "CBioseq_set::GetGenomicFromGenProdSet() : \
        gen-prod set doesn't contain the genomic bioseq");
}


const CBioseq& CBioseq_set::GetMasterFromSegSet(void) const
{
    if (GetClass() != eClass_segset) {
         NCBI_THROW(CException, eUnknown,
            "CBioseq_set::GetMasterFromSegSet() : incompatible class (" +
            ENUM_METHOD_NAME(EClass)()->FindName(GetClass(), true) + ")");
    }

    ITERATE (TSeq_set, it, GetSeq_set()) {
        if ((*it)->IsSeq()) {
            return (*it)->GetSeq();
        }
    }

    NCBI_THROW(CException, eUnknown, 
        "CBioseq_set::GetMasterFromSegSet() : \
        segset set doesn't contain the master bioseq");
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/07/01 15:08:14  shomrat
 * Added Class specific methods
 *
 * Revision 1.10  2005/05/20 13:35:14  shomrat
 * Added BasicCleanup()
 *
 * Revision 1.9  2004/05/19 17:26:48  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 1.8  2002/12/30 13:21:24  clausen
 * Replaced os.str() with CNcbiOstrstreamToString(os)
 *
 * Revision 1.7  2002/10/07 17:10:54  ucko
 * Include Seq-annot.hpp to make KCC happy.
 *
 * Revision 1.6  2002/10/03 18:57:12  clausen
 * Removed extra whitespace
 *
 * Revision 1.5  2002/10/03 17:18:19  clausen
 * Added GetLabel()
 *
 * Revision 1.4  2000/11/01 20:38:33  vasilche
 * Removed ECanDelete enum and related constructors.
 *
 *
 * ===========================================================================
 */
