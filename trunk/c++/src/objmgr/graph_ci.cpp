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
*   Object manager iterators
*
*/

#include <objects/objmgr/graph_ci.hpp>
#include "annot_object.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMappedGraph& CMappedGraph::Set(const CAnnotObject& annot)
{
    _ASSERT(annot.IsGraph());
    m_Graph = &annot.GetGraph();
    m_MappedGraph.Reset();
    m_Partial = annot.IsPartial();
    if ( annot.IsMappedLoc() )
        m_MappedLoc = &annot.GetMappedLoc();
    else
        m_MappedLoc.Reset();
    return *this;
}


CGraph_CI::CGraph_CI(CScope& scope,
                     const CSeq_loc& loc,
                     CAnnot_CI::EOverlapType overlap_type,
                     EResolveMethod resolve)
    : CAnnotTypes_CI(scope, loc,
      SAnnotSelector(CSeq_annot::C_Data::e_Graph),
      overlap_type, resolve)
{
    return;
}


CGraph_CI::CGraph_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
                     CAnnot_CI::EOverlapType overlap_type,
                     EResolveMethod resolve,
                     const CSeq_entry* entry)
    : CAnnotTypes_CI(bioseq, start, stop,
          SAnnotSelector(CSeq_annot::C_Data::e_Graph),
          overlap_type, resolve, entry)
{
    return;
}


CGraph_CI::~CGraph_CI(void)
{
    return;
}


const CMappedGraph& CGraph_CI::operator* (void) const
{
    const CAnnotObject* annot = Get();
    _ASSERT(annot);
    return m_Graph.Set(*annot);
}


const CMappedGraph* CGraph_CI::operator-> (void) const
{
    const CAnnotObject* annot = Get();
    _ASSERT(annot);
    m_Graph.Set(*annot);
    return &m_Graph;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2003/02/10 22:04:44  grichenk
* CGraph_CI resolves to CMappedGraph instead of CSeq_graph
*
* Revision 1.13  2003/02/10 15:51:27  grichenk
* + CMappedGraph
*
* Revision 1.12  2002/12/24 15:42:45  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.11  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.10  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.9  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:55  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:19  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
