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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Utility functions - declarations.
 *
 */

#ifndef UTIL__HPP
#define UTIL__HPP

#include <connect/services/netservice_api.hpp>

BEGIN_NCBI_SCOPE

void g_PrintJSON(FILE* output_stream, CJsonNode node,
        const char* indent = "\t");

CJsonNode g_ExecAnyCmdToJson(CNetService service,
        CNetService::EServiceType service_type,
        const string& command, bool multiline);

CJsonNode g_ServerInfoToJson(CNetService service,
        CNetService::EServiceType service_type);

void g_GetUserAndHost(string* user, string* host);

END_NCBI_SCOPE

#endif // UTIL__HPP
