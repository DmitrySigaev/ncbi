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
 */

/// @file Spliced_seg.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'seqalign.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Spliced_seg_.hpp


#ifndef OBJECTS_SEQALIGN_SPLICED_SEG_HPP
#define OBJECTS_SEQALIGN_SPLICED_SEG_HPP


// generated includes
#include <objects/seqalign/Spliced_seg_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeq_align;

/////////////////////////////////////////////////////////////////////////////
class NCBI_SEQALIGN_EXPORT CSpliced_seg : public CSpliced_seg_Base
{
    typedef CSpliced_seg_Base Tparent;
public:
    // constructor
    CSpliced_seg(void);
    // destructor
    ~CSpliced_seg(void);

    /// types
    typedef int TDim;

    /// Validators
    void Validate    (bool full_test = false) const;
    TDim CheckNumRows(void)                   const {
        return 2;
    }

    /// GetSeqRange
    /// NB: In case the product-type is protein, these only return the
    /// amin part of Prot-pos.  The frame is ignored.
    CRange<TSeqPos> GetSeqRange(TDim row) const;
    TSeqPos         GetSeqStart(TDim row) const;
    TSeqPos         GetSeqStop (TDim row) const;

    /// Get strand (the first one if segments have different strands).
    ENa_strand      GetSeqStrand(TDim row) const;

    /// Convert this alignment to a discontinuous segment
    CRef<CSeq_align> AsDiscSeg() const;

private:
    // Prohibit copy constructor and assignment operator
    CSpliced_seg(const CSpliced_seg& value);
    CSpliced_seg& operator=(const CSpliced_seg& value);

};

/////////////////// CSpliced_seg inline methods

// constructor
inline
CSpliced_seg::CSpliced_seg(void)
{
}


/////////////////// end of CSpliced_seg inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQALIGN_SPLICED_SEG_HPP
/* Original file checksum: lines: 86, chars: 2460, CRC32: 4b1fdf5 */