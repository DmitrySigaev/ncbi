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
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
* ===========================================================================
*/
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/general/Dbtag.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_loc_mapper.hpp>

#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// CBioseqContext

// constructor
CBioseqContext::CBioseqContext
(const CBioseq_Handle& seq,
 CFlatFileContext& ffctx,
 CMasterContext* mctx) :
    m_Handle(seq),
    m_Repr(CSeq_inst::eRepr_not_set),
    m_Mol(CSeq_inst::eMol_not_set),
    m_HasParts(false),
    m_IsPart(false),
    m_PartNumber(0),
    m_IsDeltaLitOnly(false),
    m_IsProt(false),
    m_IsInGPS(false),
    m_IsGED(false),
    m_IsPDB(false),
    m_IsSP(false),
    m_IsTPA(false),
    m_IsRefSeq(false),
    m_RefseqInfo(0),
    m_IsGbGenomeProject(false),  // GenBank Genome project data (AE)
    m_IsNcbiCONDiv(false),       // NCBI CON division (CH)
    m_IsPatent(false),
    m_IsGI(false),
    m_IsWGS(false),
    m_IsWGSMaster(false),
    m_IsHup(false),
    m_Gi(0),
    m_ShowGBBSource(false),
    m_FFCtx(ffctx),
    m_Master(mctx)
{
    x_Init(seq, m_FFCtx.GetLocation());
}


// destructor
CBioseqContext::~CBioseqContext(void)
{
}


const CSeq_id& CBioseqContext::GetPreferredSynonym(const CSeq_id& id) const
{
    if ( id.IsGi()  &&  id.GetGi() == m_Gi ) {
        return *m_PrimaryId;
    }

    CBioseq_Handle h = m_Handle.GetScope().GetBioseqHandleFromTSE(id, m_Handle);
    if ( h ) {
        if ( h == m_Handle ) {
            return *m_PrimaryId;
        } else if ( h.GetSeqId().NotEmpty() ) {
            return *FindBestChoice(h.GetBioseqCore()->GetId(), CSeq_id::Score);
        }
    }

    return id;
}



// initialization
void CBioseqContext::x_Init(const CBioseq_Handle& seq, const CSeq_loc* user_loc)
{
    _ASSERT(seq);
    _ASSERT(seq.IsSetInst());

    // NB: order of execution is important
    x_SetId();
    m_Repr = x_GetRepr();
    m_Mol  = seq.GetInst_Mol();
    m_Molinfo.Reset(x_GetMolInfo());

    if ( IsSegmented() ) {
        m_HasParts = x_HasParts();
    }
    m_IsPart = x_IsPart();
    if ( m_IsPart ) {
        _ASSERT(m_Master);
        m_PartNumber = x_GetPartNumber();
    }
    if ( IsDelta() ) {
        m_IsDeltaLitOnly = x_IsDeltaLitOnly();
    }

    m_IsProt = CSeq_inst::IsAa(seq.GetInst_Mol());

    m_IsInGPS = x_IsInGPS();

    x_SetLocation(user_loc);
    
}


void CBioseqContext::x_SetLocation(const CSeq_loc* user_loc)
{
    _ASSERT(user_loc == 0  ||  user_loc->IsInt()  ||  user_loc->IsWhole());
    CRef<CSeq_loc> source;

    if ( user_loc != 0 ) {
        // map the location to the current bioseq
        CSeq_loc_Mapper mapper(m_Handle);
        source.Reset(mapper.Map(*user_loc));
        
        if ( source->IsWhole()  ||
             source->GetStart(kInvalidSeqPos) == 0  &&
             source->GetEnd(kInvalidSeqPos) == m_Handle.GetInst_Length() - 1) {
            source.Reset();
        }
        _ASSERT(!source  ||  source->IsInt());

        if ( source ) {
            CScope& scope = GetScope();

            CSeq_loc target;
            target.SetInt().SetFrom(0);
            target.SetInt().SetTo(GetLength(*source, &scope) - 1);
            target.SetId(*m_PrimaryId);

            m_Mapper.Reset(new CSeq_loc_Mapper(*source, target, &scope));
            m_Mapper->SetMergeAll();
        }
    }
    // if no location is specified do the entire sequence
    if ( !source ) {
        source.Reset(new CSeq_loc);
        source->SetWhole(*m_PrimaryId);
    }
    _ASSERT(source);
    m_Location = source;

    
}


void CBioseqContext::x_SetId(void)
{
    m_PrimaryId.Reset(new CSeq_id);
    m_PrimaryId->Assign(GetId(m_Handle, eGetId_Best));
    m_Accession = m_PrimaryId->GetSeqIdString(true);

    ITERATE (CBioseq::TId, id_iter, m_Handle.GetBioseqCore()->GetId()) {
        const CSeq_id& id = **id_iter;
        const CTextseq_id* tsip = id.GetTextseq_Id();
        const string& acc = (tsip != 0  &&  tsip->CanGetAccession()) ?
            tsip->GetAccession() : kEmptyStr;

        CSeq_id::EAccessionInfo acc_info = id.IdentifyAccession();
        unsigned int acc_type = acc_info & CSeq_id::eAcc_type_mask;
        unsigned int acc_div =  acc_info & CSeq_id::eAcc_division_mask;

        switch ( id.Which() ) {
        // Genbank, Embl or Ddbj
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            m_IsGED = true;
            m_IsGbGenomeProject = m_IsGbGenomeProject  ||
                ((acc_type & CSeq_id::eAcc_gb_genome) != 0);
            m_IsNcbiCONDiv = m_IsNcbiCONDiv  ||
                ((acc_type & CSeq_id::eAcc_gb_con) != 0);
            break;
        // Patent
        case CSeq_id::e_Patent:
            m_IsPatent = true;
            break;
        // RefSeq
        case CSeq_id::e_Other:
            m_IsRefSeq = true;
            m_RefseqInfo = acc_info;
            break;
        // Gi
        case CSeq_id::e_Gi:
            m_IsGI = true;
            m_Gi = id.GetGi();
            break;
        // PDB
        case CSeq_id::e_Pdb:
            m_IsPDB = true;
            break;
        // TPA
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            m_IsTPA = true;
            break;
        case CSeq_id::e_General:
            if ( id.GetGeneral().CanGetDb() ) {
                if ( !NStr::CompareCase(id.GetGeneral().GetDb(), "BankIt") ) {
                    m_IsTPA = true;
                }
            }
            break;
        // nothing special
        case CSeq_id::e_not_set:
        case CSeq_id::e_Local:
        case CSeq_id::e_Gibbsq:
        case CSeq_id::e_Gibbmt:
        case CSeq_id::e_Giim:
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
            m_IsSP = true;
            break;
        case CSeq_id::e_Prf:
        default:
            break;
        }

        // WGS
        m_IsWGS = m_IsWGS  ||  (acc_div == CSeq_id::eAcc_wgs);
        
        if ( m_IsWGS  &&  !acc.empty() ) {
            size_t len = acc.length();
            m_IsWGSMaster = 
                ((len == 12  ||  len == 15)  &&  NStr::EndsWith(acc, "000000"))  ||
                (len == 13  &&  NStr::EndsWith(acc, "0000000"));
            if ( m_IsWGSMaster ) {
                m_WGSMasterAccn = acc;
                m_WGSMasterName = tsip->CanGetName() ? tsip->GetName() : kEmptyStr;
            }
        } 

        // GBB source
        m_ShowGBBSource = m_ShowGBBSource  ||  
                          ((acc_type & CSeq_id::eAcc_gsdb_dirsub) != 0);
    }
}


CSeq_inst::TRepr CBioseqContext::x_GetRepr(void) const
{
    return m_Handle.IsSetInst_Repr() ?
        m_Handle.GetInst_Repr() : CSeq_inst::eRepr_not_set;
}


const CMolInfo* CBioseqContext::x_GetMolInfo(void) const
{
    CSeqdesc_CI desc(m_Handle, CSeqdesc::e_Molinfo);
    return desc ? &desc->GetMolinfo() : 0;
}


bool CBioseqContext::x_IsPart() const
{
    if ( m_Repr == CSeq_inst::eRepr_raw    ||
         m_Repr == CSeq_inst::eRepr_const  ||
         m_Repr == CSeq_inst::eRepr_delta  ||
         m_Repr == CSeq_inst::eRepr_virtual ) {
        CSeq_entry_Handle eh = m_Handle.GetParentEntry();
        _ASSERT(eh  &&  eh.IsSeq());

        eh = eh.GetParentEntry();
        if ( eh  &&  eh.IsSet() ) {
            CBioseq_set_Handle bsst = eh.GetSet();
            if ( bsst.IsSetClass()  &&  
                 bsst.GetClass() == CBioseq_set::eClass_parts ) {
                return true;
            }
        }
    }
    return false;
}


bool CBioseqContext::x_HasParts(void) const
{
    _ASSERT(IsSegmented());

    CSeq_entry_Handle h =
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if ( !h ) {
        return false;
    }
    
    // make sure the segmented set contains our bioseq
    {{
        bool has_seq = false;
        for ( CSeq_entry_CI it(h); it; ++it ) {
            if ( it->IsSeq()  &&  it->GetSeq() == m_Handle ) {
                has_seq = true;
                break;
            }
        }
        if ( !has_seq ) {
            return false;
        }
    }}

    // find the parts set
    {{
        for ( CSeq_entry_CI it(h); it; ++it ) {
            if ( it->IsSet()  &&  it->GetSet().IsSetClass()  &&
                it->GetSet().GetClass() == CBioseq_set::eClass_parts ) {
                return true;
            }
        }
    }}

    return false;
}


bool CBioseqContext::x_IsDeltaLitOnly(void) const
{
    _ASSERT(IsDelta());

    if ( m_Handle.IsSetInst_Ext() ) {
        const CBioseq_Handle::TInst_Ext& ext = m_Handle.GetInst_Ext();
        if ( ext.IsDelta() ) {
            ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                if ( (*it)->IsLoc() ) {
                    return false;
                }
            }
        }
    }
    return true;
}


SIZE_TYPE CBioseqContext::x_GetPartNumber(void)
{
    return m_Master ? m_Master->GetPartNumber(m_Handle) : 0;
}


bool CBioseqContext::x_IsInGPS(void) const
{
    CSeq_entry_Handle e = 
        m_Handle.GetExactComplexityLevel(CBioseq_set::eClass_gen_prod_set);
    return e;
}


bool CBioseqContext::DoContigStyle(void) const
{
    const CFlatFileConfig& cfg = Config();
    if ( cfg.IsStyleContig() ) {
        return true;
    } else if ( cfg.IsStyleNormal() ) {
        if ( (IsSegmented()  &&  !HasParts())  ||
             (IsDelta()  &&  !IsDeltaLitOnly()) ) {
            return true;
        }
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
//
// CMasterContext


CMasterContext::CMasterContext(const CBioseq_Handle& seq) :
    m_Handle(seq)
{
    _ASSERT(seq);
    _ASSERT(seq.GetInst_Ext().IsSeg());

    x_SetNumParts();
    x_SetBaseName();
}


CMasterContext::~CMasterContext(void)
{
}


SIZE_TYPE CMasterContext::GetPartNumber(const CBioseq_Handle& part)
{
    if ( !part ) {
        return 0;
    }
    CScope& scope = m_Handle.GetScope();

    SIZE_TYPE serial = 1;
    ITERATE (CSeg_ext::Tdata, it, m_Handle.GetInst_Ext().GetSeg().Get()) {
        if ( (*it)->IsNull() ) {
            continue;
        }
        const CSeq_id& id = GetId(**it);
        CBioseq_Handle bsh = scope.GetBioseqHandleFromTSE(id, m_Handle);
        if ( bsh   &&  bsh == part ) {
            return serial;
        }
        ++serial;
    }

    return 0;
}


void CMasterContext::x_SetNumParts(void)
{
    SIZE_TYPE count = 0;
    // count only non-gap parts
    ITERATE (CSeg_ext::Tdata, it, m_Handle.GetInst_Ext().GetSeg().Get()) {
        if ( (*it)->IsNull() ) {
            continue;
        }
        ++count;
    }
    m_NumParts = count;
}


void CMasterContext::x_SetBaseName(void)
{
    // !!!
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2004/04/27 15:12:24  shomrat
* Added logic for partial range formatting
*
* Revision 1.14  2004/04/22 15:52:00  shomrat
* Refactring of context
*
* Revision 1.13  2004/03/31 17:15:09  shomrat
* name changes
*
* Revision 1.12  2004/03/26 17:23:38  shomrat
* fixed initialization of m_IsWGS
*
* Revision 1.11  2004/03/25 20:36:02  shomrat
* Use handles
*
* Revision 1.10  2004/03/24 18:57:04  vasilche
* CBioseq_Handle::GetComplexityLevel() now returns CSeq_entry_Handle.
*
* Revision 1.9  2004/03/18 15:36:26  shomrat
* + new flag ShowFtableRefs
*
* Revision 1.8  2004/03/10 21:25:42  shomrat
* Fix m_RefseqInfo initialization
*
* Revision 1.7  2004/03/05 18:46:26  shomrat
* Added customization flags
*
* Revision 1.6  2004/02/19 18:03:29  shomrat
* changed implementation of GetPreferredSynonym
*
* Revision 1.5  2004/02/11 22:49:04  shomrat
* using types in flag file
*
* Revision 1.4  2004/02/11 16:49:16  shomrat
* added user customization flags
*
* Revision 1.3  2004/01/14 16:09:04  shomrat
* multiple changes (work in progress)
*
* Revision 1.2  2003/12/18 17:43:31  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:18:44  shomrat
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
