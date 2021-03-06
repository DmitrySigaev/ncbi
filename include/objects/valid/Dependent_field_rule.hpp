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

/// @file Dependent_field_rule.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'valid.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Dependent_field_rule_.hpp


#ifndef OBJECTS_VALID_DEPENDENT_FIELD_RULE_HPP
#define OBJECTS_VALID_DEPENDENT_FIELD_RULE_HPP


// generated includes
#include <objects/valid/Dependent_field_rule_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_VALID_EXPORT CDependent_field_rule : public CDependent_field_rule_Base
{
    typedef CDependent_field_rule_Base Tparent;
public:
    // constructor
    CDependent_field_rule(void);
    // destructor
    ~CDependent_field_rule(void);

    bool DoesStringMatchRuleExpression(string str) const;

private:
    // Prohibit copy constructor and assignment operator
    CDependent_field_rule(const CDependent_field_rule& value);
    CDependent_field_rule& operator=(const CDependent_field_rule& value);

};

/////////////////// CDependent_field_rule inline methods

// constructor
inline
CDependent_field_rule::CDependent_field_rule(void)
{
}


/////////////////// end of CDependent_field_rule inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_VALID_DEPENDENT_FIELD_RULE_HPP
/* Original file checksum: lines: 86, chars: 2617, CRC32: ecf25a19 */
