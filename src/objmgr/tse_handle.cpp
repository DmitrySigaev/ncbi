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
*    Handle to top level Seq-entry
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const CTSE_Info& CTSE_Handle::x_GetTSE_Info(void) const
{
    return *GetTSE_Lock();
}


CScope_Impl& CTSE_Handle::x_GetScopeImpl(void) const
{
    return x_GetScopeInfo().GetScopeImpl();
}


CScope& CTSE_Handle::GetScope(void) const
{
    return x_GetScopeImpl().GetScope();
}


CBlobIdKey CTSE_Handle::GetBlobId(void) const
{
    CBlobIdKey ret;
    if ( *this ) {
        const CTSE_Info& tse = x_GetTSE_Info();
        ret = CBlobIdKey(tse.GetDataSource().GetDataLoader(), tse.GetBlobId());
    }
    return ret;
}


CBioseq_Handle CTSE_Handle::GetBioseqHandle(const CSeq_id_Handle& id) const
{
    return x_GetScopeImpl().GetBioseqHandleFromTSE(id, *this);
}


CBioseq_Handle CTSE_Handle::GetBioseqHandle(const CSeq_id& id) const
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
}


bool CTSE_Handle::AddUsedTSE(const CTSE_Handle& tse) const
{
    return x_GetScopeInfo().AddUsedTSE(tse);
}


END_SCOPE(objects)
END_NCBI_SCOPE
