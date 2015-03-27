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

/// @file Source_qual_choice.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'macro.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Source_qual_choice_.hpp


#ifndef OBJECTS_MACRO_SOURCE_QUAL_CHOICE_HPP
#define OBJECTS_MACRO_SOURCE_QUAL_CHOICE_HPP


// generated includes
#include <objects/macro/Source_qual_choice_.hpp>
#include <objects/macro/Source_qual_.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/seqfeat/BioSource.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class CSource_qual_choice : public CSource_qual_choice_Base
{
    typedef CSource_qual_choice_Base Tparent;
public:
    // constructor
    CSource_qual_choice(void);
    // destructor
    ~CSource_qual_choice(void);

    string GetLimitedSourceQualFromBioSource(const CBioSource& biosrc, 
                                    const CString_constraint& str_cons) const;
    bool IsSubsrcQual(ESource_qual src_qual) const;

private:
    // Prohibit copy constructor and assignment operator
    CSource_qual_choice(const CSource_qual_choice& value);
    CSource_qual_choice& operator=(const CSource_qual_choice& value);
};

/////////////////// CSource_qual_choice inline methods

// constructor
inline
CSource_qual_choice::CSource_qual_choice(void)
{
}


/////////////////// end of CSource_qual_choice inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_MACRO_SOURCE_QUAL_CHOICE_HPP
/* Original file checksum: lines: 86, chars: 2557, CRC32: 6e393d06 */