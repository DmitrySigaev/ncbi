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
*   ESearch request
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/esearch.hpp>
#include <cgi/cgi_util.hpp>


BEGIN_NCBI_SCOPE


CESearch_Request::CESearch_Request(const string& db,
                                   CRef<CEUtils_ConnContext>& ctx)
    : CEUtils_Request(ctx),
      m_UseHistory(true),
      m_RelDate(0),
      m_RetStart(0),
      m_RetMax(0),
      m_RetType(eRetType_none),
      m_Sort(eSort_none)
{
    SetDatabase(db);
}


CESearch_Request::~CESearch_Request(void)
{
}


string CESearch_Request::GetScriptName(void) const
{
    return "esearch.fcgi";
}


inline
const char* CESearch_Request::x_GetRetTypeName(void) const
{
    static const char* s_RetTypeName[] = {
        "none", "count", "ulist"
    };
    return s_RetTypeName[m_RetType];
}


inline
const char* CESearch_Request::x_GetSortName(void) const
{
    static const char* s_SortName[] = {
        "none", "author", "last+author", "journal", "pub+date"
    };
    return s_SortName[m_Sort];
}


string CESearch_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if (m_UseHistory) {
        args += "&usehistory=y";
    }
    if ( !m_Term.empty() ) {
        args += "&term=" +
            URL_EncodeString(m_Term, eUrlEncode_ProcessMarkChars);
    }
    if ( !m_Field.empty() ) {
        args += "&field=" +
            URL_EncodeString(m_Field, eUrlEncode_ProcessMarkChars);
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
    if (m_RetStart > 0) {
        args += "&retstart=" + NStr::IntToString(m_RetStart);
    }
    if (m_RetMax > 0) {
        args += "&retmax=" + NStr::IntToString(m_RetMax);
    }
    if ( m_RetType != eRetType_none ) {
        args += "&rettype=";
        args += x_GetRetTypeName();
    }
    if ( m_Sort != eSort_none ) {
        args += "&sort=";
        args += URL_EncodeString(x_GetSortName(),
            eUrlEncode_ProcessMarkChars);
    }
    return args;
}


ESerialDataFormat CESearch_Request::GetSerialDataFormat(void) const
{
    return eSerial_Xml;
}


CRef<esearch::CESearchResult> CESearch_Request::GetESearchResult(void)
{
    CObjectIStream* is = GetObjectIStream();
    _ASSERT(is);
    CRef<esearch::CESearchResult> res(new esearch::CESearchResult);
    *is >> *res;
    Disconnect();
    // Save context data
    bool have_content = res->GetData().IsInfo()  &&
        res->GetData().GetInfo().IsSetContent();
    if ( have_content ) {
        if ( res->GetData().GetInfo().GetContent().IsSetWebEnv() ) {
            GetConnContext()->SetWebEnv(
                res->GetData().GetInfo().GetContent().GetWebEnv());
        }
        if ( res->GetData().GetInfo().GetContent().IsSetQueryKey() ) {
            GetConnContext()->SetQueryKey(
                res->GetData().GetInfo().GetContent().GetQueryKey());
        }
    }
    return res;
}


END_NCBI_SCOPE
