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

/// @file PubMedId.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'biblio.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: PubMedId_.hpp


#ifndef OBJECTS_BIBLIO_PUBMEDID_HPP
#define OBJECTS_BIBLIO_PUBMEDID_HPP


// generated includes
#include <objects/biblio/PubMedId_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_BIBLIO_EXPORT CPubMedId : public CPubMedId_Base
{
    typedef CPubMedId_Base Tparent;
public:
    CPubMedId(void) {}

    /// Explicit constructor from the primitive type.
    explicit CPubMedId(const ncbi::TEntrezId& data)
        : Tparent(data) {}

#ifdef NCBI_STRICT_GI
    /// Explicit constructor from zero.
    explicit CPubMedId(std::nullptr_t zero)
        : Tparent((TIntId)0) {}
#endif
};

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_BIBLIO_PUBMEDID_HPP
/* Original file checksum: lines: 70, chars: 2168, CRC32: 981284c0 */
