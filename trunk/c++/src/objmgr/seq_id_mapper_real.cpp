
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
*       Map Seq-id to a simple (integer) handle
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:06:23  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/data_source.hpp>
#include "seq_id_mapper_real.hpp"
#include "seq_id_mapper.hpp"

#include <corelib/ncbithr.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// There is no standard seq-id to string convertion. Create one here.
inline string SeqIdToString(const CSeq_id& id);
inline
string SeqIdToString(const CSeq_id& id)
{
    ostrstream out;
    out << id.DumpAsFasta();
    return CNcbiOstrstreamToString(out);
}

////////////////////////////////////////////////////////////////////

CSeqIdMapper_Real CSeqIdMapper::sm_Mapper;

CSeqIdMapper_Real::~CSeqIdMapper_Real()
{
}

CSeqIdMapper_Real::TKey CSeqIdMapper_Real::SeqIdToHandle(const CSeq_id& id)
{
    string sid = SeqIdToString(id);
    CFastMutexGuard guard(m_Mutex);
    TIdMap::iterator it = m_IdMap.find(sid);
    if (it != m_IdMap.end())
        return it->second;
    m_IdMap.insert(TIdMap::value_type(sid, m_NextHandle));
    return m_NextHandle++;
}

CSeq_id* CSeqIdMapper_Real::HandleToSeqId(TKey key) const
{
    string sid;
    CFastMutexGuard guard(m_Mutex);
    iterate ( TIdMap, it, m_IdMap ) {
        if ( key == it->second ) {
            sid = it->first;
            break;
        }
    }
    CSeq_id* id = new CSeq_id(sid);
    return id;
}

END_SCOPE(objects)
END_NCBI_SCOPE
