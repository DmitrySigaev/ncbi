/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'seqalign.asn'.
 */

// standard includes
#include <algorithm>
#include <objects/seqalign/seqalign_exception.hpp>

// generated includes
#include <objects/seqalign/Dense_seg.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CDense_seg::~CDense_seg(void)
{
}


CDense_seg::TNumseg CDense_seg::CheckNumSegs() const
{
    const CDense_seg::TStarts&  starts  = GetStarts();
    const CDense_seg::TStrands& strands = GetStrands();
    const CDense_seg::TLens&    lens    = GetLens();
    const CDense_seg::TWidths&  widths  = GetWidths();

    const size_t& numrows = GetDim();
    const size_t& numsegs = GetNumseg();
    const size_t  num     = numrows * numsegs;

    if (starts.size() != num) {
        string errstr = string("CDense_seg::CheckNumSegs():")
            + " starts.size is inconsistent with dim * numseg";
        NCBI_THROW(CSeqalignException, eInvalidAlignment, errstr);
    }
    if (lens.size() != numsegs) {
        string errstr = string("CDense_seg::CheckNumSegs():")
            + " lens.size is inconsistent with numseg";
        NCBI_THROW(CSeqalignException, eInvalidAlignment, errstr);
    }
    if (strands.size()  &&  strands.size() != num) {
        string errstr = string("CDense_seg::CheckNumSegs():")
            + " strands.size is inconsistent with dim * numseg";
        NCBI_THROW(CSeqalignException, eInvalidAlignment, errstr);
    }
    if (widths.size()  &&  widths.size() != numrows) {
        string errstr = string("CDense_seg::CheckNumSegs():")
            + " widths.size is inconsistent with dim";
        NCBI_THROW(CSeqalignException, eInvalidAlignment, errstr);
    }
    return numsegs;
}


TSeqPos CDense_seg::GetSeqStart(TDim row) const
{
    const TDim&    dim    = GetDim();
    const TNumseg& numseg = CheckNumSegs();
    const TStarts& starts = GetStarts();

    if (row < 0  ||  row >= dim) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CDense_seg::GetSeqStop():"
                   " Invalid row number");
    }

    TSignedSeqPos start;
    if (CanGetStrands()  &&  GetStrands().size()  &&  GetStrands()[row] == eNa_strand_minus) {
        TNumseg seg = numseg;
        int pos = (seg - 1) * dim + row;
        while (seg--) {
            if ((start = starts[pos]) >= 0) {
                return start;
            }
            pos -= dim;
        }
    } else {
        TNumseg seg = -1;
        int pos = row;
        while (++seg < numseg) {
            if ((start = starts[pos]) >= 0) {
                return start;
            }
            pos += dim;
        }
    }
    NCBI_THROW(CSeqalignException, eInvalidAlignment,
               "CDense_seg::GetSeqStart(): Row is empty");
}


TSeqPos CDense_seg::GetSeqStop(TDim row) const
{
    const TDim& dim       = GetDim();
    const TNumseg& numseg = CheckNumSegs();
    const TStarts& starts = GetStarts();

    if (row < 0  ||  row >= dim) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CDense_seg::GetSeqStop():"
                   " Invalid row number");
    }

    TSignedSeqPos start;
    if (CanGetStrands()  &&  GetStrands().size()  &&  GetStrands()[row] == eNa_strand_minus) {
        TNumseg seg = -1;
        int pos = row;
        while (++seg < numseg) {
            if ((start = starts[pos]) >= 0) {
                return start + GetLens()[seg] - 1;
            }
            pos += dim;
        }
    } else {
        TNumseg seg = numseg;
        int pos = (seg - 1) * dim + row;
        while (seg--) {
            if ((start = starts[pos]) >= 0) {
                return start + GetLens()[seg] - 1;
            }
            pos -= dim;
        }        
    }
    NCBI_THROW(CSeqalignException, eInvalidAlignment,
               "CDense_seg::GetSeqStop(): Row is empty");
}


void CDense_seg::Validate(bool full_test) const
{
    const CDense_seg::TStarts&  starts  = GetStarts();
    const CDense_seg::TStrands& strands = GetStrands();
    const CDense_seg::TLens&    lens    = GetLens();
    const CDense_seg::TWidths&  widths  = GetWidths();

    const size_t& numrows = CheckNumRows();
    const size_t& numsegs = CheckNumSegs();

    if (full_test) {
        const size_t  max     = numrows * (numsegs -1);

        bool strands_exist = !strands.empty();

        size_t numseg = 0, numrow = 0, offset = 0;
        for (numrow = 0;  numrow < numrows;  numrow++) {
            TSignedSeqPos min_start = -1, start;
            bool plus = strands_exist ? 
                strands[numrow] != eNa_strand_minus:
                true;
            
            if (plus) {
                offset = 0;
            } else {
                offset = max;
            }
            
            for (numseg = 0;  numseg < numsegs;  numseg++) {
                start = starts[offset + numrow];
                if (start >= 0) {
                    if (start < min_start) {
                        string errstr = string("CDense_seg::Validate():")
                            + " Starts are not consistent!"
                            + " Row=" + NStr::IntToString(numrow) +
                            " Seg=" + NStr::IntToString(plus ? numseg :
                                                        numsegs - 1 - numseg) +
                            " MinStart=" + NStr::IntToString(min_start) +
                            " Start=" + NStr::IntToString(start);
                        
                        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                                   errstr);
                    }
                    min_start = start + 
                        lens[plus ? numseg : numsegs - 1 - numseg] *
                        (widths.size() == numrows ?
                         widths[numrow] : 1);
                }
                if (plus) {
                    offset += numrows;
                } else {
                    offset -= numrows;
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
// PRE : none
// POST: same alignment, opposite orientation
void CDense_seg::Reverse(void)
{
    //flip strands
    NON_CONST_ITERATE (CDense_seg::TStrands, i, SetStrands()) {
        switch (*i) {
        case eNa_strand_plus:  *i = eNa_strand_minus; break;
        case eNa_strand_minus: *i = eNa_strand_plus;  break;
        default:                    break;//do nothing if not + or -
        }
    }

    //reverse list o' lengths
    {
        CDense_seg::TLens::iterator f = SetLens().begin();
        CDense_seg::TLens::iterator r = SetLens().end();
        while (f < r) {
            swap(*(f++), *(--r));
        }
    }

    //reverse list o' starts
    CDense_seg::TStarts &starts = SetStarts();
    int f = 0;
    int r = (GetNumseg() - 1) * GetDim();
    while (f < r) {
        for (int i = 0;  i < GetDim();  ++i) {
            swap(starts[f+i], starts[r+i]);
        }
        f += GetDim();
        r -= GetDim();
    }
}

//-----------------------------------------------------------------------------
// PRE : numbers of the rows to swap
// POST: alignment rearranged with row1 where row2 used to be & vice versa
void CDense_seg::SwapRows(TDim row1, TDim row2)
{
    if (row1 >= GetDim()  ||  row1 < 0  ||
        row2 >= GetDim()  ||  row2 < 0) {
        NCBI_THROW(CSeqalignException, eOutOfRange,
                   "Row numbers supplied to CDense_seg::SwapRows must be "
                   "in the range [0, dim)");
    }

    //swap ids
    swap(SetIds()[row1], SetIds()[row2]);

    int idxStop = GetNumseg()*GetDim();
    
    //swap starts
    for(int i = 0; i < idxStop; i += GetDim()) {
        swap(SetStarts()[i+row1], SetStarts()[i+row2]);
    }

    //swap strands
    for(int i = 0; i < idxStop; i += GetDim()) {
        swap(SetStrands()[i+row1], SetStrands()[i+row2]);
    }
}


void CDense_seg::RemapToLoc(TDim row, const CSeq_loc& loc)
{
    if (loc.IsWhole()) {
        return;
    }

    TSeqPos row_stop  = GetSeqStop(row);

    size_t  ttl_loc_len = 0;
    {{
        CSeq_loc_CI seq_loc_i(loc);
        do {
            ttl_loc_len += seq_loc_i.GetRange().GetLength();
        } while (++seq_loc_i);
    }}

    // check the validity of the seq-loc
    if (ttl_loc_len < row_stop + 1) {
        string errstr = string("CDense_seg::RemapToLoc():")
            + " Seq-loc is not long enough to"
            + " cover the alignment!"
            + " Maximum row seq pos is " + NStr::IntToString(row_stop)
            + " The total seq-loc len is only "
            + NStr::IntToString(ttl_loc_len) +
            + ", it should be at least " + NStr::IntToString(row_stop+1)
            + " (= max seq pos + 1).";
        NCBI_THROW(CSeqalignException, eOutOfRange, errstr);
    }

    const CDense_seg::TStarts&  starts  = GetStarts();
    const CDense_seg::TStrands& strands = GetStrands();
    const CDense_seg::TLens&    lens    = GetLens();

    const size_t& numrows = CheckNumRows();
    const size_t& numsegs = CheckNumSegs();

    CSeq_loc_CI seq_loc_i(loc);

    TSeqPos start, loc_len, len, len_so_far;
    start = seq_loc_i.GetRange().GetFrom();
    len = loc_len = seq_loc_i.GetRange().GetLength();
    len_so_far = 0;
    
    bool row_plus = !strands.size() || strands[row] != eNa_strand_minus;
    bool loc_plus = seq_loc_i.GetStrand() != eNa_strand_minus;

    // iterate through segments
    size_t  idx = loc_plus ? row : (numsegs - 1) * numrows + row;
    TNumseg seg = loc_plus ? 0 : numsegs - 1;
    while (loc_plus ? seg < GetNumseg() : seg >= 0) {
        if (starts[idx] == -1) {
            // ignore gaps in our sequence
            if (loc_plus) {
                idx += numrows; seg++;
            } else {
                idx -= numrows; seg--;
            }
            continue;
        }

        // iterate the seq-loc if needed
        if ((loc_plus == row_plus ?
             starts[idx] : ttl_loc_len - starts[idx] - lens[seg])
            > len_so_far + loc_len) {

            if (++seq_loc_i) {
                len_so_far += len;
                len   = seq_loc_i.GetRange().GetLength();
                start = seq_loc_i.GetRange().GetFrom();
            } else {
                NCBI_THROW(CSeqalignException, eInvalidInputData,
                           "CDense_seg::RemapToLoc():"
                           " Internal error");
            }

            // assert the strand is the same
            if (loc_plus != (seq_loc_i.GetStrand() != eNa_strand_minus)) {
                NCBI_THROW(CSeqalignException, eInvalidInputData,
                           "CDense_seg::RemapToLoc():"
                           " The strand should be the same accross"
                           " the input seq-loc");
            }
        }

        // offset for the starting position
        if (loc_plus == row_plus) {
            SetStarts()[idx] += start - len_so_far;
        } else {
            SetStarts()[idx] = 
                start - len_so_far + ttl_loc_len - starts[idx] - lens[seg];
        }

        if (lens[seg] > len) {
            TSignedSeqPos len_diff = lens[seg] - len;
            while (1) {
                // move to the next loc part that extends beyond our length
                ++seq_loc_i;
                if (seq_loc_i) {
                    start = seq_loc_i.GetRange().GetFrom();
                } else {
                    NCBI_THROW(CSeqalignException, eOutOfRange,
                               "CDense_seg::RemapToLoc():"
                               " Internal error");
                }

                // split our segment
                SetLens().insert(SetLens().begin() + 
                                 (loc_plus ? seg : seg + 1),
                                 len);
                SetLens()[loc_plus ? seg + 1 : seg] = len_diff;

                // insert new data to account for our split segment
                TStarts temp_starts(numrows, -1);
                for (size_t row_i = 0, tmp_idx = seg * numrows;
                     row_i < numrows;  ++row_i, ++tmp_idx) {
                    TSignedSeqPos& this_start = SetStarts()[tmp_idx];
                    if (this_start != -1) {
                        temp_starts[row_i] = this_start;
                        if (loc_plus == (strands[row_i] != eNa_strand_minus)) {
                            if ((size_t) row == row_i) {
                                temp_starts[row_i] = start;
                            } else {
                                temp_starts[row_i] += len;
                            }
                        } else {
                            this_start += len_diff;
                        }
                    }
                }

                len_so_far += loc_len;
                len = loc_len = seq_loc_i.GetRange().GetLength();

                SetStarts().insert(SetStarts().begin() +
                                   (loc_plus ? seg + 1 : seg) * numrows,
                                   temp_starts.begin(), temp_starts.end());
                
                if (strands.size()) {
                    SetStrands().insert
                        (SetStrands().begin(),
                         strands.begin(), strands.begin() + numrows);
                }

                SetNumseg()++;
                
                if ((len_diff = lens[seg] - len) > 0) {
                    if (loc_plus) {
                        idx += numrows; seg++;
                    } else {
                        idx -= numrows; seg--;
                    }
                } else {
                    break;
                }
            }
        } else {
            len -= lens[seg];
        }

        if (loc_plus) {
            idx += numrows; seg++;
        } else {
            idx -= numrows; seg--;
        }
    } // while iterating through segments
    
    // finally, modify the strands if different
    if (loc_plus != row_plus) {
        if (!strands.size()) {
            // strands do not exist, create them
            SetStrands().resize(GetNumseg() * GetDim(), eNa_strand_plus);
        }
        for (seg = 0, idx = row;
             seg < GetNumseg(); seg++, idx += numrows) {
            SetStrands()[idx] = loc_plus ? eNa_strand_plus : eNa_strand_minus;
        }
    }

}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2004/03/09 21:57:03  todorov
* Fixed the out-of-range exception txt
*
* Revision 1.9  2004/02/12 20:54:15  yazhuk
* Fixed GetStartPos() GetStopPos() handling of empty m_Strands
*
* Revision 1.8  2003/12/19 20:15:21  todorov
* RemapToLoc should do nothing in case of a whole Seq-loc
*
* Revision 1.7  2003/11/20 21:26:33  todorov
* RemapToLoc bug fixes: + seg inc; + loc_len vs len
*
* Revision 1.6  2003/11/04 14:44:46  todorov
* +RemapToLoc
*
* Revision 1.5  2003/09/25 17:50:14  dicuccio
* Changed testing of STL container size to use empty() and avoid warning on MSVC
*
* Revision 1.4  2003/09/16 15:31:14  todorov
* Added validation methods. Added seq range methods
*
* Revision 1.3  2003/08/26 21:10:49  ucko
* #include Seq_id.hpp
*
* Revision 1.2  2003/08/26 20:28:38  johnson
* added 'SwapRows' method
*
* Revision 1.1  2003/08/13 18:12:03  johnson
* added 'Reverse' method
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1885, CRC32: 4483973b */
