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
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2000/11/30 21:54:05  ostell
 * added Match()
 *
 * Revision 1.1  2000/11/30 18:40:23  ostell
 * added Textseq_id.Match
 *
 *
 * ===========================================================================
 */

#ifndef OBJECTS_SEQLOC_PATENT_SEQ_ID_HPP
#define OBJECTS_SEQLOC_PATENT_SEQ_ID_HPP


// generated includes
#include <objects/seqloc/Patent_seq_id_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CPatent_seq_id : public CPatent_seq_id_Base
{
    typedef CPatent_seq_id_Base Tparent;
public:
    // constructor
    CPatent_seq_id(void);
    // destructor
    ~CPatent_seq_id(void);

    // comparison function
    bool Match(const CPatent_seq_id& psip2) const;

};



/////////////////// CPatent_seq_id inline methods

// constructor
inline
CPatent_seq_id::CPatent_seq_id(void)
{
}


/////////////////// end of CPatent_seq_id inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQLOC_PATENT_SEQ_ID_HPP
/* Original file checksum: lines: 85, chars: 2297, CRC32: 12ed4808 */
