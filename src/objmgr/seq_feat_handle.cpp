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
*   Seq-feat handle
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/annot_collector.hpp>

#include <objmgr/impl/seq_annot_edit_commands.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/////////////////////////////////////////////////////////////////////////////
// CSeq_feat_Handle


CSeq_feat_Handle::CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                                   TIndex index)
    : m_Annot(annot),
      m_AnnotIndex(index)
{
    _ASSERT(IsPlainFeat());
    _ASSERT(!IsRemoved());
    _ASSERT(x_GetAnnotObject_Info().IsFeat());
}


CSeq_feat_Handle::CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                                   const SSNP_Info& snp_info,
                                   CCreatedFeat_Ref& created_ref)
    : m_Annot(annot),
      m_AnnotIndex(-1),
      m_CreatedFeat(&created_ref)
{
    _ASSERT(annot.x_GetInfo().x_HasSNP_annot_Info());
    m_AnnotIndex = -1 - x_GetSNP_annot_Info().GetIndex(snp_info);
    _ASSERT(IsTableSNP());
}


void CSeq_feat_Handle::Reset(void)
{
    m_CreatedFeat.Reset();
    m_AnnotIndex = eNull;
    m_Annot.Reset();
}


const CSeq_annot_SNP_Info& CSeq_feat_Handle::x_GetSNP_annot_Info(void) const
{
    return x_GetSeq_annot_Info().x_GetSNP_annot_Info();
}


const CAnnotObject_Info& CSeq_feat_Handle::x_GetAnnotObject_InfoAny(void) const
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::x_GetAnnotObject: not Seq-feat info");
    }
    return x_GetSeq_annot_Info().GetInfo(x_GetAnnotIndex());
}


const CAnnotObject_Info& CSeq_feat_Handle::x_GetAnnotObject_Info(void) const
{
    const CAnnotObject_Info& info = x_GetAnnotObject_InfoAny();
    if ( info.IsRemoved() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::x_GetAnnotObject_Info: "
                   "Seq-feat was removed");
    }
    return info;
}


const SSNP_Info& CSeq_feat_Handle::x_GetSNP_InfoAny(void) const
{
    if ( !IsTableSNP() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::GetSNP_Info: not SNP info");
    }
    return x_GetSNP_annot_Info().GetInfo(-1 - x_GetAnnotIndex());
}


const SSNP_Info& CSeq_feat_Handle::x_GetSNP_Info(void) const
{
    const SSNP_Info& info = x_GetSNP_InfoAny();
    if ( info.IsRemoved() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::GetSNP_Info: SNP was removed");
    }
    return info;
}


const CSeq_feat& CSeq_feat_Handle::x_GetPlainSeq_feat(void) const
{
    return x_GetAnnotObject_Info().GetFeat();
}


CConstRef<CSeq_feat> CSeq_feat_Handle::GetSeq_feat(void) const
{
    if ( IsPlainFeat() ) {
        return ConstRef(&x_GetPlainSeq_feat());
    }
    else {
        return m_CreatedFeat->MakeOriginalFeature(*this);
    }
}


CSeq_feat_Handle::TRange CSeq_feat_Handle::GetRange(void) const
{
    if ( IsPlainFeat() ) {
        return x_GetPlainSeq_feat().GetLocation().GetTotalRange();
    }
    else {
        const SSNP_Info& info = x_GetSNP_Info();
        return TRange(info.GetFrom(), info.GetTo());
    }
}


CSeq_id::TGi CSeq_feat_Handle::GetSNPGi(void) const
{
    return x_GetSNP_annot_Info().GetGi();
}


const string& CSeq_feat_Handle::GetSNPComment(void) const
{
    return x_GetSNP_annot_Info().x_GetComment(x_GetSNP_Info().m_CommentIndex);
}


size_t CSeq_feat_Handle::GetSNPAllelesCount(void) const
{
    const SSNP_Info& snp = x_GetSNP_Info();
    size_t count = 0;
    for (; count < SSNP_Info::kMax_AllelesCount; ++count) {
        if (snp.m_AllelesIndices[count] == SSNP_Info::kNo_AlleleIndex) {
            break;
        }
    }
    return count;
}


const string& CSeq_feat_Handle::GetSNPAllele(size_t index) const
{
    _ASSERT(index < SSNP_Info::kMax_AllelesCount);
    const SSNP_Info& snp = x_GetSNP_Info();
    _ASSERT(snp.m_AllelesIndices[index] != SSNP_Info::kNo_AlleleIndex);
    return x_GetSNP_annot_Info().x_GetAllele(snp.m_AllelesIndices[index]);
}


bool CSeq_feat_Handle::IsRemoved(void) const
{
    if ( IsPlainFeat() ) {
        return x_GetAnnotObject_InfoAny().IsRemoved();
    }
    else {
        return x_GetSNP_InfoAny().IsRemoved();
    }
}


void CSeq_feat_Handle::Remove(void) const
{
    CSeq_feat_EditHandle(*this).Remove();
}


void CSeq_feat_Handle::Replace(const CSeq_feat& new_feat) const
{
    CSeq_feat_EditHandle(*this).Replace(new_feat);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_feat_EditHandle


CSeq_feat_EditHandle::CSeq_feat_EditHandle(const CSeq_feat_Handle& h)
    : CSeq_feat_Handle(h)
{
    GetAnnot(); // force check of editing mode
}


CSeq_feat_EditHandle::CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot,
                                           TIndex index)
    : CSeq_feat_Handle(annot, index)
{
}


CSeq_feat_EditHandle::CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot,
                                           const SSNP_Info& snp_info,
                                           CCreatedFeat_Ref& created_ref)
    : CSeq_feat_Handle(annot, snp_info, created_ref)
{
}


void CSeq_feat_EditHandle::Remove(void) const
{
    typedef CSeq_annot_Remove_EditCommand<CSeq_feat_EditHandle> TCommand;
    CCommandProcessor processor(GetAnnot().x_GetScopeImpl());
    processor.run(new TCommand(*this));
}


void CSeq_feat_EditHandle::Replace(const CSeq_feat& new_feat) const
{
    typedef CSeq_annot_Replace_EditCommand<CSeq_feat_EditHandle> TCommand;
    CCommandProcessor processor(GetAnnot().x_GetScopeImpl());
    processor.run(new TCommand(*this, new_feat));
}

void CSeq_feat_EditHandle::Update(void) const
{
    GetAnnot().x_GetInfo().Update(x_GetAnnotIndex());
}

void CSeq_feat_EditHandle::x_RealRemove(void) const
{
    if ( IsPlainFeat() ) {
        GetAnnot().x_GetInfo().Remove(x_GetAnnotIndex());
        _ASSERT(IsRemoved());
    }
    else {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CSeq_feat_Handle::Remove: handle is SNP table");
    }
}


void CSeq_feat_EditHandle::x_RealReplace(const CSeq_feat& new_feat) const
{
    if ( IsPlainFeat() ) {
        GetAnnot().x_GetInfo().
            Replace(x_GetAnnotIndex(), new_feat);
        _ASSERT(!IsRemoved());
    }
    else {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CSeq_feat_Handle::Replace: handle is SNP table");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_ftable_CI

CSeq_annot_ftable_CI::CSeq_annot_ftable_CI(const CSeq_annot_Handle& annot,
                                           TFlags flags)
    : m_Flags(flags)
{
    if ( !annot.IsFtable() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_annot_ftable_CI: annot is not ftable");
    }
    m_Feat.m_Annot = annot;
    if ( m_Flags & fOnlyTable ) {
        // only table features requested
        if ( GetAnnot().x_GetInfo().x_HasSNP_annot_Info() ) {
            // start with table features
            m_Feat.m_AnnotIndex = -1;
        }
        else {
            // no requested features
            x_Reset();
            return;
        }
    }
    else {
        // start with plain features
        m_Feat.m_AnnotIndex = 0;
    }
    // find by flags
    x_Settle();
}


bool CSeq_annot_ftable_CI::x_IsValid(void) const
{
    if ( !GetAnnot() ) {
        // null iterator
        return false;
    }
    CSeq_feat_Handle::TIndex index = m_Feat.m_AnnotIndex;
    if ( index == m_Feat.eNull ) {
        // end of features
        return false;
    }
    if ( index >= 0 ) {
        // scanning plain features
        CSeq_feat_Handle::TIndex count =
            GetAnnot().x_GetInfo().GetAnnotObjectInfos().size();
        return index < count || GetAnnot().x_GetInfo().x_HasSNP_annot_Info();
    }
    else {
        // scanning table features
        CSeq_feat_Handle::TIndex count =
            GetAnnot().x_GetInfo().x_GetSNP_annot_Info().size();
        index = -1 - index;
        return index < count;
    }
}


void CSeq_annot_ftable_CI::x_Step(void)
{
    _ASSERT(m_Feat.m_AnnotIndex != m_Feat.eNull);
    if ( m_Feat.IsPlainFeat() ) {
        // scanning plain features
        CSeq_feat_Handle::TIndex count =
            GetAnnot().x_GetInfo().GetAnnotObjectInfos().size();
        if ( ++m_Feat.m_AnnotIndex == count ) {
            // no more plain features
            if ( (m_Flags & fIncludeTable) &&
                 GetAnnot().x_GetInfo().x_HasSNP_annot_Info() ) {
                // switch to table
                m_Feat.m_AnnotIndex = -1;
            }
        }
    }
    else {
        --m_Feat.m_AnnotIndex;
    }
}


void CSeq_annot_ftable_CI::x_Reset(void)
{
    // mark end of features
    m_Feat.m_AnnotIndex = m_Feat.eNull;
}


void CSeq_annot_ftable_CI::x_Settle(void)
{
    while ( x_IsValid() ) {
        if ( m_Feat.IsRemoved() ) {
            x_Step();
        }
        else {
            return;
        }
    }
    x_Reset();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_ftable_I

CSeq_annot_ftable_I::CSeq_annot_ftable_I(const CSeq_annot_EditHandle& annot,
                                           TFlags flags)
    : m_Annot(annot), m_Flags(flags)
{
    if ( !annot.IsFtable() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_annot_ftable_I: annot is not ftable");
    }
    m_Feat.m_Annot = annot;
    if ( m_Flags & fOnlyTable ) {
        // only table features requested
        if ( GetAnnot().x_GetInfo().x_HasSNP_annot_Info() ) {
            // start with table features
            m_Feat.m_AnnotIndex = -1;
        }
        else {
            // no requested features
            x_Reset();
            return;
        }
    }
    else {
        // start with plain features
        m_Feat.m_AnnotIndex = 0;
    }
    // find by flags
    x_Settle();
}


bool CSeq_annot_ftable_I::x_IsValid(void) const
{
    if ( !GetAnnot() ) {
        // null iterator
        return false;
    }
    CSeq_feat_Handle::TIndex index = m_Feat.m_AnnotIndex;
    if ( index == m_Feat.eNull ) {
        // end of features
        return false;
    }
    if ( index >= 0 ) {
        // scanning plain features
        CSeq_feat_Handle::TIndex count =
            GetAnnot().x_GetInfo().GetAnnotObjectInfos().size();
        return index < count || GetAnnot().x_GetInfo().x_HasSNP_annot_Info();
    }
    else {
        // scanning table features
        CSeq_feat_Handle::TIndex count =
            GetAnnot().x_GetInfo().x_GetSNP_annot_Info().size();
        index = -1 - index;
        return index < count;
    }
}


void CSeq_annot_ftable_I::x_Step(void)
{
    _ASSERT(m_Feat.m_AnnotIndex != m_Feat.eNull);
    if ( m_Feat.IsPlainFeat() ) {
        // scanning plain features
        CSeq_feat_Handle::TIndex count =
            GetAnnot().x_GetInfo().GetAnnotObjectInfos().size();
        if ( ++m_Feat.m_AnnotIndex == count ) {
            // no more plain features
            if ( (m_Flags & fIncludeTable) &&
                 GetAnnot().x_GetInfo().x_HasSNP_annot_Info() ) {
                // switch to table
                m_Feat.m_AnnotIndex = -1;
            }
        }
    }
    else {
        --m_Feat.m_AnnotIndex;
    }
}


void CSeq_annot_ftable_I::x_Reset(void)
{
    // mark end of features
    m_Feat.m_AnnotIndex = m_Feat.eNull;
}


void CSeq_annot_ftable_I::x_Settle(void)
{
    while ( x_IsValid() ) {
        if ( m_Feat.IsRemoved() ) {
            x_Step();
        }
        else {
            return;
        }
    }
    x_Reset();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.17  2006/09/20 14:00:21  vasilche
 * Implemented user API to Update() annotation index.
 *
 * Revision 1.16  2006/08/07 15:25:06  vasilche
 * Introduced CSeq_feat_EditHandle.
 * Introduced CSeq_annot_ftable_CI & CSeq_annot_ftable_I.
 *
 * Revision 1.15  2006/07/12 16:17:31  vasilche
 * Added CSeq_annot_ftable_CI.
 *
 * Revision 1.14  2005/11/15 19:22:08  didenko
 * Added transactions and edit commands support
 *
 * Revision 1.13  2005/10/04 15:54:44  vasilche
 * Workaround icc 9 optimizer bug.
 *
 * Revision 1.12  2005/09/20 15:45:36  vasilche
 * Feature editing API.
 * Annotation handles remember annotations by index.
 *
 * Revision 1.11  2005/08/23 17:03:01  vasilche
 * Use CAnnotObject_Info pointer instead of annotation index in annot handles.
 *
 * Revision 1.10  2005/04/07 16:30:42  vasilche
 * Inlined handles' constructors and destructors.
 * Optimized handles' assignment operators.
 *
 * Revision 1.9  2005/03/15 19:10:29  vasilche
 * SSNP_Info structure is defined in separate header.
 *
 * Revision 1.8  2005/03/07 17:29:04  vasilche
 * Added "SNP" to names of SNP access methods
 *
 * Revision 1.7  2005/02/24 19:13:34  grichenk
 * Redesigned CMappedFeat not to hold the whole annot collector.
 *
 * Revision 1.6  2004/12/28 18:40:30  vasilche
 * Added GetScope() method.
 *
 * Revision 1.5  2004/12/22 15:56:12  vasilche
 * Used CSeq_annot_Handle in annotations' handles.
 *
 * Revision 1.4  2004/11/04 19:21:18  grichenk
 * Marked non-handle versions of SetLimitXXXX as deprecated
 *
 * Revision 1.3  2004/08/04 14:53:26  vasilche
 * Revamped object manager:
 * 1. Changed TSE locking scheme
 * 2. TSE cache is maintained by CDataSource.
 * 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
 * 4. Fixed processing of split data.
 *
 * Revision 1.2  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/05/04 18:06:06  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
