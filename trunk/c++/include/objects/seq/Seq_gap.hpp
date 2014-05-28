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

/// @file Seq_gap.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'seq.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Seq_gap_.hpp


#ifndef OBJECTS_SEQ_SEQ_GAP_HPP
#define OBJECTS_SEQ_SEQ_GAP_HPP


// generated includes
#include <objects/seq/Seq_gap_.hpp>

#include <objects/seq/Linkage_evidence.hpp>

// generated classes


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_SEQ_EXPORT CSeq_gap : public CSeq_gap_Base
{
    typedef CSeq_gap_Base Tparent;
public:
    // constructor
    CSeq_gap(void);
    // destructor
    ~CSeq_gap(void);

    void ChangeType(TType linkage_type);
    void SetLinkageTypeScaffold(CLinkage_evidence::TType evidence_type);
    void SetLinkageTypeLinkedRepeat(CLinkage_evidence::TType evidence_type);
    bool AddLinkageEvidence(CLinkage_evidence::TType evidence_type);


private:
    // Prohibit copy constructor and assignment operator
    CSeq_gap(const CSeq_gap& value);
    CSeq_gap& operator=(const CSeq_gap& value);

};

/////////////////// CSeq_gap inline methods

// constructor
inline
CSeq_gap::CSeq_gap(void)
{
}


/////////////////// end of CSeq_gap inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQ_SEQ_GAP_HPP
/* Original file checksum: lines: 86, chars: 2354, CRC32: 25e00c0f */
