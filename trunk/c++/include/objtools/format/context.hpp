#ifndef OBJTOOLS_FORMAT___CONTEXT__HPP
#define OBJTOOLS_FORMAT___CONTEXT__HPP

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
* Author:  Aaron Ucko, Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>

#include <objtools/format/flat_file_generator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CFFContext;


// When formatting segmented bioseq CMasterContext holds information
// on the Master bioseq.
class CMasterContext : public CObject
{
public:
    CMasterContext(const CBioseq& seq, CFFContext& ctx);
    ~CMasterContext(void);

    const CBioseq& GetBioseq(void) const { return *m_Seq; }
    CBioseq_Handle& GetHandle(void) { return m_Handle; }
    const CBioseq_Handle& GetHandle(void) const { return m_Handle; }
    SIZE_TYPE GetNumSegments(void) const;
    const string& GetBaseName(void) const { return m_BaseName; }

    SIZE_TYPE GetPartNumber(const CBioseq_Handle& seq) const;

private:
    void x_SetBaseName(const CBioseq& seq);

    // data
    string             m_BaseName;
    CBioseq_Handle     m_Handle;
    CConstRef<CBioseq> m_Seq;
    mutable CFFContext*   m_Ctx;
};


// information on the bioseq being formatted
class CBioseqContext : public CObject
{
public:
    CBioseqContext(const CBioseq& seq, CFFContext& ctx);
    CBioseq_Handle& GetHandle(void);
    const CBioseq_Handle& GetHandle(void) const;

    SIZE_TYPE GetNumSegments(void) const;

    bool IsPart(void) const;
    SIZE_TYPE GetPartNumber(void) const;
    
    bool IsProt(void) const;
    CSeq_inst::TRepr  GetRepr  (void) const;
    CMolInfo::TTech   GetTech  (void) const;
    CMolInfo::TBiomol GetBiomol(void) const;
    CSeq_inst::TMol   GetMol   (void) const;

    bool IsSegmented(void) const;
    bool HasParts(void) const;

    bool IsDelta(void) const;
    bool IsDeltaLitOnly(void) const;

    const string& GetAccession(void) const; // ???
    CSeq_id* GetPrimaryId(void);
    const CSeq_id* GetPrimaryId(void) const;

    int  GetGI(void) const;
    const string& GetWGSMasterAccn(void) const;
    const string& GetWGSMasterName(void) const;

    const CSeq_id& GetPreferredSynonym(const CSeq_id& id) const;

    const CSeq_loc* GetLocation(void) const;
    void SetLocation(const CSeq_loc* loc);

    bool ShowGBBSource(void) const;

    // ID queries
    bool IsGED(void) const;  // Genbank, EMBL or DDBJ
    bool IsPDB(void) const;
    bool IsSP (void) const;  // SwissProt
    bool IsTPA(void) const;  // Third-Party Annotation
    bool IsPatent(void) const;
    bool IsGbGenomeProject(void) const; // AE
    bool IsNcbiCONDiv(void) const;      // CH
    bool IsGI(void) const;
    bool IsWGS(void) const;
    bool IsWGSMaster(void) const;
    bool IsHup(void) const;  // ??? should move to global
    // RefSeq ID queries
    bool IsRefSeq(void) const;
    bool IsRSCompleteGenomic  (void) const;  // NC_
    bool IsRSIncompleteGenomic(void) const;  // NG_
    bool IsRSMRna             (void) const;  // NM_
    bool IsRSNonCodingRna     (void) const;  // NR_
    bool IsRSProtein          (void) const;  // NP_
    bool IsRSContig           (void) const;  // NT_
    bool IsRSIntermedWGS      (void) const;  // NW_
    bool IsRSPredictedMRna    (void) const;  // XM_
    bool IsRSPredictedNCRna   (void) const;  // XR_
    bool IsRSPredictedProtein (void) const;  // XP_
    bool IsRSWGSNuc           (void) const;  // NZ_
    bool IsRSWGSProt          (void) const;  // ZP_
    bool IsRSGenome           (void) const;  // NS_ (?)
    
private:
    SIZE_TYPE  x_CountSegs(const CBioseq& seq) const;
    bool x_HasParts(const CBioseq& seq) const;
    bool x_IsDeltaLitOnly(const CBioseq& seq);
    bool x_IsPart(const CBioseq& seq) const;
    const CBioseq& x_GetMasterForPart(const CBioseq& part) const;
    SIZE_TYPE x_GetPartNumber(const CBioseq_Handle& seq) const;
    
    
    void x_SetId(const CBioseq& seq);
    void x_SetAccession(const CBioseq& seq);

    CSeq_inst::TRepr x_GetRepr(const CBioseq& seq) const;
    const CMolInfo*  x_GetMolinfo(void);

    // data
    CBioseq_Handle        m_Handle;
    CSeq_inst::TRepr      m_Repr;
    CConstRef<CMolInfo>   m_Molinfo;
    CSeq_inst::TMol       m_Mol;
    string                m_Accession;  // !!! should be string& ?
    CRef<CSeq_id>         m_PrimaryId;
    string                m_WGSMasterAccn;
    string                m_WGSMasterName;

    SIZE_TYPE   m_SegsCount;     // # of segments (set iff bioseq is segmented)
    bool        m_IsPart;
    SIZE_TYPE   m_PartNumber;
    bool        m_HasParts;
    bool        m_IsDeltaLitOnly;

    bool m_IsProt;  // Protein
    bool m_IsGPS;   // Gene-Prod Set
    bool m_IsGED;   // Genbank, Embl or Ddbj
    bool m_IsPDB;
    bool m_IsSP;    // SwissProt
    bool m_IsTPA;   // Third Party Annotation
    bool m_IsRefSeq;
    unsigned int m_RefseqInfo;
    bool m_IsGbGenomeProject;       // GenBank Genome project data
    bool m_IsNcbiCONDiv;            // NCBI CON division
    bool m_IsPatent;
    bool m_IsGI;
    bool m_IsWGS;
    bool m_IsWGSMaster;
    bool m_IsHup;
    int  m_GI;
    bool m_ShowGBBSource;
    
    CConstRef<CSeq_loc>     m_Location;
    mutable CFFContext*     m_Ctx;
    const CMasterContext*   m_Master;
};


// FlatContext

class CFFContext : public CObject
{
public:
 
    // types
    typedef CFlatFileGenerator::TFormat       TFormat;
    typedef CFlatFileGenerator::TMode         TMode;
    typedef CFlatFileGenerator::TStyle        TStyle;
    typedef CFlatFileGenerator::TFilter       TFilter;
    typedef CFlatFileGenerator::TFlags        TFlags;

    // constructor
    CFFContext(CScope& scope, TFormat format, TMode mode, TStyle style,
        TFilter filter, TFlags flags);

    // destructor
    ~CFFContext(void);

    TFormat GetFormat(void) const;
    void SetFormat(TFormat format);
    bool IsFormatGenBank(void) const {
        return GetFormat() == CFlatFileGenerator::eFormat_GenBank;
    }
    bool IsFormatEMBL   (void) const {
        return GetFormat() == CFlatFileGenerator::eFormat_EMBL;
    }
    bool IsFormatDDBJ   (void) const {
        return GetFormat() == CFlatFileGenerator::eFormat_DDBJ;
    }
    bool IsFormatGBSeq  (void) const {
        return GetFormat() == CFlatFileGenerator::eFormat_GBSeq;
    }
    bool IsFormatFTable (void) const {
        return GetFormat() == CFlatFileGenerator::eFormat_FTable;
    }

    TMode GetMode(void) const;
    void SetMode(TMode mode);
    bool IsModeRelease(void) const {
        return GetMode()  == CFlatFileGenerator::eMode_Release;
    }
    bool IsModeEntrez (void) const {
        return GetMode()  == CFlatFileGenerator::eMode_Entrez;
    }
    bool IsModeGBench (void) const {
        return GetMode()  == CFlatFileGenerator::eMode_GBench;
    }
    bool IsModeDump   (void) const {
        return GetMode()  == CFlatFileGenerator::eMode_Dump;
    }

    TStyle GetStyle(void) const;
    void SetStyle(TStyle style);

    TFilter GetFilterFlags(void) const;
    void SetFilterFlags(TFilter filter_flags);

    const CSeq_entry& GetTSE(void) const;
    void SetTSE(const CSeq_entry& tse);

    const CBioseq& GetActiveBioseq(void) const;
    void SetActiveBioseq(const CBioseq& seq);

    const CBioseq* GetMasterBioseq(void) const;
    void SetMasterBioseq(const CBioseq& master);

    const CSeq_loc* GetLocation(void) const;
    void SetLocation(const CSeq_loc* loc);
    
    CScope& GetScope (void);
    void SetScope(CScope& scope);

    const CSubmit_block* GetSeqSubmit(void) const;
    void SetSubmit(const CSubmit_block& sub);

    CBioseq_Handle& GetHandle(void);
    const CBioseq_Handle& GetHandle(void) const;
    void SetHandle(CBioseq_Handle& handle);

    const string& GetAccession(void) const;
    const string& GetWGSMasterAccn(void) const;
    const string& GetWGSMasterName(void) const;
    CSeq_id* GetPrimaryId(void);
    const CSeq_id* GetPrimaryId(void) const;
    int  GetGI(void) const;
    const CSeq_id& GetPreferredSynonym(const CSeq_id& id) const;
    CSeq_inst::TRepr  GetRepr  (void) const;
    CMolInfo::TTech   GetTech  (void) const;
    CMolInfo::TBiomol GetBiomol(void) const;
    CSeq_inst::TMol   GetMol   (void) const;

    bool IsSegmented(void) const;
    bool HasParts(void) const;
    SIZE_TYPE GetNumParts(void) const;

    bool IsDelta(void) const;
    bool IsDeltaLitOnly(void) const;

    bool IsPart(void) const;
    SIZE_TYPE GetPartNumber(void) const;

    // flags calculated by examining data in record
    bool IsProt(void) const;
    bool IsNa(void) const { return !IsProt(); }
    bool IsGED(void) const;  // Genbank, EMBL or DDBJ
    bool IsPDB(void) const;
    bool IsSP (void) const;  // SwissProt
    bool IsTPA(void) const;  // Third-Party Annotation
    bool IsPatent(void) const;
    bool IsGbGenomeProject(void) const; // AE
    bool IsNcbiCONDiv(void) const;      // CH
    bool IsGI(void) const;
    bool IsWGS(void) const;
    bool IsWGSMaster(void) const;
    bool IsHup(void) const;  // ??? should move to global
    // RefSeq ID queries
    bool IsRefSeq(void) const;
    bool IsRSCompleteGenomic  (void) const;  // NC_
    bool IsRSIncompleteGenomic(void) const;  // NG_
    bool IsRSMRna             (void) const;  // NM_
    bool IsRSNonCodingRna     (void) const;  // NR_
    bool IsRSProtein          (void) const;  // NP_
    bool IsRSContig           (void) const;  // NT_
    bool IsRSIntermedWGS      (void) const;  // NW_
    bool IsRSPredictedMRna    (void) const;  // XM_
    bool IsRSPredictedNCRna   (void) const;  // XR_
    bool IsRSPredictedProtein (void) const;  // XP_
    bool IsRSWGSNuc           (void) const;  // NZ_
    bool IsRSWGSProt          (void) const;  // ZP_
    bool IsRSGenome           (void) const;  // NS_ (?)
    
    const CMasterContext* Master(void) const { return m_Master; }
    const CBioseqContext* Bioseq(void) const { return m_Bioseq; }

    // flags
    bool SupressLocalId    (void) const { return x_Flags().SupressLocalId();     }
    bool ValidateFeats     (void) const { return x_Flags().ValidateFeats();      }
    bool IgnorePatPubs     (void) const { return x_Flags().IgnorePatPubs();      }
    bool DropShortAA       (void) const { return x_Flags().DropShortAA();        }
    bool AvoidLocusColl    (void) const { return x_Flags().AvoidLocusColl();     }
    bool IupacaaOnly       (void) const { return x_Flags().IupacaaOnly();        }
    bool DropBadCitGens    (void) const { return x_Flags().DropBadCitGens();     }
    bool NoAffilOnUnpub    (void) const { return x_Flags().NoAffilOnUnpub();     }
    bool DropIllegalQuals  (void) const { return x_Flags().DropIllegalQuals();   }
    bool CheckQualSyntax   (void) const { return x_Flags().CheckQualSyntax();    }
    bool NeedRequiredQuals (void) const { return x_Flags().NeedRequiredQuals();  }
    bool NeedOrganismQual  (void) const { return x_Flags().NeedOrganismQual();   }
    bool NeedAtLeastOneRef (void) const { return x_Flags().NeedAtLeastOneRef();  }
    bool CitArtIsoJta      (void) const { return x_Flags().CitArtIsoJta();       }
    bool DropBadDbxref     (void) const { return x_Flags().DropBadDbxref();      }
    bool UseEmblMolType    (void) const { return x_Flags().UseEmblMolType();     }
    bool HideBankItComment (void) const { return x_Flags().HideBankItComment();  }
    bool CheckCDSProductId (void) const { return x_Flags().CheckCDSProductId();  }
    bool SupressSegLoc     (void) const { return x_Flags().SupressSegLoc();      }
    bool SrcQualsToNote    (void) const { return x_Flags().SrcQualsToNote();     }
    bool HideEmptySource   (void) const { return x_Flags().HideEmptySource();    }
    bool GoQualsToNote     (void) const { return x_Flags().GoQualsToNote();      }
    bool GeneSynsToNote    (void) const { return x_Flags().GeneSynsToNote();     }
    bool ForGBRelease      (void) const { return x_Flags().ForGBRelease();       }

    bool HideImpFeat       (void) const { return x_Flags().HideImpFeat();        }
    bool HideSnpFeat       (void) const { return x_Flags().HideSnpFeat();        }
    bool HideExonFeat      (void) const { return x_Flags().HideExonFeat();       }
    bool HideIntronFeat    (void) const { return x_Flags().HideIntronFeat();     }
    bool HideRemImpFeat    (void) const { return x_Flags().HideRemImpFeat();     }
    bool HideGeneRIFs      (void) const { return x_Flags().HideGeneRIFs();       }
    bool OnlyGeneRIFs      (void) const { return x_Flags().OnlyGeneRIFs();       }
    bool LatestGeneRIFs    (void) const { return x_Flags().LatestGeneRIFs();     }
    bool ShowContigFeatures(void) const { return x_Flags().ShowContigFeatures(); }
    bool ShowContigAndSeq  (void) const { return x_Flags().ShowContigAndSeq();   }
    bool DoHtml            (void) const { return x_Flags().DoHtml();             }

    bool ShowGBBSource(void) const;
    

private:

    class CFlags : public CObject
    {
    public:
        CFlags(TMode mode, TFlags flags);
        
        // mode dependant flags
        bool SupressLocalId   (void) const  { return m_SupressLocalId;    }
        bool ValidateFeats    (void) const  { return m_ValidateFeats;     }
        bool IgnorePatPubs    (void) const  { return m_IgnorePatPubs;     }
        bool DropShortAA      (void) const  { return m_DropShortAA;       }
        bool AvoidLocusColl   (void) const  { return m_AvoidLocusColl;    }
        bool IupacaaOnly      (void) const  { return m_IupacaaOnly;       }
        bool DropBadCitGens   (void) const  { return m_DropBadCitGens;    }
        bool NoAffilOnUnpub   (void) const  { return m_NoAffilOnUnpub;    }
        bool DropIllegalQuals (void) const  { return m_DropIllegalQuals;  }
        bool CheckQualSyntax  (void) const  { return m_CheckQualSyntax;   }
        bool NeedRequiredQuals(void) const  { return m_NeedRequiredQuals; }
        bool NeedOrganismQual (void) const  { return m_NeedOrganismQual;  }
        bool NeedAtLeastOneRef(void) const  { return m_NeedAtLeastOneRef; }
        bool CitArtIsoJta     (void) const  { return m_CitArtIsoJta;      }
        bool DropBadDbxref    (void) const  { return m_DropBadDbxref;     }
        bool UseEmblMolType   (void) const  { return m_UseEmblMolType;    }
        bool HideBankItComment(void) const  { return m_HideBankItComment; }
        bool CheckCDSProductId(void) const  { return m_CheckCDSProductId; }
        bool SupressSegLoc    (void) const  { return m_SupressSegLoc;     }
        bool SrcQualsToNote   (void) const  { return m_SrcQualsToNote;    }
        bool HideEmptySource  (void) const  { return m_HideEmptySource;   }
        bool GoQualsToNote    (void) const  { return m_GoQualsToNote;     }
        bool GeneSynsToNote   (void) const  { return m_GeneSynsToNote;    }
        bool ForGBRelease     (void) const  { return m_ForGBRelease;      }

        // custumizable flags
        bool HideImpFeat       (void) const  { return m_HideImpFeat;        }
        bool HideSnpFeat       (void) const  { return m_HideSnpFeat;        }
        bool HideExonFeat      (void) const  { return m_HideExonFeat;       }
        bool HideIntronFeat    (void) const  { return m_HideIntronFeat;     }
        bool HideRemImpFeat    (void) const  { return m_HideRemImpFeat;     }
        bool HideGeneRIFs      (void) const  { return m_HideGeneRIFs;       }
        bool OnlyGeneRIFs      (void) const  { return m_OnlyGeneRIFs;       }
        bool LatestGeneRIFs    (void) const  { return m_LatestGeneRIFs;     }
        bool ShowContigFeatures(void) const  { return m_ShowContigFeatures; }
        bool ShowContigAndSeq  (void) const  { return m_ShowContigAndSeq;   }
        bool DoHtml            (void) const  { return m_DoHtml;             }
        
    private:
        // setup
        void x_SetReleaseFlags(void);
        void x_SetEntrezFlags(void);
        void x_SetGBenchFlags(void);
        void x_SetDumpFlags(void);

        // flags
        bool m_SupressLocalId;
        bool m_ValidateFeats;
        bool m_IgnorePatPubs;
        bool m_DropShortAA;
        bool m_AvoidLocusColl;
        bool m_IupacaaOnly;
        bool m_DropBadCitGens;
        bool m_NoAffilOnUnpub;
        bool m_DropIllegalQuals;
        bool m_CheckQualSyntax;
        bool m_NeedRequiredQuals;
        bool m_NeedOrganismQual;
        bool m_NeedAtLeastOneRef;
        bool m_CitArtIsoJta;
        bool m_DropBadDbxref;
        bool m_UseEmblMolType;
        bool m_HideBankItComment;
        bool m_CheckCDSProductId;
        bool m_SupressSegLoc;
        bool m_SrcQualsToNote;
        bool m_HideEmptySource;
        bool m_GoQualsToNote;
        bool m_GeneSynsToNote;
        bool m_ForGBRelease;

        // custom flags
        bool m_HideImpFeat;
        bool m_HideSnpFeat;
        bool m_HideExonFeat;
        bool m_HideIntronFeat;
        bool m_HideRemImpFeat;
        bool m_HideGeneRIFs;
        bool m_OnlyGeneRIFs;
        bool m_LatestGeneRIFs;
        bool m_ShowContigFeatures;
        bool m_ShowContigAndSeq;
        bool m_DoHtml;
    };

    const CFlags& x_Flags(void) const { return m_Flags; }

    // data
    TFormat          m_Format;
    TMode            m_Mode;
    TStyle           m_Style;
    TFilter     m_FilterFlags;
    CFlags           m_Flags;

    CConstRef<CSeq_entry>  m_TSE;
    CRef<CScope>           m_Scope;
    CConstRef<CSubmit_block> m_SeqSub;
    //bool m_IsGPS;

    
    CRef<CMasterContext> m_Master;
    CRef<CBioseqContext> m_Bioseq;
};


/////////////////////////////////////////////////////////////////////////////
//
// inline methods


/****************************************************************************/
/*                                CFFContext                              */
/****************************************************************************/

inline
CFFContext::TFormat CFFContext::GetFormat(void) const
{
    return m_Format;
}


inline
void CFFContext::SetFormat(TFormat format)
{
    m_Format = format;
}


inline
CFFContext::TMode CFFContext::GetMode(void) const
{
    return m_Mode;
}


inline
void CFFContext::SetMode(TMode mode)
{
    m_Mode = mode;
}


inline
CFFContext::TStyle CFFContext::GetStyle(void) const
{
    return m_Style;
}


inline
void CFFContext::SetStyle(TStyle style)
{
    m_Style = style;
}


inline
CFFContext::TFilter CFFContext::GetFilterFlags(void) const
{
    return m_FilterFlags;
}


inline
void CFFContext::SetFilterFlags(TFilter filter_flags)
{
    m_FilterFlags = filter_flags;
}


inline
CScope& CFFContext::GetScope (void)
{
    return *m_Scope;
}


inline
void CFFContext::SetScope(CScope& scope)
{
    m_Scope.Reset(&scope);
}


inline
const CSubmit_block* CFFContext::GetSeqSubmit(void) const
{
    return m_SeqSub;
}


inline
void CFFContext::SetSubmit(const CSubmit_block& sub)
{
    m_SeqSub.Reset(&sub);
}


inline
const CSeq_entry& CFFContext::GetTSE(void) const
{
    return *m_TSE;
}


inline
void CFFContext::SetTSE(const CSeq_entry& tse)
{
    m_TSE.Reset(&tse);
}


inline
const string& CFFContext::GetAccession(void) const
{
    return m_Bioseq->GetAccession();
}


inline 
CSeq_id* CFFContext::GetPrimaryId(void)
{
    return m_Bioseq->GetPrimaryId();
}


inline 
const CSeq_id* CFFContext::GetPrimaryId(void) const
{
    return m_Bioseq->GetPrimaryId();
}



inline
CSeq_inst::TRepr CFFContext::GetRepr(void) const
{
    return m_Bioseq->GetRepr();
}


inline
CMolInfo::TTech CFFContext::GetTech(void) const
{
    return m_Bioseq->GetTech();
}


inline
CMolInfo::TBiomol CFFContext::GetBiomol(void) const
{
    return m_Bioseq->GetBiomol();
}


inline
CSeq_inst::TMol CFFContext::GetMol(void) const
{
    return m_Bioseq->GetMol();
}


inline
bool CFFContext::IsSegmented(void) const
{
    return m_Bioseq->IsSegmented();
}


inline
bool CFFContext::HasParts(void) const
{
    return m_Bioseq->HasParts();
}


inline
SIZE_TYPE CFFContext::GetNumParts(void) const
{
    return m_Master->GetNumSegments();
}


inline
bool CFFContext::IsDelta(void) const
{
    return m_Bioseq->IsDelta();
}


inline
bool CFFContext::IsDeltaLitOnly(void) const
{
    return m_Bioseq->IsDeltaLitOnly();
}


inline
bool CFFContext::IsPart(void) const
{
    return m_Bioseq->IsPart();
}


inline
SIZE_TYPE CFFContext::GetPartNumber(void) const
{
    return m_Bioseq->GetPartNumber();
}


inline
const string& CFFContext::GetWGSMasterAccn(void) const
{
    return m_Bioseq->GetWGSMasterAccn();
}


inline
const string& CFFContext::GetWGSMasterName(void) const
{
    return m_Bioseq->GetWGSMasterName();
}


inline
int CFFContext::GetGI(void) const
{
    return m_Bioseq->GetGI();
}


inline
const CSeq_id& CFFContext::GetPreferredSynonym(const CSeq_id& id) const
{
    return m_Bioseq->GetPreferredSynonym(id);
}


inline 
bool CFFContext::IsHup(void) const
{
    return m_Bioseq->IsHup();
}


inline
const CBioseq& CFFContext::GetActiveBioseq(void) const
{
    return m_Bioseq->GetHandle().GetBioseq();
}


inline
const CSeq_loc* CFFContext::GetLocation(void) const
{
    return m_Bioseq->GetLocation();
}


inline
void CFFContext::SetLocation(const CSeq_loc* loc)
{
    m_Bioseq->SetLocation(loc);
}


inline
CBioseq_Handle& CFFContext::GetHandle(void)
{
    return m_Bioseq->GetHandle();
}


inline
const CBioseq_Handle& CFFContext::GetHandle(void) const
{
    return m_Bioseq->GetHandle();
}


inline
bool CFFContext::IsGED(void) const
{
    return m_Bioseq->IsGED();
}


inline
bool CFFContext::IsPDB(void) const
{
    return m_Bioseq->IsPDB();
}


inline
bool CFFContext::IsSP(void) const
{
    return m_Bioseq->IsSP();
}


inline
bool CFFContext::IsTPA(void) const
{
    return m_Bioseq->IsTPA();
}


inline
bool CFFContext::IsRSCompleteGenomic(void) const
{
    return m_Bioseq->IsRSCompleteGenomic();
}


inline
bool CFFContext::IsRSIncompleteGenomic(void) const
{
    return m_Bioseq->IsRSIncompleteGenomic();
}


inline
bool CFFContext::IsRSMRna(void) const
{
    return m_Bioseq->IsRSMRna();
}


inline
bool CFFContext::IsRSNonCodingRna(void) const
{
    return m_Bioseq->IsRSNonCodingRna();
}

inline
bool CFFContext::IsRSProtein(void) const
{
    return m_Bioseq->IsRSProtein();
}


inline
bool CFFContext::IsRSContig(void) const
{
    return m_Bioseq->IsRSContig();
}


inline
bool CFFContext::IsRSIntermedWGS(void) const
{
    return m_Bioseq->IsRSIntermedWGS();
}


inline
bool CFFContext::IsRSPredictedMRna(void) const
{
    return m_Bioseq->IsRSPredictedMRna();
}


inline
bool CFFContext::IsRSPredictedNCRna(void) const
{
    return m_Bioseq->IsRSPredictedNCRna();
}


inline
bool CFFContext::IsRSPredictedProtein(void) const
{
    return m_Bioseq->IsRSPredictedProtein();
}


inline
bool CFFContext::IsRSWGSNuc(void) const
{
    return m_Bioseq->IsRSWGSNuc();
}


inline
bool CFFContext::IsRSWGSProt(void) const
{
    return m_Bioseq->IsRSWGSProt();
}


inline
bool CFFContext::IsRSGenome(void) const
{
    return m_Bioseq->IsRSGenome();
}


inline
bool CFFContext::IsGbGenomeProject(void) const
{
    return m_Bioseq->IsGbGenomeProject();
}


inline
bool CFFContext::IsNcbiCONDiv(void) const
{
    return m_Bioseq->IsNcbiCONDiv();
}


inline
bool CFFContext::IsPatent(void) const
{
    return m_Bioseq->IsPatent();
}


inline
bool CFFContext::IsRefSeq(void) const
{
    return m_Bioseq->IsRefSeq();
}


inline
bool CFFContext::IsGI(void) const
{
    return m_Bioseq->IsGI();
}


inline
bool CFFContext::IsWGS(void) const
{
    return m_Bioseq->IsWGS();
}


inline
bool CFFContext::IsWGSMaster(void) const
{
    return m_Bioseq->IsWGSMaster();
}


inline
bool CFFContext::IsProt(void) const
{
    return m_Bioseq->IsProt();
}


inline
bool CFFContext::ShowGBBSource(void) const
{
    return m_Bioseq->ShowGBBSource();
}


/****************************************************************************/
/*                               CMasterContext                             */
/****************************************************************************/

/****************************************************************************/
/*                               CBioseqContext                             */
/****************************************************************************/


inline
SIZE_TYPE CBioseqContext::GetNumSegments(void) const
{
    return m_SegsCount;
}


inline
bool CBioseqContext::IsPart(void) const
{
    return m_IsPart;
}


inline
SIZE_TYPE CBioseqContext::GetPartNumber(void) const
{
    return m_PartNumber;
}


inline
CBioseq_Handle& CBioseqContext::GetHandle(void)
{
    return m_Handle;
}


inline
const CBioseq_Handle& CBioseqContext::GetHandle(void) const
{
    return m_Handle;
}


inline
bool CBioseqContext::IsProt(void) const
{
    return m_IsProt;
}


inline
const string& CBioseqContext::GetAccession(void) const
{
    return m_Accession;
}


inline
CSeq_id* CBioseqContext::GetPrimaryId(void)
{
    return m_PrimaryId;
}


inline
const CSeq_id* CBioseqContext::GetPrimaryId(void) const
{
    return m_PrimaryId;
}


inline
CSeq_inst::TRepr CBioseqContext::GetRepr(void) const
{
    return m_Repr;
}


inline
CMolInfo::TTech CBioseqContext::GetTech(void) const
{
    return (!m_Molinfo.IsNull()  &&  m_Molinfo->CanGetTech()) ? 
        m_Molinfo->GetTech() : CMolInfo::eTech_unknown;
}


inline
CMolInfo::TBiomol CBioseqContext::GetBiomol(void) const
{
    return (!m_Molinfo.IsNull()  &&  m_Molinfo->CanGetBiomol()) ? 
        m_Molinfo->GetBiomol() : CMolInfo::eBiomol_unknown;
}


inline
CSeq_inst::TMol CBioseqContext::GetMol(void) const
{
    return m_Mol;
}


inline
bool CBioseqContext::IsSegmented(void) const
{
    return GetRepr() == CSeq_inst::eRepr_seg;
}


inline
bool CBioseqContext::HasParts(void) const
{
    return m_HasParts;
}


inline
bool CBioseqContext::IsDelta(void) const
{
    return GetRepr() == CSeq_inst::eRepr_delta;
}


inline
bool CBioseqContext::IsDeltaLitOnly(void) const
{
    return m_IsDeltaLitOnly;
}


inline
const string& CBioseqContext::GetWGSMasterAccn(void) const
{
    return m_WGSMasterAccn;
}


inline
const string& CBioseqContext::GetWGSMasterName(void) const
{
    return m_WGSMasterName;
}


inline
bool CBioseqContext::IsGED(void) const
{
    return m_IsGED;
}


inline
bool CBioseqContext::IsPDB(void) const
{
    return m_IsPDB;
}


inline
bool CBioseqContext::IsSP(void) const
{
    return m_IsSP;
}


inline
bool CBioseqContext::IsTPA(void) const
{
    return m_IsTPA;
}


inline
bool CBioseqContext::IsRSCompleteGenomic(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_chromosome) != 0;  // NC_
}


inline
bool CBioseqContext::IsRSIncompleteGenomic(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_genomic) != 0;  // NG_
}


inline
bool CBioseqContext::IsRSMRna(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_mrna) != 0;  // NM_
}


inline
bool CBioseqContext::IsRSNonCodingRna(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_ncrna) != 0;  // NR_
}


inline
bool CBioseqContext::IsRSProtein(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_prot) != 0;  // NP_
}


inline
bool CBioseqContext::IsRSContig(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_contig) != 0;  // NT_
}


inline
bool CBioseqContext::IsRSIntermedWGS(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_wgs_intermed) != 0;  // NW_
}


inline
bool CBioseqContext::IsRSPredictedMRna(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_mrna_predicted) != 0;  // XM_
}


inline
bool CBioseqContext::IsRSPredictedNCRna(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_ncrna_predicted) != 0;  // XR_
}


inline
bool CBioseqContext::IsRSPredictedProtein(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_prot_predicted) != 0;  // XP_
}


inline
bool CBioseqContext::IsRSWGSNuc(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_wgs_nuc) != 0;  // NZ_
}


inline
bool CBioseqContext::IsRSWGSProt(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_wgs_prot) != 0;  // ZP_
}


inline
bool CBioseqContext::IsRSGenome(void)  const
{
    return (m_RefseqInfo & CSeq_id::eAcc_refseq_genome) != 0;  // NS_ (?)
}


inline
bool CBioseqContext::IsGbGenomeProject(void)  const
{
    return m_IsGbGenomeProject;
}


inline
bool CBioseqContext::IsNcbiCONDiv(void)  const
{
    return m_IsNcbiCONDiv;
}


inline    
bool CBioseqContext::IsPatent(void) const
{
    return m_IsPatent;
}


inline
bool CBioseqContext::IsRefSeq(void) const
{
    return m_IsRefSeq;
}


inline
bool CBioseqContext::IsGI(void) const
{
    return m_IsGI;
}


inline
bool CBioseqContext::IsWGS(void) const
{
    return m_IsWGS;
}


inline
bool CBioseqContext::IsWGSMaster(void) const
{
    return m_IsWGSMaster;
}


inline
bool CBioseqContext::IsHup(void) const
{
    return m_IsHup;
}


inline
int CBioseqContext::GetGI(void) const
{
    return m_GI;
}


inline
bool CBioseqContext::ShowGBBSource(void) const
{
    return m_ShowGBBSource;
}



inline
const CSeq_loc* CBioseqContext::GetLocation(void) const
{
    return m_Location;
}


inline
void CBioseqContext::SetLocation(const CSeq_loc* loc)
{
    m_Location.Reset(loc);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/01/14 15:50:17  shomrat
* multiple changes (work in progress)
*
* Revision 1.1  2003/12/18 17:41:20  shomrat
* file was moved from src directory
*
* Revision 1.1  2003/12/17 20:18:54  shomrat
* Initial Revision (adapted from flat lib)
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT___CONTEXT__HPP */
