#ifndef SEQ_ANNOT_HANDLE__HPP
#define SEQ_ANNOT_HANDLE__HPP

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
*    Handle to Seq-annot object
*
*/

#include <corelib/ncbiobj.hpp>

#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/tse_lock.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */

class CSeq_annot;

class CScope;

class CSeq_annot_CI;
class CAnnotTypes_CI;
class CAnnot_CI;
class CSeq_annot_Handle;
class CSeq_annot_EditHandle;
class CSeq_entry_Handle;
class CSeq_entry_EditHandle;

class CSeq_annot_Info;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_annot_Handle --
///
///  Proxy to access seq-annot objects
///

class NCBI_XOBJMGR_EXPORT CSeq_annot_Handle
{
public:
    CSeq_annot_Handle(void);

    /// Check if handle points to a seq-annot
    ///
    /// @sa
    ///    operator !()
    operator bool(void) const;

    // Check if handle does not point to a seq-annot
    ///
    /// @sa
    ///    operator bool()
    bool operator!(void) const;

    CSeq_annot_Handle& operator=(const CSeq_annot_Handle& sah);

    // Reset handle and make it not to point to any seq-annot
    void Reset(void);

    /// Check if handles point to the same seq-annot
    ///
    /// @sa
    ///     operator!=()
    bool operator==(const CSeq_annot_Handle& annot) const;

    // Check if handles point to different seq-annot
    ///
    /// @sa
    ///     operator==()
    bool operator!=(const CSeq_annot_Handle& annot) const;

    /// For usage in containers
    bool operator<(const CSeq_annot_Handle& annot) const;

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get const reference to the current seq-annot
    const CSeq_annot& GetSeq_annot(void) const;

    /// Complete and return const reference to the current seq-annot
    CConstRef<CSeq_annot> GetCompleteSeq_annot(void) const;

     /// Get parent Seq-entry handle
    ///
    /// @sa 
    ///     GetSeq_entry_Handle()
    CSeq_entry_Handle GetParentEntry(void) const;

    /// Get top level Seq-entry handle
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    /// Get 'edit' version of handle
    CSeq_annot_EditHandle GetEditHandle(void) const;

    // Seq-annot accessors
    bool IsNamed(void) const;
    const string& GetName(void) const;

protected:
    friend class CScope_Impl;
    friend class CSeq_annot_CI;

    CSeq_annot_Handle(CScope& scope,
                      const CSeq_annot_Info& annot,
                      const TTSE_Lock& tse_lock);

    void x_Set(CScope& scope, const CSeq_annot_Info& annot);

    CHeapScope          m_Scope;
    TTSE_Lock           m_TSE_Lock;
    CConstRef<CObject>  m_Info;

public: // non-public section
    const TTSE_Lock& GetTSE_Lock(void) const;
    const CSeq_annot_Info& x_GetInfo(void) const;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_annot_EditHandle --
///
///  Proxy to access and edit seq-annot objects
///

class NCBI_XOBJMGR_EXPORT CSeq_annot_EditHandle : public CSeq_annot_Handle
{
public:
    CSeq_annot_EditHandle(void);

    /// Navigate object tree
    CSeq_entry_EditHandle GetParentEntry(void) const;

    /// Eemove current annot
    void Remove(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CBioseq_set_EditHandle;
    friend class CSeq_entry_EditHandle;

    CSeq_annot_EditHandle(const CSeq_annot_Handle& h);
    CSeq_annot_EditHandle(CScope& scope,
                          CSeq_annot_Info& info,
                          const TTSE_Lock& tse_lock);

public: // non-public section
    CSeq_annot_Info& x_GetInfo(void) const;
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_annot_Handle::CSeq_annot_Handle(void)
{
}


inline
CSeq_annot_Handle::operator bool(void) const
{
    return m_Info;
}


inline
bool CSeq_annot_Handle::operator!(void) const
{
    return !m_Info;
}


inline
CScope& CSeq_annot_Handle::GetScope(void) const
{
    return *m_Scope;
}


inline
const TTSE_Lock& CSeq_annot_Handle::GetTSE_Lock(void) const
{
    return m_TSE_Lock;
}


inline
const CSeq_annot_Info& CSeq_annot_Handle::x_GetInfo(void) const
{
    return reinterpret_cast<const CSeq_annot_Info&>(*m_Info);
}


inline
bool CSeq_annot_Handle::operator==(const CSeq_annot_Handle& handle) const
{
    return m_Scope == handle.m_Scope  &&  m_Info == handle.m_Info;
}


inline
bool CSeq_annot_Handle::operator!=(const CSeq_annot_Handle& handle) const
{
    return m_Scope != handle.m_Scope  ||  m_Info != handle.m_Info;
}


inline
bool CSeq_annot_Handle::operator<(const CSeq_annot_Handle& handle) const
{
    if ( m_Scope != handle.m_Scope ) {
        return m_Scope < handle.m_Scope;
    }
    return m_Info < handle.m_Info;
}


inline
CSeq_annot_EditHandle::CSeq_annot_EditHandle(void)
{
}


inline
CSeq_annot_EditHandle::CSeq_annot_EditHandle(const CSeq_annot_Handle& h)
    : CSeq_annot_Handle(h)
{
}


inline
CSeq_annot_EditHandle::CSeq_annot_EditHandle(CScope& scope,
                                             CSeq_annot_Info& info,
                                             const TTSE_Lock& tse_lock)
    : CSeq_annot_Handle(scope, info, tse_lock)
{
}

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2004/09/29 16:52:34  kononenk
* Added doxygen formatting
*
* Revision 1.10  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.9  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.8  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.7  2004/03/31 19:54:07  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.6  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.5  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.4  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.3  2003/10/08 14:14:54  vasilche
* Use CHeapScope instead of CRef<CScope> internally.
*
* Revision 1.2  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.1  2003/09/30 16:21:59  vasilche
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
* ===========================================================================
*/

#endif//SEQ_ANNOT_HANDLE__HPP
