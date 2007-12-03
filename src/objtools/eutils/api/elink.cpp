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
* Author: Aleksey Grichenko
*
* File Description:
*   EFetch request
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/elink.hpp>
#include <cgi/cgi_util.hpp>


BEGIN_NCBI_SCOPE


CELink_Request::CELink_Request(const string& db,
                               CRef<CEUtils_ConnContext>& ctx)
    : CEUtils_Request(ctx),
      m_RelDate(0),
      m_RetMode(eRetMode_none),
      m_Cmd(eCmd_none)
{
    SetDatabase(db);
}


CELink_Request::~CELink_Request(void)
{
}


string CELink_Request::GetScriptName(void) const
{
    return "elink.fcgi";
}


inline
const char* CELink_Request::x_GetRetModeName(void) const
{
    static const char* s_RetModeName[] = {
        "none", "xml", "ref"
    };
    return s_RetModeName[m_RetMode];
}


inline
const char* CELink_Request::x_GetCommandName(void) const
{
    static const char* s_CommandName[] = {
        "none", "prlinks", "llinks", "llinkslib", "lcheck", "ncheck",
        "neighbor", "neighbor_history", "acheck"
    };
    return s_CommandName[m_Cmd];
}


string CELink_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if ( !m_DbFrom.empty() ) {
        args += "&dbfrom=" +
            URL_EncodeString(m_DbFrom, eUrlEncode_ProcessMarkChars);
    }
    string ids = m_IdGroups.AsQueryString();
    if ( !ids.empty() ) {
        args += "&" + ids;
    }
    if ( !m_Term.empty() ) {
        args += "&term=" +
            URL_EncodeString(m_Term, eUrlEncode_ProcessMarkChars);
    }
    if ( m_RelDate ) {
        args += "&reldate" + NStr::IntToString(m_RelDate);
    }
    if ( !m_MinDate.IsEmpty() ) {
        args += "&mindate=" +
            URL_EncodeString(m_MinDate.AsString("M/D/Y"),
            eUrlEncode_ProcessMarkChars);
    }
    if ( !m_MaxDate.IsEmpty() ) {
        args += "&maxdate=" +
            URL_EncodeString(m_MaxDate.AsString("M/D/Y"),
            eUrlEncode_ProcessMarkChars);
    }
    if ( !m_DateType.empty() ) {
        args += "&datetype=" + m_DateType;
    }
    if ( m_RetMode != eRetMode_none ) {
        args += "&retmode=";
        args += x_GetRetModeName();
    }
    if ( m_Cmd != eCmd_none ) {
        args += "&cmd=";
        args += x_GetCommandName();
    }
    if ( !m_LinkName.empty() ) {
        args += "&linkname=";
        args += URL_EncodeString(m_LinkName, eUrlEncode_ProcessMarkChars);
    }
    if ( !m_Holding.empty() ) {
        args += "&holding=";
        args += URL_EncodeString(m_Holding, eUrlEncode_ProcessMarkChars);
    }
    if ( !m_Version.empty() ) {
        args += "&version=";
        args += URL_EncodeString(m_Version, eUrlEncode_ProcessMarkChars);
    }
    return args;
}


ESerialDataFormat CELink_Request::GetSerialDataFormat(void) const
{
    return eSerial_Xml;
}


CRef<elink::CELinkResult> CELink_Request::GetELinkResult(void)
{
    CObjectIStream* is = GetObjectIStream();
    _ASSERT(is);
    CRef<elink::CELinkResult> res(new elink::CELinkResult);
    *is >> *res;
    Disconnect();
    return res;
}


END_NCBI_SCOPE
