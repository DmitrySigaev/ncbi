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
*   CTSE_Info_Object
*
*/


#include <objmgr/impl/tse_info_object.hpp>
#include <objmgr/impl/tse_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTSE_Info_Object::CTSE_Info_Object(void)
    : m_TSE_Info(0),
      m_Parent_Info(0),
      m_DirtyAnnotIndex(true)
{
}


CTSE_Info_Object::~CTSE_Info_Object(void)
{
}


bool CTSE_Info_Object::HaveDataSource(void) const
{
    return HaveTSE_Info() && GetTSE_Info().HaveDataSource();
}


const CTSE_Info& CTSE_Info_Object::GetTSE_Info(void) const
{
    _ASSERT(m_TSE_Info);
    return *m_TSE_Info;
}


CTSE_Info& CTSE_Info_Object::GetTSE_Info(void)
{
    _ASSERT(m_TSE_Info);
    return *m_TSE_Info;
}


CDataSource& CTSE_Info_Object::GetDataSource(void) const
{
    return GetTSE_Info().GetDataSource();
}


const CTSE_Info_Object& CTSE_Info_Object::GetBaseParent_Info(void) const
{
    _ASSERT(m_Parent_Info);
    return *m_Parent_Info;
}


CTSE_Info_Object& CTSE_Info_Object::GetBaseParent_Info(void)
{
    _ASSERT(m_Parent_Info);
    return *m_Parent_Info;
}


void CTSE_Info_Object::x_TSEAttach(CTSE_Info& tse)
{
    _ASSERT(!m_TSE_Info);
    _ASSERT(m_Parent_Info || &tse == this);
    x_TSEAttachContents(tse);
    _ASSERT(m_TSE_Info == &tse);
}


void CTSE_Info_Object::x_TSEDetach(CTSE_Info& tse)
{
    _ASSERT(m_TSE_Info == &tse);
    _ASSERT(m_Parent_Info || &tse == this);
    x_TSEDetachContents(tse);
    _ASSERT(!m_TSE_Info);
}


void CTSE_Info_Object::x_TSEAttachContents(CTSE_Info& tse)
{
    _ASSERT(!m_TSE_Info);
    m_TSE_Info = &tse;
}


void CTSE_Info_Object::x_TSEDetachContents(CTSE_Info& _DEBUG_ARG(tse))
{
    _ASSERT(m_TSE_Info == &tse);
    m_TSE_Info = 0;
}


void CTSE_Info_Object::x_DSAttach(CDataSource& ds)
{
    _ASSERT(m_TSE_Info);
    _ASSERT(m_Parent_Info || m_TSE_Info == this);
    _ASSERT(!m_Parent_Info || &ds == &GetDataSource());
    x_DSAttachContents(ds);
}


void CTSE_Info_Object::x_DSDetach(CDataSource& ds)
{
    _ASSERT(m_TSE_Info);
    _ASSERT(m_Parent_Info || m_TSE_Info == this);
    _ASSERT(!m_Parent_Info || &ds == &GetDataSource());
    x_DSDetachContents(ds);
}


void CTSE_Info_Object::x_DSAttachContents(CDataSource& _DEBUG_ARG(ds))
{
    _ASSERT(&ds == &GetDataSource());
}


void CTSE_Info_Object::x_DSDetachContents(CDataSource& _DEBUG_ARG(ds))
{
    _ASSERT(&ds == &GetDataSource());
}


void CTSE_Info_Object::x_BaseParentAttach(CTSE_Info_Object& parent)
{
    _ASSERT(!m_Parent_Info);
    _ASSERT(!m_TSE_Info);
    m_Parent_Info = &parent;
    if ( x_DirtyAnnotIndex() ) {
        x_SetParentDirtyAnnotIndex();
    }
}


void CTSE_Info_Object::x_BaseParentDetach(CTSE_Info_Object& _DEBUG_ARG(parent))
{
    _ASSERT(m_Parent_Info == &parent);
    _ASSERT(!m_TSE_Info);
    m_Parent_Info = 0;
}


void CTSE_Info_Object::x_AttachObject(CTSE_Info_Object& object)
{
    _ASSERT(&object.GetBaseParent_Info() == this);
    if ( HaveTSE_Info() ) {
        object.x_TSEAttach(GetTSE_Info());
    }
    if ( HaveDataSource() ) {
        object.x_DSAttach(GetDataSource());
    }
}


void CTSE_Info_Object::x_DetachObject(CTSE_Info_Object& object)
{
    _ASSERT(&object.GetBaseParent_Info() == this);
    if ( HaveDataSource() ) {
        object.x_DSDetach(GetDataSource());
    }
    if ( HaveTSE_Info() ) {
        object.x_TSEDetach(GetTSE_Info());
    }
}


void CTSE_Info_Object::x_SetDirtyAnnotIndex(void)
{
    if ( !x_DirtyAnnotIndex() ) {
        m_DirtyAnnotIndex = true;
        x_SetParentDirtyAnnotIndex();
    }
}


void CTSE_Info_Object::x_SetParentDirtyAnnotIndex(void)
{
    if ( HaveParent_Info() ) {
        GetBaseParent_Info().x_SetDirtyAnnotIndex();
    }
    else {
        x_SetDirtyAnnotIndexNoParent();
    }
}


void CTSE_Info_Object::x_SetDirtyAnnotIndexNoParent(void)
{
}


void CTSE_Info_Object::x_ResetDirtyAnnotIndex(void)
{
    if ( x_DirtyAnnotIndex() ) {
        m_DirtyAnnotIndex = false;
        if ( !HaveParent_Info() ) {
            x_ResetDirtyAnnotIndexNoParent();
        }
    }
}


void CTSE_Info_Object::x_ResetDirtyAnnotIndexNoParent(void)
{
}


void CTSE_Info_Object::x_UpdateAnnotIndex(CTSE_Info& tse)
{
    if ( x_DirtyAnnotIndex() ) {
        x_UpdateAnnotIndexContents(tse);
        x_ResetDirtyAnnotIndex();
    }
}


void CTSE_Info_Object::x_UpdateAnnotIndexContents(CTSE_Info& /*tse*/)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/03/16 15:47:28  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.10  2004/02/03 19:02:18  vasilche
 * Fixed broken 'dirty annot index' state after RemoveEntry().
 *
 * Revision 1.9  2004/02/02 14:46:44  vasilche
 * Several performance fixed - do not iterate whole tse set in CDataSource.
 *
 * Revision 1.8  2004/01/29 19:33:07  vasilche
 * Fixed coredump on WorkShop when invalid Seq-entry is added to CScope.
 *
 * Revision 1.7  2004/01/22 20:10:40  vasilche
 * 1. Splitted ID2 specs to two parts.
 * ID2 now specifies only protocol.
 * Specification of ID2 split data is moved to seqsplit ASN module.
 * For now they are still reside in one resulting library as before - libid2.
 * As the result split specific headers are now in objects/seqsplit.
 * 2. Moved ID2 and ID1 specific code out of object manager.
 * Protocol is processed by corresponding readers.
 * ID2 split parsing is processed by ncbi_xreader library - used by all readers.
 * 3. Updated OBJMGR_LIBS correspondingly.
 *
 * Revision 1.6  2003/12/18 16:38:07  grichenk
 * Added CScope::RemoveEntry()
 *
 * Revision 1.5  2003/12/11 17:02:50  grichenk
 * Fixed CRef resetting in constructors.
 *
 * Revision 1.4  2003/11/19 22:18:03  grichenk
 * All exceptions are now CException-derived. Catch "exception" rather
 * than "runtime_error".
 *
 * Revision 1.3  2003/09/30 16:22:03  vasilche
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
 * Revision 1.2  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.1  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 *
 * ===========================================================================
 */
