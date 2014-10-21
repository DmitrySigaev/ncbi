#ifndef NETSCHEDULE_NS_UTIL__HPP
#define NETSCHEDULE_NS_UTIL__HPP

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
 * Authors:  Victor Joukov
 *
 * File Description: Utility functions for NetSchedule
 *
 */

#include "ns_types.hpp"
#include <corelib/ncbistl.hpp>
#include <corelib/ncbireg.hpp>

#include <openssl/md5.h>


BEGIN_NCBI_SCOPE


// Validates the config file and populates a warnings list if the file has
// problems.
void NS_ValidateConfigFile(const IRegistry &  reg, vector<string> &  warnings,
                           bool  throw_port_exception);


void NS_GetConfigFileChecksum(const string &  file_name,
                              vector<string> & warnings,
                              unsigned char *  md5);
int NS_CompareChecksums(unsigned char *  lhs_md5,
                        unsigned char *  rhs_md5);


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_UTIL__HPP */

