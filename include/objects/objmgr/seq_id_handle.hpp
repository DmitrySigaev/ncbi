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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-id handle for Object Manager
*
*/


#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//
//    Handle to be used instead of CSeq_id. Supports different
//    methods of comparison: exact equality or match of seq-ids.
//


typedef size_t TSeq_id_Key;

// forward declaration
class CSeq_id_Mapper;
class CSeq_id;

class NCBI_XOBJMGR_EXPORT CSeq_id_Handle
{
public:
    // 'ctors
    CSeq_id_Handle(void); // Create an empty handle
    CSeq_id_Handle(const CSeq_id_Handle& handle);
    ~CSeq_id_Handle(void);

    CSeq_id_Handle& operator= (const CSeq_id_Handle& handle);
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

private:
    // This constructor should be used by mappers only
    CSeq_id_Handle(CSeq_id_Mapper& mapper,
                   TSeq_id_Key key);

    // Comparison methods
    // True if handles are strictly equal
    bool x_Equal(const CSeq_id_Handle& handle) const;
    // True if "this" may be resolved to "handle"
    bool x_Match(const CSeq_id_Handle& handle) const;

    const CSeq_id* x_GetSeqId(void) const;

    // Seq-id mapper (to lock/unlock the handle)
    CSeq_id_Mapper* m_Mapper;
    // Handle value
    TSeq_id_Key m_Value;
    //### Reference to the seq-id used by a group of equal handles
    //### CRef<CSeq_id> m_SeqId;

    friend class CSeq_id_Mapper;
    friend class CSeq_id_Which_Tree;
    friend class CBioseq_Handle;
    friend class CDataLoader;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeq_id_Handle::CSeq_id_Handle(void)
    : m_Mapper(0), m_Value(0) //### , m_SeqId(0)
{
}

inline
bool CSeq_id_Handle::operator== (const CSeq_id_Handle& handle) const
{
    if (m_Mapper != handle.m_Mapper) {
        THROW1_TRACE(runtime_error,
            "CSeq_id_Handle::operator==() -- "
            "Can not compare seq-id handles from different mappers");
    }
    return m_Value == handle.m_Value;
}

inline
bool CSeq_id_Handle::operator< (const CSeq_id_Handle& handle) const
{
    if (m_Mapper != handle.m_Mapper) {
        THROW1_TRACE(runtime_error,
            "CSeq_id_Handle::operator<() -- "
            "Can not compare seq-id handles from different mappers");
    }
    return m_Value < handle.m_Value;
}

inline
CSeq_id_Handle::operator bool (void) const
{
    return m_Mapper != 0; //### !m_SeqId.Empty();
}

inline
bool CSeq_id_Handle::operator! (void) const
{
    return m_Mapper == 0; //### m_SeqId.Empty();
}


END_SCOPE(objects)
END_NCBI_SCOPE


// We explicitly include seq_id_mapper.hpp at the bottom of this file
// This is done because CSeq_id_Mapper has a circular #include dependency
// with seq_if_handle.hpp; we skirt the circular reference by placing the
// #include at the bottom
#include <objects/objmgr/seq_id_mapper.hpp>


/*
* ---------------------------------------------------------------------------
* $Log$
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
