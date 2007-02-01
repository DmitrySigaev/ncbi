#ifndef OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
#define OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <gui/gui_export.h>

#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>

#include <objmgr/scope.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_explorer.hpp>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
///  CSparseAln - an alignment based on CSparse_seg and
///  CAlingRangeCollection classes rather than on CDense_seg.
/// Assumptions:
///     master is always specified in CSparse-seg and has a Seq-id
///     we display it always anchored
///     chaning anchor is not supported
class NCBI_XALNMGR_EXPORT CSparseAln : public CObject, public IAlnExplorer
{
public:
    /// Types
    typedef CPairwiseAln::TRng TRng;
    typedef CPairwiseAln::TAlnRng TAlnRng;
    typedef CPairwiseAln::TAlnRngColl TAlnRngColl;
    typedef CAnchoredAln::TDim TDim; ///< Synonym of TNumrow

    /// Constructor
    CSparseAln(const CAnchoredAln& anchored_aln,
               objects::CScope& scope);

    /// Update cache (Call if the dimension of the underlying
    /// CAnchoredAln was changed and/or total ranges were modified)
    void UpdateCache();

    /// Destructor
    virtual ~CSparseAln();


    /// Gap character modifier
    void SetGapChar(TResidue gap_char);


    /// Scope accessor
    CRef<objects::CScope> GetScope() const;


    /// Alignment dimension (number of sequence rows in the alignment)
    TDim GetDim() const;
    /// Synonym of the above
    TNumrow GetNumRows() const {
        return GetDim();
    }


    /// Seq ids
    const objects::CSeq_id&  GetSeqId(TNumrow row) const;


    /// Alignment Range
    TRng GetAlnRange() const;

    const TAlnRngColl&  GetAlignCollection(TNumrow row);

    /// Anchor
    bool    IsSetAnchor() const {
        return true; /// Always true for sparce alignments
    }
    TNumrow GetAnchor() const {
        return m_AnchoredAln->GetAnchorRow();
    }

    /// Sequence range in alignment coords (strand ignored)
    TSignedSeqPos GetSeqAlnStart(TNumrow row) const;
    TSignedSeqPos GetSeqAlnStop(TNumrow row) const;
    TSignedRange  GetSeqAlnRange (TNumrow row) const;

    /// Sequence range in sequence coords
    TSeqPos GetSeqStart(TNumrow row) const;
    TSeqPos GetSeqStop(TNumrow row) const;
    TRange  GetSeqRange(TNumrow row) const;

    /// Strands
    bool IsPositiveStrand(TNumrow row) const;
    bool IsNegativeStrand(TNumrow row) const;

    /// Position mapping functions
    TSignedSeqPos GetAlnPosFromSeqPos(TNumrow row, TSeqPos seq_pos,
                                      ESearchDirection dir = eNone,
                                      bool try_reverse_dir = true) const;
    TSignedSeqPos GetSeqPosFromAlnPos(TNumrow for_row, TSeqPos aln_pos,
                                      ESearchDirection dir = eNone,
                                      bool try_reverse_dir = true) const;


    /// Fetch sequence
    string& GetSeqString   (TNumrow row,                           //< which row
                            string &buffer,                        //< provide an empty buffer for the output
                            TSeqPos seq_from, TSeqPos seq_to,      //< what range
                            bool force_translation = false) const; //< optional na2aa translation (na only!)

    string& GetSeqString   (TNumrow row,                           //< which row
                            string &buffer,                        //< provide an empty buffer for the output
                            const TRange &seq_rng,                 //< what range
                            bool force_translation = false) const; //< optional na2aa translation (na only!)

    string& GetAlnSeqString(TNumrow row,                           //< which row
                            string &buffer,                        //< an empty buffer for the output
                            const TSignedRange &aln_rng,           //< what range (in aln coords)
                            bool force_translation = false) const; //< optional na2aa translation (na only!)


    /// Bioseq handle accessor
    const objects::CBioseq_Handle&  GetBioseqHandle(TNumrow row) const;

    virtual IAlnSegmentIterator*
    CreateSegmentIterator(TNumrow row,
                          const TSignedRange& range,
                          IAlnSegmentIterator::EFlags flags) const;

    /// Wheather the alignment is translated (heterogenous), e.g. nuc-prot
    bool IsTranslated() const;


    // genetic code
    enum EConstants {
        kDefaultGenCode = 1
    };

    // Static utilities:
    static void TranslateNAToAA(const string& na, string& aa,
                                int gen_code = kDefaultGenCode); //< per http://www.ncbi.nlm.nih.gov/collab/FT/#7.5.5
protected:
    CSeqVector& x_GetSeqVector(TNumrow row) const;


    typedef CAnchoredAln::TPairwiseAlnVector TPairwiseAlnVector;


    const CConstRef<CAnchoredAln> m_AnchoredAln;
    const TPairwiseAlnVector& m_PairwiseAlns;
    mutable CRef<objects::CScope> m_Scope;
    TRng m_FirstRange; ///< the extent of all segments in aln coords
    vector<TRng> m_SecondRanges;
    TResidue m_GapChar;
    mutable vector<objects::CBioseq_Handle> m_BioseqHandles;
    mutable vector<CRef<CSeqVector> > m_SeqVectors;

};






END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2007/01/11 21:43:29  todorov
 * Added force_translation flag.
 *
 * Revision 1.6  2007/01/04 21:13:24  todorov
 * Added the ability to display translated alignments.
 *
 * Revision 1.5  2006/12/12 20:53:54  todorov
 * Seq-ids are now in the CPairwiseAln's.
 *
 * Revision 1.4  2006/12/06 21:29:55  todorov
 * Using CConstRef instead of const & for m_AnchoredAln.
 *
 * Revision 1.3  2006/12/01 17:53:54  todorov
 * + NCBI_XALNMGR_EXPORT
 *
 * Revision 1.2  2006/11/16 18:09:40  todorov
 * Anchor row is obtained from the underlying anchored aln.
 * Doxygenized comments.
 *
 * Revision 1.1  2006/11/16 13:45:08  todorov
 * Moved over from gui/widgets/aln_data and refactored to adapt to the
 * new aln framework.
 *
 * Revision 1.6  2006/09/05 12:35:26  dicuccio
 * White space changes: tabs -> spaces; trim trailing spaces
 *
 * Revision 1.5  2006/07/03 14:33:59  yazhuk
 * Redesigned according to the new approach
 *
 * Revision 1.4  2005/10/25 13:20:46  dicuccio
 * White space clean-up: trimmed trailing spaces; tabs -> spaces
 *
 * Revision 1.3  2005/09/19 12:21:04  dicuccio
 * White space changes: trim trailing white space; use spaces not tabs
 *
 * Revision 1.2  2005/06/27 14:37:33  yazhuk
 * TAlignRangeColl renamed to TAlignColl
 *
 * Revision 1.1  2005/06/13 19:03:56  yazhuk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // OBJTOOLS_ALNMGR___SPARSE_ALN__HPP
