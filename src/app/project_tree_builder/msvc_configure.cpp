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

#include <app/project_tree_builder/msvc_configure.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

BEGIN_NCBI_SCOPE


CMsvcConfigure::CMsvcConfigure(void)
{
}


CMsvcConfigure::~CMsvcConfigure(void)
{
}


void CMsvcConfigure::operator() (const CMsvcSite&         site, 
                                 const list<SConfigInfo>& configs,
                                 const string&            root_dir)
{
    LOG_POST(Info << "Starting configure.");
    
    InitializeFrom(site);
    
    ITERATE(list<string>, p, m_ConfigureDefines) {
        const string& define = *p;
        if( ProcessDefine(define, site, configs) ) {
            LOG_POST(Info << "Configure define enabled : "  + define);
            m_ConfigSite[define] = '1';
        } else {
            LOG_POST(Info << "Configure define disabled : " + define);
            m_ConfigSite[define] = '0';
        }
    }

    // Write ncbi_conf_msvc_site.h:
    string ncbi_conf_msvc_site_path = 
        CDirEntry::ConcatPath(root_dir, m_ConfigureDefinesPath);
    LOG_POST(Info << "MSVC site configuration will be in file : " 
                      + ncbi_conf_msvc_site_path);
    string candidate_path = ncbi_conf_msvc_site_path + ".candidate";
    WriteNcbiconfMsvcSite(candidate_path);
    PromoteIfDifferent(ncbi_conf_msvc_site_path, candidate_path);

    LOG_POST(Info << "Configure finished.");
}


void CMsvcConfigure::InitializeFrom(const CMsvcSite& site)
{
    m_ConfigSite.clear();

    m_ConfigureDefinesPath = site.GetConfigureDefinesPath();
    if ( m_ConfigureDefinesPath.empty() )
        NCBI_THROW(CProjBulderAppException, 
           eConfigureDefinesPath, ""); //TODO - message
    else
        LOG_POST(Info  << "Path to configure defines file is: " +
                           m_ConfigureDefinesPath);

    site.GetConfigureDefines(&m_ConfigureDefines);
    if( m_ConfigureDefines.empty() )
        LOG_POST(Warning << "No configure defines.");
    else
        LOG_POST(Info    << "Configure defines: " + 
                            NStr::Join(m_ConfigureDefines, ", ") );
}


static bool s_LibOk(const SLibInfo& lib_info)
{
    if ( lib_info.IsEmpty() )
        return false;
    if ( !CDirEntry(lib_info.m_IncludeDir).Exists() ) {
        LOG_POST(Warning << "No LIB INCLUDE dir : " + lib_info.m_IncludeDir);
        return false;
    }
    if ( !CDirEntry(lib_info.m_LibPath).Exists() ) {
        LOG_POST(Warning << "No LIBPATH : " + lib_info.m_LibPath);
        return false;
    }
    ITERATE(list<string>, p, lib_info.m_Libs) {
        const string& lib = *p;
        string lib_path_abs = CDirEntry::ConcatPath(lib_info.m_LibPath, lib);
        if ( !CDirEntry(lib_path_abs).Exists() ) {
            LOG_POST(Warning << "No LIB : " + lib_path_abs);
            return false;
        }
    }

    return true;
}


bool CMsvcConfigure::ProcessDefine(const string& define, 
                                   const CMsvcSite& site, 
                                   const list<SConfigInfo>& configs) const
{
    if ( !site.IsDescribed(define) ) {
        LOG_POST(Info << "Not defined in site: " + define);
        return false;
    }
    list<string> components;
    site.GetComponents(define, &components);
    ITERATE(list<string>, p, components) {
        const string& component = *p;
        ITERATE(list<SConfigInfo>, n, configs) {
            const SConfigInfo& config = *n;
            SLibInfo lib_info;
            site.GetLibInfo(component, config, &lib_info);
            if ( !s_LibOk(lib_info) )
                return false;
        }
    }

    return true;
}


void CMsvcConfigure::WriteNcbiconfMsvcSite(const string& full_path) const
{
    CNcbiOfstream  ofs(full_path.c_str(), 
                       IOS_BASE::out | IOS_BASE::trunc );
    if ( !ofs )
	    NCBI_THROW(CProjBulderAppException, eFileCreation, full_path);

    ofs << "#ifndef NCBI_CONF_MSVC_SITE_HEADER" << endl;
    ofs << "#define NCBI_CONF_MSVC_SITE_HEADER" << endl;
    ofs << endl;
    ofs << endl;

    ofs <<"/* $Id$" << endl;
    ofs <<"* ===========================================================================" << endl;
    ofs <<"*" << endl;
    ofs <<"*                            PUBLIC DOMAIN NOTICE" << endl;
    ofs <<"*               National Center for Biotechnology Information" << endl;
    ofs <<"*" << endl;
    ofs <<"*  This software/database is a \"United States Government Work\" under the" << endl;
    ofs <<"*  terms of the United States Copyright Act.  It was written as part of" << endl;
    ofs <<"*  the author's official duties as a United States Government employee and" << endl;
    ofs <<"*  thus cannot be copyrighted.  This software/database is freely available" << endl;
    ofs <<"*  to the public for use. The National Library of Medicine and the U.S." << endl;
    ofs <<"*  Government have not placed any restriction on its use or reproduction." << endl;
    ofs <<"*" << endl;
    ofs <<"*  Although all reasonable efforts have been taken to ensure the accuracy" << endl;
    ofs <<"*  and reliability of the software and data, the NLM and the U.S." << endl;
    ofs <<"*  Government do not and cannot warrant the performance or results that" << endl;
    ofs <<"*  may be obtained by using this software or data. The NLM and the U.S." << endl;
    ofs <<"*  Government disclaim all warranties, express or implied, including" << endl;
    ofs <<"*  warranties of performance, merchantability or fitness for any particular" << endl;
    ofs <<"*  purpose." << endl;
    ofs <<"*" << endl;
    ofs <<"*  Please cite the author in any work or product based on this material." << endl;
    ofs <<"*" << endl;
    ofs <<"* ===========================================================================" << endl;
    ofs <<"*" << endl;
    ofs <<"* Author:  ......." << endl;
    ofs <<"*" << endl;
    ofs <<"* File Description:" << endl;
    ofs <<"*   ......." << endl;
    ofs <<"*" << endl;
    ofs <<"* Remark:" << endl;
    ofs <<"*   This code was originally generated by application proj_tree_builder" << endl;
    ofs <<"*/" << endl;
    ofs << endl;
    ofs << endl;


    ITERATE(TConfigSite, p, m_ConfigSite) {
        ofs << "#define " << p->first << " " << p->second << endl;
    }
    ofs << endl;
    ofs << "#endif // NCBI_CONF_MSVC_SITE_HEADER" << endl;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/02/10 18:23:33  gorelenk
 * Implemented overwriting of site defines file *.h only when new file is
 * different from already present one.
 *
 * Revision 1.2  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.1  2004/02/04 23:19:22  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
