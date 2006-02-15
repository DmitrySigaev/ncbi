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

#include <ncbi_pch.hpp>
#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>

BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CMsvcSolutionGenerator::CMsvcSolutionGenerator
                                            (const list<SConfigInfo>& configs)
    :m_Configs(configs)
{
}


CMsvcSolutionGenerator::~CMsvcSolutionGenerator(void)
{
}


void 
CMsvcSolutionGenerator::AddProject(const CProjItem& project)
{
    m_Projects[CProjKey(project.m_ProjType, 
                        project.m_ID)] = CPrjContext(project);
}

void CMsvcSolutionGenerator::AddUtilityProject(
    const string& full_path, const CVisualStudioProject& prj)
{
    m_UtilityProjects.push_back(
        TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID()));
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}


void 
CMsvcSolutionGenerator::AddConfigureProject(const string& full_path,
                                            const CVisualStudioProject& prj)
{
    m_ConfigureProjects.push_back(
        TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID()));
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}


void 
CMsvcSolutionGenerator::AddBuildAllProject(const string& full_path,
                                           const CVisualStudioProject& prj)
{
    m_BuildAllProject = TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID());
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}

void CMsvcSolutionGenerator::VerifyProjectDependencies(void)
{
    for (bool changed=true; changed;) {
        changed = false;
        NON_CONST_ITERATE(TProjects, p, m_Projects) {
            CPrjContext& project = p->second;
            if (project.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
                continue;
            }
            ITERATE(list<CProjKey>, p, project.m_Project.m_Depends) {
                const CProjKey& id = *p;
                if ( id.Type() == CProjKey::eLib &&
                     GetApp().GetSite().IsLibWithChoice(id.Id()) &&
                     GetApp().GetSite().GetChoiceForLib(id.Id()) == CMsvcSite::e3PartyLib ) {
                        continue;
                }
                TProjects::const_iterator n = m_Projects.find(id);
                if (n == m_Projects.end()) {
                    CProjKey id_alt(CProjKey::eMsvc,id.Id());
                    n = m_Projects.find(id_alt);
                }
                if (n != m_Projects.end()) {
                    const CPrjContext& prj_i = n->second;
                    if (prj_i.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
                        project.m_Project.m_MakeType = eMakeType_ExcludedByReq;
                        changed = true;
                        break;
                    }
                }
            }
        }
    }
}

void 
CMsvcSolutionGenerator::SaveSolution(const string& file_path)
{
    VerifyProjectDependencies();

    CDirEntry::SplitPath(file_path, &m_SolutionDir);

    // Create dir for output sln file
    CDir(m_SolutionDir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    // Start sln file
    ofs << MSVC_SOLUTION_HEADER_LINE
        << GetApp().GetRegSettings().GetSolutionFileFormatVersion() << endl;

    // Utility projects
    ITERATE(list<TUtilityProject>, p, m_UtilityProjects) {
        const TUtilityProject& utl_prj = *p;
        WriteUtilityProject(utl_prj, ofs);
    }
    // Configure projects
    ITERATE(list<TUtilityProject>, p, m_ConfigureProjects) {
        const TUtilityProject& utl_prj = *p;
        WriteConfigureProject(utl_prj, ofs);
    }
    // BuildAll project
    if ( !m_BuildAllProject.first.empty() &&
        !m_BuildAllProject.second.empty() ) {
        WriteBuildAllProject(m_BuildAllProject, ofs);
    }

    // Projects from the projects tree
    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectAndSection(ofs, p->second);
    }


    // Start "Global" section
    ofs << "Global" << endl;
	
    // Write all configurations
    if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
        ofs << '\t' << "GlobalSection(SolutionConfigurationPlatforms) = preSolution" << endl;
    } else {
        ofs << '\t' << "GlobalSection(SolutionConfiguration) = preSolution" << endl;
    }
    ITERATE(list<SConfigInfo>, p, m_Configs) {
        string config = (*p).m_Name;
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            config = ConfigName(config);
        }
        ofs << '\t' << '\t' << config << " = " << config << endl;
    }
    ofs << '\t' << "EndGlobalSection" << endl;
    
    if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
        ofs << '\t' << "GlobalSection(ProjectConfigurationPlatforms) = postSolution" << endl;
    } else {
        ofs << '\t' << "GlobalSection(ProjectConfiguration) = postSolution" << endl;
    }

    list<string> proj_guid;
    // Utility projects
    ITERATE(list<TUtilityProject>, p, m_UtilityProjects) {
        const TUtilityProject& utl_prj = *p;
//        WriteUtilityProjectConfiguration(utl_prj, ofs);
        proj_guid.push_back(utl_prj.second);
    }
    ITERATE(list<TUtilityProject>, p, m_ConfigureProjects) {
        const TUtilityProject& utl_prj = *p;
//        WriteUtilityProjectConfiguration(utl_prj, ofs);
        proj_guid.push_back(utl_prj.second);
    }
    // BuildAll project
    if ( !m_BuildAllProject.first.empty() &&
         !m_BuildAllProject.second.empty() ) {
//        WriteUtilityProjectConfiguration(m_BuildAllProject, ofs);
        proj_guid.push_back(m_BuildAllProject.second);
    }
    // Projects from tree
    ITERATE(TProjects, p, m_Projects) {
//        WriteProjectConfigurations(ofs, p->second);
        proj_guid.push_back(p->second.m_GUID);
    }
    proj_guid.sort();
    proj_guid.unique();
    WriteProjectConfigurations( ofs, proj_guid);
    ofs << '\t' << "EndGlobalSection" << endl;

    // meanless stuff
/*
    ofs << '\t' 
        << "GlobalSection(ExtensibilityGlobals) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
	ofs << '\t' << "GlobalSection(ExtensibilityAddIns) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
*/
   
    //End of global section
    ofs << "EndGlobal" << endl;
}


//------------------------------------------------------------------------------
CMsvcSolutionGenerator::CPrjContext::CPrjContext(void)
{
    Clear();
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CPrjContext& context)
{
    SetFrom(context);
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CProjItem& project)
    :m_Project(project)
{
    m_GUID = project.m_GUID;

    CMsvcPrjProjectContext project_context(project);
    if (project.m_ProjType == CProjKey::eMsvc) {
        m_ProjectName = project_context.ProjectName();

        string project_rel_path = project.m_Sources.front();
        m_ProjectPath = CDirEntry::ConcatPath(project.m_SourcesBaseDir, 
                                             project_rel_path);
        m_ProjectPath = CDirEntry::NormalizePath(m_ProjectPath);

    } else {
        m_ProjectName = project_context.ProjectName();
        m_ProjectPath = CDirEntry::ConcatPath(project_context.ProjectDir(),
                                              project_context.ProjectName());
        m_ProjectPath += MSVC_PROJECT_FILE_EXT;
    }
}


CMsvcSolutionGenerator::CPrjContext& 
CMsvcSolutionGenerator::CPrjContext::operator= (const CPrjContext& context)
{
    if (this != &context) {
        Clear();
        SetFrom(context);
    }
    return *this;
}


CMsvcSolutionGenerator::CPrjContext::~CPrjContext(void)
{
    Clear();
}


void 
CMsvcSolutionGenerator::CPrjContext::Clear(void)
{
    //TODO
}


void 
CMsvcSolutionGenerator::CPrjContext::SetFrom
    (const CPrjContext& project_context)
{
    m_Project     = project_context.m_Project;

    m_GUID        = project_context.m_GUID;
    m_ProjectName = project_context.m_ProjectName;
    m_ProjectPath = project_context.m_ProjectPath;
}


void 
CMsvcSolutionGenerator::WriteProjectAndSection(CNcbiOfstream&     ofs, 
                                               const CPrjContext& project)
{
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID 
        << "\") = \"" 
        << project.m_ProjectName 
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.m_ProjectPath)
        << "\", \"";

    ofs << project.m_GUID 
        << "\"" 
        << endl;

    list<string> proj_guid;

    ITERATE(list<CProjKey>, p, project.m_Project.m_Depends) {

        const CProjKey& id = *p;

        if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
            // Do not generate lib-to-lib depends.
            if (project.m_Project.m_ProjType == CProjKey::eLib  &&
                id.Type() == CProjKey::eLib) {
                continue;
            }
        }
        if ( GetApp().GetSite().IsLibWithChoice(id.Id()) ) {
            if ( GetApp().GetSite().GetChoiceForLib(id.Id()) == CMsvcSite::e3PartyLib ) {
                continue;
            }
        }
        TProjects::const_iterator n = m_Projects.find(id);
        if (n == m_Projects.end()) {
// also check user projects
            CProjKey id_alt(CProjKey::eMsvc,id.Id());
            n = m_Projects.find(id_alt);
            if (n == m_Projects.end()) {
                LOG_POST(Warning << "Project " + 
                        project.m_ProjectName + " depends on missing project " + id.Id());
                continue;
            }
        }
        const CPrjContext& prj_i = n->second;
        proj_guid.push_back(prj_i.m_GUID);
    }
    if (!proj_guid.empty()) {
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        proj_guid.sort();
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    ofs << "EndProject" << endl;
}

void CMsvcSolutionGenerator::BeginUtilityProject(
    const TUtilityProject& project, CNcbiOfstream& ofs)
{
    string name = m_PathToName[project.first];
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID
        << "\") = \"" 
        << name //CDirEntry(project.first).GetBase() //basename
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.first)
        << "\", \"";

    ofs << project.second //m_GUID 
        << "\"" 
        << endl;
}

void CMsvcSolutionGenerator::EndUtilityProject(
    const TUtilityProject& project, CNcbiOfstream& ofs)
{
    ofs << "EndProject" << endl;
}

void 
CMsvcSolutionGenerator::WriteUtilityProject(const TUtilityProject& project,
                                            CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    EndUtilityProject(project,ofs);
}

void 
CMsvcSolutionGenerator::WriteConfigureProject(const TUtilityProject& project,
                                            CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
        string ptb("project_tree_builder");
        CProjKey id_ptb(CProjKey::eApp,ptb);
        TProjects::const_iterator n = m_Projects.find(id_ptb);
        if (n == m_Projects.end()) {
            LOG_POST(Warning << "Project " + 
                    project.first + " depends on missing project " + id_ptb.Id());
        } else {
            const CPrjContext& prj_i = n->second;

            ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
            ofs << '\t' << '\t' 
                << prj_i.m_GUID 
                << " = " 
                << prj_i.m_GUID << endl;
            ofs << '\t' << "EndProjectSection" << endl;
        }
    }
    EndUtilityProject(project,ofs);
}


void 
CMsvcSolutionGenerator::WriteBuildAllProject(const TUtilityProject& project, 
                                             CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    list<string> proj_guid;
    
    ITERATE(TProjects, p, m_Projects) {
//        const CProjKey&    id    = p->first;
        const CPrjContext& prj_i = p->second;
        if (prj_i.m_Project.m_MakeType == eMakeType_Excluded) {
            LOG_POST(Info << "For reference only: " << prj_i.m_ProjectName);
            continue;
        }
        if (prj_i.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
            LOG_POST(Info << "Cannot be built due to unmet requirements: " << prj_i.m_ProjectName);
            continue;
        }
        proj_guid.push_back(prj_i.m_GUID);
    }

    if (!proj_guid.empty()) {
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        proj_guid.sort();
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    EndUtilityProject(project,ofs);
}


void 
CMsvcSolutionGenerator::WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                                   const CPrjContext& project)
{
    ITERATE(list<SConfigInfo>, p, m_Configs) {

        const SConfigInfo& cfg_info = *p;

        CMsvcPrjProjectContext context(project.m_Project);
        
//        bool config_enabled = context.IsConfigEnabled(cfg_info, 0);

        const string& config = cfg_info.m_Name;
        string cfg1 = config;
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            cfg1 = ConfigName(config);
        }

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << cfg1
            << ".ActiveCfg = " 
            << ConfigName(config)
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << cfg1
            << ".Build.0 = " 
            << ConfigName(config)
            << endl;
    }
}
void CMsvcSolutionGenerator::WriteProjectConfigurations(
    CNcbiOfstream&  ofs, const list<string>& projects)
{
    ITERATE(list<string>, p, projects) {
        ITERATE(list<SConfigInfo>, c, m_Configs) {
            const SConfigInfo& cfg_info = *c;
            const string& config = cfg_info.m_Name;
            string cfg1 = config;
            if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
                cfg1 = ConfigName(config);
            }
            ofs << '\t' << '\t' << *p << '.' << cfg1
                << ".ActiveCfg = " << ConfigName(config) << endl;
            ofs << '\t' << '\t' << *p << '.' << cfg1
                << ".Build.0 = " << ConfigName(config) << endl;
        }
    }
}


void 
CMsvcSolutionGenerator::WriteUtilityProjectConfiguration(const TUtilityProject& project,
                                                        CNcbiOfstream& ofs)
{
    ITERATE(list<SConfigInfo>, p, m_Configs) {

        const string& config = (*p).m_Name;
        string cfg1 = config;
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            cfg1 = ConfigName(config);
        }

        ofs << '\t' 
            << '\t' 
            << project.second // project.m_GUID 
            << '.' 
            << cfg1
            << ".ActiveCfg = " 
            << ConfigName(config)
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.second // project.m_GUID 
            << '.' 
            << cfg1
            << ".Build.0 = " 
            << ConfigName(config)
            << endl;
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.31  2006/02/15 19:47:24  gouriano
 * Exclude projects with unmet requirements from BUILD-ALL
 *
 * Revision 1.30  2006/01/23 18:26:15  gouriano
 * Generate project GUID early, sort projects in solution by GUID
 *
 * Revision 1.29  2006/01/10 20:20:33  gouriano
 * Corrected writing configuration info
 *
 * Revision 1.28  2006/01/10 17:39:21  gouriano
 * Corrected solution generation for MSVC 2005 Express
 *
 * Revision 1.27  2005/12/27 14:57:51  gouriano
 * Adjustments for MSVC 2005 Express
 *
 * Revision 1.26  2005/09/20 20:01:03  gouriano
 * Corrected project dependencies for lib with choice
 *
 * Revision 1.25  2005/01/31 16:36:43  gouriano
 * Do not include certain projects into BUILD-ALL one
 *
 * Revision 1.24  2005/01/19 15:16:29  gouriano
 * Check user projects when generating dependencies
 *
 * Revision 1.23  2004/12/20 21:07:33  gouriano
 * Eliminate compiler warnings
 *
 * Revision 1.22  2004/12/20 15:25:18  gouriano
 * Changed diagnostic output
 *
 * Revision 1.21  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.20  2004/11/23 20:12:12  gouriano
 * Tune libraries with the choice for each configuration independently
 *
 * Revision 1.19  2004/11/17 19:55:14  gouriano
 * Corrected warning message
 *
 * Revision 1.18  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.17  2004/05/10 19:51:16  gorelenk
 * Changed CMsvcSolutionGenerator::WriteProjectAndSection .
 *
 * Revision 1.16  2004/04/19 15:42:56  gorelenk
 * Changed implementation of CMsvcSolutionGenerator::WriteProjectAndSection :
 * added test for lib choice.
 *
 * Revision 1.15  2004/02/25 19:45:00  gorelenk
 * +BuildAll utility project.
 *
 * Revision 1.14  2004/02/24 23:26:17  gorelenk
 * Changed implementation  of member-function WriteProjectConfigurations
 * of class CMsvcSolutionGenerator.
 *
 * Revision 1.13  2004/02/24 21:15:31  gorelenk
 * Added test for config availability for project to implementation  of
 * member-function WriteProjectConfigurations of class CMsvcSolutionGenerator.
 *
 * Revision 1.12  2004/02/20 22:53:26  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.11  2004/02/12 23:15:29  gorelenk
 * Implemented utility projects creation and configure re-build of the app.
 *
 * Revision 1.10  2004/02/12 17:45:12  gorelenk
 * Redesigned projects saving.
 *
 * Revision 1.9  2004/02/12 16:27:58  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.8  2004/02/10 18:20:16  gorelenk
 * Changed LOG_POST messages.
 *
 * Revision 1.7  2004/02/05 00:00:48  gorelenk
 * Changed log messages generation.
 *
 * Revision 1.6  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
