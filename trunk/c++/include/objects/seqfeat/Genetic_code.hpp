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
 *   'seqfeat.asn'.
 */

#ifndef OBJECTS_SEQFEAT_GENETIC_CODE_HPP
#define OBJECTS_SEQFEAT_GENETIC_CODE_HPP


// generated includes
#include <objects/seqfeat/Genetic_code_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQFEAT_EXPORT CGenetic_code : public CGenetic_code_Base
{
    typedef CGenetic_code_Base Tparent;
public:
    // constructor
    CGenetic_code(void);
    // destructor
    ~CGenetic_code(void);

    // Returns the value of the first NAME field in this genetic code,
    // or the empty string if none exist.
    const string& GetName(void) const;

    // Retrieve the genetic-code's numeric ID.
    int GetId(void) const;
    // Set the genetic-code's numeric ID.
    void SetId(int);

    // Try to retrieve the ncbieaa and sncbieaa strings.
    // If they don't exist return empty strings instead.
    const string& GetNcbieaa(void) const;
    const string& GetSncbieaa(void) const;

private:
    // Prohibit copy constructor and assignment operator
    CGenetic_code(const CGenetic_code& value);
    CGenetic_code& operator=(const CGenetic_code& value);

    // cache 
    mutable const string* m_Name;
    mutable int           m_Id;
    mutable const string* m_Ncbieaa;
    mutable const string* m_Sncbieaa;
};



/////////////////// CGenetic_code inline methods

// constructor
inline
CGenetic_code::CGenetic_code(void) :
    m_Name(0), m_Id(255), m_Ncbieaa(0), m_Sncbieaa(0)
{
}


/////////////////// end of CGenetic_code inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_SEQFEAT_GENETIC_CODE_HPP
/* Original file checksum: lines: 93, chars: 2451, CRC32: b121101b */