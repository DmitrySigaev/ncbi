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
 *   'biblio.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.5  2004/02/24 16:50:42  grichenk
 * Removed Pub.hpp
 *
 * Revision 1.4  2004/02/24 15:53:43  grichenk
 * Redesigned GetLabel(), moved most functionality from pub to biblio
 *
 * Revision 1.3  2002/12/26 12:38:37  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.2  2002/01/16 18:56:22  grichenk
 * Removed CRef<> argument from choice variant setter, updated sources to
 * use references instead of CRef<>s
 *
 * Revision 1.1  2002/01/10 20:09:04  clausen
 * Added GetLabel
 *
 *
 * ===========================================================================
 */

#ifndef OBJECTS_BIBLIO_CIT_PROC_HPP
#define OBJECTS_BIBLIO_CIT_PROC_HPP


// generated includes
#include <objects/biblio/Cit_proc_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_BIBLIO_EXPORT CCit_proc : public CCit_proc_Base
{
    typedef CCit_proc_Base Tparent;
public:
    // constructor
    CCit_proc(void);
    // destructor
    ~CCit_proc(void);
    
    // Appends a label to "label" based on content
    void GetLabel(string* label) const;

private:
    // Prohibit copy constructor and assignment operator
    CCit_proc(const CCit_proc& value);
    CCit_proc& operator=(const CCit_proc& value);

};



/////////////////// CCit_proc inline methods

// constructor
inline
CCit_proc::CCit_proc(void)
{
}

inline
void CCit_proc::GetLabel(string* label) const
{
    GetBook().GetLabel(label);
}

/////////////////// end of CCit_proc inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_BIBLIO_CIT_PROC_HPP
/* Original file checksum: lines: 90, chars: 2383, CRC32: 1511d023 */
