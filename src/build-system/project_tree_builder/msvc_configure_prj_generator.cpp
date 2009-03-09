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
#include <corelib/ncbistre.hpp>
#include "msvc_configure_prj_generator.hpp"
#include "msvc_prj_defines.hpp"
#include "proj_builder_app.hpp"

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

CMsvcConfigureProjectGenerator::CMsvcConfigureProjectGenerator
                                  (const string&            output_dir,
                                   const list<SConfigInfo>& configs,
                                   bool                     dll_build,
                                   const string&            project_dir,
                                   const string&            tree_root,
                                   const string&            subtree_to_build,
                                   const string&            solution_to_build,
                                   bool  build_ptb)
:m_Name          ("-CONFIGURE-"),
 m_NameGui       ("-CONFIGURE-DIALOG-"),
 m_OutputDir     (output_dir),
 m_Configs       (configs),
 m_DllBuild      (dll_build),
 m_ProjectDir    (project_dir),
 m_TreeRoot      (tree_root),
 m_SubtreeToBuild(subtree_to_build),
 m_SolutionToBuild(solution_to_build),
 m_ProjectItemExt("._"),
 m_SrcFileName   ("configure"),
 m_SrcFileNameGui("configure_dialog"),
 m_FilesSubdir   ("UtilityProjectsFiles"),
 m_BuildPtb(build_ptb)
{
    m_CustomBuildCommand = "@echo on\n";

    // Macrodefines PTB_PATH  path to project_tree_builder app
    //              TREE_ROOT path to project tree root
    //              SLN_PATH  path to solution to buil
    string ptb_path_par = "$(ProjectDir)" + 
                           CDirEntry::AddTrailingPathSeparator
                                 (CDirEntry::CreateRelativePath(m_ProjectDir, 
                                                                m_OutputDir));
    if (m_BuildPtb) {
        if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
            ptb_path_par += "Release";
        } else {
            ptb_path_par += "ReleaseDLL";
        }
    } else {
        ptb_path_par += "$(ConfigurationName)";
    }

    string tree_root_par = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
                            CDirEntry::CreateRelativePath(m_ProjectDir,tree_root));
#if 1
    string sln_path_par = "$(ProjectDir)" + 
        CDirEntry::CreateRelativePath(m_ProjectDir, GetApp().m_Solution);
#else
    string sln_path_par  = "$(SolutionPath)";
#endif

    m_CustomBuildCommand =  "set PTB_PATH="  + ptb_path_par  + "\n";
    m_CustomBuildCommand += "set TREE_ROOT=" + tree_root_par + "\n";
    m_CustomBuildCommand += "set SLN_PATH="  + sln_path_par  + "\n";
    m_CustomBuildCommand += "set PTB_PLATFORM=$(PlatformName)\n";

    string build_root = CDirEntry::AddTrailingPathSeparator(
        CDirEntry::ConcatPath(
            GetApp().GetProjectTreeInfo().m_Compilers,
            GetApp().GetRegSettings().m_CompilersSubdir));
    string bld_root_par = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
            CDirEntry::CreateRelativePath(m_ProjectDir, build_root));
    m_CustomBuildCommand += "set BUILD_TREE_ROOT="  + bld_root_par  + "\n";
    m_CustomBuildCommand += "copy /Y $(InputFileName) $(InputName).bat\n";
    m_CustomBuildCommand += "call $(InputName).bat\n";
    m_CustomBuildCommand += "if errorlevel 1 exit 1\n";
    if (m_BuildPtb) {
        m_CustomBuildOutput   = ptb_path_par  + "\\project_tree_builder.exe";
    }
    CreateUtilityProject(m_Name, m_Configs, &m_Xmlprj);
    CreateUtilityProject(m_NameGui, m_Configs, &m_XmlprjGui);
}


CMsvcConfigureProjectGenerator::~CMsvcConfigureProjectGenerator(void)
{
}


void CMsvcConfigureProjectGenerator::SaveProject(bool with_gui)
{
    string name(with_gui ? m_NameGui : m_Name);
    string srcfile(with_gui ? m_SrcFileNameGui : m_SrcFileName);
    CVisualStudioProject& xmlprj = with_gui ? m_XmlprjGui : m_Xmlprj;

    {{
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Configure");
        filter->SetAttlist().SetFilter("");

        //Include collected source files
        CreateProjectFileItem(with_gui);
        CRef< CFFile > file(new CFFile());
        file->SetAttlist().SetRelativePath(srcfile + m_ProjectItemExt);
        SCustomBuildInfo build_info;
        string source_file_path_abs = 
            CDirEntry::ConcatPath(m_ProjectDir, srcfile + m_ProjectItemExt);
        source_file_path_abs = CDirEntry::NormalizePath(source_file_path_abs);
        build_info.m_SourceFile  = source_file_path_abs;
        build_info.m_Description = "Configure solution : $(SolutionName)";
        build_info.m_CommandLine = m_CustomBuildCommand;
        string outputs("$(InputPath).aanofile.out;");
        outputs += m_CustomBuildOutput;
        build_info.m_Outputs     = outputs;//"$(InputPath).aanofile.out";
        
        AddCustomBuildFileToFilter(filter, 
                                   m_Configs, 
                                   m_ProjectDir, 
                                   build_info);
        xmlprj.SetFiles().SetFilter().push_back(filter);

    }}



    SaveIfNewer(GetPath(with_gui), xmlprj);
}

string CMsvcConfigureProjectGenerator::GetPath(bool with_gui) const
{
    string project_path = CDirEntry::ConcatPath(m_ProjectDir, "_CONFIGURE_");
    if (with_gui) {
        project_path += "DIALOG_";
    }
    project_path += MSVC_PROJECT_FILE_EXT;
    return project_path;
}

const CVisualStudioProject&
CMsvcConfigureProjectGenerator::GetVisualStudioProject(bool with_gui) const
{
    if (with_gui) {
        return m_XmlprjGui;
    }
    return m_Xmlprj;
}

void CMsvcConfigureProjectGenerator::CreateProjectFileItem(bool with_gui) const
{
    string file_path = CDirEntry::ConcatPath(m_ProjectDir,
        with_gui ? m_SrcFileNameGui : m_SrcFileName);
    file_path += m_ProjectItemExt;

    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(file_path, &dir);
    CDir project_dir(dir);
    if ( !project_dir.Exists() ) {
        CDir(dir).CreatePath();
    }
    
    // Prototype of command line for launch project_tree_builder (See above)
    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    GetApp().RegisterGeneratedFile( file_path );
    ofs << "set PTB_FLAGS=";
    if ( m_DllBuild )
        ofs << " -dll";
    if (!m_BuildPtb) {
        ofs << " -nobuildptb";
    }
    if (GetApp().m_AddMissingLibs) {
        ofs << " -ext";
    }
    if (!GetApp().m_ScanWholeTree) {
        ofs << " -nws";
    }
    if (!GetApp().m_BuildRoot.empty()) {
        ofs << " -extroot \"" << GetApp().m_BuildRoot << "\"";
    }
    if (with_gui /*|| GetApp().m_ConfirmCfg*/) {
        ofs << " -cfg";
    }
    if (GetApp().m_ProjTags != "*") {
        ofs << " -projtag \"" << GetApp().m_ProjTags << "\"";
    }
    ofs << "\n";
    ofs << "set PTB_PROJECT_REQ=" << m_SubtreeToBuild << "\n";
    ofs << "call \"%BUILD_TREE_ROOT%\\ptb.bat\"\n";
    ofs << "if errorlevel 1 exit 1\n";
}
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE
