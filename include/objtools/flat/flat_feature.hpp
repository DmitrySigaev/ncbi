#ifndef OBJECTS_FLAT___FLAT_FEATURE__HPP
#define OBJECTS_FLAT___FLAT_FEATURE__HPP

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
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*/

#include <objects/flat/flat_formatter.hpp>
#include <objects/flat/flat_qual_slots.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// low-level formatted qualifier
struct SFlatQual : public CObject
{
    enum EStyle {
        eEmpty,   // /name [value ignored]
        eQuoted,  // /name="value"
        eUnquoted // /name=value
    };

    SFlatQual(const string& name, const string& value, EStyle style = eQuoted)
        : m_Name(name), m_Value(value), m_Style(style)
        { }

    string m_Name, m_Value;
    EStyle m_Style;
};
typedef vector<CRef<SFlatQual> > TFlatQuals;


// abstract qualifier value -- see flat_quals.hpp for derived classes
class IFlatQV : public CObject
{
public:
    enum EFlags {
        fIsNote   = 0x1,
        fIsSource = 0x2
    };
    typedef int TFlags; // binary OR of EFlags

    virtual void Format(TFlatQuals& quals, const string& name,
                        SFlatContext& ctx, TFlags flags = 0) const = 0;

protected:
    static void x_AddFQ(TFlatQuals& q, const string& n, const string& v,
                        SFlatQual::EStyle st = SFlatQual::eQuoted)
        { q.push_back(CRef<SFlatQual>(new SFlatQual(n, v, st))); }
};


struct SFlatFeature : public CObject
{
    string                   m_Key;
    CRef<SFlatLoc>           m_Loc;
    vector<CRef<SFlatQual> > m_Quals;
    CConstRef<CSeq_feat>     m_Feat;
};


class IFlattishFeature : public IFlatItem
{
public:
    CRef<SFlatFeature> Format(void) const;
    void Format(IFlatFormatter& f) const { f.FormatFeature(*Format()); }
    bool operator<(const IFlattishFeature& f2) const 
        { return m_Feat->Compare(*f2.m_Feat, *m_Loc, *f2.m_Loc) < 0; }

protected:
    IFlattishFeature(const CSeq_feat& feat, SFlatContext& ctx,
                     const CSeq_loc* loc = 0)
        : m_Feat(&feat), m_Context(&ctx),
          m_Loc(loc ? loc : &feat.GetLocation())
        { }

    virtual void x_AddQuals   (void) const = 0;
    virtual void x_FormatQuals(void) const = 0;

    CConstRef<CSeq_feat>        m_Feat;
    mutable CRef<SFlatContext>  m_Context;
    // below fields populated on demand
    mutable CConstRef<CSeq_loc> m_Loc;
    mutable CRef<SFlatFeature>  m_FF; // cached here
};


class CFlattishFeature : public IFlattishFeature
{
public:
    CFlattishFeature(const CSeq_feat& feat, SFlatContext& ctx,
                     const CSeq_loc* loc = 0, bool is_product = false)
        : IFlattishFeature(feat, ctx, loc), m_IsProduct(is_product)
        { }
    CFlattishFeature(const CMappedFeat& feat, SFlatContext& ctx,
                     const CSeq_loc* loc = 0, bool is_product = false)
        : IFlattishFeature(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_IsProduct(is_product)
        { }

private:
    void x_AddQuals(void) const;
    void x_AddQuals(const CGene_ref& gene) const;
    void x_AddQuals(const CCdregion& cds)  const;
    void x_AddQuals(const CProt_ref& prot) const;
    // ...

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual(EFeatureQualifier slot, const IFlatQV* value) const
        { m_Quals.insert(TQuals::value_type(slot,CConstRef<IFlatQV>(value))); }
    void x_ImportQuals(const CSeq_feat::TQual& quals) const;

    void x_FormatQuals   (void) const;
    void x_FormatQual    (EFeatureQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const;
    void x_FormatNoteQual(EFeatureQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const
        { x_FormatQual(slot, name, flags | IFlatQV::fIsNote); }

    typedef multimap<EFeatureQualifier, CConstRef<IFlatQV> > TQuals;
    mutable CSeqFeatData::ESubtype m_Type;
    mutable TQuals                 m_Quals;
    bool                           m_IsProduct;
};


class CFlattishSourceFeature : public IFlattishFeature
{
public:
    CFlattishSourceFeature(const CSeq_feat& feat, SFlatContext& ctx,
                           const CSeq_loc* loc = 0)
        : IFlattishFeature(feat, ctx, loc), m_WasDesc(false)
        { }
    CFlattishSourceFeature(const CMappedFeat& feat, SFlatContext& ctx,
                           const CSeq_loc* loc = 0)
        : IFlattishFeature(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_WasDesc(false)
        { }
    CFlattishSourceFeature(const CBioSource& src, SFlatContext& ctx);
    CFlattishSourceFeature(const COrg_ref& org,   SFlatContext& ctx);

private:
    void x_AddQuals(void)                  const;
    void x_AddQuals(const CBioSource& src) const;
    void x_AddQuals(const COrg_ref&   org) const;

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual (ESourceQualifier slot, const IFlatQV* value) const
        { m_Quals.insert(TQuals::value_type(slot,CConstRef<IFlatQV>(value))); }

    void x_FormatQuals   (void) const;
    void x_FormatQual    (ESourceQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const;
    void x_FormatNoteQual(ESourceQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const
        { x_FormatQual(slot, name, flags | IFlatQV::fIsNote); }

    typedef multimap<ESourceQualifier, CConstRef<IFlatQV> > TQuals;

    bool           m_WasDesc;
    mutable TQuals m_Quals;
};


///////////////////////////////////////////////////////////////////////////
//
// INLINE METHODS


inline
CFlattishSourceFeature::CFlattishSourceFeature(const CBioSource& src,
                                               SFlatContext& ctx)
    : IFlattishFeature(*new CSeq_feat, ctx, ctx.m_Location), m_WasDesc(true)
{
    CSeq_feat& feat = const_cast<CSeq_feat&>(*m_Feat);
    feat.SetData().SetBiosrc(const_cast<CBioSource&>(src));
    feat.SetLocation().Assign(*m_Loc);
}


inline
CFlattishSourceFeature::CFlattishSourceFeature(const COrg_ref& org,
                                               SFlatContext& ctx)
    : IFlattishFeature(*new CSeq_feat, ctx, ctx.m_Location), m_WasDesc(true)
{
    CSeq_feat& feat = const_cast<CSeq_feat&>(*m_Feat);
    feat.SetData().SetOrg(const_cast<COrg_ref&>(org));
    feat.SetLocation().Assign(*m_Loc);
}


inline
void CFlattishFeature::x_FormatQual(EFeatureQualifier slot, const string& name,
                                    IFlatQV::TFlags flags) const
{
    typedef TQuals::const_iterator TQCI;
    pair<TQCI, TQCI> range
        = const_cast<const TQuals&>(m_Quals).equal_range(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(m_FF->m_Quals, name, *m_Context, flags);
    }
}


inline
void CFlattishSourceFeature::x_FormatQual(ESourceQualifier slot,
                                          const string& name,
                                          IFlatQV::TFlags flags) const
{
    typedef TQuals::const_iterator TQCI;
    pair<TQCI, TQCI> range
        = const_cast<const TQuals&>(m_Quals).equal_range(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(m_FF->m_Quals, name, *m_Context,
                           flags | IFlatQV::fIsSource);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_FEATURE__HPP */
