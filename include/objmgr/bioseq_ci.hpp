#ifndef BIOSEQ_CI__HPP
#define BIOSEQ_CI__HPP

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
*   Iterates over bioseqs from a given seq-entry and scope
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/scope.hpp>

#include <objects/seq/Seq_inst.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CBioseq_Handle;
class CSeq_entry;


class NCBI_XOBJMGR_EXPORT CBioseq_CI : public CBioseq_CI_Base
{
public:
    // 'ctors
    CBioseq_CI(void);
    // Iterate over bioseqs from the entry taken from the scope. Use optional
    // filter to iterate over selected bioseq types only.
    CBioseq_CI(CScope& scope, const CSeq_entry& entry,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);
    CBioseq_CI(const CBioseq_CI& bioseq_ci);
    ~CBioseq_CI(void);

    CScope& GetScope(void) const;

    CBioseq_CI& operator= (const CBioseq_CI& bioseq_ci);
    CBioseq_CI& operator++ (void);
    operator bool (void) const;

    const CBioseq_Handle& operator* (void) const;
    const CBioseq_Handle* operator-> (void) const;

private:
    typedef CScope_Impl::TBioseq_HandleSet    TBioseq_HandleSet;
    typedef TBioseq_HandleSet::const_iterator THandleIterator;

    CHeapScope           m_Scope;
    TBioseq_HandleSet    m_Handles;
    THandleIterator      m_Current;
};


inline
CBioseq_CI& CBioseq_CI::operator++ (void)
{
    if ( m_Current != m_Handles.end() ) {
        m_Current++;
    }
    return *this;
}


inline
CBioseq_CI::operator bool (void) const
{
    return m_Current != m_Handles.end();
}


inline
const CBioseq_Handle& CBioseq_CI::operator* (void) const
{
    return *m_Current;
}


inline
const CBioseq_Handle* CBioseq_CI::operator-> (void) const
{
    return &(*m_Current);
}


inline
CScope& CBioseq_CI::GetScope(void) const
{
    return m_Scope;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2004/02/19 17:15:02  vasilche
* Removed unused include.
*
* Revision 1.13  2003/10/01 19:24:36  vasilche
* Added export specifier to CBioseq_CI as it's not completely inlined anymore.
*
* Revision 1.12  2003/09/30 16:21:59  vasilche
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
* Revision 1.11  2003/09/16 14:38:13  dicuccio
* Removed export specifier - the entire class is inlined, and the export
* specifier confuses MSVC in such cases.  Added #include for scope.hpp, which
* contains the base class (previously undefined without this).
*
* Revision 1.10  2003/09/03 19:59:59  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.9  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.8  2003/03/10 17:51:36  kuznets
* iterate->ITERATE
*
* Revision 1.7  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.6  2003/02/04 16:01:48  dicuccio
* Removed export specification so that MSVC won't try to export an inlined class
*
* Revision 1.5  2002/12/26 20:42:55  dicuccio
* Added Win32 export specifier.  Removed unimplemented (private) operator++(int)
*
* Revision 1.4  2002/12/05 19:28:29  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.3  2002/10/15 13:37:28  grichenk
* Fixed inline declarations
*
* Revision 1.2  2002/10/02 17:58:21  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.1  2002/09/30 20:00:48  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // BIOSEQ_CI__HPP
