#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_SET_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_SET_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Seq_entry info -- entry for data source
*
*/


#include <objmgr/impl/bioseq_base_info.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declaration
class CBioseq_set;
class CSeq_entry_Info;

////////////////////////////////////////////////////////////////////
//
//  CBioseq_set_Info::
//
//    General information and indexes for Bioseq-set object
//


class NCBI_XOBJMGR_EXPORT CBioseq_set_Info : public CBioseq_Base_Info
{
    typedef CBioseq_Base_Info TParent;
public:
    // 'ctors
    CBioseq_set_Info(void);
    explicit CBioseq_set_Info(const CBioseq_set_Info& info);
    explicit CBioseq_set_Info(CBioseq_set& seqset);
    virtual ~CBioseq_set_Info(void);

    typedef CBioseq_set TObject;

    CConstRef<TObject> GetCompleteBioseq_set(void) const;
    CConstRef<TObject> GetBioseq_setCore(void) const;

    // Bioseq-set access
    typedef TObject::TId TId;
    bool IsSetId(void) const;
    const TId& GetId(void) const;
    void SetId(const TId& v);

    bool IsSetDescr(void) const;
    const TDescr& GetDescr(void) const;
    void SetDescr(TDescr& v);
    void ResetDescr(void);

    typedef TObject::TColl TColl;
    bool IsSetColl(void) const;
    const TColl& GetColl(void) const;
    void SetColl(const TColl& v);

    typedef TObject::TLevel TLevel;
    bool IsSetLevel(void) const;
    TLevel GetLevel(void) const;
    void SetLevel(TLevel v);

    typedef TObject::TClass TClass;
    bool IsSetClass(void) const;
    TClass GetClass(void) const;
    void SetClass(TClass v);

    typedef TObject::TRelease TRelease;
    bool IsSetRelease(void) const;
    const TRelease& GetRelease(void) const;
    void SetRelease(const TRelease& v);

    typedef TObject::TDate TDate;
    bool IsSetDate(void) const;
    const TDate& GetDate(void) const;
    void SetDate(const TDate& v);

    typedef vector< CRef<CSeq_entry_Info> > TSeq_set;
    bool IsSetSeq_set(void) const;
    const TSeq_set& GetSeq_set(void) const;
    CRef<CSeq_entry_Info> AddEntry(CSeq_entry& entry, int index = -1);
    void AddEntry(CRef<CSeq_entry_Info> entry, int index = -1);
    void RemoveEntry(CRef<CSeq_entry_Info> entry);

    // initialization
    // attaching/detaching to CDataSource (it's in CTSE_Info)
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    // attaching/detaching to CTSE_Info
    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    // index
    void UpdateAnnotIndex(void) const;
    virtual void x_UpdateAnnotIndexContents(CTSE_Info& tse);
    
    // modification
    void x_AttachEntry(CRef<CSeq_entry_Info> info);
    void x_DetachEntry(CRef<CSeq_entry_Info> info);

protected:
    friend class CDataSource;
    friend class CScope_Impl;

    friend class CTSE_Info;
    friend class CSeq_entry_Info;
    friend class CBioseq_Info;
    friend class CSeq_annot_Info;

    friend class CSeq_entry_CI;
    friend class CSeq_entry_I;
    friend class CSeq_annot_CI;
    friend class CSeq_annot_I;
    friend class CAnnotTypes_CI;

    void x_DSAttachContents(void);
    void x_DSDetachContents(void);

    void x_ParentAttach(CSeq_entry_Info& parent);
    void x_ParentDetach(CSeq_entry_Info& parent);

    TObject& x_GetObject(void);
    const TObject& x_GetObject(void) const;

    void x_SetObject(TObject& obj);
    void x_SetObject(const CBioseq_set_Info& info);

    int x_GetBioseq_set_Id(const CObject_id& object_id);

    TObjAnnot& x_SetObjAnnot(void);
    void x_ResetObjAnnot(void);

    void x_DoUpdateObject(void);
    static CRef<TObject> sx_ShallowCopy(const TObject& obj);

private:
    // core object
    CRef<TObject>       m_Object;

    // members
    TSeq_set            m_Seq_set;

    // index information
    int                 m_Bioseq_set_Id;

    // Hide copy methods
    CBioseq_set_Info& operator= (const CBioseq_set_Info&);
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CBioseq_set& CBioseq_set_Info::x_GetObject(void)
{
    return *m_Object;
}


inline
const CBioseq_set& CBioseq_set_Info::x_GetObject(void) const
{
    return *m_Object;
}


inline
bool CBioseq_set_Info::IsSetId(void) const
{
    return m_Object->IsSetId();
}


inline
const CBioseq_set_Info::TId& CBioseq_set_Info::GetId(void) const
{
    return m_Object->GetId();
}


inline
bool CBioseq_set_Info::IsSetColl(void) const
{
    return m_Object->IsSetColl();
}


inline
const CBioseq_set_Info::TColl& CBioseq_set_Info::GetColl(void) const
{
    return m_Object->GetColl();
}


inline
bool CBioseq_set_Info::IsSetLevel(void) const
{
    return m_Object->IsSetLevel();
}


inline
CBioseq_set_Info::TLevel CBioseq_set_Info::GetLevel(void) const
{
    return m_Object->GetLevel();
}


inline
bool CBioseq_set_Info::IsSetClass(void) const
{
    return m_Object->IsSetClass();
}


inline
CBioseq_set_Info::TClass CBioseq_set_Info::GetClass(void) const
{
    return m_Object->GetClass();
}


inline
bool CBioseq_set_Info::IsSetRelease(void) const
{
    return m_Object->IsSetRelease();
}


inline
const CBioseq_set_Info::TRelease& CBioseq_set_Info::GetRelease(void) const
{
    return m_Object->GetRelease();
}


inline
bool CBioseq_set_Info::IsSetDate(void) const
{
    return m_Object->IsSetDate();
}


inline
const CBioseq_set_Info::TDate& CBioseq_set_Info::GetDate(void) const
{
    return m_Object->GetDate();
}


inline
bool CBioseq_set_Info::IsSetSeq_set(void) const
{
    return m_Object->IsSetSeq_set();
}


inline
const CBioseq_set_Info::TSeq_set& CBioseq_set_Info::GetSeq_set(void) const
{
    return m_Seq_set;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.1  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.8  2004/02/03 19:02:16  vasilche
* Fixed broken 'dirty annot index' state after RemoveEntry().
*
* Revision 1.7  2003/12/18 16:38:06  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.6  2003/11/28 15:13:25  grichenk
* Added CSeq_entry_Handle
*
* Revision 1.5  2003/09/30 16:22:01  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.4  2003/08/04 17:02:59  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.3  2003/07/25 21:41:29  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.2  2003/07/25 15:25:24  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.1  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
*
* ===========================================================================
*/

#endif//OBJECTS_OBJMGR_IMPL___BIOSEQ_SET_INFO__HPP
