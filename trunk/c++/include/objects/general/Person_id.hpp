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
 *   'general.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.3  2005/05/20 13:29:48  shomrat
 * Added BasicCleanup()
 *
 * Revision 1.2  2002/12/26 12:40:33  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.1  2002/01/10 19:47:41  clausen
 * Added GetLabel
 *
 *
 * ===========================================================================
 */

#ifndef OBJECTS_GENERAL_PERSON_ID_HPP
#define OBJECTS_GENERAL_PERSON_ID_HPP


// generated includes
#include <objects/general/Person_id_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_GENERAL_EXPORT CPerson_id : public CPerson_id_Base
{
    typedef CPerson_id_Base Tparent;
public:
    // constructor
    CPerson_id(void);
    // destructor
    ~CPerson_id(void);
    
    enum ETypeLabel {
        eGenbank,
        eEmbl};
     
    // Appends a label onto label. If type == eGenbank a comma is used
    // between last name and initials for a structure name and other
    // commas are not removed. If type == eEmbl, all commas are replace 
    // by spaces.   
    void GetLabel(string* label, ETypeLabel type = eGenbank) const;

    // Basic data cleanup
    void BasicCleanup(bool fix_initials);

private:
    // Prohibit copy constructor and assignment operator
    CPerson_id(const CPerson_id& value);
    CPerson_id& operator=(const CPerson_id& value);

};



/////////////////// CPerson_id inline methods

// constructor
inline
CPerson_id::CPerson_id(void)
{
}


/////////////////// end of CPerson_id inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_GENERAL_PERSON_ID_HPP
/* Original file checksum: lines: 90, chars: 2405, CRC32: a107380d */
