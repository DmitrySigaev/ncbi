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
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/general/Dbtag.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// CFFContext


CFFContext::CFFContext
(CScope& scope,
 TFormat format,
 TMode mode,
 TStyle style,
 TFilter filter,
 TFlags flags) :
    m_Format(format), m_Mode(mode), m_Style(style), m_FilterFlags(filter),
    m_Flags(mode, flags),
    m_TSE(0), m_Scope(&scope), m_SeqSub(0),
    m_Master(0), m_Bioseq(0)
{
}


CFFContext::~CFFContext(void)
{
}


void CFFContext::SetMasterBioseq(const CBioseq& seq)
{
    m_Master.Reset(new CMasterContext(seq, *this));
}


void CFFContext::SetActiveBioseq(const CBioseq& seq)
{
    m_Bioseq.Reset(new CBioseqContext(seq, *this));
}


/////////////////////////////////////////////////////////////////////////////
//
// CMasterContext


CMasterContext::CMasterContext(const CBioseq& seq, CFFContext& ctx) :
    m_Seq(&seq), m_Ctx(&ctx)
{
    _ASSERT(seq.CanGetInst());
    _ASSERT(seq.GetInst().CanGetExt());
    _ASSERT(seq.GetInst().GetExt().IsSeg());

    m_Handle = m_Ctx->GetScope().GetBioseqHandle(seq);

    x_SetBaseName(seq);
}


CMasterContext::~CMasterContext(void)
{
}


SIZE_TYPE CMasterContext::GetPartNumber(const CBioseq_Handle& h) const
{
    if ( !h ) {
        return 0;
    }

    CScope& scope = m_Ctx->GetScope();

    SIZE_TYPE serial = 1;
    ITERATE (CSeg_ext::Tdata, it, m_Seq->GetInst().GetExt().GetSeg().Get()) {
        if ( (*it)->IsNull() ) {
            continue;
        }

        CBioseq_Handle bsh = scope.GetBioseqHandle(**it);
        if ( bsh   &&  bsh == h ) {
            return serial;
        }
        ++serial;
    }

    return 0;
}


SIZE_TYPE CMasterContext::GetNumSegments(void) const
{
    SIZE_TYPE count = 0;
    ITERATE (CSeg_ext::Tdata, it, m_Seq->GetInst().GetExt().GetSeg().Get()) {
        if ( (*it)->IsNull() ) {
            continue;
        }
        ++count;
    }

    return count;
}


void CMasterContext::x_SetBaseName(const CBioseq& seq)
{
    // !!!
}


/////////////////////////////////////////////////////////////////////////////
//
// CBioseqContext

CBioseqContext::CBioseqContext(const CBioseq& seq, CFFContext& ctx) :
    m_Repr(CSeq_inst::eRepr_not_set),
    m_Mol(CSeq_inst::eMol_not_set),
    m_Accession(kEmptyStr),
    m_PrimaryId(0),
    m_WGSMasterAccn(kEmptyStr),
    m_WGSMasterName(kEmptyStr),
    m_SegsCount(0),
    m_IsPart(false),
    m_PartNumber(0),
    m_HasParts(false),
    m_IsDeltaLitOnly(false),
    m_IsProt(false),
    m_IsGPS(false),
    m_IsGED(false),
    m_IsPDB(false),
    m_IsSP(false),
    m_IsTPA(false),
    m_IsRefSeq(false),
    m_RefseqInfo(0),
    m_IsGbGenomeProject(false),       // GenBank Genome project data (AE)
    m_IsNcbiCONDiv(false),            // NCBI CON division (CH)
    m_IsPatent(false),
    m_IsGI(false),
    m_IsWGS(false),
    m_IsWGSMaster(false),
    m_IsHup(false),
    m_GI(0),
    m_ShowGBBSource(false),
    m_Location(0),
    m_Ctx(&ctx),
    m_Master(ctx.Master())
{
    if ( !seq.CanGetInst() ) {
        return;
        //NCBI_THROW(...) !!!
    }

    m_Handle = ctx.GetScope().GetBioseqHandle(seq);
    if ( !m_Handle ) {
        //NCBI_THROW(...) !!!
    }

    // NB: order of execution is important
    m_Repr = x_GetRepr(seq);
    m_Molinfo.Reset(x_GetMolinfo());
    if ( seq.GetInst().CanGetMol() ) {
        m_Mol  = seq.GetInst().GetMol();
    }
    if ( IsSegmented() ) {
        //m_SegsCount = x_CountSegs(seq);
        m_HasParts = x_HasParts(seq);
    }
    if ( IsDelta() ) {
        m_IsDeltaLitOnly = x_IsDeltaLitOnly(seq);
    }

    m_IsPart = x_IsPart(seq);
    if ( IsPart() ) {
        if ( !m_Master ) {
            m_Ctx->SetMasterBioseq(x_GetMasterForPart(seq));
            m_Master = m_Ctx->Master();
        }
        m_PartNumber = x_GetPartNumber(m_Handle);
    }

    m_IsProt = seq.IsAa();

    x_SetId(seq);
    
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole(*m_PrimaryId);
    m_Location.Reset(loc);
}


// count the number of non-virtual parts
SIZE_TYPE CBioseqContext::x_CountSegs(const CBioseq& seq) const
{
    const CSeq_inst& inst = seq.GetInst();
    CScope& scope = m_Ctx->GetScope();

    if ( !inst.CanGetExt()  ||  !inst.GetExt().IsSeg() ) {
        return 0;
    }

    SIZE_TYPE num_parts = 0;
    ITERATE(CSeg_ext::Tdata, loc, inst.GetExt().GetSeg().Get()) {
        try {
            const CSeq_id& id = GetId(**loc, &scope);
            if ( id.IsGi() ) {
                CBioseq_Handle bsh = scope.GetBioseqHandle(id);
                if ( !bsh ) {
                    continue;
                }
                const CBioseq& part = bsh.GetBioseq();
                CSeq_inst::TRepr part_repr = x_GetRepr(part);
                
                ITERATE(CBioseq::TId, part_id, part.GetId()) {
                    if ( (*part_id)->IsGibbsq()  ||
                         (*part_id)->IsGibbmt()  ||
                         (*part_id)->IsGiim() ) {
                        if ( part_repr == CSeq_inst::eRepr_virtual ) {
                            continue;
                        }
                    }
                }
            }
            ++num_parts;
        } catch ( CException& ) {
            // just continue to the next segment
        }
    }
    return num_parts;
}


CSeq_inst::TRepr CBioseqContext::x_GetRepr(const CBioseq& seq) const
{
    try
    {
        return seq.GetInst().GetRepr();
    } catch ( CException& ) {
        return CSeq_inst::eRepr_not_set;
    }
}


const CMolInfo* CBioseqContext::x_GetMolinfo(void) 
{
    CSeqdesc_CI mi(GetHandle(), CSeqdesc::e_Molinfo);
    return mi ? &(mi->GetMolinfo()) : 0;
}

/*
CMolInfo::TTech CBioseqContext::x_GetTech(void)
{
    CSeqdesc_CI mi(GetHandle(), CSeqdesc::e_Molinfo);
    return mi->GetMolinfo().CanGetTech() ? mi->GetMolinfo().GetTech() :
        CMolInfo::eTech_unknown;
}
*/


bool CBioseqContext::x_IsPart(const CBioseq& seq) const
{
    if ( m_Repr == CSeq_inst::eRepr_raw    ||
         m_Repr == CSeq_inst::eRepr_const  ||
         m_Repr == CSeq_inst::eRepr_delta  ||
         m_Repr == CSeq_inst::eRepr_virtual ) {
        const CSeq_entry* e = seq.GetParentEntry();
        _ASSERT(e != 0  &&  e->IsSeq());

        e = e->GetParentEntry();
        if ( e != 0  &&  e->IsSet() ) {
            const CBioseq_set& bsst = e->GetSet();
            if ( bsst.CanGetClass()  &&  
                 bsst.GetClass() == CBioseq_set::eClass_parts ) {
                return true;
            }
        }
    }
    return false;
}


SIZE_TYPE CBioseqContext::x_GetPartNumber(const CBioseq_Handle& h) const
{
    return m_Master->GetPartNumber(h);
}


const CBioseq& CBioseqContext::x_GetMasterForPart(const CBioseq& part) const
{
    CBioseq_Handle bsh = m_Ctx->GetScope().GetBioseqHandle(part);
    const CSeq_entry& segset =
        bsh.GetComplexityLevel(CBioseq_set::eClass_segset);
    _ASSERT(segset.IsSet());
    const CBioseq_set& bsst = segset.GetSet();
    _ASSERT(!bsst.GetSeq_set().empty());
    _ASSERT(bsst.GetSeq_set().front()->IsSeq());

    return bsst.GetSeq_set().front()->GetSeq();
}


void CBioseqContext::x_SetId(const CBioseq& seq)
{
    m_PrimaryId.Reset(FindBestChoice(seq.GetId(), CSeq_id::Score));
    m_Accession = m_PrimaryId->GetSeqIdString(true);

    ITERATE(CBioseq::TId, id_iter, seq.GetId()) {
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
            m_RefseqInfo = acc_type;
            break;
        // Gi
        case CSeq_id::e_Gi:
            m_IsGI = true;
            m_GI = id.GetGi();
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
        m_IsWGS = m_IsWGS  ||  ((acc_div & CSeq_id::eAcc_wgs) != 0);
        
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


const CSeq_id& CBioseqContext::GetPreferredSynonym(const CSeq_id& id) const
{
    if ( id.IsGi()  &&  id.GetGi() == m_GI ) {
        return *m_PrimaryId;
    }

    CBioseq_Handle h = m_Handle.GetScope().GetBioseqHandle(id);
    if ( h == m_Handle ) {
        return *m_PrimaryId;
    } else if ( h  &&  h.GetSeqId().NotEmpty() ) {
        return *FindBestChoice(h.GetBioseqCore()->GetId(), CSeq_id::Score);
    } else {
        return id;
    }
}


bool CBioseqContext::x_HasParts(const CBioseq& seq) const
{
    _ASSERT(m_Ctx);
    _ASSERT(IsSegmented());

    CBioseq_Handle bsh = m_Ctx->GetScope().GetBioseqHandle(seq);

    const CSeq_entry& segset_entry =
        bsh.GetComplexityLevel(CBioseq_set::eClass_segset);
    if ( !segset_entry.IsSet() ) { 
        return false;
    }
    const CBioseq_set& segset = segset_entry.GetSet();
    if ( !segset.CanGetClass()  ||
         segset.GetClass() != CBioseq_set::eClass_segset ) {
        return false;
    }

    CBioseq_set::TSeq_set::const_iterator iter = segset.GetSeq_set().begin();
    CBioseq_set::TSeq_set::const_iterator end  = segset.GetSeq_set().end();
    for ( ; iter != end; ++iter ) {
        if ( (*iter)->IsSeq()  &&  &((*iter)->GetSeq()) == &seq ) {
            ++iter;
            if ( (iter != end)  &&  (*iter)->IsSet() ) {
                const CBioseq_set& parts = (*iter)->GetSet();
                if ( parts.CanGetClass()  &&
                     parts.GetClass() == CBioseq_set::eClass_parts ) {
                    return true;
                }
            }
            break;
        }
    }

    return false;
}


bool CBioseqContext::x_IsDeltaLitOnly(const CBioseq& seq)
{
    _ASSERT(IsDelta());

    if ( seq.GetInst().CanGetExt()  &&  seq.GetInst().GetExt().IsDelta() ) {
        ITERATE (CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
            if ( (*it)->IsLoc() ) {
                return false;
            }
        }
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
// Flags
CFFContext::CFlags::CFlags(TMode mode, TFlags flags) :
    m_HideImpFeat((flags & CFlatFileGenerator::fHideImportedFeatures) != 0),
    m_HideSnpFeat((flags & CFlatFileGenerator::fHideSNPFeatures) != 0),
    m_HideExonFeat((flags & CFlatFileGenerator::fHideExonFeatures) != 0),
    m_HideIntronFeat((flags & CFlatFileGenerator::fHideIntronFeatures) != 0),
    m_HideRemImpFeat((flags & CFlatFileGenerator::fHideRemoteImpFeats) != 0),
    m_HideGeneRIFs((flags & CFlatFileGenerator::fHideGeneRIFs) != 0),
    m_OnlyGeneRIFs((flags & CFlatFileGenerator::fOnlyGeneRIFs) != 0),
    m_LatestGeneRIFs((flags & CFlatFileGenerator::fLatestGeneRIFs) != 0),
    m_ShowContigFeatures((flags & CFlatFileGenerator::fShowContigFeatures) != 0),
    m_ShowContigAndSeq((flags & CFlatFileGenerator::fShowContigAndSeq) != 0)
{
    switch ( mode ) {
    case CFlatFileGenerator::eMode_Release:
        x_SetReleaseFlags();
        break;
    case CFlatFileGenerator::eMode_Entrez:
        x_SetEntrezFlags();
        break;
    case CFlatFileGenerator::eMode_GBench:
        x_SetGBenchFlags();
        break;
    case CFlatFileGenerator::eMode_Dump:
        x_SetDumpFlags();
        break;
    }
}


void CFFContext::CFlags::x_SetReleaseFlags(void)
{
    m_SupressLocalId    = true;
    m_ValidateFeats     = true;
    m_IgnorePatPubs     = true;
    m_DropShortAA       = true;
    m_AvoidLocusColl    = true;
    m_IupacaaOnly       = true;
    m_DropBadCitGens    = true;
    m_NoAffilOnUnpub    = true;
    m_DropIllegalQuals  = true;
    m_CheckQualSyntax   = true;
    m_NeedRequiredQuals = true;
    m_NeedOrganismQual  = true;
    m_NeedAtLeastOneRef = true;
    m_CitArtIsoJta      = true;
    m_DropBadDbxref     = true;
    m_UseEmblMolType    = true;
    m_HideBankItComment = true;
    m_CheckCDSProductId = true;
    m_SupressSegLoc     = true;
    m_SrcQualsToNote    = true;
    m_HideEmptySource   = true;
    m_GoQualsToNote     = true;
    m_GeneSynsToNote    = true;
    m_ForGBRelease      = true;
}


void CFFContext::CFlags::x_SetEntrezFlags(void)
{
    m_SupressLocalId    = false;
    m_ValidateFeats     = true;
    m_IgnorePatPubs     = true;
    m_DropShortAA       = true;
    m_AvoidLocusColl    = true;
    m_IupacaaOnly       = false;
    m_DropBadCitGens    = true;
    m_NoAffilOnUnpub    = true;
    m_DropIllegalQuals  = true;
    m_CheckQualSyntax   = true;
    m_NeedRequiredQuals = true;
    m_NeedOrganismQual  = true;
    m_NeedAtLeastOneRef = false;
    m_CitArtIsoJta      = true;
    m_DropBadDbxref     = true;
    m_UseEmblMolType    = true;
    m_HideBankItComment = true;
    m_CheckCDSProductId = false;
    m_SupressSegLoc     = false;
    m_SrcQualsToNote    = true;
    m_HideEmptySource   = true;
    m_GoQualsToNote     = true;
    m_GeneSynsToNote    = true;
    m_ForGBRelease      = false;
}


void CFFContext::CFlags::x_SetGBenchFlags(void)
{
    m_SupressLocalId    = false;
    m_ValidateFeats     = false;
    m_IgnorePatPubs     = false;
    m_DropShortAA       = false;
    m_AvoidLocusColl    = false;
    m_IupacaaOnly       = false;
    m_DropBadCitGens    = false;
    m_NoAffilOnUnpub    = true;
    m_DropIllegalQuals  = false;
    m_CheckQualSyntax   = false;
    m_NeedRequiredQuals = false;
    m_NeedOrganismQual  = false;
    m_NeedAtLeastOneRef = false;
    m_CitArtIsoJta      = false;
    m_DropBadDbxref     = false;
    m_UseEmblMolType    = false;
    m_HideBankItComment = false;
    m_CheckCDSProductId = false;
    m_SupressSegLoc     = false;
    m_SrcQualsToNote    = false;
    m_HideEmptySource   = false;
    m_GoQualsToNote     = false;
    m_GeneSynsToNote    = false;
    m_ForGBRelease      = false;
}


void CFFContext::CFlags::x_SetDumpFlags(void)
{
    m_SupressLocalId    = false;
    m_ValidateFeats     = false;
    m_IgnorePatPubs     = false;
    m_DropShortAA       = false;
    m_AvoidLocusColl    = false;
    m_IupacaaOnly       = false;
    m_DropBadCitGens    = false;
    m_NoAffilOnUnpub    = false;
    m_DropIllegalQuals  = false;
    m_CheckQualSyntax   = false;
    m_NeedRequiredQuals = false;
    m_NeedOrganismQual  = false;
    m_NeedAtLeastOneRef = false;
    m_CitArtIsoJta      = false;
    m_DropBadDbxref     = false;
    m_UseEmblMolType    = false;
    m_HideBankItComment = false;
    m_CheckCDSProductId = false;
    m_SupressSegLoc     = false;
    m_SrcQualsToNote    = false;
    m_HideEmptySource   = false;
    m_GoQualsToNote     = false;
    m_GeneSynsToNote    = false;
    m_ForGBRelease      = false;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
