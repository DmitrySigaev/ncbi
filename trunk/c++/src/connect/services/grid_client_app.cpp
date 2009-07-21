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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>
#include <connect/services/ns_client_factory.hpp>

#include <corelib/blob_storage.hpp>
#include <corelib/ncbireg.hpp>

BEGIN_NCBI_SCOPE


void CGridClientApp::Init(void)
{

    CNcbiApplication::Init();

    CNetScheduleClientFactory ns_cf(GetConfig());
    CNetScheduleAPI ns_api = ns_cf.CreateInstance();
    ns_api.SetProgramVersion(GetProgramVersion());

    CBlobStorageFactory bs_cf(GetConfig());
    m_NSStorage.reset(bs_cf.CreateInstance());

    CGridClient::ECleanUp cleanup = UseAutomaticCleanup() ?
        CGridClient::eAutomaticCleanup :
        CGridClient::eManualCleanup;

    CGridClient::EProgressMsg pmsg = UseProgressMessage() ?
        CGridClient::eProgressMsgOn :
        CGridClient::eProgressMsgOff;

    bool use_embedded_input = false;
    if (!GetConfig().Get(kNetScheduleAPIDriverName, "use_embedded_storage").empty())
        use_embedded_input = GetConfig().
            GetBool(kNetScheduleAPIDriverName, "use_embedded_storage", false, 0,
                    CNcbiRegistry::eReturn);
    else
        use_embedded_input = GetConfig().
            GetBool(kNetScheduleAPIDriverName, "use_embedded_input", false, 0,
                    CNcbiRegistry::eReturn);

    m_GridClient.reset(new CGridClient(ns_api.GetSubmitter(), *m_NSStorage,
                                       cleanup, pmsg, use_embedded_input));
}

bool CGridClientApp::UseProgressMessage() const
{
    return true;
}
bool CGridClientApp::UseAutomaticCleanup() const
{
    return true;
}

END_NCBI_SCOPE
