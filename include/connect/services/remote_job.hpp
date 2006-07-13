#ifndef CONNECT_SERVICES__REMOTE_JOB_HPP
#define CONNECT_SERVICES__REMOTE_JOB_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:  
 *
 */

// This file is DEPRECATED

#include <connect/services/remote_app_mb.hpp>

BEGIN_NCBI_SCOPE

#define CRemoteAppRequest CRemoteAppRequestMB
#define CRemoteAppRequest_Executer CRemoteAppRequestMB_Executer
#define CRemoteAppResult CRemoteAppResultMB
#define CRemoteAppResult_Executer CRemoteAppResultMB_Executer

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/07/13 14:32:38  didenko
 * Modified the implemention of remote application's request and result classes
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__REMOTE_JOB_HPP
