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

/// @file Comment_rule.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'valid.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Comment_rule_.hpp


#ifndef OBJECTS_VALID_COMMENT_RULE_HPP
#define OBJECTS_VALID_COMMENT_RULE_HPP


// generated includes
#include <objects/valid/Comment_rule_.hpp>
#include <objects/valid/Field_rule.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_VALID_EXPORT CComment_rule : public CComment_rule_Base
{
    typedef CComment_rule_Base Tparent;
public:
    // constructor
    CComment_rule(void);
    // destructor
    ~CComment_rule(void);

    const CField_rule& FindFieldRule (const string& field_name) const;

private:
    // Prohibit copy constructor and assignment operator
    CComment_rule(const CComment_rule& value);
    CComment_rule& operator=(const CComment_rule& value);

};

/////////////////// CComment_rule inline methods

// constructor
inline
CComment_rule::CComment_rule(void)
{
}


/////////////////// end of CComment_rule inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_VALID_COMMENT_RULE_HPP
/* Original file checksum: lines: 86, chars: 2465, CRC32: 988ff800 */
