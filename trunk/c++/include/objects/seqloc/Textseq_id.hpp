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
 */

#ifndef OBJECTS_SEQLOC_TEXTSEQ_ID_HPP
#define OBJECTS_SEQLOC_TEXTSEQ_ID_HPP


// generated includes
#include <objects/seqloc/Textseq_id_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQLOC_EXPORT CTextseq_id : public CTextseq_id_Base
{
    typedef CTextseq_id_Base Tparent;

public:
    // 'ctors
    CTextseq_id(void);
    virtual ~CTextseq_id(void);

    /// Set all fields with a single call.  Fields not specified will
    /// be reset.
    /// @param acc
    ///   New value of accession field (may end with a dot and a
    ///   version unless allow_dot_version is false).
    /// @param name
    ///   New value of name (locus) field.
    /// @param version
    ///   New value of version field.  If acc also specifies a
    ///   non-zero version, the two values must agree.
    /// @param release
    ///   New value of release field.
    /// @param allow_dot_version
    ///   Whether to parse an optional dot-delimited version off the
    ///   end of acc.
    CTextseq_id& Set(const CTempString& acc_in,
                     const CTempString& name_in           = kEmptyStr,
                     int                version           = 0,
                     const CTempString& release_in        = kEmptyStr,
                     bool               allow_dot_version = true);

    /// Comparison functions.
    bool Match(const CTextseq_id& tsip2) const;
    int Compare(const CTextseq_id& tsip2) const;
    bool operator<(const CTextseq_id& tsip2) const;

    /// Format the contents FASTA string style
    ostream& AsFastaString(ostream& s, bool allow_version = true) const;

private:
    // Prohibit copy constructor & assignment operator.
    CTextseq_id(const CTextseq_id&);
    CTextseq_id& operator= (const CTextseq_id&);
};



/////////////////// CTextseq_id inline methods

// constructor
inline
CTextseq_id::CTextseq_id(void)
{
}


inline
bool CTextseq_id::operator<(const CTextseq_id& tsip2) const
{
    return Compare(tsip2) < 0;
}


/////////////////// end of CTextseq_id inline methods

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_SEQLOC_TEXTSEQ_ID_HPP
/* Original file checksum: lines: 85, chars: 2258, CRC32: fc9c35de */
