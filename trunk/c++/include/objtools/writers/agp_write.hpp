/*  $Id$
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
 * Authors:  Josh Cherry
 *
 * File Description:  Write agp file
 *
 */

#include <corelib/ncbistd.hpp>

#include <objmgr/seq_map.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

///
/// This version takes a scope.  This is useful when
/// the sequence being written has components that
/// are not in the database but have been added to
/// a scope known by the caller, or when the caller
/// knows of a scope that somehow goes along with a
/// sequence (e.g., a gbench plugin).
void NCBI_XOBJWRITE_EXPORT AgpWrite(CNcbiOstream& os,
                                    const objects::CSeqMap& seq_map,
                                    const string& object_id,
                                    const string& gap_type,
                                    bool linkage,
                                    objects::CScope& scope);


///
/// Write a bioseq in AGP format
void NCBI_XOBJWRITE_EXPORT AgpWrite(CNcbiOstream& os,
                                    const objects::CBioseq_Handle& handle,
                                    const string& object_id,
                                    const string& gap_type,
                                    bool linkage);

///
/// Write a location in AGP format
///
void NCBI_XOBJWRITE_EXPORT AgpWrite(CNcbiOstream& os,
                                    const objects::CBioseq_Handle& handle,
                                    TSeqPos from, TSeqPos to,
                                    const string& object_id,
                                    const string& gap_type,
                                    bool linkage);




END_NCBI_SCOPE
