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
*   Annotation type indexes
*
*/

#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/annot_object_index.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// All ranges are in format [x, y)

const size_t kAnnotTypeMax = CSeq_annot::C_Data::e_MaxChoice - 1;
const size_t kFeatTypeMax = CSeqFeatData::e_MaxChoice - 1;
const size_t kFeatSubtypeMax = CSeqFeatData::eSubtype_max;

CAnnotType_Index::TIndexRangeTable CAnnotType_Index::sm_AnnotTypeIndexRange;

CAnnotType_Index::TIndexRangeTable CAnnotType_Index::sm_FeatTypeIndexRange;

CAnnotType_Index::TIndexTable CAnnotType_Index::sm_FeatSubtypeIndex;

bool CAnnotType_Index::sm_TablesInitialized = false;


void CAnnotType_Index::x_InitIndexTables(void)
{
    // Check flag, lock tables
    _ASSERT(!sm_TablesInitialized);
    sm_AnnotTypeIndexRange.resize(kAnnotTypeMax + 1);
    sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_not_set].first = 0;
    sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_Align] = TIndexRange(0,1);
    sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_Graph] = TIndexRange(1,2);
    sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_Ftable].first = 2;

    vector< vector<size_t> > type_subtypes(kFeatTypeMax+1);
    for ( size_t subtype = 0; subtype <= kFeatSubtypeMax; ++subtype ) {
        size_t type = CSeqFeatData::
            GetTypeFromSubtype(CSeqFeatData::ESubtype(subtype));
        if ( type != CSeqFeatData::e_not_set ||
             subtype == CSeqFeatData::eSubtype_bad ) {
            type_subtypes[type].push_back(subtype);
        }
    }

    sm_FeatTypeIndexRange.resize(kFeatTypeMax + 1);
    sm_FeatSubtypeIndex.resize(kFeatSubtypeMax + 1);

    size_t cur_idx =
        sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_Ftable].first;
    for ( size_t type = 0; type <= kFeatTypeMax; ++type ) {
        sm_FeatTypeIndexRange[type].first = cur_idx;
        if ( type != CSeqFeatData::e_not_set ) {
            sm_FeatTypeIndexRange[type].second =
                cur_idx + type_subtypes[type].size();
        }
        ITERATE ( vector<size_t>, it, type_subtypes[type] ) {
            sm_FeatSubtypeIndex[*it] = cur_idx++;
        }
    }

    sm_FeatTypeIndexRange[CSeqFeatData::e_not_set].second = cur_idx;
    sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_Ftable].second = cur_idx;
    sm_AnnotTypeIndexRange[CSeq_annot::C_Data::e_not_set].second = cur_idx;
    
    sm_TablesInitialized = true;
}


size_t CAnnotType_Index::GetTypeIndex(const CAnnotObject_Info& info)
{
    Initialize();
    if ( info.GetFeatSubtype() != CSeqFeatData::eSubtype_any ) {
        size_t index = GetSubtypeIndex(info.GetFeatSubtype());
        if ( index ) {
            return index;
        }
    }
    else if ( info.GetFeatType() != CSeqFeatData::e_not_set ) {
        const TIndexRange r = GetFeatTypeRange(info.GetFeatType());
        if ( r.second == r.first + 1 ) {
            return r.first;
        }
    }
    else {
        const TIndexRange r = GetAnnotTypeRange(info.GetAnnotType());
        if ( r.second == r.first + 1 ) {
            return r.first;
        }
    }
    NCBI_THROW(CObjMgrException, eOtherError,
               "CAnnotObject_Info is incompatible with CAnnotType_Index indexes");
}

size_t CAnnotType_Index::GetTypeIndex(const SAnnotObject_Key& key)
{
    Initialize();
    return GetTypeIndex(*key.m_AnnotObject_Info);
}


CAnnotType_Index::TIndexRange
CAnnotType_Index::GetIndexRange(const SAnnotTypeSelector& sel)
{
    Initialize();
    TIndexRange r;
    if ( sel.GetFeatSubtype() != CSeqFeatData::eSubtype_any ) {
        r.first = GetSubtypeIndex(sel.GetFeatSubtype());
        r.second = r.first? r.first + 1: 0;
    }
    else if ( sel.GetFeatType() != CSeqFeatData::e_not_set ) {
        r = GetFeatTypeRange(sel.GetFeatType());
    }
    else {
        r = GetAnnotTypeRange(sel.GetAnnotType());
    }
    return r;
}


CAnnotType_Index::TIndexRange
CAnnotType_Index::GetIndexRange(const SAnnotTypeSelector& sel,
                                const SIdAnnotObjs& objs)
{
    TIndexRange range;
    range = GetIndexRange(sel);
    range.second = min(range.second, objs.m_AnnotSet.size());
    return range;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/02/04 18:03:21  grichenk
* Initial revision
*
*
* ===========================================================================
*/
