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

#include <set>


BEGIN_NCBI_SCOPE


CMsvcPrjProjectContext::CMsvcPrjProjectContext(const CProjItem& project)
{
    //
    m_ProjectName         = project.m_ID;

    // Collect all dirs of source files into m_SourcesDirsAbs:
    set<string> sources_dirs;
    sources_dirs.insert(project.m_SourcesBaseDir);
    ITERATE(list<string>, p, project.m_Sources) {
        
        string dir;
        CDirEntry::SplitPath(*p, &dir);
        sources_dirs.insert(dir);
    }
    copy(sources_dirs.begin(), 
         sources_dirs.end(), 
         back_inserter(m_SourcesDirsAbs));

    // /src/
    const string src_tag(string(1,CDirEntry::GetPathSeparator()) + 
                         "src" +
                         CDirEntry::GetPathSeparator());
 
    // /include/
    const string include_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "include" +
                             CDirEntry::GetPathSeparator());


    // /compilers/msvc7_prj/
    const string project_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "compilers" +
                             CDirEntry::GetPathSeparator() + 
                             GetApp().GetRegSettings().m_CompilersSubdir +
                             CDirEntry::GetPathSeparator()); 


    // Creating project dir:
    m_ProjectDir = project.m_SourcesBaseDir;
    m_ProjectDir.replace(m_ProjectDir.find(src_tag), 
                         src_tag.length(), 
                         project_tag);

    //TODO 
    m_AdditionalLinkerOptions = "";
    //TODO 
    m_AdditionalLibrarianOptions = "";

    m_ProjType = project.m_ProjType;

     // Generate include dirs:
    //Root for all include dirs:
    string incl_dir = string(project.m_SourcesBaseDir, 
                             0, 
                             project.m_SourcesBaseDir.find(src_tag));
    m_IncludeDirsRoot = CDirEntry::ConcatPath(incl_dir, "include");


    ITERATE(list<string>, p, m_SourcesDirsAbs) {
        //Make include dirs from the source dirs
        string include_dir(*p);
        include_dir.replace(include_dir.find(src_tag), 
                            src_tag.length(), 
                            include_tag);
        m_IncludeDirsAbs.push_back(include_dir);
    }
    
  
    m_AdditionalIncludeDirectories = 
        CDirEntry::CreateRelativePath(m_ProjectDir, m_IncludeDirsRoot);
    
    m_MsvcProjectMakefile = 
        auto_ptr<CMsvcProjectMakefile>
            (new CMsvcProjectMakefile
                    (CDirEntry::ConcatPath
                            (project.m_SourcesBaseDir, 
                             CreateMsvcProjectMakefileName(project))));
}


const CMsvcProjectMakefile& CMsvcPrjProjectContext::GetMsvcProjectMakefile(void) const
{
    return *m_MsvcProjectMakefile;
}


CMsvcPrjGeneralContext::CMsvcPrjGeneralContext
    (const SConfigInfo&            config, 
     const CMsvcPrjProjectContext& prj_context)
     :m_Config          (config),
      m_MsvcMetaMakefile(GetApp().GetMetaMakefile())
{
    //m_Type
    switch ( prj_context.ProjectType() ) {
    case CProjItem::eLib:
        m_Type = eLib;
        break;
    case CProjItem::eApp:
        m_Type = eExe;
        break;
    default:
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, eProjectType, 
                        NStr::IntToString(prj_context.ProjectType()));
    }
    

    //m_OutputDirectory;
    // /compilers/msvc7_prj/
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

    if (m_Type == eLib)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "lib");
    else if (m_Type == eExe)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin");
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }

    string output_dir_abs = CDirEntry::ConcatPath(output_dir_prefix, config.m_Name);
    m_OutputDirectory = CDirEntry::CreateRelativePath(project_dir, 
                                                      output_dir_abs);
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

//------------------------------------------------------------------------------

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
    m_PreBuildEvent = 
        auto_ptr<IPreBuildEventTool>(new CPreBuildEventToolDummyImpl());
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


//------------------------------------------------------------------------------------------
static ICompilerTool* 
s_CreateCompilerTool(const CMsvcPrjGeneralContext& general_context,
					 const CMsvcPrjProjectContext& project_context)
{
    //-------- EXE ---------
    //
    if ( s_IsDebug (general_context, project_context) )
	     return new CCompilerToolImpl <SDebug>
                        (project_context.AdditionalIncludeDirectories(),
                         project_context.GetMsvcProjectMakefile(),
                         general_context.m_Config.m_RuntimeLibrary,
                         general_context.GetMsvcMetaMakefile());


    if ( s_IsRelease(general_context, project_context) )
	     return new CCompilerToolImpl <SRelease>
                        (project_context.AdditionalIncludeDirectories(),
                         project_context.GetMsvcProjectMakefile(),
                         general_context.m_Config.m_RuntimeLibrary,
                         general_context.GetMsvcMetaMakefile());


    // unsupported tool
    return NULL;
}


static ILinkerTool* 
s_CreateLinkerTool(const CMsvcPrjGeneralContext& general_context,
                   const CMsvcPrjProjectContext& project_context)
{
    //---- EXE ----
    if ( s_IsExe  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CLinkerToolImpl<SDebug, SApp>
                       (project_context.AdditionalLinkerOptions(),
                        project_context.ProjectName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile());

    if ( s_IsExe    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CLinkerToolImpl<SRelease, SApp>
                       (project_context.AdditionalLinkerOptions(),
                        project_context.ProjectName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile());

    //---- LIB ----
    if ( s_IsLib(general_context, project_context) )
        return new CLinkerToolDummyImpl();

    //---- DLL ----
    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CLinkerToolImpl<SDebug, SDll>
                       (project_context.AdditionalLinkerOptions(),
                        project_context.ProjectName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile());

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CLinkerToolImpl<SRelease, SDll>
                       (project_context.AdditionalLinkerOptions(),
                        project_context.ProjectName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile());
    // unsupported tool
    return NULL;
}


static ILibrarianTool* 
s_CreateLibrarianTool(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsLib  (general_context, project_context) &&
         s_IsDebug(general_context, project_context))
	    return new CLibrarianToolImpl<SDebug>
                                (project_context.AdditionalLibrarianOptions(),
								 project_context.ProjectName(),
                                 project_context.GetMsvcProjectMakefile(),
                                 general_context.GetMsvcMetaMakefile());

    if ( s_IsLib    (general_context, project_context) &&
         s_IsRelease(general_context, project_context))
	    return new CLibrarianToolImpl<SRelease>
                                (project_context.AdditionalLibrarianOptions(),
								 project_context.ProjectName(),
                                 project_context.GetMsvcProjectMakefile(),
                                 general_context.GetMsvcMetaMakefile());

    // unsupported tool
    return new CLibrarianToolDummyImpl();
}


static IResourceCompilerTool* s_CreateResourceCompilerTool(
                                  const CMsvcPrjGeneralContext& general_context,
                                  const CMsvcPrjProjectContext& project_context)
{

    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>();

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>();

    // unsupported tool
    return new CResourceCompilerToolDummyImpl();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */

