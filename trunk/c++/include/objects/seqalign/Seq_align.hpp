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

#ifndef OBJECTS_SEQALIGN_SEQ_ALIGN_HPP
#define OBJECTS_SEQALIGN_SEQ_ALIGN_HPP


// generated includes
#include <objects/seqalign/Seq_align_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQALIGN_EXPORT CSeq_align : public CSeq_align_Base
{
    typedef CSeq_align_Base Tparent;
public:
    // constructor
    CSeq_align(void);
    // destructor
    ~CSeq_align(void);

    /// Reverse the segments' orientation
    /// NOTE: currently *only* works for dense-seg
    void Reverse(void);

    // Create a Dense-seg from a Std-seg
    // Used by AlnMgr to handle nucl2prot alignments
    CRef<CSeq_align> CreateDensegFromStdseg(void) const;

private:
    // Prohibit copy constructor and assignment operator
    CSeq_align(const CSeq_align& value);
    CSeq_align& operator=(const CSeq_align& value);

};



/////////////////// CSeq_align inline methods

// constructor
inline
CSeq_align::CSeq_align(void)
{
}


/////////////////// end of CSeq_align inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/08/19 21:10:39  todorov
* +CreateDensegFromStdseg
*
* Revision 1.1  2003/08/13 18:11:35  johnson
* added 'Reverse' method
*
*
* ===========================================================================
*/

#endif // OBJECTS_SEQALIGN_SEQ_ALIGN_HPP
/* Original file checksum: lines: 93, chars: 2426, CRC32: 6ba198f0 */
