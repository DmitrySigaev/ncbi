#ifndef SEQ_ALIGN_HANDLE__HPP
#define SEQ_ALIGN_HANDLE__HPP

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
*   Seq-align handle
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CScope;
class CSeq_annot_Handle;

class CSeq_align_Handle
{
public:
    CSeq_align_Handle(void);
    CSeq_align_Handle(CScope& scope,
                      const CSeq_annot_Info& annot_info,
                      size_t index);
    ~CSeq_align_Handle(void);

    CSeq_annot_Handle GetAnnot(void) const;

    CConstRef<CSeq_align> GetSeq_align(void) const;

    // Mappings for CSeq_align methods
    CSeq_align::EType GetType(void) const;
    bool IsSetDim(void) const;
    CSeq_align::TDim GetDim(void) const;
    bool IsSetScore(void) const;
    const CSeq_align::TScore& GetScore(void) const;
    const CSeq_align::TSegs& GetSegs(void) const;
    bool IsSetBounds(void) const;
    const CSeq_align::TBounds& GetBounds(void) const;

private:
    const CSeq_align& x_GetSeq_align(void) const;

    CHeapScope                 m_Scope;
    CConstRef<CSeq_annot_Info> m_Annot;
    size_t                     m_Index;
};


inline
CSeq_align::EType CSeq_align_Handle::GetType(void) const
{
    return x_GetSeq_align().GetType();
}


inline
bool CSeq_align_Handle::IsSetDim(void) const
{
    return x_GetSeq_align().IsSetDim();
}


inline
CSeq_align::TDim CSeq_align_Handle::GetDim(void) const
{
    return x_GetSeq_align().GetDim();
}


inline
bool CSeq_align_Handle::IsSetScore(void) const
{
    return x_GetSeq_align().IsSetScore();
}


inline
const CSeq_align::TScore& CSeq_align_Handle::GetScore(void) const
{
    return x_GetSeq_align().GetScore();
}


inline
const CSeq_align::TSegs& CSeq_align_Handle::GetSegs(void) const
{
    return x_GetSeq_align().GetSegs();
}


inline
bool CSeq_align_Handle::IsSetBounds(void) const
{
    return x_GetSeq_align().IsSetBounds();
}


inline
const CSeq_align::TBounds& CSeq_align_Handle::GetBounds(void) const
{
    return x_GetSeq_align().GetBounds();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/05/04 18:06:06  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_ALIGN_HANDLE__HPP
