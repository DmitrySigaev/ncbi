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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_feat_Handle::CSeq_feat_Handle(void)
    : m_AnnotInfoType(eType_null)
{
    return;
}


CSeq_feat_Handle::CSeq_feat_Handle(CScope& scope,
                                   const CSeq_annot_Info& annot_info,
                                   size_t index)
    : m_Scope(scope),
      m_Annot(&annot_info),
      m_AnnotInfoType(eType_Seq_annot_Info),
      m_Index(index)
{
    return;
}


CSeq_feat_Handle::CSeq_feat_Handle(CScope& scope,
                                   const CSeq_annot_SNP_Info& snp_info,
                                   size_t index)
    : m_Scope(scope),
      m_Annot(&snp_info.GetParentSeq_annot_Info()),
      m_AnnotInfoType(eType_Seq_annot_SNP_Info),
      m_Index(index)
{
    return;
}


CSeq_feat_Handle::~CSeq_feat_Handle(void)
{
    return;
}


const SSNP_Info& CSeq_feat_Handle::x_GetSNP_Info(void) const
{
    _ASSERT(m_AnnotInfoType == eType_Seq_annot_SNP_Info);
    return m_Annot->x_GetSNP_annot_Info().GetSNP_Info(m_Index);
}


const CSeq_feat& CSeq_feat_Handle::x_GetSeq_feat(void) const
{
    _ASSERT(m_AnnotInfoType == eType_Seq_annot_Info);
    return m_Annot->GetAnnotObject_Info(m_Index).GetFeat();
}


CConstRef<CSeq_feat> CSeq_feat_Handle::GetSeq_feat(void) const
{
    switch (m_AnnotInfoType) {
    case eType_Seq_annot_Info:
        {
            return ConstRef(&x_GetSeq_feat());
        }
    case eType_Seq_annot_SNP_Info:
        {
            return x_GetSNP_Info().
                CreateSeq_feat(m_Annot->x_GetSNP_annot_Info());
        }
    default:
        {
            return CConstRef<CSeq_feat>(0);
        }
    }
}


CSeq_annot_Handle CSeq_feat_Handle::GetAnnot(void) const
{
    if ( m_Annot ) {
        return CSeq_annot_Handle(m_Scope.GetScope(), *m_Annot);
    }
    return CSeq_annot_Handle();
}


CSeq_feat_Handle::TRange CSeq_feat_Handle::GetRange(void) const
{
    switch (m_AnnotInfoType) {
    case eType_Seq_annot_Info:
        {
            return x_GetSeq_feat().GetLocation().GetTotalRange();
        }
    case eType_Seq_annot_SNP_Info:
        {
            const SSNP_Info& snp_info = x_GetSNP_Info();
            return TRange(snp_info.GetFrom(), snp_info.GetTo());
        }
    default:
        {
            return TRange::GetEmpty();
        }
    }
}


CSeq_id::TGi CSeq_feat_Handle::GetGi(void) const
{
    _ASSERT(m_AnnotInfoType == eType_Seq_annot_SNP_Info);
    return m_Annot->x_GetSNP_annot_Info().GetGi();
}


size_t CSeq_feat_Handle::GetAllelesCount(void) const
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


string CSeq_feat_Handle::GetAllele(size_t index) const
{
    _ASSERT(index < SSNP_Info::kMax_AllelesCount);
    const SSNP_Info& snp = x_GetSNP_Info();
    _ASSERT(snp.m_AllelesIndices[index] != SSNP_Info::kNo_AlleleIndex);
    return m_Annot->x_GetSNP_annot_Info().
        x_GetAllele(snp.m_AllelesIndices[index]);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/05/04 18:06:06  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
