#ifndef OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP
#define OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id handle for Object Manager
*
*/


#include <objects/seqloc/Seq_id.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//
//    Handle to be used instead of CSeq_id. Supports different
//    methods of comparison: exact equality or match of seq-ids.
//

// forward declaration
class CSeq_id;
class CSeq_id_Handle;
class CSeq_id_Mapper;
class CSeq_id_Which_Tree;


class NCBI_XOBJMGR_EXPORT CSeq_id_Info
{
public:
    const CSeq_id& GetSeq_id(void) const
        {
            return *m_Seq_id;
        }

protected:
    explicit CSeq_id_Info(const CConstRef<CSeq_id>& seq_id);
    ~CSeq_id_Info(void);

    friend class CSeq_id_Handle;     // for counter
    friend class CSeq_id_Mapper;     // for creation/deletion
    friend class CSeq_id_Which_Tree; // for creation/deletion

    CAtomicCounter     m_Counter;
    CConstRef<CSeq_id> m_Seq_id;

private:
    CSeq_id_Info(const CSeq_id_Info&);
    const CSeq_id_Info& operator=(const CSeq_id_Info&);
};


class NCBI_XOBJMGR_EXPORT CSeq_id_Handle
{
public:
    // 'ctors
    CSeq_id_Handle(CSeq_id_Info* info = 0);
    CSeq_id_Handle(const CSeq_id_Handle& handle);
    ~CSeq_id_Handle(void);

    const CSeq_id_Handle& operator= (const CSeq_id_Handle& handle);
    bool operator== (const CSeq_id_Handle& handle) const;
    bool operator<  (const CSeq_id_Handle& handle) const;

    // Check if the handle is a valid or an empty one
    operator bool(void) const;
    bool operator! (void) const;

    // Reset the handle (remove seq-id reference)
    void Reset(void);

    // True if "this" is a better bioseq than "h".
    bool IsBetter(const CSeq_id_Handle& h) const;

    string AsString() const;

    const CSeq_id& GetSeqId(void) const;
    const CSeq_id* GetSeqIdOrNull(void) const;
    const CSeq_id* x_GetSeqId(void) const;

private:

    void x_AddReference(void);
    void x_AddReferenceIfSet(void);
    void x_RemoveLastReference(void);
    void x_RemoveReference(void);
    void x_RemoveReferenceIfSet(void);

    // Comparison methods
    // True if handles are strictly equal
    bool x_Equal(const CSeq_id_Handle& handle) const;
    // True if "this" may be resolved to "handle"
    bool x_Match(const CSeq_id_Handle& handle) const;

    // Seq-id info
    CSeq_id_Info* m_Info;

    friend class CSeq_id_Mapper;
    friend class CSeq_id_Which_Tree;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Info
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_id_Info::CSeq_id_Info(const CConstRef<CSeq_id>& seq_id)
    : m_Seq_id(seq_id)
{
    m_Counter.Set(0);
}


inline
CSeq_id_Info::~CSeq_id_Info(void)
{
    _ASSERT(m_Counter.Get() == 0);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Handle
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_id_Handle::operator bool (void) const
{
    return m_Info != 0;
}


inline
bool CSeq_id_Handle::operator! (void) const
{
    return m_Info == 0;
}


inline
void CSeq_id_Handle::x_AddReference(void)
{
    m_Info->m_Counter.Add(1);
}


inline
void CSeq_id_Handle::x_RemoveReference(void)
{
    if ( m_Info->m_Counter.Add(-1) == 0 ) {
        x_RemoveLastReference();
    }
}


inline
void CSeq_id_Handle::x_AddReferenceIfSet(void)
{
    if ( m_Info ) {
        x_AddReference();
    }
}


inline
void CSeq_id_Handle::x_RemoveReferenceIfSet(void)
{
    if ( m_Info ) {
        x_RemoveReference();
    }
}


inline
CSeq_id_Handle::CSeq_id_Handle(CSeq_id_Info* info)
    : m_Info(info)
{
    x_AddReferenceIfSet();
}


inline
CSeq_id_Handle::CSeq_id_Handle(const CSeq_id_Handle& h)
    : m_Info(h.m_Info)
{
    x_AddReferenceIfSet();
}


inline
CSeq_id_Handle::~CSeq_id_Handle(void)
{
    x_RemoveReferenceIfSet();
}


inline
const CSeq_id_Handle& CSeq_id_Handle::operator=(const CSeq_id_Handle& h)
{
    if ( m_Info != h.m_Info ) {
        x_RemoveReferenceIfSet();
        m_Info = h.m_Info;
        x_AddReferenceIfSet();
    }
    return *this;
}


inline
void CSeq_id_Handle::Reset(void)
{
    if ( m_Info ) {
        x_RemoveReference();
        m_Info = 0;
    }
}


inline
bool CSeq_id_Handle::operator== (const CSeq_id_Handle& handle) const
{
    return m_Info == handle.m_Info;
}


inline
bool CSeq_id_Handle::operator< (const CSeq_id_Handle& handle) const
{
    return m_Info < handle.m_Info;
}


inline
const CSeq_id& CSeq_id_Handle::GetSeqId(void) const
{
    _ASSERT(m_Info);
    return *m_Info->m_Seq_id;
}


inline
const CSeq_id* CSeq_id_Handle::GetSeqIdOrNull(void) const
{
    return !m_Info? 0: m_Info->m_Seq_id.GetPointer();
}


inline
const CSeq_id* CSeq_id_Handle::x_GetSeqId(void) const
{
    return GetSeqIdOrNull();
}


END_SCOPE(objects)
END_NCBI_SCOPE


// We explicitly include seq_id_mapper.hpp at the bottom of this file
// This is done because CSeq_id_Mapper has a circular #include dependency
// with seq_if_handle.hpp; we skirt the circular reference by placing the
// #include at the bottom
//#include <objects/objmgr/seq_id_mapper.hpp>


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2003/06/10 19:06:34  vasilche
* Simplified CSeq_id_Mapper and CSeq_id_Handle.
*
* Revision 1.13  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.12  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.11  2003/02/21 14:33:50  grichenk
* Display warning but don't crash on uninitialized seq-ids.
*
* Revision 1.10  2002/12/26 20:44:02  dicuccio
* Added Win32 export specifier.  Added #include for seq_id_mapper - at bottom of
* file to skirt circular include dependency.
*
* Revision 1.9  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/29 21:19:57  gouriano
* added debug dump
*
* Revision 1.7  2002/05/02 20:42:35  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.6  2002/03/18 17:26:32  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.5  2002/03/15 18:10:05  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.4  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/12 19:41:40  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.2  2002/01/29 17:06:12  grichenk
* + operator !()
*
* Revision 1.1  2002/01/23 21:56:35  grichenk
* Splitted id_handles.hpp
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP */
