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
 * Revision 6.5  2001/10/12 19:32:57  ucko
 * move BREAK to a central location; move CBioseq::GetTitle to object manager
 *
 * Revision 6.4  2001/10/04 19:11:54  ucko
 * Centralize (rudimentary) code to get a sequence's title.
 *
 * Revision 6.3  2001/07/16 16:20:19  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

// standard includes

// generated includes
#include <objects/seq/Bioseq.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CBioseq::~CBioseq(void)
{
}

void CBioseq::Assign(const CSerialUserOp& source)
{
    const CBioseq& src = dynamic_cast<const CBioseq&>(source);
    m_ParentEntry = src.m_ParentEntry;
}

bool CBioseq::Equals(const CSerialUserOp& object) const
{
    const CBioseq& obj = dynamic_cast<const CBioseq&>(object);
    return m_ParentEntry == obj.m_ParentEntry;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1871, CRC32: 1d5d7d05 */
