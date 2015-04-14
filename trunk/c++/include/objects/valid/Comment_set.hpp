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

/// @file Comment_set.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'valid.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Comment_set_.hpp


#ifndef OBJECTS_VALID_COMMENT_SET_HPP
#define OBJECTS_VALID_COMMENT_SET_HPP


// generated includes
#include <objects/valid/Comment_set_.hpp>
#include <objects/general/User_object.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_VALID_EXPORT CComment_set : public CComment_set_Base
{
    typedef CComment_set_Base Tparent;
public:
    // constructor
    CComment_set(void);
    // destructor
    ~CComment_set(void);

    const CComment_rule& FindCommentRule (const string& prefix) const;
    static CConstRef<CComment_set> GetCommentRules();
    static vector<string> GetFieldNames(const string& prefix);
    static list<string> GetKeywords(const CUser_object& user);
    

private:
    // Prohibit copy constructor and assignment operator
    CComment_set(const CComment_set& value);
    CComment_set& operator=(const CComment_set& value);

};

/////////////////// CComment_set inline methods

// constructor
inline
CComment_set::CComment_set(void)
{
}


/////////////////// end of CComment_set inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_VALID_COMMENT_SET_HPP
/* Original file checksum: lines: 86, chars: 2446, CRC32: b867d4e */