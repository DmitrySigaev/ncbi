#ifndef OBJECTS_OBJMGR_IMPL___TSE_ASSIGNER__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_ASSIGNER__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/seq_id_translator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static const int kTSE_Place_id = 0;

class NCBI_XOBJMGR_EXPORT ITSE_Assigner : public CObject
{
public:
    typedef CTSE_Chunk_Info::TPlace          TPlace;
    typedef CTSE_Chunk_Info::TSequence       TSequence;
    typedef CTSE_Chunk_Info::TAssembly       TAssembly;
    typedef CTSE_Chunk_Info::TChunkId        TChunkId;
    typedef CTSE_Chunk_Info::TLocationSet    TLocationSet;
    typedef CTSE_Chunk_Info::TDescInfo       TDescInfo;
    typedef CTSE_Chunk_Info::TBioseq_setId   TBioseq_setId;
    typedef CTSE_Chunk_Info::TAssemblyInfo   TAssemblyInfo;
    typedef CTSE_Chunk_Info::TBioseqId       TBioseqId;

    virtual ~ITSE_Assigner() {}

    virtual void AddDescInfo(CTSE_Info&, const TDescInfo& info, TChunkId chunk_id) = 0;
    virtual void AddAnnotPlace(CTSE_Info&, const TPlace& place, TChunkId chunk_id) = 0;
    virtual void AddBioseqPlace(CTSE_Info&, TBioseq_setId place_id, TChunkId chunk_id) = 0;
    virtual void AddSeq_data(CTSE_Info&, const TLocationSet& location, 
                             CTSE_Chunk_Info& chunk) = 0;
    virtual void AddAssemblyInfo(CTSE_Info&, const TAssemblyInfo& info, TChunkId chunk_id) = 0;

    virtual void UpdateAnnotIndex(CTSE_Info&, CTSE_Chunk_Info& chunk) = 0;

    // loading results
    virtual void LoadDescr(CTSE_Info&, const TPlace& place, 
                           const CSeq_descr& descr) = 0;
    virtual void LoadAnnot(CTSE_Info&, const TPlace& place, 
                           CRef<CSeq_annot> annot) = 0;
    virtual void LoadBioseq(CTSE_Info&, const TPlace& place, 
                            CRef<CSeq_entry> entry) = 0;
    virtual void LoadSequence(CTSE_Info&, const TPlace& place, TSeqPos pos,
                              const TSequence& sequence) = 0;
    virtual void LoadAssembly(CTSE_Info&, const TBioseqId& seq_id,
                              const TAssembly& assembly) = 0;
    virtual void LoadSeq_entry(CTSE_Info&, CSeq_entry& entry, 
                               CTSE_SetObjectInfo* set_info) = 0;

    // get attach points from CTSE_Info
    static CBioseq_Base_Info& x_GetBase(CTSE_Info& tse, const TPlace& place);
    static CBioseq_Info& x_GetBioseq(CTSE_Info& tse, const TPlace& place);
    static CBioseq_set_Info& x_GetBioseq_set(CTSE_Info& tse, const TPlace& place);
    static CBioseq_Info& x_GetBioseq(CTSE_Info& tse, const TBioseqId& id);
    static CBioseq_set_Info& x_GetBioseq_set(CTSE_Info& tse, TBioseq_setId id);

    void SetSeqIdTranslator(CRef<ISeq_id_Translator> translator) { m_SeqIdTranslator = translator; }

protected:

    void PatchId(CSeq_entry& orig) const;
    CSeq_id_Handle PatchId(const CSeq_id_Handle& orig) const;
    TPlace PatchId(const TPlace& orig) const;
    void PatchId(CSeq_annot& orig) const;

    CRef<ISeq_id_Translator> m_SeqIdTranslator;


};


class NCBI_XOBJMGR_EXPORT CTSE_Default_Assigner : public ITSE_Assigner
{
public:

    CTSE_Default_Assigner();

    virtual ~CTSE_Default_Assigner();

    virtual void AddDescInfo(CTSE_Info&, const TDescInfo& info, TChunkId chunk_id);
    virtual void AddAnnotPlace(CTSE_Info&, const TPlace& place, TChunkId chunk_id);
    virtual void AddBioseqPlace(CTSE_Info&, TBioseq_setId place_id, TChunkId chunk_id);
    virtual void AddSeq_data(CTSE_Info&, const TLocationSet& location, 
                             CTSE_Chunk_Info& chunk);
    virtual void AddAssemblyInfo(CTSE_Info&, const TAssemblyInfo& info, TChunkId chunk_id);

    virtual void UpdateAnnotIndex(CTSE_Info&, CTSE_Chunk_Info& chunk);

    // loading results
    virtual void LoadDescr(CTSE_Info&, const TPlace& place, 
                           const CSeq_descr& descr);
    virtual void LoadAnnot(CTSE_Info&, const TPlace& place, 
                           CRef<CSeq_annot> annot);
    virtual void LoadBioseq(CTSE_Info&, const TPlace& place, 
                            CRef<CSeq_entry> entry);
    virtual void LoadSequence(CTSE_Info&, const TPlace& place, TSeqPos pos,
                              const TSequence& sequence);
    virtual void LoadAssembly(CTSE_Info&, const TBioseqId& seq_id,
                              const TAssembly& assembly);
    virtual void LoadSeq_entry(CTSE_Info&, CSeq_entry& entry, 
                               CTSE_SetObjectInfo* set_info);

protected:
    void LoadDescr_NoPatch(CTSE_Info& tse, 
                           const TPlace& place, 
                           const CSeq_descr& descr);
    void LoadAnnot_NoPatch(CTSE_Info& tse,
                           const TPlace& place, 
                           CRef<CSeq_annot> annot);
    void LoadBioseq_NoPatch(CTSE_Info& tse,
                            const TPlace& place, 
                            CRef<CSeq_entry> entry);
    void LoadSequence_NoPatch(CTSE_Info& tse, 
                              const TPlace& place, 
                              TSeqPos pos, 
                              const TSequence& sequence);
    void LoadSeq_entry_NoPatch(CTSE_Info& tse,
                               CSeq_entry& entry, 
                               CTSE_SetObjectInfo* set_info);


private:


    CTSE_Default_Assigner(const CTSE_Default_Assigner&);
    CTSE_Default_Assigner& operator=(const CTSE_Default_Assigner&);
};



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
inline ITSE_Assigner::TPlace 
PatchSeqId(const ITSE_Assigner::TPlace& info, 
           const ISeq_id_Translator& tr)
{
    return make_pair(tr.TranslateToOrig(info.first), info.second);
}

inline CSeq_id_Handle PatchSeqId(const CSeq_id_Handle& id, 
                                 const ISeq_id_Translator& tr)
{
    return tr.TranslateToOrig(id);
}

inline void PatchSeqId(CSeq_id& id, const ISeq_id_Translator& tr)
{
    CSeq_id_Handle handle = tr.TranslateToOrig(CSeq_id_Handle::GetHandle(id));
    id.Assign(*handle.GetSeqId());
}

template<class TCont> 
inline void PatchSeqIds( TCont& ids, const ISeq_id_Translator& tr)
{
    typedef typename TCont::iterator TContIt;
    typedef typename TCont::value_type value_type;

    for( TContIt it = ids.begin(); it != ids.end(); ++it ) {
        value_type& sid = *it;
        PatchSeqId(*sid, tr);
    }   
}

template<class T>
inline CRef<T> PatchSeqId_Copy(const T& orig, const ISeq_id_Translator& tr)
{
    CRef<T> ret(new T);
    ret->Assign(orig);
    PatchSeqId(*ret, tr);
    return ret;
}

NCBI_XOBJMGR_EXPORT
void PatchSeqId(CSeq_loc& loc, const ISeq_id_Translator& tr);

class CSeq_feat;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CSeq_feat& feat, const ISeq_id_Translator& tr);

class CSeq_align;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CSeq_align& align, const ISeq_id_Translator& tr);

class CSeq_graph;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CSeq_graph& graph, const ISeq_id_Translator& tr);

class CSeq_annot;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CSeq_annot& annot, const ISeq_id_Translator& tr);

class CSeq_entry;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CSeq_entry& entry, const ISeq_id_Translator& tr);

class CBioseq;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CBioseq& bioseq, const ISeq_id_Translator& tr);

class CBioseq_set;
NCBI_XOBJMGR_EXPORT
void PatchSeqId(CBioseq_set& bioseq_set, const ISeq_id_Translator& tr);

END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.5  2005/11/15 15:54:31  vasilche
* Replaced CTSE_SNP_InfoMap with CTSE_SetObjectInfo to allow additional info.
*
* Revision 1.4  2005/08/31 19:36:44  didenko
* Reduced the number of objects copies which are being created while doing PatchSeqIds
*
* Revision 1.3  2005/08/31 14:47:14  didenko
* Changed the object parameter type for LoadAnnot and LoadBioseq methods
*
* Revision 1.2  2005/08/29 16:15:01  didenko
* Modified default implementation of ITSE_Assigner in a way that it can be used as base class for
* the user's implementations of this interface
*
* Revision 1.1  2005/08/25 14:05:36  didenko
* Restructured TSE loading process
*
*
* ===========================================================================
*/

#endif //OBJECTS_OBJMGR_IMPL___TSE_ASSIGNER__HPP
