
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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <app/project_tree_builder/msvc_dlls_info.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <corelib/ncbistre.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


CMsvcDllsInfo::CMsvcDllsInfo(const CNcbiRegistry& registry)
    :m_Registry(registry)
{
}


CMsvcDllsInfo::~CMsvcDllsInfo(void)
{
}


void CMsvcDllsInfo::GetDllsList(list<string>* dlls_ids) const
{
    dlls_ids->clear();

    string dlls_ids_str = 
        m_Registry.GetString("DllBuild", "DLLs", "");
    
    NStr::Split(dlls_ids_str, LIST_SEPARATOR, *dlls_ids);
}


bool CMsvcDllsInfo::SDllInfo::IsEmpty(void) const
{
    return  m_Hosting.empty() &&
            m_Depends.empty();
}
 
       
void CMsvcDllsInfo::SDllInfo::Clear(void)
{
    m_Hosting.clear();
    m_Depends.clear();
}


void CMsvcDllsInfo::GelDllInfo(const string& dll_id, SDllInfo* dll_info) const
{
    dll_info->Clear();

    string hosting_str = m_Registry.GetString(dll_id, "Hosting", "");
    NStr::Split(hosting_str, LIST_SEPARATOR, dll_info->m_Hosting);

    string depends_str = m_Registry.GetString(dll_id, "Dependencies", "");
    NStr::Split(depends_str, LIST_SEPARATOR, dll_info->m_Depends);
}


string CMsvcDllsInfo::GetDllHost(const string& lib_id) const
{
    list<string> dlls_ids;
    GetDllsList(&dlls_ids);
    ITERATE(list<string>, p, dlls_ids) {
        const string& dll_id = *p;
        SDllInfo dll_info;
        GelDllInfo(dll_id, &dll_info);
        if(find(dll_info.m_Hosting.begin(),
                dll_info.m_Hosting.end(),
                lib_id) != dll_info.m_Hosting.end()) {
            return dll_id;
        }
    }
    return "";
}


bool CMsvcDllsInfo::IsDllHosted(const string& lib_id) const
{
    return !GetDllHost(lib_id).empty();
}


void CMsvcDllsInfo::GetLibPrefixes(const string& lib_id, 
                                   list<string>* prefixes) const
{
    prefixes->clear();
    string prefixes_str = m_Registry.GetString(lib_id, "PREFIX", "");
    NStr::Split(prefixes_str, LIST_SEPARATOR, *prefixes);

}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/03/08 23:34:06  gorelenk
 * Implemented member-functions GelDllInfo, IsDllHosted, GetDllHost and
 * GetLibPrefixes of class CMsvcDllsInfo.
 *
 * Revision 1.2  2004/03/03 22:16:45  gorelenk
 * Added implementation of class CMsvcDllsInfo.
 *
 * Revision 1.1  2004/03/03 17:07:14  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
