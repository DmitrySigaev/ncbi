#ifndef TOOLS_IMPLEMENT_HEADER
#define TOOLS_IMPLEMENT_HEADER

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


#include <string>

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_traits.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CConfigurationImpl --
///
/// Implementation of IConfiguration interface.
///
/// Accepts trait class as a template parameter.

template <class ConfTrait> 
class CConfigurationImpl : public IConfiguration
{
public:
    CConfigurationImpl(	const string& output_directory, 
                        const string& configuration_name )
                        :m_OutputDirectory  (output_directory),
                         m_ConfigurationName(configuration_name)
    {
    }

    virtual string Name(void) const
    {
	    return ConfigName(m_ConfigurationName);
    }
    virtual string OutputDirectory(void) const
    {
	    return m_OutputDirectory;
    }
    virtual string IntermediateDirectory(void) const
    {
	    return m_ConfigurationName;
    }
    virtual string ConfigurationType(void) const
    {
	    return ConfTrait::ConfigurationType();
    }
    virtual string CharacterSet(void) const
    {
	    return "2";
    }


private:
    string m_OutputDirectory;
    string m_ConfigurationName;

    CConfigurationImpl(void);
    CConfigurationImpl(const CConfigurationImpl&);
    CConfigurationImpl& operator= (const CConfigurationImpl&);
};


static string s_GetDefaultPreprocessorDefinitions
                            (const SConfigInfo&                   config, 
                             CMsvcPrjGeneralContext::TTargetType  target_type)
{
    string defines = config.m_Debug ? "_DEBUG;" : "NDEBUG;" ;
    switch (target_type) {
    case CMsvcPrjGeneralContext::eLib:
        defines +=  "WIN32;_LIB;";
        break;
    case CMsvcPrjGeneralContext::eExe:
        defines += "WIN32;_CONSOLE;";
        break;
    case CMsvcPrjGeneralContext::eDll:
        defines += "WIN32;_WINDOWS;_USRDLL;";
        break;
    }
    return defines;
}



static string s_GetLibsPrefixesPreprocessorDefinitions
                             (const CProjKey&                      project_id,
                              CMsvcPrjGeneralContext::TTargetType  target_type,
                              const list<CProjKey>&                depends)
{

    string defines;

    // Handling only dlls
    if (GetApp().GetBuildType().GetType() != CBuildType::eDll)
        return defines;
    
    // Project itself
    list<string> self;
    if (project_id.Type() == CProjKey::eLib && 
        GetApp().GetDllsInfo().IsDllHosted(project_id.Id()) ) {

        list<string> prefixes;
        GetApp().GetDllsInfo().GetLibPrefixes(project_id.Id(), &prefixes);
        ITERATE(list<string>, p, prefixes) {
            string define;
            const string& prefix = *p;
            define += prefix;
            define += "=";
            define += "__declspec(dllexport)";
            define += ';';
            self.push_back(define);
        }
    } 


    // Imports from dll
    list<string> dll_imports;
    ITERATE(list<CProjKey>, p, depends) {
    
        const CProjKey& lib_id = *p;
        if (lib_id.Type() == CProjKey::eApp)
            continue;
        if (GetApp().GetDllsInfo().IsDllHosted(lib_id.Id())) {

            list<string> prefixes;
            GetApp().GetDllsInfo().GetLibPrefixes(lib_id.Id(), &prefixes);
            ITERATE(list<string>, n, prefixes) {
                string define;
                const string& prefix = *n;
                define += prefix;
                define += "=";
                define += "__declspec(dllimport)";
                define += ';';
                dll_imports.push_back(define);
            }
        }
    }
    // Links to static libs
    list<string> static_links;

#if 0
    ITERATE(list<CProjKey>, p, depends) {
    
        const CProjKey& lib_id = *p;
        if (lib_id.Type() == CProjKey::eApp)
            continue;

        if ( !dll_build ||
             !GetApp().GetDllsInfo().IsDllHosted(lib_id.Id()) ) {

            list<string> prefixes;
            GetApp().GetDllsInfo().GetLibPrefixes(lib_id.Id(), &prefixes);
            ITERATE(list<string>, n, prefixes) {
                string define;
                const string& prefix = *n;
                define += prefix;
                define += "=";
                define += ';';
                static_links.push_back(define);
            }            
        }
    }
    if ( !dll_build && project_id.Type() == CProjKey::eLib ) {

        set<string> depend_host_ids;
        GetApp().GetDllsInfo().GetHostDependsFromHosts
            ( GetApp().GetDllsInfo().GetDllHost(project_id.Id()), 
              &depend_host_ids);
        ITERATE(set<string>, n, depend_host_ids) {
            const string& host_id = *n;
            CMsvcDllsInfo::SDllInfo host_info;
            GetApp().GetDllsInfo().GelDllInfo(host_id, &host_info);
            ITERATE(list<string>, m, host_info.m_Hosting) {

                const string& depend_lib_id = *m;

                list<string> prefixes;
                GetApp().GetDllsInfo().GetLibPrefixes(depend_lib_id, &prefixes);
                ITERATE(list<string>, k, prefixes) {
                    string define;
                    const string& prefix = *k;
                    define += prefix;
                    define += "=";
                    define += ';';
                    static_links.push_back(define);
                }
            }
        }
    }

#endif
    // Finalizing
    defines += NStr::Join(self,"");
    defines += NStr::Join(dll_imports,"");
    defines += NStr::Join(static_links,"");

    return defines;
}

/////////////////////////////////////////////////////////////////////////////
///
/// CCompilerToolImpl --
///
/// Implementation of ICompilerTool interface.
///
/// Uses msvc makefiles information
class CCompilerToolImpl : public ICompilerTool
{
public:
    typedef CMsvcPrjGeneralContext::TTargetType TTargetType;

    CCompilerToolImpl(const string&               additional_include_dirs,
                      const CMsvcProjectMakefile& project_makefile,
                      const string&               runtimeLibraryOption,
                      const CMsvcMetaMakefile&    meta_makefile,
                      const SConfigInfo&          config,
                      TTargetType                 target_type,
                      const list<string>&         defines,
                      const CProjKey&             project_id,
                      const list<CProjKey>&       depends)
	    :m_AdditionalIncludeDirectories(additional_include_dirs),
         m_MsvcProjectMakefile         (project_makefile),
         m_RuntimeLibraryOption        (runtimeLibraryOption),
         m_MsvcMetaMakefile            (meta_makefile),
         m_Config                      (config),
         m_TargetType                  (target_type),
         m_Defines                     (defines),
         m_ProjectId                   (project_id),
         m_Depends                     (depends)
    {
    }

    virtual string Name(void) const
    {
	    return "VCCLCompilerTool";
    }

#define SUPPORT_COMPILER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetCompilerOpt(m_MsvcMetaMakefile, \
                              m_MsvcProjectMakefile, \
                              #opt, \
                              m_Config ); \
    }

    SUPPORT_COMPILER_OPTION(Optimization)

    virtual string AdditionalIncludeDirectories(void) const
    {
	    return m_AdditionalIncludeDirectories;
    }

    //SUPPORT_COMPILER_OPTION(PreprocessorDefinitions)
    virtual string PreprocessorDefinitions(void) const
    {
        string defines = 
            s_GetDefaultPreprocessorDefinitions(m_Config, m_TargetType);

       defines += 
            s_GetLibsPrefixesPreprocessorDefinitions(m_ProjectId, 
                                                     m_TargetType, 
                                                     m_Depends);

        ITERATE(list<string>, p, m_Defines) {
            const string& define = *p;
            defines += define;
            defines += ';';
        }

        defines += GetCompilerOpt(m_MsvcMetaMakefile,
                                  m_MsvcProjectMakefile,
                                  "PreprocessorDefinitions",
                                  m_Config );
        return defines;
    }

    SUPPORT_COMPILER_OPTION(MinimalRebuild)
    SUPPORT_COMPILER_OPTION(BasicRuntimeChecks)

    virtual string RuntimeLibrary(void) const
    {
	    return m_RuntimeLibraryOption;
    }

    SUPPORT_COMPILER_OPTION(RuntimeTypeInfo)
    SUPPORT_COMPILER_OPTION(UsePrecompiledHeader)
    SUPPORT_COMPILER_OPTION(WarningLevel)
    SUPPORT_COMPILER_OPTION(Detect64BitPortabilityProblems)
    SUPPORT_COMPILER_OPTION(DebugInformationFormat)
    SUPPORT_COMPILER_OPTION(CompileAs)
    SUPPORT_COMPILER_OPTION(InlineFunctionExpansion)
    SUPPORT_COMPILER_OPTION(OmitFramePointers)
    SUPPORT_COMPILER_OPTION(StringPooling)
    SUPPORT_COMPILER_OPTION(EnableFunctionLevelLinking)


    //Latest additions
    SUPPORT_COMPILER_OPTION(OptimizeForProcessor)
    SUPPORT_COMPILER_OPTION(StructMemberAlignment)
    SUPPORT_COMPILER_OPTION(CallingConvention)
    SUPPORT_COMPILER_OPTION(IgnoreStandardIncludePath)
    SUPPORT_COMPILER_OPTION(ExceptionHandling)
    SUPPORT_COMPILER_OPTION(BufferSecurityCheck)
    SUPPORT_COMPILER_OPTION(DisableSpecificWarnings)
    SUPPORT_COMPILER_OPTION(UndefinePreprocessorDefinitions)
    SUPPORT_COMPILER_OPTION(AdditionalOptions)
    SUPPORT_COMPILER_OPTION(GlobalOptimizations)
    SUPPORT_COMPILER_OPTION(FavorSizeOrSpeed)
    SUPPORT_COMPILER_OPTION(BrowseInformation)

private:
    string                      m_AdditionalIncludeDirectories;
    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    string                      m_RuntimeLibraryOption;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;
    SConfigInfo                 m_Config;
    list<string>                m_Defines;
    CProjKey                    m_ProjectId;
    list<CProjKey>              m_Depends;

    TTargetType                 m_TargetType;

    // No value-type semantics
    CCompilerToolImpl(void);
    CCompilerToolImpl(const CCompilerToolImpl&);
    CCompilerToolImpl& operator= (const CCompilerToolImpl&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLinkerToolImpl --
///
/// Implementation of ILinkerTool interface.
///
/// Accepts trait classes as a template parameters.

template <class ConfTrait > 
class CLinkerToolImpl : public ILinkerTool
{
public:
    CLinkerToolImpl(const string& additional_options,
                    const string& additional_library_directories,
                    const string& project_id,
                    const CMsvcProjectMakefile& project_makefile,
                    const CMsvcMetaMakefile&    meta_makefile,
                    const SConfigInfo&          config)
	    :m_AdditionalOptions    (additional_options),
         m_AdditionalLibraryDirectories(additional_library_directories),
		 m_ProjectId            (project_id),
         m_MsvcProjectMakefile  (project_makefile),
         m_MsvcMetaMakefile     (meta_makefile),
         m_Config               (config)
    {
    }
    virtual string Name(void) const
    {
	    return "VCLinkerTool";
    }
    virtual string AdditionalOptions(void) const
    {
	    return m_AdditionalOptions + " " +
               GetLinkerOpt(m_MsvcMetaMakefile,
                            m_MsvcProjectMakefile,
                            "AdditionalOptions", 
                            m_Config );
    }
    virtual string OutputFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectId + ConfTrait::TargetExtension();
    }

#define SUPPORT_LINKER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetLinkerOpt(m_MsvcMetaMakefile, \
                            m_MsvcProjectMakefile, \
                            #opt, \
                            m_Config ); \
    }
    
    SUPPORT_LINKER_OPTION(LinkIncremental)
    SUPPORT_LINKER_OPTION(GenerateDebugInformation)

    virtual string ProgramDatabaseFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectId + ".pdb";
    }

    SUPPORT_LINKER_OPTION(SubSystem)
    
    virtual string ImportLibrary(void) const
    {
	    return string("$(OutDir)/") + m_ProjectId + ".lib";
    }

    SUPPORT_LINKER_OPTION(TargetMachine)
    SUPPORT_LINKER_OPTION(OptimizeReferences)
    SUPPORT_LINKER_OPTION(EnableCOMDATFolding)
    SUPPORT_LINKER_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_LINKER_OPTION(IgnoreDefaultLibraryNames)

    virtual string AdditionalLibraryDirectories(void) const
    {
	    return m_AdditionalLibraryDirectories;
    }

private:
    string      m_AdditionalOptions;
    string      m_AdditionalLibraryDirectories;
    string      m_ProjectId;
    SConfigInfo m_Config;

    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;

    CLinkerToolImpl(void);
    CLinkerToolImpl(const CLinkerToolImpl&);
    CLinkerToolImpl& operator= (const CLinkerToolImpl&);
};


#define SUPPORT_DUMMY_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return ""; \
    }


/////////////////////////////////////////////////////////////////////////////
///
/// CLinkerToolDummyImpl --
///
/// Implementation of ILinkerTool interface.
///
/// Dummy (name-only) implementation.
class CLinkerToolDummyImpl : public ILinkerTool // for LIB targets:
{
public:
    CLinkerToolDummyImpl()
    {
    }
    virtual string Name(void) const
    {
	    return "VCLinkerTool";
    }
    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(OutputFile)
    SUPPORT_DUMMY_OPTION(LinkIncremental)
    SUPPORT_DUMMY_OPTION(GenerateDebugInformation)
    SUPPORT_DUMMY_OPTION(ProgramDatabaseFile)
    SUPPORT_DUMMY_OPTION(SubSystem)
    SUPPORT_DUMMY_OPTION(ImportLibrary)
    SUPPORT_DUMMY_OPTION(TargetMachine)
    SUPPORT_DUMMY_OPTION(OptimizeReferences)
    SUPPORT_DUMMY_OPTION(EnableCOMDATFolding)

    virtual string IgnoreAllDefaultLibraries(void) const
    {
        return "FALSE";
    }
    virtual string IgnoreDefaultLibraryNames(void) const
    {
        return "FALSE";
    }

    SUPPORT_DUMMY_OPTION(AdditionalLibraryDirectories)

private:
    CLinkerToolDummyImpl(const CLinkerToolDummyImpl&);
    CLinkerToolDummyImpl& operator= (const CLinkerToolDummyImpl&);
};

/////////////////////////////////////////////////////////////////////////////
///
/// CLibrarianToolImpl --
///
/// Implementation of ILibrarianTool interface.
///
/// Implementation for LIB targets.
class CLibrarianToolImpl : public ILibrarianTool
{
public:
    CLibrarianToolImpl( const string& additional_options,
                        const string& additional_library_directories,
                        const string& project_id,
                        const CMsvcProjectMakefile& project_makefile,
                        const CMsvcMetaMakefile&    meta_makefile,
                        const SConfigInfo&          config)
	    :m_AdditionalOptions    (additional_options),
         m_AdditionalLibraryDirectories(additional_library_directories),
	     m_ProjectId            (project_id),
         m_MsvcProjectMakefile  (project_makefile),
         m_MsvcMetaMakefile     (meta_makefile),
         m_Config               (config)
    {
    }
    virtual string Name(void) const
    {
	    return "VCLibrarianTool";
    }

    virtual string AdditionalOptions(void) const
    {
	    return m_AdditionalOptions;
    }
    virtual string OutputFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectId + ".lib";
    }

#define SUPPORT_LIBRARIAN_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetLinkerOpt(m_MsvcMetaMakefile, \
                            m_MsvcProjectMakefile, \
                            #opt, \
                            m_Config ); \
    }
    
    SUPPORT_LIBRARIAN_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_LIBRARIAN_OPTION(IgnoreDefaultLibraryNames)

    virtual string AdditionalLibraryDirectories(void) const
    {
	    return m_AdditionalLibraryDirectories;
    }

private:
    string      m_AdditionalOptions;
    string      m_AdditionalLibraryDirectories;
    string      m_ProjectId;
    SConfigInfo m_Config;
   
    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;

    CLibrarianToolImpl(void);
    CLibrarianToolImpl(const CLibrarianToolImpl&);
    CLibrarianToolImpl& operator= (const CLibrarianToolImpl&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLibrarianToolDummyImpl --
///
/// Implementation of ILibrarianTool interface.
///
/// Dummy (name-only) implementation for APP and DLL targets.

class CLibrarianToolDummyImpl : public ILibrarianTool // for APP and DLL
{
public:
    CLibrarianToolDummyImpl(void)
    {
    }

    virtual string Name(void) const
    {
	    return "VCLibrarianTool";
    }

    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(OutputFile)
    SUPPORT_DUMMY_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_DUMMY_OPTION(IgnoreDefaultLibraryNames)
    SUPPORT_DUMMY_OPTION(AdditionalLibraryDirectories)

private:
	CLibrarianToolDummyImpl(const CLibrarianToolDummyImpl&);
	CLibrarianToolDummyImpl& operator= (const CLibrarianToolDummyImpl&);
};


class CPreBuildEventToolDummyImpl : public IPreBuildEventTool // for APP and DLL
{
public:
    CPreBuildEventToolDummyImpl(void)
    {
    }

    virtual string Name(void) const
    {
	    return "VCPreBuildEventTool";
    }

    SUPPORT_DUMMY_OPTION(CommandLine)

private:
	CPreBuildEventToolDummyImpl(const CPreBuildEventToolDummyImpl&);
	CPreBuildEventToolDummyImpl& operator= (const CPreBuildEventToolDummyImpl&);
};


class CPreBuildEventToolLibImpl : public IPreBuildEventTool // for LIB
{
public:
    CPreBuildEventToolLibImpl(const list<string>& lib_depends)
        :m_LibDepends(lib_depends)
    {
    }

    virtual string Name(void) const
    {
	    return "VCPreBuildEventTool";
    }

    virtual string CommandLine(void) const
    {
        string command_line;
        if ( !m_LibDepends.empty() ) {
            command_line += "@echo on\n";
        }
        ITERATE(list<string>, p, m_LibDepends)
        {
            const string& lib = *p;
            
            command_line += "IF EXIST $(OutDir)\\" + lib;
            command_line += " GOTO HAVE_" + lib + "\n";

            command_line += "devenv /build $(ConfigurationName) /project ";
            command_line += lib;
            command_line += " \"$(SolutionPath)\"\n";

            command_line += ":HAVE_" + lib + "\n";
        }
        return command_line;
    }

private:
    list<string> m_LibDepends;

	CPreBuildEventToolLibImpl(const CPreBuildEventToolLibImpl&);
	CPreBuildEventToolLibImpl& operator= (const CPreBuildEventToolLibImpl&);
};


/// Dummy (name-only) tool implementations.

#define DEFINE_NAME_ONLY_DUMMY_TOOL(C,I,N)\
class C : public I\
{\
public:\
    C()\
    {\
    }\
    virtual string Name(void) const\
    {\
	    return N;\
    }\
private:\
    C(const C&);\
    C& operator= (const C&);\
};

DEFINE_NAME_ONLY_DUMMY_TOOL(CCustomBuildToolDummyImpl,
                            ICustomBuildTool, 
                            "VCCustomBuildTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CMIDLToolDummyImpl, 
                            IMIDLTool, 
                            "VCMIDLTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CPostBuildEventToolDummyImpl,
                            IPostBuildEventTool, 
                            "VCPostBuildEventTool"); 
#if 0
DEFINE_NAME_ONLY_DUMMY_TOOL(CPreBuildEventToolDummyImpl,
                            IPreBuildEventTool, 
                            "VCPreBuildEventTool"); 
#endif

DEFINE_NAME_ONLY_DUMMY_TOOL(CPreLinkEventToolDummyImpl,
                            IPreLinkEventTool, 
                            "VCPreLinkEventTool");


/////////////////////////////////////////////////////////////////////////////
///
/// CResourceCompilerToolImpl --
///
/// Implementation of IResourceCompilerTool interface.
///
/// Accepts traits as a template parameter.

template <class DebugReleaseTrait>
class CResourceCompilerToolImpl : public IResourceCompilerTool
{
public:
    CResourceCompilerToolImpl(const string&               additional_include_dirs,
                              const CMsvcProjectMakefile& project_makefile,
                              const CMsvcMetaMakefile&    meta_makefile,
                              const SConfigInfo&          config)
      :m_AdditionalIncludeDirectories(additional_include_dirs),
       m_MsvcProjectMakefile(project_makefile),
       m_MsvcMetaMakefile   (meta_makefile),
       m_Config             (config)
    {
    }
    virtual string Name(void) const
    {
	    return "VCResourceCompilerTool";
    }

    virtual string AdditionalIncludeDirectories(void) const
    {
	    return m_AdditionalIncludeDirectories;
    }

#define SUPPORT_RESOURCE_COMPILER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetResourceCompilerOpt(m_MsvcMetaMakefile, \
                                      m_MsvcProjectMakefile, \
                                      #opt, \
                                      m_Config ); \
    }
    
    SUPPORT_RESOURCE_COMPILER_OPTION(AdditionalOptions)
    SUPPORT_RESOURCE_COMPILER_OPTION(Culture)


    virtual string PreprocessorDefinitions(void) const
    {
	    return DebugReleaseTrait::PreprocessorDefinitions();
    }

private:
    string m_AdditionalIncludeDirectories;
    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;
    const SConfigInfo&          m_Config;

    CResourceCompilerToolImpl(const CResourceCompilerToolImpl&);
    CResourceCompilerToolImpl& operator= (const CResourceCompilerToolImpl&);

};


/////////////////////////////////////////////////////////////////////////////
///
/// CResourceCompilerToolImpl --
///
/// Implementation of IResourceCompilerTool interface.
///
/// Dummy (name-only) implementation.

class CResourceCompilerToolDummyImpl : public IResourceCompilerTool //no resources
{
public:
    CResourceCompilerToolDummyImpl()
    {
    }
    virtual string Name(void) const
    {
        return "VCResourceCompilerTool";
    }

    SUPPORT_DUMMY_OPTION(AdditionalIncludeDirectories)
    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(Culture)
    SUPPORT_DUMMY_OPTION(PreprocessorDefinitions)

private:
    CResourceCompilerToolDummyImpl
        (const CResourceCompilerToolDummyImpl&);
    CResourceCompilerToolDummyImpl& operator= 
        (const CResourceCompilerToolDummyImpl&);
};


/// Dummy (name-only) tool implementations.

DEFINE_NAME_ONLY_DUMMY_TOOL(CWebServiceProxyGeneratorToolDummyImpl,
                            IWebServiceProxyGeneratorTool, 
                            "VCWebServiceProxyGeneratorTool");

DEFINE_NAME_ONLY_DUMMY_TOOL(CXMLDataGeneratorToolDummyImpl,
                            IXMLDataGeneratorTool, 
                            "VCXMLDataGeneratorTool");

DEFINE_NAME_ONLY_DUMMY_TOOL(CManagedWrapperGeneratorToolDummyImpl,
                            IManagedWrapperGeneratorTool, 
                            "VCManagedWrapperGeneratorTool");

DEFINE_NAME_ONLY_DUMMY_TOOL(CAuxiliaryManagedWrapperGeneratorToolDummyImpl,
                            IAuxiliaryManagedWrapperGeneratorTool, 
                            "VCAuxiliaryManagedWrapperGeneratorTool");



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/03/08 23:30:29  gorelenk
 * Added static helper function s_GetLibsPrefixesPreprocessorDefinitions.
 * Changed declaration of class CCompilerToolImpl.
 *
 * Revision 1.8  2004/02/23 20:43:43  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.7  2004/02/20 22:54:46  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.6  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.5  2004/01/28 17:55:06  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.4  2004/01/26 19:25:42  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.3  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // TOOLS_IMPLEMENT_HEADER
