#ifndef OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP
#define OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP

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
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/format/items/flat_qual_slots.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFeatHeaderItem : public CFlatItem
{
public:
    CFeatHeaderItem(CFFContext& ctx);
    void Format(IFormatter& formatter,
        IFlatTextOStream& text_os) const {
        formatter.FormatFeatHeader(*this, text_os);
    }

    const CSeq_id& GetId(void) const { return *m_Id; }  // for FTable format

private:
    void x_GatherInfo(CFFContext& ctx);

    // data
    CConstRef<CSeq_id>  m_Id;  // for FTable format
};


class CFlatFeature : public CObject
{
public:
    CFlatFeature(const string& key, const CFlatSeqLoc& loc, const CSeq_feat& feat)
        : m_Key(key), m_Loc(&loc), m_Feat(&feat) { }

    typedef vector<CRef<CFlatQual> > TQuals;

    const string&      GetKey  (void) const { return m_Key;   }
    const CFlatSeqLoc& GetLoc  (void) const { return *m_Loc;  }
    const TQuals&      GetQuals(void) const { return m_Quals; }
    const CSeq_feat&   GetFeat (void) const { return *m_Feat; }

    TQuals& SetQuals(void) { return m_Quals; }

private:
    string                  m_Key;
    CConstRef<CFlatSeqLoc>  m_Loc;
    TQuals                  m_Quals;
    CConstRef<CSeq_feat>    m_Feat;
};


class CFeatureItemBase : public CFlatItem
{
public:
    CRef<CFlatFeature> Format(void) const;
    void Format(IFormatter& formatter,
        IFlatTextOStream& text_os) const {
        formatter.FormatFeature(*this, text_os);
    }
    bool operator<(const CFeatureItemBase& f2) const 
        { return m_Feat->Compare(*f2.m_Feat, GetLoc(), f2.GetLoc()) < 0; }

    const CSeq_feat& GetFeat(void)  const { return *m_Feat; }
    const CSeq_loc&  GetLoc(void)   const { 
        return m_Loc ? *m_Loc : m_Feat->GetLocation();
    }
    virtual string GetKey(void) const
        { return m_Feat->GetData().GetKey(CSeqFeatData::eVocabulary_genbank); }

protected:

    // constructor
    CFeatureItemBase(const CSeq_feat& feat, CFFContext& ctx,
                     const CSeq_loc* loc = 0);

    virtual void x_AddQuals   (CFFContext& ctx) const = 0;
    virtual void x_FormatQuals(void) const = 0;

    CConstRef<CSeq_feat>    m_Feat;
    CConstRef<CSeq_loc>     m_Loc;

    mutable CRef<CFlatFeature>  m_FF; ///< populated on demand then cached here
};


class CFeatureItem : public CFeatureItemBase
{
public:
    enum EMapped
    {
        eMapped_not_mapped,
        eMapped_from_genomic,
        eMapped_from_cdna,
        eMapped_from_prot
    };

    // constructors
    CFeatureItem(const CSeq_feat& feat, CFFContext& ctx,
                 const CSeq_loc* loc = 0, EMapped mapped = eMapped_not_mapped)
        : CFeatureItemBase(feat, ctx, loc), m_Mapped(mapped)
    {
        x_GatherInfo(ctx);
    }
    CFeatureItem(const CMappedFeat& feat, CFFContext& ctx,
                 const CSeq_loc* loc = 0, EMapped mapped = eMapped_not_mapped)
        : CFeatureItemBase(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_Mapped(mapped)
    {
        x_GatherInfo(ctx);
    }
    string GetKey(void) const;

    bool IsMapped           (void) const { return m_Mapped != eMapped_not_mapped;   }
    bool IsMappedFromGenomic(void) const { return m_Mapped == eMapped_from_genomic; }
    bool IsMappedFromCDNA   (void) const { return m_Mapped == eMapped_from_cdna;    }
    bool IsMappedFromProt   (void) const { return m_Mapped == eMapped_from_prot;    }

private:
    void x_GatherInfo(CFFContext& ctx);

    // qualifier collection
    void x_AddQuals(CFFContext& ctx)       const;
    void x_AddQuals(const CCdregion& cds)  const;
    void x_AddQuals(const CProt_ref& prot) const;
    void x_AddGeneQuals(const CSeq_feat& gene, CScope& scope) const;
    void x_AddCdregionQuals(const CSeq_feat& cds, CFFContext& ctx,
        bool& pseudo) const;
    const CProt_ref* x_AddProteinQuals(CBioseq_Handle& prot) const;
    void x_AddProductIdQuals(CBioseq_Handle& prod, EFeatureQualifier slot) const;
    void x_AddRnaQuals(const CSeq_feat& feat, CFFContext& ctx,
        bool& pseudo) const;
    void x_AddProtQuals(const CSeq_feat& feat, CFFContext& ctx,
        bool pseudo) const;
    void x_AddRegionQuals(const CSeq_feat& feat, CFFContext& ctx) const;
    void x_AddQuals(const CGene_ref& gene) const;
    void x_AddExtQuals(const CSeq_feat::TExt& ext) const;
    void x_AddGoQuals(const CUser_object& uo) const;
    void x_AddExceptionQuals(CFFContext& ctx) const;
    void x_ImportQuals(const CSeq_feat::TQual& quals) const;
    void x_CleanQuals(void) const;
    
    // typdef
    typedef CQualContainer<EFeatureQualifier> TQuals;
    typedef TQuals::iterator TQI;
    typedef TQuals::const_iterator TQCI;

    
    // qualifiers container
    void x_AddQual(EFeatureQualifier slot, const IFlatQVal* value) const {
        m_Quals.AddQual(slot, value);
    }
    void x_RemoveQuals(EFeatureQualifier slot) const {
        m_Quals.RemoveQuals(slot);
    }
    bool x_HasQual(EFeatureQualifier slot) const { 
        return m_Quals.HasQual(slot);
    }
    pair<TQCI, TQCI> x_GetQual(EFeatureQualifier slot) const {
        return const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    }
    
    // format
    void x_FormatQuals(void) const;
    void x_FormatNoteQuals(void) const;
    void x_FormatQual(EFeatureQualifier slot, const string& name,
        CFlatFeature::TQuals& qvec, IFlatQVal::TFlags flags = 0) const;
    void x_FormatNoteQual(EFeatureQualifier slot, const string& name, 
        CFlatFeature::TQuals& qvec, IFlatQVal::TFlags flags = 0) const 
        { x_FormatQual(slot, name, qvec, flags | IFlatQVal::fIsNote); }

    // data
    mutable CSeqFeatData::ESubtype m_Type;
    mutable TQuals                 m_Quals;
    EMapped                        m_Mapped;
};


class CSourceFeatureItem : public CFeatureItemBase
{
public:
    typedef CRange<TSeqPos> TRange;

    CSourceFeatureItem(const CBioSource& src, TRange range, CFFContext& ctx);
    CSourceFeatureItem(const CSeq_feat& feat, CFFContext& ctx,
                           const CSeq_loc* loc = 0)
        : CFeatureItemBase(feat, ctx, loc), m_WasDesc(false)
    {
        x_GatherInfo(ctx);
    }
    CSourceFeatureItem(const CMappedFeat& feat, CFFContext& ctx,
                           const CSeq_loc* loc = 0)
        : CFeatureItemBase(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_WasDesc(false)
    {
        x_GatherInfo(ctx);
    }

    bool WasDesc(void) const { return m_WasDesc; }
    const CBioSource& GetSource(void) const {
        return m_Feat->GetData().GetBiosrc();
    }
    string GetKey(void) const { return "source"; }

private:
    void x_GatherInfo(CFFContext& ctx) {
        x_AddQuals(ctx);
    }

    void x_AddQuals(CFFContext& ctx)       const;
    void x_AddQuals(const CBioSource& src, CFFContext& ctx) const;
    void x_AddQuals(const COrg_ref& org, CFFContext& ctx) const;

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual (ESourceQualifier slot, const IFlatQVal* value) const {
        m_Quals.AddQual(slot, value); 
    }

    void x_FormatQuals   (void) const;
    void x_FormatGBNoteQuals(void) const;
    void x_FormatNoteQuals(void) const;
    void x_FormatQual(ESourceQualifier slot, const string& name,
        CFlatFeature::TQuals& qvec, IFlatQVal::TFlags flags = 0) const;
    void x_FormatNoteQual(ESourceQualifier slot, const string& name,
            CFlatFeature::TQuals& qvec, IFlatQVal::TFlags flags = 0) const {
        x_FormatQual(slot, name, qvec, flags | IFlatQVal::fIsNote); 
    }

    typedef CQualContainer<ESourceQualifier> TQuals;
    typedef TQuals::const_iterator           TQCI;

    bool           m_WasDesc;
    mutable TQuals m_Quals;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2004/03/31 15:59:54  ucko
* CFeatureItem::x_GetQual: make sure to call the const version of
* GetQuals to fix WorkShop build errors.
*
* Revision 1.9  2004/03/30 20:26:32  shomrat
* Separated quals container from feature class
*
* Revision 1.8  2004/03/25 20:27:52  shomrat
* moved constructor body to .cpp file
*
* Revision 1.7  2004/03/18 15:27:25  shomrat
* + GetId for ftable format
*
* Revision 1.6  2004/03/08 21:00:48  shomrat
* Exception qualifiers gathering
*
* Revision 1.5  2004/03/05 18:49:26  shomrat
* enhancements to qualifier collection and formatting
*
* Revision 1.4  2004/02/11 22:48:18  shomrat
* override GetKey
*
* Revision 1.3  2004/02/11 16:38:51  shomrat
* added methods for gathering and formatting of source features
*
* Revision 1.2  2004/01/14 15:57:23  shomrat
* removed commented code
*
* Revision 1.1  2003/12/17 19:46:43  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP */
