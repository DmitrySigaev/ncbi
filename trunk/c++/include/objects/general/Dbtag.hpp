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
 * Revision 1.2  2001/06/25 18:51:58  grichenk
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.1  2000/11/21 18:58:04  vasilche
 * Added Match() methods for CSeq_id, CObject_id and CDbtag.
 *
 *
 * ===========================================================================
 */

#ifndef OBJECTS_GENERAL_DBTAG_HPP
#define OBJECTS_GENERAL_DBTAG_HPP


// generated includes
#include <objects/general/Dbtag_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CDbtag : public CDbtag_Base
{
    typedef CDbtag_Base Tparent;
public:
    // constructor
    CDbtag(void);
    // destructor
    ~CDbtag(void);

    // check for identity
    bool Match(const CDbtag& dbt2) const;
private:
    // Prohibit copy constructor & assignment operator
    CDbtag(const CDbtag&);
    CDbtag& operator= (const CDbtag&);
};



/////////////////// CDbtag inline methods

// constructor
inline
CDbtag::CDbtag(void)
{
}


/////////////////// end of CDbtag inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_GENERAL_DBTAG_HPP
