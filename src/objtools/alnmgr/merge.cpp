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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment merger
*
* ===========================================================================
*/

#include <objects/alnmgr/alnmix.hpp>
#include <serial/iterator.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#ifdef HAVE_NCBI_C
#include <ctools/asn_converter.hpp>

//for CObjectOStreamAsnBinary << const ncbi::objects::CSeq_align
#include <serial/serial.hpp> 

#include <ncbi.h>
#include <alignmgr2.h>
#include <accid1.h>
#include <lsqfetch.h>
#endif


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

bool CAlnMix::x_MergeInit()
{
#ifdef HAVE_NCBI_C
    LocalSeqFetchInit(FALSE);
    ID1BioseqFetchEnable("wally", FALSE);
    /* standard setup */
    ErrSetFatalLevel(SEV_MAX);
    ErrClearOptFlags(EO_SHOW_USERSTR);
    UseLocalAsnloadDataAndErrMsg();
    ErrPathReset();
    if ( !AllObjLoad() )
    {
        NCBI_THROW(CException, eUnknown,
                   "CAlnMix::x_MergeInit: AllObjLoad failed");
    }
    if ( !SubmitAsnLoad() )
    {
        NCBI_THROW(CException, eUnknown,
                   "CAlnMix::x_MergeInit: SubmitAsnLoad failed");
    }
    if ( !FeatDefSetLoad() )
    {
        NCBI_THROW(CException, eUnknown,
                   "CAlnMix::x_MergeInit: FeatDefSetLoad failed");
    }
    if ( !SeqCodeSetLoad() )
    {
        NCBI_THROW(CException, eUnknown,
                   "CAlnMix::x_MergeInit: SeqCodeSetLoad failed");
    }
    if ( !GeneticCodeTableLoad() )
    {
        NCBI_THROW(CException, eUnknown,
                   "CAlnMix::x_MergeInit: AllObjLoad failed");
    }
    ObjMgrSetHold(); 
    return true;
#else
    NCBI_THROW(CException, eUnknown,
               "CAlnMix::x_MergeInit: C Toolkit not present");
#endif
}


void CAlnMix::x_Merge()
{
    m_DS = null;

    CSeq_align aln;

    list< CRef< CSeq_align > >& sa_lst = aln.SetSegs().SetDisc().Set();
    iterate (TConstDSs, ds_it, m_InputDSs) {
        CRef<CSeq_align> sa(new CSeq_align());
        sa->SetSegs().SetDenseg(**ds_it);
        sa_lst.push_back(sa);
    }

#ifdef HAVE_NCBI_C
    DECLARE_ASN_CONVERTER(CSeq_align, SeqAlign, converter);
    SeqAlignPtr sap = converter.ToC(aln);

    // merge
    x_MergeInit();
    if (m_MergeFlags & fGen2EST) {
        AlnMgr2IndexAsRows(sap,
                           !(m_MergeFlags & fNegativeStrand),
                           m_MergeFlags & fTruncateOverlaps);
    } else {
        AlnMgr2IndexSeqAlign(sap);
    }

    if (!((AMAlignIndex2Ptr)(sap->saip))->sharedaln) {
        NCBI_THROW(CAlnException, eUnknownMergeFailure,
                   string("CAlnMix::x_Merge(): ") + 
                   (m_MergeFlags & fGen2EST ? "Gen2EST":"Nucl2Nucl") +
                   " merge failed. " 
                   "Try using different merge parameters!");
    }
    
    // convert back to c++
    CRef<CSeq_align> shared(new CSeq_align());
    converter.FromC(((AMAlignIndex2Ptr)(sap->saip))->sharedaln, shared);

    SeqAlignFree(sap);

    CTypeIterator<CDense_seg> ds_it = Begin(*shared);

    m_DS = &*ds_it;
#else
    NCBI_THROW(CException, eUnknown,
               "CAlnMix::x_Merge: C Toolkit not present");
#endif
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2002/11/04 21:29:08  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.6  2002/10/24 21:42:07  todorov
* adding Dense-segs instead of Seq-aligns
*
* Revision 1.5  2002/10/22 21:06:18  ucko
* Conditionalize the code that needs the C Toolkit on HAVE_NCBI_C.
*
* Revision 1.4  2002/10/10 17:16:15  todorov
* .
*
* Revision 1.3  2002/10/08 18:02:34  todorov
* changed the aln lst input param
*
* Revision 1.2  2002/09/27 17:35:08  todorov
* added a merge exception
*
* Revision 1.1  2002/08/23 14:43:53  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
