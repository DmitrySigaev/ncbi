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

#include <app/project_tree_builder/stl_msvc_usage.hpp>

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_tools_implement.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <set>


BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------
CMsvcPrjProjectContext::CMsvcPrjProjectContext(const CProjItem& project)
{
    //MSVC project name created from project type and project ID
    m_ProjectName  = CreateProjectName(CProjKey(project.m_ProjType, 
                                                project.m_ID));
    m_ProjectId    = project.m_ID;

    m_SourcesBaseDir = project.m_SourcesBaseDir;
    m_Requires       = project.m_Requires;

    // Get msvc project makefile
    m_MsvcProjectMakefile = 
        auto_ptr<CMsvcProjectMakefile>
            (new CMsvcProjectMakefile
                    (CDirEntry::ConcatPath
                            (m_SourcesBaseDir, 
                             CreateMsvcProjectMakefileName(project))));

    // Collect all dirs of source files into m_SourcesDirsAbs:
    set<string> sources_dirs;
    sources_dirs.insert(m_SourcesBaseDir);
    ITERATE(list<string>, p, project.m_Sources) {

        const string& src_rel = *p;
        string src_path = CDirEntry::ConcatPath(m_SourcesBaseDir, src_rel);
        src_path = CDirEntry::NormalizePath(src_path);

        string dir;
        CDirEntry::SplitPath(src_path, &dir);
        sources_dirs.insert(dir);
    }
    copy(sources_dirs.begin(), 
         sources_dirs.end(), 
         back_inserter(m_SourcesDirsAbs));


    // Creating project dir:
    if (project.m_ProjType == CProjKey::eDll) {
        //For dll - it is so
        m_ProjectDir = project.m_SourcesBaseDir;

    } else {
        m_ProjectDir = GetApp().GetProjectTreeInfo().m_Compilers;
        m_ProjectDir = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  GetApp().GetRegSettings().m_CompilersSubdir);
        m_ProjectDir = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  GetApp().GetBuildType().GetTypeStr());
        m_ProjectDir = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  CDirEntry::CreateRelativePath
                                      (GetApp().GetProjectTreeInfo().m_Src, 
                                      m_SourcesBaseDir));
        m_ProjectDir = CDirEntry::AddTrailingPathSeparator(m_ProjectDir);
    }

    m_ProjType = project.m_ProjType;

    // Generate include dirs:
    // Include dirs for appropriate src dirs
    set<string> include_dirs;
    ITERATE(list<string>, p, project.m_Sources) {
        //create full path for src file
        const string& src_rel = *p;
        string src_abs  = CDirEntry::ConcatPath(m_SourcesBaseDir, src_rel);
        src_abs = CDirEntry::NormalizePath(src_abs);
        //part of path (from <src> dir)
        string rel_path  = 
            CDirEntry::CreateRelativePath(GetApp().GetProjectTreeInfo().m_Src, 
                                          src_abs);
        //add this part to <include> dir
        string incl_path = 
            CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, 
                                  rel_path);
        string incl_dir;
        CDirEntry::SplitPath(incl_path, &incl_dir);
        include_dirs.insert(incl_dir);

    }
    copy(include_dirs.begin(), 
         include_dirs.end(), 
         back_inserter(m_IncludeDirsAbs));


    // Get custom build files and adjust pathes 
    m_MsvcProjectMakefile->GetCustomBuildInfo(&m_CustomBuildInfo);
    NON_CONST_ITERATE(list<SCustomBuildInfo>, p, m_CustomBuildInfo) {
       SCustomBuildInfo& build_info = *p;
       string file_path_abs = 
           CDirEntry::ConcatPath(m_SourcesBaseDir, build_info.m_SourceFile);
       build_info.m_SourceFile = 
           CDirEntry::CreateRelativePath(m_ProjectDir, file_path_abs);           
    }

    // Collect include dirs, specified in project Makefiles
    m_ProjectIncludeDirs = project.m_IncludeDirs;

    // LIBS from Makefiles
    m_ProjectLibs = project.m_Libs3Party;

    // Proprocessor definitions from makefiles:
    m_Defines = project.m_Defines;
    if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
        m_Defines.push_back(GetApp().GetDllsInfo().GetBuildDefine());
        if (project.m_ProjType == CProjKey::eDll) {
            CMsvcDllsInfo::SDllInfo dll_info;
            GetApp().GetDllsInfo().GetDllInfo(project.m_ID, &dll_info);
            m_Defines.push_back(dll_info.m_DllDefine);
        }
    }
    // Pre-Builds for LIB projects:
    if (m_ProjType == CProjKey::eLib) {
        ITERATE(list<CProjKey>, p, project.m_Depends) {
            const CProjKey& proj_key = *p;
            if (proj_key.Type() == CProjKey::eLib) {
                m_PreBuilds.push_back(CreateProjectName(proj_key));
            }
        }
    }

    // Libraries from NCBI C Toolkit
    m_NcbiCLibs = project.m_NcbiCLibs;
}


string CMsvcPrjProjectContext::AdditionalIncludeDirectories
                                            (const SConfigInfo& cfg_info) const
{
    list<string> add_include_dirs_list;
    add_include_dirs_list.push_back 
        (CDirEntry::CreateRelativePath(m_ProjectDir, 
                                      GetApp().GetProjectTreeInfo().m_Include));
    
    //take into account project include dirs
    ITERATE(list<string>, p, m_ProjectIncludeDirs) {
        const string& dir_abs = *p;
        add_include_dirs_list.push_back(SameRootDirs(m_ProjectDir,dir_abs) ?
                CDirEntry::CreateRelativePath(m_ProjectDir, dir_abs) :
                dir_abs);
    }

    //MSVC Makefile additional include dirs
    list<string> makefile_add_incl_dirs;
    m_MsvcProjectMakefile->GetAdditionalIncludeDirs(cfg_info, 
                                                    &makefile_add_incl_dirs);

    ITERATE(list<string>, p, makefile_add_incl_dirs) {
        const string& dir = *p;
        string dir_abs = 
            CDirEntry::AddTrailingPathSeparator
                (CDirEntry::ConcatPath(m_SourcesBaseDir, dir));
        dir_abs = CDirEntry::NormalizePath(dir_abs);
        dir_abs = 
            CDirEntry::CreateRelativePath
                        (m_ProjectDir, dir_abs);
        add_include_dirs_list.push_back(dir_abs);
    }

    // Additional include dirs for 3-party libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( !lib_info.m_IncludeDir.empty() ) {
            const string& dir_abs = lib_info.m_IncludeDir;
            add_include_dirs_list.push_back(SameRootDirs(m_ProjectDir,dir_abs)?
                                CDirEntry::CreateRelativePath(m_ProjectDir, 
                                                              dir_abs) :
                                dir_abs);
        }
    }

    //Leave only unique dirs and join them to string
    add_include_dirs_list.sort();
    add_include_dirs_list.unique();
    return NStr::Join(add_include_dirs_list, ", ");
}


string CMsvcPrjProjectContext::AdditionalLinkerOptions
                                            (const SConfigInfo& cfg_info) const
{
    list<string> additional_libs;

    // Take into account requires, default and makefiles libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( !lib_info.m_Libs.empty() ) {
            copy(lib_info.m_Libs.begin(), 
                 lib_info.m_Libs.end(), 
                 back_inserter(additional_libs));
        }
    }

    // NCBI C Toolkit libs
    ITERATE(list<string>, p, m_NcbiCLibs) {
        string ncbi_lib = *p + ".lib";
        additional_libs.push_back(ncbi_lib);        
    }
    additional_libs.sort();
    additional_libs.unique();

    return NStr::Join(additional_libs, " ");
}


string CMsvcPrjProjectContext::AdditionalLibrarianOptions
                                            (const SConfigInfo& cfg_info) const
{
    return AdditionalLinkerOptions(cfg_info);
}


string CMsvcPrjProjectContext::AdditionalLibraryDirectories
                                            (const SConfigInfo& cfg_info) const
{
    string dirs_str("");

    // Take into account requires, default and makefiles libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( !lib_info.m_LibPath.empty() ) {
            dirs_str += lib_info.m_LibPath;
            dirs_str += ", ";
        }
    }

    return dirs_str;
}


void CMsvcPrjProjectContext::CreateLibsList(list<string>* libs_list) const
{
    libs_list->clear();
    // We'll build libs list.
    *libs_list = m_Requires;
    //take into account default libs from site:
    libs_list->push_back(MSVC_DEFAULT_LIBS_TAG);
    //and LIBS from Makefiles:
    ITERATE(list<string>, p, m_ProjectLibs) {
        const string& lib = *p;
        list<string> components;
        GetApp().GetSite().GetComponents(lib, &components);
        copy(components.begin(), 
             components.end(), back_inserter(*libs_list));

    }
    libs_list->sort();
    libs_list->unique();
}

const CMsvcProjectMakefile& 
CMsvcPrjProjectContext::GetMsvcProjectMakefile(void) const
{
    return *m_MsvcProjectMakefile;
}


bool CMsvcPrjProjectContext::IsRequiresOk(const CProjItem& prj)
{
    ITERATE(list<string>, p, prj.m_Requires) {
        const string& requires = *p;
        if ( !GetApp().GetSite().IsProvided(requires) )
            return false;
    }
    return true;
}


bool CMsvcPrjProjectContext::IsConfigEnabled(const SConfigInfo& config) const
{
    list<string> libs_3party;
    ITERATE(list<string>, p, m_ProjectLibs) {
        const string& lib = *p;
        list<string> components;
        GetApp().GetSite().GetComponents(lib, &components);
        copy(components.begin(), 
             components.end(), back_inserter(libs_3party));
    }
    
    ITERATE(list<string>, p, libs_3party) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, config, &lib_info);
        
        if ( lib_info.IsEmpty() ) 
            continue;

        if ( !GetApp().GetSite().IsLibEnabledInConfig(requires, config) )
            return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
CMsvcPrjGeneralContext::CMsvcPrjGeneralContext
    (const SConfigInfo&            config, 
     const CMsvcPrjProjectContext& prj_context)
     :m_Config          (config),
      m_MsvcMetaMakefile(GetApp().GetMetaMakefile())
{
    //m_Type
    switch ( prj_context.ProjectType() ) {
    case CProjKey::eLib:
        m_Type = eLib;
        break;
    case CProjKey::eApp:
        m_Type = eExe;
        break;
    case CProjKey::eDll:
        m_Type = eDll;
        break;
    default:
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, eProjectType, 
                        NStr::IntToString(prj_context.ProjectType()));
    }
    

    //m_OutputDirectory;
    // /compilers/msvc7_prj/
    string output_dir_abs = GetApp().GetProjectTreeInfo().m_Compilers;
    output_dir_abs = 
            CDirEntry::ConcatPath(output_dir_abs, 
                                  GetApp().GetRegSettings().m_CompilersSubdir);
    output_dir_abs = 
            CDirEntry::ConcatPath(output_dir_abs, 
                                  GetApp().GetBuildType().GetTypeStr());
    if (m_Type == eLib)
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "lib");
    else if (m_Type == eExe)
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "bin");
    else if (m_Type == eDll) // same dir as exe 
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "bin"); 
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }

    output_dir_abs = 
        CDirEntry::ConcatPath(output_dir_abs, config.m_Name);
    m_OutputDirectory = 
        CDirEntry::CreateRelativePath(prj_context.ProjectDir(), 
                                      output_dir_abs);

#if 0

    const string project_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "compilers" +
                             CDirEntry::GetPathSeparator() + 
                             GetApp().GetRegSettings().m_CompilersSubdir +
                             CDirEntry::GetPathSeparator());
    
    string project_dir = prj_context.ProjectDir();
    string output_dir_prefix = 
        string (project_dir, 
                0, 
                project_dir.find(project_tag) + project_tag.length());
    
    output_dir_prefix = 
        CDirEntry::ConcatPath(output_dir_prefix, 
                              GetApp().GetBuildType().GetTypeStr());

    if (m_Type == eLib)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "lib");
    else if (m_Type == eExe)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin");
    else if (m_Type == eDll) // same dir as exe 
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin"); 
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }

    //output to ..static\DebugDLL or ..dll\DebugDLL
    string output_dir_abs = 
        CDirEntry::ConcatPath(output_dir_prefix, config.m_Name);
    m_OutputDirectory = 
        CDirEntry::CreateRelativePath(project_dir, output_dir_abs);
#endif
}

//------------------------------------------------------------------------------
static IConfiguration* s_CreateConfiguration
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ICompilerTool* s_CreateCompilerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ILinkerTool* s_CreateLinkerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ILibrarianTool* s_CreateLibrarianTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static IResourceCompilerTool* s_CreateResourceCompilerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

//-----------------------------------------------------------------------------
CMsvcTools::CMsvcTools(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    //configuration
    m_Configuration = 
        auto_ptr<IConfiguration>(s_CreateConfiguration(general_context, 
                                                       project_context));
    //compiler
    m_Compiler = 
        auto_ptr<ICompilerTool>(s_CreateCompilerTool(general_context, 
                                                     project_context));
    //Linker:
    m_Linker = 
        auto_ptr<ILinkerTool>(s_CreateLinkerTool(general_context, 
                                                 project_context));
    //Librarian
    m_Librarian = 
        auto_ptr<ILibrarianTool>(s_CreateLibrarianTool(general_context, 
                                                       project_context));
    //Dummies
    m_CustomBuid = auto_ptr<ICustomBuildTool>(new CCustomBuildToolDummyImpl());
    m_MIDL       = auto_ptr<IMIDLTool>(new CMIDLToolDummyImpl());
    m_PostBuildEvent =
        auto_ptr<IPostBuildEventTool>(new CPostBuildEventToolDummyImpl());

    //Pre-build event - special case for LIB projects
    if (project_context.ProjectType() == CProjKey::eLib) {
        m_PreBuildEvent = 
            auto_ptr<IPreBuildEventTool>(new CPreBuildEventToolLibImpl
                                                (project_context.PreBuilds()));
    } else {
        m_PreBuildEvent = 
            auto_ptr<IPreBuildEventTool>(new CPreBuildEventToolDummyImpl());
    }
    m_PreLinkEvent = 
        auto_ptr<IPreLinkEventTool>(new CPreLinkEventToolDummyImpl());

    //Resource Compiler
    m_ResourceCompiler =
        auto_ptr<IResourceCompilerTool>
                        (s_CreateResourceCompilerTool(general_context, 
                                                      project_context));

    //Dummies
    m_WebServiceProxyGenerator =
        auto_ptr<IWebServiceProxyGeneratorTool>
                        (new CWebServiceProxyGeneratorToolDummyImpl());

    m_XMLDataGenerator = 
        auto_ptr<IXMLDataGeneratorTool>
                        (new CXMLDataGeneratorToolDummyImpl());

    m_ManagedWrapperGenerator =
        auto_ptr<IManagedWrapperGeneratorTool>
                        (new CManagedWrapperGeneratorToolDummyImpl());

    m_AuxiliaryManagedWrapperGenerator=
        auto_ptr<IAuxiliaryManagedWrapperGeneratorTool> 
                        (new CAuxiliaryManagedWrapperGeneratorToolDummyImpl());
}


IConfiguration* CMsvcTools::Configuration(void) const
{
    return m_Configuration.get();
}


ICompilerTool* CMsvcTools::Compiler(void) const
{
    return m_Compiler.get();
}


ILinkerTool* CMsvcTools::Linker(void) const
{
    return m_Linker.get();
}


ILibrarianTool* CMsvcTools::Librarian(void) const
{
    return m_Librarian.get();
}


ICustomBuildTool* CMsvcTools::CustomBuid(void) const
{
    return m_CustomBuid.get();
}


IMIDLTool* CMsvcTools::MIDL(void) const
{
    return m_MIDL.get();
}


IPostBuildEventTool* CMsvcTools::PostBuildEvent(void) const
{
    return m_PostBuildEvent.get();
}


IPreBuildEventTool* CMsvcTools::PreBuildEvent(void) const
{
    return m_PreBuildEvent.get();
}


IPreLinkEventTool* CMsvcTools::PreLinkEvent(void) const
{
    return m_PreLinkEvent.get();
}


IResourceCompilerTool* CMsvcTools::ResourceCompiler(void) const
{
    return m_ResourceCompiler.get();
}


IWebServiceProxyGeneratorTool* CMsvcTools::WebServiceProxyGenerator(void) const
{
    return m_WebServiceProxyGenerator.get();
}


IXMLDataGeneratorTool* CMsvcTools::XMLDataGenerator(void) const
{
    return m_XMLDataGenerator.get();
}


IManagedWrapperGeneratorTool* CMsvcTools::ManagedWrapperGenerator(void) const
{
    return m_ManagedWrapperGenerator.get();
}


IAuxiliaryManagedWrapperGeneratorTool* 
                       CMsvcTools::AuxiliaryManagedWrapperGenerator(void) const
{
    return m_AuxiliaryManagedWrapperGenerator.get();
}


CMsvcTools::~CMsvcTools()
{
}


static bool s_IsExe(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::TTargetType::eExe;
}


static bool s_IsLib(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::TTargetType::eLib;
}


static bool s_IsDll(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::TTargetType::eDll;
}


static bool s_IsDebug(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Config.m_Debug;
}


static bool s_IsRelease(const CMsvcPrjGeneralContext& general_context,
                        const CMsvcPrjProjectContext& project_context)
{
    return !(general_context.m_Config.m_Debug);
}


//-----------------------------------------------------------------------------
// Creators:
static IConfiguration* 
s_CreateConfiguration(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsExe(general_context, project_context) )
	    return new CConfigurationImpl<SApp>
                       (general_context.OutputDirectory(), 
                        general_context.ConfigurationName());

    if ( s_IsLib(general_context, project_context) )
	    return new CConfigurationImpl<SLib>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName());

    if ( s_IsDll(general_context, project_context) )
	    return new CConfigurationImpl<SDll>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName());
    return NULL;
}


static ICompilerTool* 
s_CreateCompilerTool(const CMsvcPrjGeneralContext& general_context,
					 const CMsvcPrjProjectContext& project_context)
{
    return new CCompilerToolImpl
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.m_Config.m_RuntimeLibrary,
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config,
        general_context.m_Type,
        project_context.Defines());
}


static ILinkerTool* 
s_CreateLinkerTool(const CMsvcPrjGeneralContext& general_context,
                   const CMsvcPrjProjectContext& project_context)
{
    //---- EXE ----
    if ( s_IsExe  (general_context, project_context) )
        return new CLinkerToolImpl<SApp>
                       (project_context.AdditionalLinkerOptions
                                            (general_context.m_Config),
                        project_context.AdditionalLibraryDirectories
                                            (general_context.m_Config),
                        project_context.ProjectId(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config);


    //---- LIB ----
    if ( s_IsLib(general_context, project_context) )
        return new CLinkerToolDummyImpl();

    //---- DLL ----
    if ( s_IsDll  (general_context, project_context) )
        return new CLinkerToolImpl<SDll>
                       (project_context.AdditionalLinkerOptions
                                            (general_context.m_Config),
                        project_context.AdditionalLibraryDirectories
                                            (general_context.m_Config),
                        project_context.ProjectId(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config);

    // unsupported tool
    return NULL;
}


static ILibrarianTool* 
s_CreateLibrarianTool(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsLib  (general_context, project_context) )
	    return new CLibrarianToolImpl
                                (project_context.AdditionalLibrarianOptions
                                                    (general_context.m_Config),
                                 project_context.AdditionalLibraryDirectories
                                                    (general_context.m_Config),
								 project_context.ProjectId(),
                                 project_context.GetMsvcProjectMakefile(),
                                 general_context.GetMsvcMetaMakefile(),
                                 general_context.m_Config);

    // dummy tool
    return new CLibrarianToolDummyImpl();
}


static IResourceCompilerTool* s_CreateResourceCompilerTool
                                (const CMsvcPrjGeneralContext& general_context,
                                 const CMsvcPrjProjectContext& project_context)
{

    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);

    if ( s_IsExe  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);


    if ( s_IsExe    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);


    // dummy tool
    return new CResourceCompilerToolDummyImpl();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.23  2004/03/16 21:46:17  gorelenk
 * Changed implementation of
 * CMsvcPrjProjectContext::AdditionalIncludeDirectories : implemented
 * uniqueness of include dirs from 3-party libraries.
 *
 * Revision 1.22  2004/03/16 16:37:33  gorelenk
 * Changed msvc7_prj subdirs structure: Separated "static" and "dll" branches.
 *
 * Revision 1.21  2004/03/10 21:28:42  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.20  2004/03/10 16:44:21  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext.
 *
 * Revision 1.19  2004/03/08 23:36:11  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.18  2004/03/02 23:33:55  gorelenk
 * Changed implementation of class CMsvcPrjGeneralContext constructor.
 *
 * Revision 1.17  2004/02/26 15:15:38  gorelenk
 * Changed implementation of CMsvcPrjProjectContext::IsConfigEnabled.
 *
 * Revision 1.16  2004/02/24 20:54:26  gorelenk
 * Added implementation of member-function bool IsConfigEnabled
 * of class CMsvcPrjProjectContext.
 *
 * Revision 1.15  2004/02/24 18:13:26  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 * Changed implementation of member-function AdditionalLinkerOptions of
 * class CMsvcPrjProjectContext.
 *
 * Revision 1.14  2004/02/23 20:58:41  gorelenk
 * Fixed double properties apperience in generated MSVC projects.
 *
 * Revision 1.13  2004/02/23 20:42:57  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.12  2004/02/20 22:53:26  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.11  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.10  2004/02/05 16:28:47  gorelenk
 * GetComponents was moved to class CMsvcSite.
 *
 * Revision 1.9  2004/02/05 00:02:08  gorelenk
 * Added support of user site and  Makefile defines.
 *
 * Revision 1.8  2004/02/03 17:17:38  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.7  2004/01/29 15:46:44  gorelenk
 * Added support of default libs, defined in user site
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

