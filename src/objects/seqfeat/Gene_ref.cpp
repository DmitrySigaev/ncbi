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
 *   'seqfeat.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2002/11/15 17:40:35  ucko
 * Before using syn, make sure it's not only set but non-empty.
 *
 * Revision 6.1  2002/01/10 19:58:38  clausen
 * Added GetLabel
 *
 *
 * ===========================================================================
 */

// standard includes

// generated includes
#include <objects/seqfeat/Gene_ref.hpp>

#include <objects/general/Dbtag.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CGene_ref::~CGene_ref(void)
{
}

// Appends a label to "label" based on content
void CGene_ref::GetLabel(string* label) const
{
    if (IsSetLocus()) {
        *label += GetLocus();
    } else if (IsSetDesc()) {
        *label += GetDesc();
    } else if (IsSetSyn() && !GetSyn().empty()) {
        *label += *GetSyn().begin();
    } else if (IsSetDb() && GetDb().size() > 0) {
        GetDb().front()->GetLabel(label);
    } else if (IsSetMaploc()) {
        *label += GetMaploc();
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1885, CRC32: 1a0ef8cd */
