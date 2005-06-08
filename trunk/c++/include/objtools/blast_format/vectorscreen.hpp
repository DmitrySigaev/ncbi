#ifndef VECTORSCREEN_HPP
#define VECTORSCREEN_HPP
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
 * Author:  Jian Ye
 *
 * @file vectorscreen.hpp
 *   vector screen display (using HTML table) 
 *
 */

#include <html/html.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/scope.hpp>
#include <util/range.hpp>

//forward declarations
class CVecscreenTest;  //For internal test only


/** @addtogroup blast_format
 *
 * @{
 */

BEGIN_NCBI_SCOPE 
BEGIN_SCOPE(objects)


/**
 * Example:
 * @code
 * CVecscreen vec(align_set, master_length);
 * vec.SetImagePath("images/");
 * CRef<CSeq_align_set> temp_aln = vec.VecscreenDisplay(cout);
 * @endcode
 */

class CVecscreen{
public:
  
    ///vector match defines
    enum MatchType{
        eStrong = 0,
        eModerate,
        eWeak,
        eSuspect,
        eNoMatch
    };

    ///Constructors
    ///@param seqalign: alignment to show
    ///@param master_length: master seq length
    ///
    CVecscreen(const CSeq_align_set& seqalign, TSeqPos master_length);
    
    ///Destructor
    ~CVecscreen();
    
    ///Set path to pre-made image gif files with different colors
    ///@param path: the path.  i.e. "mypath/". Internal default "./"
    ///
    void SetImagePath(string path) { 
        m_ImagePath = path;
    }  
    
    ///show alignment graphic view and return the processed seqalign
    ///@param out: stream for display
    ///@return: the processed seqalign ref
    ///
    CRef<CSeq_align_set> VecscreenDisplay(CNcbiOstream& out);
    
private:
    
    ///Internal match info
    struct AlnInfo {
        CRange<TSeqPos> range;
        MatchType type;
    };
    
    ///the current seqalign
    CConstRef<CSeq_align_set> m_SeqalignSetRef;
    ///the processed seqalign
    CRef<CSeq_align_set> m_FinalSeqalign;
    ///gif image file path
    string m_ImagePath;
    ///master seq length
    TSeqPos m_MasterLen;
    ///internal match list
    list<AlnInfo*> m_AlnInfoList;

    ///Sort on range from
    ///@param info1: the first range
    ///@param info2: the second range
    ///
    inline static bool FromRangeAscendingSort(AlnInfo* const& info1,
                                              AlnInfo* const& info2)
    {
        return info1->range.GetFrom() < info2->range.GetFrom();
    } 

    ///merge overlapping seqalign
    ///@param seqalign: the seqalign to merge
    ///
    void x_MergeSeqalign(CSeq_align_set& seqalign);
   
    ///merge a seqalign if its range is in another seqalign
    ///@param seqalign: the seqalign to merge
    ///
    void x_MergeInclusiveSeqalign(CSeq_align_set& seqalign);

    ///merge a seqalign if its range is in another higher ranked seqalign
    ///@param seqalign_higher: higher-ranked seqalign
    ///@param seqalign_lower: lower-ranked seqalign
    ///
    void x_MergeLowerRankSeqalign(CSeq_align_set& seqalign_higher,
                                  CSeq_align_set& seqalign_lower);

    ///Get match type
    ///@param seqalign: the seqalign
    ///@param master_len: the master seq length
    ///@return: the type
    ///
    MatchType x_GetMatchType(const CSeq_align& seqalign,
                             TSeqPos master_len);

    ///Build non overlapping internal match list
    ///@param seqalign_vec: a vecter of catagorized seqalign set
    ///
    void x_BuildNonOverlappingRange(vector<CRef<CSeq_align_set> > seqalign_vec);

    ///get align info
    ///@param from: align from
    ///@param to: align to
    ///@param type: the match type
    ///
    AlnInfo* x_GetAlnInfo(TSeqPos from, TSeqPos to, MatchType type);

    ///Output the graphic
    ///@param out: the stream for output
    ///
    void x_BuildHtmlBar(CNcbiOstream& out);
    
    ///For internal test 
    friend class ::CVecscreenTest;
 
};



END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif


/* 
*============================================================
*$Log$
*Revision 1.2  2005/06/08 16:12:57  jianye
*move AlnFromRangeAscendingSort to .cpp
*
*Revision 1.1  2005/05/25 16:17:59  jianye
*initial checkin
*

*===========================================================
*/
