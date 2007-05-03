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
 *   using the following specifications:
 *   'seqalign.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqalign/Dense_diag.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CDense_diag::~CDense_diag(void)
{
}


void CDense_diag::Validate() const
{
    const size_t& numrows = CheckNumRows();

    const CDense_diag::TStrands& strands = GetStrands();

    if ( !strands.empty()  &&  
         strands.size() != numrows) {
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "strands.size inconsistent with dim");
    }
}


void CDense_diag::OffsetRow(TDim row,
                            TSignedSeqPos offset)
{
    if (offset == 0) return;

    CheckNumRows();
    if (row >= GetDim()) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "row > dim");
    }
    _ASSERT(GetStarts()[row] >= 0);
    if (offset < 0) {
        _ASSERT((TSignedSeqPos)GetStarts()[row] + offset >= 0);
        if ((TSignedSeqPos)GetStarts()[row] < -offset) {
            NCBI_THROW(CSeqalignException, eOutOfRange,
                       "Negative offset greater than seq position");
        }
    }
    SetStarts()[row] += offset;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1896, CRC32: 7bba37e0 */
