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
 *   'seq.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.5  2001/07/25 19:11:07  grichenk
 * Equals() and Assign() re-declared as protected
 *
 * Revision 1.4  2001/07/16 16:22:42  grichenk
 * Added CSerialUserOp class to create Assign() and Equals() methods for
 * user-defind classes.
 * Added SerialAssign<>() and SerialEquals<>() functions.
 *
 * Revision 1.3  2001/06/25 18:51:59  grichenk
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.2  2001/06/21 19:47:34  grichenk
 * Copy constructor and operator=() moved to "private" section
 *
 * Revision 1.1  2001/06/13 15:00:06  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#ifndef OBJECTS_SEQ_BIOSEQ_HPP
#define OBJECTS_SEQ_BIOSEQ_HPP


// generated includes
#include <objects/seq/Bioseq_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeq_entry;

class CBioseq : public CBioseq_Base, public CSerialUserOp
{
    typedef CBioseq_Base Tparent;
public:
    // constructor
    CBioseq(void);
    // destructor
    ~CBioseq(void);
    // Manage Seq-entry tree structure
    CSeq_entry* GetParentEntry(void) const;

protected:
    // From CSerialUserOp
    virtual void Assign(const CSerialUserOp& source);
    virtual bool Equals(const CSerialUserOp& object) const;

private:
    // Prohibit copy constructor and assignment operator
    CBioseq(const CBioseq& value);
    CBioseq& operator= (const CBioseq& value);

    // Seq-entry containing the Bioseq
    void SetParentEntry(CSeq_entry* entry);
    CSeq_entry* m_ParentEntry;
    friend class CSeq_entry;
};



/////////////////// CBioseq inline methods

// constructor
inline
CBioseq::CBioseq(void)
    : m_ParentEntry(0)
{
}

inline
void CBioseq::SetParentEntry(CSeq_entry* entry)
{
    m_ParentEntry = entry;
}

inline
CSeq_entry* CBioseq::GetParentEntry(void) const
{
    return m_ParentEntry;
}

/////////////////// end of CBioseq inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQ_BIOSEQ_HPP
/* Original file checksum: lines: 85, chars: 2191, CRC32: 21fd3921 */
