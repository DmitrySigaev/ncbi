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
 * Author:  Aaron Ucko, NCBI
 *
 * File Description:
 *   ID1 network client
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'id1.asn'.
 */

#ifndef OBJECTS_ID1_ID1_CLIENT_HPP
#define OBJECTS_ID1_ID1_CLIENT_HPP


// generated includes
#include <objects/id1/id1_client_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_ID1_EXPORT CID1Client : public CID1Client_Base
{
    typedef CID1Client_Base Tparent;
public:
    // constructor
    CID1Client(void);
    // destructor
    ~CID1Client(void);

    // overloaded to give explanations of error codes
    void Ask(const TRequest& request, TReply& reply,
             TReplyChoice::E_Choice wanted);
    // ensure that the base class's other Ask remains available
    // (not all compilers handle the appropriate "using" directive)
    void Ask(const TRequest& request, TReply& reply)
        { CID1Client_Base::Ask(request, reply); }

    void SetAllowDeadEntries(bool ok) { m_AllowDeadEntries = ok;   }
    bool GetAllowDeadEntries(void)    { return m_AllowDeadEntries; }

    virtual CRef<CSeq_entry> AskGetsefromgi(const CID1server_maxcomplex& req,
                                            TReply* reply = 0);
    // Simplified interface for retrieving entries by gi or sequence id
    CRef<CSeq_entry> FetchEntry(int gi, int max_complexity = 0);
    CRef<CSeq_entry> FetchEntry(const CSeq_id& id, int max_complexity = 0);
    CRef<CSeq_entry> FetchEntry(const string& id_string,
                                int max_complexity = 0);

private:
    bool m_AllowDeadEntries;

    // Prohibit copy constructor and assignment operator
    CID1Client(const CID1Client& value);
    CID1Client& operator=(const CID1Client& value);

};



/////////////////// CID1Client inline methods

// constructor
inline
CID1Client::CID1Client(void)
    : m_AllowDeadEntries(false)
{
}


/////////////////// end of CID1Client inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/07/01 15:45:50  ucko
* Overload Ask to add descriptive text to known error codes, using
* information from Michael Kimelman.
*
* Revision 1.2  2004/05/20 18:23:21  jcherry
* Added a simplified interface for retrieving entries
*
* Revision 1.1  2003/04/09 16:00:13  ucko
* Give all RPC clients unique basenames.
*
* Revision 1.2  2003/03/21 16:00:03  shomrat
* Added export specifier
*
* Revision 1.1  2002/11/13 20:13:42  ucko
* Add datatool-generated client classes
*
*
* ===========================================================================
*/

#endif // OBJECTS_ID1_ID1_CLIENT_HPP
/* Original file checksum: lines: 93, chars: 2368, CRC32: 45b6687e */
