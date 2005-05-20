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
 *   using the following specifications:
 *   'seq.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/general/cleanup_utils.hpp>

// generated includes
#include <objects/seq/Pubdesc.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CPubdesc::~CPubdesc(void)
{
}


// perform basic cleanup functionality (trim spaces from strings etc.)
void CPubdesc::BasicCleanup(void)
{
    if (IsSetPub()) {
        SetPub().BasicCleanup();
    }

    CLEAN_STRING_MEMBER(Name);

    if (x_IsOnlinePub()) {
        TRUNCATE_SPACES(Comment);
    } else {
        CLEAN_STRING_MEMBER(Comment);
    }
}

bool CPubdesc::x_IsOnlinePub(void) const
{
    if (IsSetPub()) {
        ITERATE (TPub::Tdata, it, GetPub().Get()) {
            if ((*it)->IsGen()) {
                const CCit_gen& gen = (*it)->GetGen();
                if (gen.IsSetCit()  &&
                    NStr::StartsWith(gen.GetCit(), "Online Publication", NStr::eNocase)) {
                    return true;
                }
            }
        }
    }
    return false;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 6.1  2005/05/20 13:34:26  shomrat
* Added BasicCleanup()
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 65, chars: 1877, CRC32: 671f8fb7 */
