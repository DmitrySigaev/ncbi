#ifndef SRA__READER__SRA__SNPREAD__HPP
#define SRA__READER__SRA__SNPREAD__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SNP files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <util/simple_buffer.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <map>
#include <list>

#include <ncbi/ncbi.h>
#include <insdc/insdc.h>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_entry;
class CSeq_annot;
class CSeq_graph;
class CSeq_feat;
class CSeq_interval;
class CSeq_point;
class CUser_object;
class CUser_field;
class CBioseq;
class CSeq_literal;
class CSeq_annot_SNP_Info;

class CSNPDbSeqIterator;
class CSNPDbPageIterator;
class CSNPDbFeatIterator;

struct SSNPDb_Defs
{
    enum ESearchMode {
        eSearchByOverlap,
        eSearchByStart
    };

    typedef Uint8 TFilter;

    struct SFilter {
        SFilter()
            : m_Filter(0),
              m_FilterMask(0)
            {
            }
        SFilter(TFilter filter,
                TFilter filter_mask = TFilter(-1))
            : m_Filter(filter),
              m_FilterMask(filter_mask)
            {
            }

        void SetNoFilter(void)
            {
                m_Filter = 0;
                m_FilterMask = 0;
            }
        void SetFilter(TFilter filter,
                       TFilter filter_mask = TFilter(-1))
            {
                m_Filter = filter;
                m_FilterMask = filter_mask;
            }

        void Normalize(void)
            {
                m_Filter &= m_FilterMask;
            }

        bool IsSet(void) const
            {
                return m_FilterMask != 0;
            }
        bool Matches(TFilter bits) const
            {
                return (bits & m_FilterMask) == m_Filter;
            }

        TFilter m_Filter;
        TFilter m_FilterMask;
    };

    struct SSelector {
        SSelector(const SFilter& filter = SFilter())
            : m_SearchMode(eSearchByOverlap),
              m_Filter(filter)
            {
            }
        SSelector(ESearchMode search_mode,
                  const SFilter& filter = SFilter())
            : m_SearchMode(search_mode),
              m_Filter(filter)
            {
            }

        SSelector& SetNoFilter(void)
            {
                m_Filter = SFilter();
                return *this;
            }
        SSelector& SetFilter(const SFilter& filter)
            {
                m_Filter = filter;
                return *this;
            }
        SSelector& SetFilter(TFilter filter,
                             TFilter filter_mask = TFilter(-1))
            {
                m_Filter = SFilter(filter, filter_mask);
                return *this;
            }

        ESearchMode m_SearchMode;
        SFilter m_Filter;
    };
};


class NCBI_SRAREAD_EXPORT CSNPDb_Impl : public CObject, public SSNPDb_Defs
{
public:
    CSNPDb_Impl(CVDBMgr& mgr,
                CTempString path_or_acc);
    virtual ~CSNPDb_Impl(void);

    // SSeqInfo holds cached refseq information - ids, len, rows
    struct SSeqInfo {
        string m_Name, m_SeqId;
        CBioseq::TId m_Seq_ids;
        CSeq_id_Handle m_Seq_id_Handle;
        CRef<CSeq_id>& GetMainSeq_id(void) {
            return m_Seq_ids.front();
        }
        const CRef<CSeq_id>& GetMainSeq_id(void) const {
            return m_Seq_ids.front();
        }
        const CSeq_id_Handle& GetMainSeq_id_Handle(void) const {
            return m_Seq_id_Handle;
        }
        struct SPageSet {
            TSeqPos m_SeqPos;
            TSeqPos m_PageCount;
            TVDBRowId m_RowId;

            TSeqPos GetSeqPosEnd(TSeqPos page_size) const {
                return m_SeqPos + m_PageCount*page_size;
            }
            TVDBRowId GetRowIdEnd(void) const {
                return m_RowId + m_PageCount;
            }
        };

        typedef vector<SPageSet> TPageSets; // sorted by sequence position
        TPageSets m_PageSets;
        bool m_Circular;
        //vector<TSeqPos> m_AlnOverStarts; // relative to m_RowFirst
    };
    typedef list<SSeqInfo> TSeqInfoList;
    typedef map<string, TSeqInfoList::iterator, PNocase> TSeqInfoMapByName;
    typedef map<CSeq_id_Handle, TSeqInfoList::iterator> TSeqInfoMapBySeq_id;
    
    const TSeqInfoList& GetSeqInfoList(void) const {
        return m_SeqList;
    }
    const TSeqInfoMapByName& GetSeqInfoMapByName(void) const {
        return m_SeqMapByName;
    }
    const TSeqInfoMapBySeq_id& GetSeqInfoMapBySeq_id(void) const {
        return m_SeqMapBySeq_id;
    }

    TSeqPos GetPageSize(void) const;

protected:
    friend class CSNPDbSeqIterator;
    friend class CSNPDbPageIterator;
    friend class CSNPDbFeatIterator;

    // SSNPTableCursor is helper accessor structure for SNP feature table
    struct SSNPTableCursor;

    // SExtraTableCursor is helper accessor structure for extra info (alleles)
    struct SExtraTableCursor;

    // open tables
    void OpenTable(CVDBTable& table,
                   const char* table_name,
                   volatile bool& table_is_opened);

    const CVDBTable& SNPTable(void) {
        return m_SNPTable;
    }

    void OpenExtraTable(void);

    const CVDBTable& ExtraTable(void) {
        if ( !m_ExtraTableIsOpened ) {
            OpenExtraTable();
        }
        return m_ExtraTable;
    }

    // get table accessor object for exclusive access
    CRef<SSNPTableCursor> SNP(TVDBRowId row = 0);
    // return table accessor object for reuse
    void Put(CRef<SSNPTableCursor>& curs, TVDBRowId row = 0);

    // get extra table accessor object for exclusive access
    CRef<SExtraTableCursor> Extra(TVDBRowId row = 0);
    // return table accessor object for reuse
    void Put(CRef<SExtraTableCursor>& curs, TVDBRowId row = 0);

private:
    CVDBMgr m_Mgr;
    string m_DbPath;
    CVDB m_Db;
    CVDBTable m_SNPTable;
    CVDBTable m_ExtraTable;

    CFastMutex m_TableMutex;
    bool m_ExtraTableIsOpened;

    CVDBObjectCache<SSNPTableCursor> m_SNP;
    CVDBObjectCache<SExtraTableCursor> m_Extra;

    TSeqInfoList m_SeqList; // list of cached refseqs' information
    TSeqInfoMapByName m_SeqMapByName; // index for refseq info lookup
    TSeqInfoMapBySeq_id m_SeqMapBySeq_id; // index for refseq info lookup
};


class CSNPDb : public CRef<CSNPDb_Impl>, public SSNPDb_Defs
{
public:
    CSNPDb(void)
        {
        }
    explicit CSNPDb(CSNPDb_Impl* impl)
        : CRef<CSNPDb_Impl>(impl)
        {
        }
    CSNPDb(CVDBMgr& mgr,
           CTempString path_or_acc)
        : CRef<CSNPDb_Impl>(new CSNPDb_Impl(mgr, path_or_acc))
        {
        }
};


template<class Enum>
class CSafeFlags {
public:
    typedef Enum enum_type;
    typedef typename underlying_type<enum_type>::type storage_type;

    CSafeFlags()
        : m_Flags(0)
        {
        }
    CSafeFlags(enum_type flags)
        : m_Flags(flags)
        {
        }

    storage_type get() const
        {
            return m_Flags;
        }

    DECLARE_OPERATOR_BOOL(m_Flags != 0);

    bool operator==(const CSafeFlags& b) const
        {
            return get() == b.get();
        }
    bool operator!=(const CSafeFlags& b) const
        {
            return get() != b.get();
        }

    CSafeFlags operator&(const CSafeFlags& b) const
        {
            return get() & b.get();
        }
    CSafeFlags operator|(const CSafeFlags& b) const
        {
            return get() | b.get();
        }
    CSafeFlags operator^(const CSafeFlags& b) const
        {
            return get() ^ b.get();
        }
    CSafeFlags& operator&=(const CSafeFlags& b)
        {
            m_Flags &= b.get();
            return *this;
        }
    CSafeFlags& operator|(const CSafeFlags& b)
        {
            m_Flags |= b.get();
            return *this;
        }
    CSafeFlags& operator^(const CSafeFlags& b)
        {
            m_Flags ^= b.get();
            return *this;
        }
    CSafeFlags without(const CSafeFlags& b) const
        {
            return CSafeFlags(get()&~b.get());
        }
    CSafeFlags& reset(const CSafeFlags& b)
        {
            m_Flags &= ~b.get();
            return *this;
        }

    CSafeFlags operator~() const
        {
            return CSafeFlags(~get());
        }

private:
    CSafeFlags(storage_type flags)
        : m_Flags(flags)
        {
        }

    storage_type m_Flags;
};
template<class Enum> inline CSafeFlags<Enum> ToFlags(Enum v)
{
    return CSafeFlags<Enum>(v);
}


class NCBI_SRAREAD_EXPORT CSNPDbSeqIterator : public SSNPDb_Defs
{
public:
    CSNPDbSeqIterator(void)
        {
        }
    CSNPDbSeqIterator(const CSNPDb& db,
                      CSNPDb_Impl::TSeqInfoList::const_iterator iter)
        : m_Db(db),
          m_Iter(iter)
        {
        }
    explicit CSNPDbSeqIterator(const CSNPDb& db);
    enum EByName {
        eByName
    };
    CSNPDbSeqIterator(const CSNPDb& db, const string& name,
                      EByName /*by_name*/);
    CSNPDbSeqIterator(const CSNPDb& db, const CSeq_id_Handle& seq_id);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Db && m_Iter != m_Db->GetSeqInfoList().end());

    const CSNPDb_Impl::SSeqInfo& GetInfo(void) const;
    const CSNPDb_Impl::SSeqInfo& operator*(void) const {
        return GetInfo();
    }
    const CSNPDb_Impl::SSeqInfo* operator->(void) const {
        return &GetInfo();
    }

    CSNPDbSeqIterator& operator++(void) {
        ++m_Iter;
        return *this;
    }

    const string& GetAccession(void) const {
        return m_Iter->m_SeqId;
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return m_Iter->GetMainSeq_id();
    }
    const CSeq_id_Handle& GetSeqIdHandle(void) const {
        return m_Iter->GetMainSeq_id_Handle();
    }
    const CBioseq::TId& GetSeqIds(void) const {
        return m_Iter->m_Seq_ids;
    }

    bool IsCircular(void) const;

    TSeqPos GetPageSize(void) const {
        return GetDb().GetPageSize();
    }
    TSeqPos GetMaxSNPLength(void) const;

    Uint8 GetSNPCount(CRange<TSeqPos> range) const;
    Uint8 GetSNPCount(void) const;
    CRange<TSeqPos> GetSNPRange(void) const;
    CRange<TVDBRowId> GetVDBRowRange(void) const;

    enum EFlags {
        fSingleGraph    = 1<<0,
        fDefaultFlags   = 0
    };
    typedef CSafeFlags<EFlags> TFlags;

    CRef<CSeq_graph> GetCoverageGraph(CRange<TSeqPos> range,
                                      const SFilter& filter = SFilter()) const;
    CRef<CSeq_annot> GetCoverageAnnot(CRange<TSeqPos> range,
                                      const SFilter& filter,
                                      TFlags flags = fDefaultFlags) const;
    CRef<CSeq_annot> GetCoverageAnnot(CRange<TSeqPos> range,
                                      TFlags flags = fDefaultFlags) const;

    CRef<CSeq_annot> GetFeatAnnot(CRange<TSeqPos> range,
                                  const SFilter& filter,
                                  TFlags flags = fDefaultFlags) const;
    CRef<CSeq_annot> GetFeatAnnot(CRange<TSeqPos> range,
                                  TFlags flags = fDefaultFlags) const;
    typedef vector< CRef<CSeq_annot> > TAnnotSet;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 const string& annot_name,
                                 const SFilter& filter,
                                 TFlags flags = fDefaultFlags) const;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 const SFilter& filter,
                                 TFlags flags = fDefaultFlags) const;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 TFlags flags = fDefaultFlags) const;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 const string& annot_name,
                                 TFlags flags = fDefaultFlags) const;
    typedef pair< CRef<CSeq_annot>, CRef<CSeq_annot_SNP_Info> > TPackedAnnot;
    TPackedAnnot GetPackedFeatAnnot(CRange<TSeqPos> range,
                                    const SFilter& filter,
                                    TFlags flags = fDefaultFlags) const;
    TPackedAnnot GetPackedFeatAnnot(CRange<TSeqPos> range,
                                    TFlags flags = fDefaultFlags) const;

protected:
    friend class CSNPDbPageIterator;

    CSNPDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }

private:
    CSNPDb m_Db;
    CSNPDb_Impl::TSeqInfoList::const_iterator m_Iter;
};


class NCBI_SRAREAD_EXPORT CSNPDbPageIterator : public SSNPDb_Defs
{
public:
    CSNPDbPageIterator(void);

    CSNPDbPageIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       TSeqPos ref_pos = 0,
                       TSeqPos window = 0,
                       ESearchMode search_mode = eSearchByOverlap);
    CSNPDbPageIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       COpenRange<TSeqPos> ref_range,
                       ESearchMode search_mode = eSearchByOverlap);
    CSNPDbPageIterator(const CSNPDbSeqIterator& seq,
                       COpenRange<TSeqPos> ref_range,
                       ESearchMode search_mode = eSearchByOverlap);
    CSNPDbPageIterator(const CSNPDbPageIterator& iter);
    ~CSNPDbPageIterator(void);

    CSNPDbPageIterator& operator=(const CSNPDbPageIterator& iter);

    CSNPDbPageIterator& Select(COpenRange<TSeqPos> ref_range,
                               ESearchMode search_mode = eSearchByOverlap);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_CurrPagePos < m_SearchRange.GetToOpen());

    CSNPDbPageIterator& operator++(void) {
        x_Next();
        return *this;
    }

    const CRange<TSeqPos>& GetSearchRange(void) const {
        return m_SearchRange;
    }
    ESearchMode GetSearchMode(void) const {
        return m_SearchMode;
    }

    const string& GetAccession(void) const {
        return GetRefIter().GetAccession();
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return GetRefIter().GetSeqId();
    }
    CSeq_id_Handle GetSeqIdHandle(void) const {
        return GetRefIter().GetSeqIdHandle();
    }

    TSeqPos GetPageSize(void) const {
        return GetDb().GetPageSize();
    }
    TSeqPos GetPagePos(void) const {
        return m_CurrPagePos;
    }
    CRange<TSeqPos> GetPageRange(void) const;

    TVDBRowId GetPageRowId(void) const {
        return m_CurrPageRowId;
    }

    const CSNPDbSeqIterator& GetRefIter(void) const {
        return m_SeqIter;
    }

    Uint4 GetFeatCount(void) const;

    CTempString GetFeatType(void) const;

    CVDBValueFor<Uint4> GetCoverageValues(void) const;

protected:
    friend class CSNPDbFeatIterator;

    CSNPDb_Impl& GetDb(void) const {
        return GetRefIter().GetDb();
    }
    const CSNPDb_Impl::SSNPTableCursor& Cur(void) const {
        return *m_Cur;
    }

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

    void x_Next(void);

private:
    CSNPDbSeqIterator m_SeqIter; // refseq selector
    CRef<CSNPDb_Impl::SSNPTableCursor> m_Cur; // SNP table accessor
    
    CRange<TSeqPos> m_SearchRange; // requested refseq range
    
    size_t m_CurrPageSet;
    TVDBRowId m_CurrPageRowId;
    TSeqPos m_CurrPagePos;
    
    ESearchMode m_SearchMode;
};


class NCBI_SRAREAD_EXPORT CSNPDbFeatIterator : public SSNPDb_Defs
{
public:
    CSNPDbFeatIterator(void);
    CSNPDbFeatIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       TSeqPos ref_pos = 0,
                       TSeqPos window = 0,
                       const SSelector& sel = SSelector());
    CSNPDbFeatIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       COpenRange<TSeqPos> ref_range,
                       const SSelector& sel = SSelector());
    CSNPDbFeatIterator(const CSNPDbSeqIterator& seq,
                       COpenRange<TSeqPos> ref_range,
                       const SSelector& sel = SSelector());
    CSNPDbFeatIterator(const CSNPDbFeatIterator& iter);
    ~CSNPDbFeatIterator(void);

    CSNPDbFeatIterator& operator=(const CSNPDbFeatIterator& iter);

    CSNPDbFeatIterator& Select(COpenRange<TSeqPos> ref_range,
                               const SSelector& sel = SSelector());

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_CurrFeatId < m_FirstBadFeatId);

    CSNPDbFeatIterator& operator++(void) {
        x_Next();
        return *this;
    }

    TVDBRowId GetExtraRowId(void) const;

    const CSNPDbPageIterator& GetPageIter(void) const {
        return m_PageIter;
    }
    const CSNPDbSeqIterator& GetRefIter(void) const {
        return GetPageIter().GetRefIter();
    }

    const string& GetAccession(void) const {
        return GetPageIter().GetAccession();
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return GetPageIter().GetSeqId();
    }
    CSeq_id_Handle GetSeqIdHandle(void) const {
        return GetPageIter().GetSeqIdHandle();
    }
    const CRange<TSeqPos>& GetSearchRange(void) const {
        return GetPageIter().GetSearchRange();
    }
    ESearchMode GetSearchMode(void) const {
        return GetPageIter().GetSearchMode();
    }
 
    TSeqPos GetSNPPosition(void) const {
        return m_CurRange.GetFrom();
    }
    TSeqPos GetSNPLength(void) const {
        return m_CurRange.GetLength();
    }

    CTempString GetAlleles(void) const;

    enum EFeatIdPrefix {
        eFeatIdPrefix_none = 0,
        eFeatIdPrefix_rs = 1,
        eFeatIdPrefix_ss = 2
    };
    Uint4 GetFeatIdPrefix(void) const;
    Uint8 GetFeatId(void) const;

    Uint8 GetQualityCodes(void) const;
    void GetQualityCodes(vector<char>& codes) const;

    enum EFlags {
        fIncludeAlleles      = 1<<0,
        fIncludeRsId         = 1<<1,
        fIncludeQualityCodes = 1<<2,
        fIncludeNeighbors    = 1<<3,
        fUseSharedObjects    = 1<<8,
        fDefaultFlags = ( fIncludeAlleles |
                          fIncludeRsId |
                          fIncludeQualityCodes |
                          fIncludeNeighbors |
                          fUseSharedObjects )
    };
    typedef CSafeFlags<EFlags> TFlags;
    
    CRef<CSeq_feat> GetSeq_feat(TFlags flags = fDefaultFlags) const;

protected:
    CSNPDb_Impl& GetDb(void) const {
        return GetPageIter().GetDb();
    }
    const CSNPDb_Impl::SSNPTableCursor& Cur(void) const {
        return GetPageIter().Cur();
    }

    TVDBRowId GetPageRowId(void) const {
        return GetPageIter().GetPageRowId();
    }

    TSeqPos x_GetFrom(void) const;
    TSeqPos x_GetLength(void) const;
    
    void x_Init(const CSNPDb& snp_db);

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

    void x_Next(void);
    void x_Settle(void);
    enum EExcluded {
        eIncluded,
        eExluded,
        ePassedTheRegion
    };
    EExcluded x_Excluded(void);

    void x_SetFilter(const SSelector& sel);
    void x_InitPage(void);

private:
    CSNPDbPageIterator m_PageIter;
    mutable CRef<CSNPDb_Impl::SExtraTableCursor> m_Extra; // for alleles
    mutable TVDBRowId m_ExtraRowId;

    COpenRange<TSeqPos> m_CurRange; // current SNP refseq range
    SFilter m_Filter;

    Uint4 m_CurrFeatId, m_FirstBadFeatId;

    typedef CRef<CObject_id> TObjectIdCache;
    typedef map<CTempString, CRef<CUser_field> > TUserFieldCache;

    struct SCreateCache;
    mutable AutoPtr<SCreateCache> m_CreateCache;
    SCreateCache& x_GetCreateCache(void) const;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__SNPREAD__HPP
