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
 *   'seq.asn'.
 */

#ifndef OBJECTS_SEQ_SEQ_ANNOT_HPP
#define OBJECTS_SEQ_SEQ_ANNOT_HPP


// generated includes
#include <objects/seq/Seq_annot_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQ_EXPORT CSeq_annot : public CSeq_annot_Base
{
    typedef CSeq_annot_Base Tparent;
public:
    // constructor
    CSeq_annot(void);
    // destructor
    ~CSeq_annot(void);

    // Removes any existing CAnnotdesc(s) of type name and adds
    // new CAnnotdesc of type name
    void AddName(const string &name);

    // Adds a CAnnotdesc of type title
    void AddTitle(const string &title);

    // Adds a CAnnotdesc of type comment
    void AddComment(const string &comment);

private:
    // Prohibit copy constructor and assignment operator
    CSeq_annot(const CSeq_annot& value);
    CSeq_annot& operator=(const CSeq_annot& value);

};



/////////////////// CSeq_annot inline methods

// constructor
inline
CSeq_annot::CSeq_annot(void)
{
}


/////////////////// end of CSeq_annot inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/05/07 10:59:14  clausen
* Added AddName, AddTitle, AddCmment
*
*
* ===========================================================================
*/

#endif // OBJECTS_SEQ_SEQ_ANNOT_HPP
/* Original file checksum: lines: 93, chars: 2396, CRC32: bdd8950d */
