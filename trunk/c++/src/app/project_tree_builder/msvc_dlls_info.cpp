
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
#include <app/project_tree_builder/proj_projects.hpp>
#include <app/project_tree_builder/proj_tree_builder.hpp>
#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_prj_files_collector.hpp>

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


void CMsvcDllsInfo::GetBuildConfigs(list<SConfigInfo>* config) const
{
    config->clear();

    string configs_str = m_Registry.GetString("DllBuild", "Configurations", "");
    list<string> config_names;
    NStr::Split(configs_str, LIST_SEPARATOR, config_names);
    LoadConfigInfoByNames(m_Registry, config_names, config);
}


string CMsvcDllsInfo::GetBuildDefine(void) const
{
    return m_Registry.GetString("DllBuild", "BuildDefine", "");
}


bool CMsvcDllsInfo::SDllInfo::IsEmpty(void) const
{
    return  m_Hosting.empty() &&
            m_Depends.empty() &&
            m_DllDefine.empty();
}
 
       
void CMsvcDllsInfo::SDllInfo::Clear(void)
{
    m_Hosting.clear();
    m_Depends.clear();
    m_DllDefine.erase();
}


void CMsvcDllsInfo::GetDllInfo(const string& dll_id, SDllInfo* dll_info) const
{
    dll_info->Clear();

    string hosting_str = m_Registry.GetString(dll_id, "Hosting", "");
    NStr::Split(hosting_str, LIST_SEPARATOR, dll_info->m_Hosting);

    string depends_str = m_Registry.GetString(dll_id, "Dependencies", "");
    NStr::Split(depends_str, LIST_SEPARATOR, dll_info->m_Depends);

    dll_info->m_DllDefine = m_Registry.GetString(dll_id, "DllDefine", "");
}


bool CMsvcDllsInfo::IsDllHosted(const string& lib_id) const
{
    return !GetDllHost(lib_id).empty();
}


string CMsvcDllsInfo::GetDllHost(const string& lib_id) const
{
    list<string> dll_list;
    GetDllsList(&dll_list);

    ITERATE(list<string>, p, dll_list) {
        const string& dll_id = *p;
        SDllInfo dll_info;
        GetDllInfo(dll_id, &dll_info);
        if (find(dll_info.m_Hosting.begin(),
                 dll_info.m_Hosting.end(),
                 lib_id) != dll_info.m_Hosting.end()) {
            return dll_id;
        }
    }

    return "";
}


//-----------------------------------------------------------------------------
void FilterOutDllHostedProjects(const CProjectItemsTree& tree_src, 
                                CProjectItemsTree*       tree_dst)
{
    tree_dst->m_RootSrc = tree_src.m_RootSrc;

    tree_dst->m_Projects.clear();
    ITERATE(CProjectItemsTree::TProjects, p, tree_src.m_Projects) {

        const CProjKey&  proj_id = p->first;
        const CProjItem& project = p->second;

        bool dll_hosted = (proj_id.Type() == CProjKey::eLib)  &&
                           GetApp().GetDllsInfo().IsDllHosted(proj_id.Id());
        if ( !dll_hosted ) {
            tree_dst->m_Projects[proj_id] = project;
        }
    }    
}

static bool s_IsInTree(CProjKey::TProjType      proj_type,
                       const string&            proj_id,
                       const CProjectItemsTree& tree)
{
    return tree.m_Projects.find
                  (CProjKey(proj_type, 
                            proj_id)) != 
                                    tree.m_Projects.end();
}

static void s_InitalizeDllProj(const string&                  dll_id, 
                               const CMsvcDllsInfo::SDllInfo& dll_info,
                               const CProjectItemsTree&       whole_tree,
                               CProjItem*                     dll)
{
    dll->m_Name           = dll_id;
    dll->m_ID             = dll_id;
    dll->m_ProjType       = CProjKey::eDll;

    ITERATE(list<string>, p, dll_info.m_Depends) {

        const string& depend_id = *p;

        // Is this a dll?
        CMsvcDllsInfo::SDllInfo dll_info;
        GetApp().GetDllsInfo().GetDllInfo(depend_id, &dll_info);
        if ( !dll_info.IsEmpty() ) {
            dll->m_Depends.push_back(CProjKey(CProjKey::eDll, 
                                               depend_id));    
        } else  {
            if ( s_IsInTree(CProjKey::eApp, depend_id, whole_tree) ) {
                dll->m_Depends.push_back(CProjKey(CProjKey::eApp, 
                                               depend_id)); 
            }
            else if ( s_IsInTree(CProjKey::eLib, depend_id, whole_tree) ) {
                dll->m_Depends.push_back(CProjKey(CProjKey::eLib, 
                                               depend_id)); 
            } else  {
                LOG_POST(Error << "Can not find project : " + depend_id);
            }
        }
    }

    string dll_project_dir = GetApp().GetProjectTreeInfo().m_Compilers;
    dll_project_dir = 
        CDirEntry::ConcatPath(dll_project_dir, 
                              GetApp().GetRegSettings().m_CompilersSubdir);
    dll_project_dir = 
        CDirEntry::ConcatPath(dll_project_dir, dll_id);

    dll_project_dir = CDirEntry::AddTrailingPathSeparator(dll_project_dir);

    dll->m_SourcesBaseDir = dll_project_dir;

}


static void s_AddProjItemToDll(const CProjItem& lib, CProjItem* dll)
{
    CMsvcPrjProjectContext lib_context(lib);
    CMsvcPrjFilesCollector collector(lib_context, lib);
    // Sources - all pathes are relative to one dll->m_SourcesBaseDir
    ITERATE(list<string>, p, collector.SourceFiles()) {
        const string& rel_path = *p;
        string abs_path = CDirEntry::ConcatPath(lib_context.ProjectDir(), rel_path);
        abs_path = CDirEntry::NormalizePath(abs_path);
        string dir;
        string base;
        CDirEntry::SplitPath(abs_path, &dir, &base);
        string abs_source_path = dir + base;

        string new_rel_path = 
            CDirEntry::CreateRelativePath(dll->m_SourcesBaseDir, 
                                          abs_source_path);
        dll->m_Sources.push_back(new_rel_path);
    }
    dll->m_Sources.push_back("..\\dll_main");
    dll->m_Sources.sort();
    dll->m_Sources.unique();

    // Depends
    ITERATE(list<CProjKey>, p, lib.m_Depends) {

        const CProjKey& depend_id = *p;

        bool dll_hosted = (depend_id.Type() == CProjKey::eLib)  &&
                           GetApp().GetDllsInfo().IsDllHosted(depend_id.Id());
        if ( !dll_hosted ) {
            dll->m_Depends.push_back(depend_id);
        } else {
            dll->m_Depends.push_back(CProjKey(CProjKey::eDll, 
                                           GetApp().GetDllsInfo().GetDllHost(depend_id.Id())));
        }
    }
    dll->m_Depends.sort();
    dll->m_Depends.unique();


    // m_Requires
    copy(lib.m_Requires.begin(), 
         lib.m_Requires.end(), back_inserter(dll->m_Requires));
    dll->m_Requires.sort();
    dll->m_Requires.unique();

    // Libs 3-Party
    copy(lib.m_Libs3Party.begin(), 
         lib.m_Libs3Party.end(), back_inserter(dll->m_Libs3Party));
    dll->m_Libs3Party.sort();
    dll->m_Libs3Party.unique();

    // m_IncludeDirs
    copy(lib.m_IncludeDirs.begin(), 
         lib.m_IncludeDirs.end(), back_inserter(dll->m_IncludeDirs));
    dll->m_IncludeDirs.sort();
    dll->m_IncludeDirs.unique();

    // m_DatatoolSources
    copy(lib.m_DatatoolSources.begin(), 
         lib.m_DatatoolSources.end(), back_inserter(dll->m_DatatoolSources));
    dll->m_DatatoolSources.sort();
    dll->m_DatatoolSources.unique();

    // m_Defines
    copy(lib.m_Defines.begin(), 
         lib.m_Defines.end(), back_inserter(dll->m_Defines));
    dll->m_Defines.sort();
    dll->m_Defines.unique();

    // m_NcbiCLibs
    copy(lib.m_NcbiCLibs.begin(), 
         lib.m_NcbiCLibs.end(), back_inserter(dll->m_NcbiCLibs));
    dll->m_NcbiCLibs.sort();
    dll->m_NcbiCLibs.unique();

    auto_ptr<CMsvcProjectMakefile> msvc_project_makefile = 
        auto_ptr<CMsvcProjectMakefile>
            (new CMsvcProjectMakefile
                    (CDirEntry::ConcatPath
                            (lib.m_SourcesBaseDir, 
                             CreateMsvcProjectMakefileName(lib))));

}


void CreateDllBuildTree(const CProjectItemsTree& tree_src, 
                        CProjectItemsTree*       tree_dst)
{
    // Build whole tree to get all hostee(s)
    CProjectItemsTree whole_tree;
    CProjectDummyFilter pass_all_filter;
    CProjectTreeBuilder::BuildProjectTree(&pass_all_filter, 
                                          GetApp().GetProjectTreeInfo().m_Src, 
                                          &whole_tree);

    tree_dst->m_RootSrc = tree_src.m_RootSrc;

    FilterOutDllHostedProjects(tree_src, tree_dst);

    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree_dst->m_Projects) {

        list<CProjKey> new_depends;
        CProjItem& project = p->second;
        ITERATE(list<CProjKey>, n, project.m_Depends) {
            const CProjKey& depend_id = *n;

            bool dll_hosted = (depend_id.Type() == CProjKey::eLib)  &&
                               GetApp().GetDllsInfo().IsDllHosted(depend_id.Id());
            if ( !dll_hosted ) {
                new_depends.push_back(depend_id);
            } else {
                new_depends.push_back(CProjKey(CProjKey::eDll, 
                                               GetApp().GetDllsInfo().GetDllHost(depend_id.Id())));
            }
        }
        new_depends.sort();
        new_depends.unique();
        project.m_Depends = new_depends;
    }

    list<string> dll_ids;
    CreateDllsList(tree_src, &dll_ids);
    ITERATE(list<string>, p, dll_ids) {

        const string& dll_id = *p;
        CMsvcDllsInfo::SDllInfo dll_info;
        GetApp().GetDllsInfo().GetDllInfo(dll_id, &dll_info);

        CProjItem dll;
        s_InitalizeDllProj(dll_id, dll_info, whole_tree, &dll);

        ITERATE(list<string>, n, dll_info.m_Hosting) {
            const string& lib_id = *n;
            CProjectItemsTree::TProjects::const_iterator k = 
                whole_tree.m_Projects.find(CProjKey(CProjKey::eLib, lib_id));
            if (k == tree_src.m_Projects.end()) {
                LOG_POST(Error << "No project " +
                                   lib_id + " hosted in dll : " + dll_id);
                continue;
            }

            const CProjItem& lib = k->second;
            s_AddProjItemToDll(lib, &dll);
        }

        tree_dst->m_Projects[CProjKey(CProjKey::eDll, dll_id)] = dll;
    }
}



void CreateDllsList(const CProjectItemsTree& tree_src,
                    list<string>*            dll_ids)
{
    dll_ids->clear();

    set<string> dll_set;

    ITERATE(CProjectItemsTree::TProjects, p, tree_src.m_Projects) {

        const CProjKey&  proj_id = p->first;
        const CProjItem& project = p->second;

        bool dll_hosted = (proj_id.Type() == CProjKey::eLib)  &&
                           GetApp().GetDllsInfo().IsDllHosted(proj_id.Id());
        if ( dll_hosted ) {
            dll_set.insert(GetApp().GetDllsInfo().GetDllHost(proj_id.Id()));
        }
    }    

    copy(dll_set.begin(), dll_set.end(), back_inserter(*dll_ids));
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/03/12 20:20:00  gorelenk
 * Changed implementation of s_InitalizeDllProj - changed processing of
 * dll dependencies.
 *
 * Revision 1.5  2004/03/10 22:54:58  gorelenk
 * Changed implementation of s_InitalizeDllProj - added dependencies from
 * other dll.
 *
 * Revision 1.4  2004/03/10 16:36:39  gorelenk
 * Implemented functions FilterOutDllHostedProjects,
 * CreateDllBuildTree and CreateDllsList.
 *
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
