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
*   CSeq_annot_Info info -- entry for data source information about Seq-annot
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Info::CSeq_annot_Info(const CSeq_annot& annot)
{
    x_SetObject(annot);
}


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot_SNP_Info& snp_annot)
{
    x_SetSNP_annot_Info(snp_annot);
}


CSeq_annot_Info::CSeq_annot_Info(const CSeq_annot_Info& info)
{
    x_SetObject(info);
}


CSeq_annot_Info::~CSeq_annot_Info(void)
{
}


const CBioseq_Base_Info& CSeq_annot_Info::GetParentBioseq_Base_Info(void) const
{
    return static_cast<const CBioseq_Base_Info&>(GetBaseParent_Info());
}


CBioseq_Base_Info& CSeq_annot_Info::GetParentBioseq_Base_Info(void)
{
    return static_cast<CBioseq_Base_Info&>(GetBaseParent_Info());
}


const CSeq_entry_Info& CSeq_annot_Info::GetParentSeq_entry_Info(void) const
{
    return GetParentBioseq_Base_Info().GetParentSeq_entry_Info();
}


CSeq_entry_Info& CSeq_annot_Info::GetParentSeq_entry_Info(void)
{
    return GetParentBioseq_Base_Info().GetParentSeq_entry_Info();
}


void CSeq_annot_Info::x_ParentAttach(CBioseq_Base_Info& parent)
{
    x_BaseParentAttach(parent);
}


void CSeq_annot_Info::x_ParentDetach(CBioseq_Base_Info& parent)
{
    x_BaseParentDetach(parent);
}


void CSeq_annot_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    x_DSMapObject(m_Object, ds);
    if ( m_SNP_Info ) {
        m_SNP_Info->x_DSAttach(ds);
    }
}


void CSeq_annot_Info::x_DSDetachContents(CDataSource& ds)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_DSDetach(ds);
    }
    ITERATE ( TDSMappedObjects, it, m_DSMappedObjects ) {
        x_DSUnmapObject(*it, ds);
    }
    m_DSMappedObjects.clear();
    TParent::x_DSDetachContents(ds);
}


void CSeq_annot_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    m_DSMappedObjects.push_back(obj);
    ds.x_Map(obj, this);
}


void CSeq_annot_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
}


void CSeq_annot_Info::x_TSEAttachContents(CTSE_Info& tse)
{
    CRef<CSeq_annot_SNP_Info> snp_info = tse.x_GetSNP_Info(m_Object);
    if ( snp_info ) {
        _ASSERT(!m_SNP_Info);
        m_SNP_Info = snp_info;
        snp_info->x_ParentAttach(*this);
        _ASSERT(&snp_info->GetParentSeq_annot_Info() == this);
        x_AttachObject(*snp_info);
    }
    TParent::x_TSEAttachContents(tse);
    if ( m_SNP_Info ) {
        m_SNP_Info->x_TSEAttach(tse);
    }
}


void CSeq_annot_Info::x_TSEDetachContents(CTSE_Info& tse)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_TSEDetach(tse);
    }
    x_UnmapAnnotObjects(tse);
    TParent::x_TSEDetachContents(tse);
}


size_t
CSeq_annot_Info::GetAnnotObjectIndex(const CAnnotObject_Info& info) const
{
    _ASSERT(&info.GetSeq_annot_Info() == this);
    return m_ObjectInfos.GetIndex(info);
}


const CAnnotName& CSeq_annot_Info::GetName(void) const
{
    return m_Name.IsNamed()? m_Name: GetTSE_Info().GetName();
}


void CSeq_annot_Info::x_UpdateName(void)
{
    if ( m_Object->IsSetDesc() ) {
        string name;
        ITERATE( CSeq_annot::TDesc::Tdata, it, m_Object->GetDesc().Get() ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                name = desc.GetName();
                break;
            }
        }
        m_Name.SetNamed(name);
    }
    else {
        m_Name.SetUnnamed();
    }
}


CConstRef<CSeq_annot> CSeq_annot_Info::GetCompleteSeq_annot(void) const
{
    x_UpdateComplete();
    return GetSeq_annotCore();
}


CConstRef<CSeq_annot> CSeq_annot_Info::GetSeq_annotCore(void) const
{
    x_UpdateCore();
    return m_Object;
}


void CSeq_annot_Info::x_SetObject(const TObject& obj)
{
    _ASSERT(!m_SNP_Info && !m_Object);
    m_Object.Reset(&obj);
    x_UpdateName();
    x_SetDirtyAnnotIndex();
}


void CSeq_annot_Info::x_SetObject(const CSeq_annot_Info& info)
{
    _ASSERT(!m_SNP_Info && !m_Object);
    m_Object = info.m_Object;
    m_Name = info.m_Name;
    if ( info.m_SNP_Info ) {
        m_SNP_Info.Reset(new CSeq_annot_SNP_Info(*info.m_SNP_Info));
        m_SNP_Info->x_ParentAttach(*this);
        x_AttachObject(*m_SNP_Info);
    }
    x_SetDirtyAnnotIndex();
}


void CSeq_annot_Info::x_SetSNP_annot_Info(CSeq_annot_SNP_Info& snp_info)
{
    _ASSERT(!m_SNP_Info && !m_Object && !snp_info.HasParent_Info());
    x_SetObject(snp_info.GetRemainingSeq_annot());
    m_SNP_Info.Reset(&snp_info);
    snp_info.x_ParentAttach(*this);
    _ASSERT(&snp_info.GetParentSeq_annot_Info() == this);
    x_AttachObject(snp_info);
}


void CSeq_annot_Info::x_DoUpdate(TNeedUpdateFlags /*flags*/)
{
    NCBI_THROW(CObjMgrException, eNotImplemented,
               "CSeq_annot_Info::x_DoUpdate: unimplemented");
}


void CSeq_annot_Info::UpdateAnnotIndex(void) const
{
    if ( x_DirtyAnnotIndex() ) {
        GetTSE_Info().UpdateAnnotIndex(*this);
        _ASSERT(!x_DirtyAnnotIndex());
    }
}


void CSeq_annot_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    m_ObjectInfos.SetName(GetName());

    const CSeq_annot::C_Data& data = m_Object->GetData();
    switch ( data.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        x_MapAnnotObjects(tse, data.GetFtable());
        break;
    case CSeq_annot::C_Data::e_Align:
        x_MapAnnotObjects(tse, data.GetAlign());
        break;
    case CSeq_annot::C_Data::e_Graph:
        x_MapAnnotObjects(tse, data.GetGraph());
        break;
    case CSeq_annot::C_Data::e_Locs:
        x_MapAnnotObjects(tse, *m_Object);
        break;
    default:
        break;
    }
    if ( m_SNP_Info ) {
        m_SNP_Info->x_UpdateAnnotIndex(tse);
    }
    TParent::x_UpdateAnnotIndexContents(tse);
}


void
CSeq_annot_Info::x_MapAnnotObjects(CTSE_Info& tse,
                                   const CSeq_annot::C_Data::TFtable& objs)
{
    m_ObjectInfos.Reserve(objs.size(), 1.1);

    CTSE_Info::TAnnotObjs& index = tse.x_SetAnnotObjs(GetName());

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    ITERATE ( CSeq_annot::C_Data::TFtable, fit, objs ) {
        const CSeq_feat& feat = **fit;

        CAnnotObject_Info* info =
            m_ObjectInfos.AddInfo(CAnnotObject_Info(feat, *this));
        key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

        info->GetMaps(hrmaps);
        annotRef.m_AnnotLocationIndex = 0;
        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                key.m_Handle = hrit->first;
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( hr.HasGaps() ) {
                    annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    annotRef.m_HandleRange->GetData() = hr;
                }
                else {
                    annotRef.m_HandleRange.Reset();
                }
                
                tse.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
            }
            ++annotRef.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_MapAnnotObjects(CTSE_Info& tse,
                                        const CSeq_annot::C_Data::TGraph& objs)
{
    m_ObjectInfos.Reserve(objs.size());

    CTSE_Info::TAnnotObjs& index = tse.x_SetAnnotObjs(GetName());

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    ITERATE ( CSeq_annot::C_Data::TGraph, git, objs ) {
        const CSeq_graph& graph = **git;

        CAnnotObject_Info* info =
            m_ObjectInfos.AddInfo(CAnnotObject_Info(graph, *this));
        key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

        info->GetMaps(hrmaps);
        annotRef.m_AnnotLocationIndex = 0;
        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                key.m_Handle = hrit->first;
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( hr.HasGaps() ) {
                    annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    annotRef.m_HandleRange->GetData() = hr;
                }
                else {
                    annotRef.m_HandleRange.Reset();
                }

                tse.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
            }
            ++annotRef.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_MapAnnotObjects(CTSE_Info& tse,
                                        const CSeq_annot::C_Data::TAlign& objs)
{
    m_ObjectInfos.Reserve(objs.size());

    CTSE_Info::TAnnotObjs& index = tse.x_SetAnnotObjs(GetName());

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    ITERATE ( CSeq_annot::C_Data::TAlign, ait, objs ) {
        const CSeq_align& align = **ait;

        CAnnotObject_Info* info =
            m_ObjectInfos.AddInfo(CAnnotObject_Info(align, *this));
        key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

        info->GetMaps(hrmaps);
        annotRef.m_AnnotLocationIndex = 0;
        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                key.m_Handle = hrit->first;
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( hr.HasGaps() ) {
                    annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    annotRef.m_HandleRange->GetData() = hr;
                }
                else {
                    annotRef.m_HandleRange.Reset();
                }

                tse.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
            }
            ++annotRef.m_AnnotLocationIndex;
        }
    }
}


void
CSeq_annot_Info::x_MapAnnotObjects(CTSE_Info& tse,
                                   const CSeq_annot& annot)
{
    m_ObjectInfos.Reserve(1);

    CTSE_Info::TAnnotObjs& index = tse.x_SetAnnotObjs(GetName());

    _ASSERT(annot.GetData().IsLocs());
    // Only one referenced location per annot is allowed
    _ASSERT(annot.GetData().GetLocs().size() == 1);
    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    vector<CHandleRangeMap> hrmaps;
    const CSeq_loc& loc = *annot.GetData().GetLocs().front();

    CAnnotObject_Info* info =
        m_ObjectInfos.AddInfo(CAnnotObject_Info(loc, *this));
    key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;

    info->GetMaps(hrmaps);
    annotRef.m_AnnotLocationIndex = 0;
    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            key.m_Handle = hrit->first;
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( hr.HasGaps() ) {
                annotRef.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                annotRef.m_HandleRange->GetData() = hr;
            }
            else {
                annotRef.m_HandleRange.Reset();
            }
            tse.x_MapAnnotObject(index, key, annotRef, m_ObjectInfos);
        }
    }
}


void CSeq_annot_Info::x_UnmapAnnotObjects(CTSE_Info& tse)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_UnmapAnnotObjects(tse);
    }
    tse.x_UnmapAnnotObjects(m_ObjectInfos);
}


void CSeq_annot_Info::x_DropAnnotObjects(CTSE_Info& tse)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_DropAnnotObjects(tse);
    }
    m_ObjectInfos.Clear();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2004/08/12 14:18:27  vasilche
 * Allow SNP Seq-entry in addition to SNP Seq-annot.
 *
 * Revision 1.19  2004/08/04 14:53:26  vasilche
 * Revamped object manager:
 * 1. Changed TSE locking scheme
 * 2. TSE cache is maintained by CDataSource.
 * 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
 * 4. Fixed processing of split data.
 *
 * Revision 1.18  2004/07/12 16:57:32  vasilche
 * Fixed loading of split Seq-descr and Seq-data objects.
 * They are loaded correctly now when GetCompleteXxx() method is called.
 *
 * Revision 1.17  2004/06/07 17:01:17  grichenk
 * Implemented referencing through locs annotations
 *
 * Revision 1.16  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.15  2004/03/26 19:42:04  vasilche
 * Fixed premature deletion of SNP annot info object.
 * Removed obsolete references to chunk info.
 *
 * Revision 1.14  2004/03/24 18:30:30  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.13  2004/03/16 15:47:28  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.12  2004/01/22 20:10:40  vasilche
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
 * Revision 1.11  2003/11/26 17:56:00  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.10  2003/11/19 22:18:03  grichenk
 * All exceptions are now CException-derived. Catch "exception" rather
 * than "runtime_error".
 *
 * Revision 1.9  2003/10/07 13:43:23  vasilche
 * Added proper handling of named Seq-annots.
 * Added feature search from named Seq-annots.
 * Added configurable adaptive annotation search (default: gene, cds, mrna).
 * Fixed selection of blobs for loading from GenBank.
 * Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
 * Fixed leaked split chunks annotation stubs.
 * Moved some classes definitions in separate *.cpp files.
 *
 * Revision 1.8  2003/09/30 16:22:03  vasilche
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
 * Revision 1.7  2003/08/27 14:29:52  vasilche
 * Reduce object allocations in feature iterator.
 *
 * Revision 1.6  2003/07/18 19:41:46  vasilche
 * Removed unused variable.
 *
 * Revision 1.5  2003/07/17 22:51:31  vasilche
 * Fixed unused variables warnings.
 *
 * Revision 1.4  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.3  2003/07/09 17:54:29  dicuccio
 * Fixed uninitialized variables in CDataSource and CSeq_annot_Info
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
