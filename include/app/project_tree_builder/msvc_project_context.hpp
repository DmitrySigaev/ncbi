#ifndef PROJECT_CONTEXT_HEADER
#define PROJECT_CONTEXT_HEADER

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

#include<list>
#include<string>
#include<map>

#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>

#include <corelib/ncbienv.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcPrjProjectContext --
///
/// Abstraction of MSVC project-specific context.
///
/// Project context provides project-specific information for 
/// project generation.

class CMsvcPrjProjectContext
{
public:
    //no value type	semantics
    CMsvcPrjProjectContext(const CProjItem& project);

    //Compiler::General
    string AdditionalIncludeDirectories(void) const
    {
        return m_AdditionalIncludeDirectories;
    }

    //Linker::General
    string ProjectName(void) const
    {
        return m_ProjectName;
    }

    string AdditionalLinkerOptions(void) const
    {
        return m_AdditionalLinkerOptions;
    }


    string AdditionalLibrarianOptions(void) const
    {
        return m_AdditionalLibrarianOptions;
    }

    string ProjectDir(void) const
    {
        return m_ProjectDir;
    }

    CProjItem::TProjType ProjectType(void) const
    {
        return m_ProjType;
    }

    const list<string>& SourcesDirsAbs(void) const
    {
        return m_SourcesDirsAbs;
    }
    const string& IncludeDirsRoot(void) const
    {
        return m_IncludeDirsRoot;
    }
    const list<string>& IncludeDirsAbs(void) const
    {
        return m_IncludeDirsAbs;
    }

    const CMsvcProjectMakefile& GetMsvcProjectMakefile(void) const;

private:
    // Prohibited to:
    CMsvcPrjProjectContext(void);
    CMsvcPrjProjectContext(const CMsvcPrjProjectContext&);
    CMsvcPrjProjectContext&	operator= (const CMsvcPrjProjectContext&);


    string m_ProjectName;

    string m_AdditionalIncludeDirectories;
    string m_AdditionalLinkerOptions;
    string m_AdditionalLibrarianOptions;

    string m_ProjectDir;

    CProjItem::TProjType m_ProjType;

    list<string> m_SourcesDirsAbs;

    string       m_IncludeDirsRoot;
    list<string> m_IncludeDirsAbs;

    auto_ptr<CMsvcProjectMakefile> m_MsvcProjectMakefile;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcPrjGeneralContext --
///
/// Abstraction of MSVC project "general" context.
///
/// Project context provides specific information for particular type of 
/// project (Exe, Lib, Dll). This information not project-specific, but 
/// more project type specific.

class CMsvcPrjGeneralContext
{
public:
    //no value type semantics
    CMsvcPrjGeneralContext(const SConfigInfo& config, 
                           const CMsvcPrjProjectContext& prj_context);

    typedef enum { 
        eExe, 
        eLib, 
        eDll, 
        eOther } TTargetType;
    TTargetType m_Type;

    SConfigInfo m_Config;

    //Configuration::General
    string OutputDirectory(void) const
    {
        return m_OutputDirectory;
    }

    string ConfigurationName(void) const
    {
        return m_Config.m_Name;
    }

    const CMsvcMetaMakefile& GetMsvcMetaMakefile(void) const
    {
        return m_MsvcMetaMakefile;
    }

private:
    /// Prohibited to:
    CMsvcPrjGeneralContext(void);
    CMsvcPrjGeneralContext(const CMsvcPrjGeneralContext&);
    CMsvcPrjGeneralContext& operator= (const CMsvcPrjGeneralContext&);

    string m_OutputDirectory;
    string m_ConfigurationName;

    const CMsvcMetaMakefile& m_MsvcMetaMakefile;
};

/////////////////////////////////////////////////////////////////////////////
///
/// ITool --
///
/// Base class for all MSVC tools.
///
/// Polymorphic hierarchy base class.

struct ITool
{
    virtual string Name(void) const = 0;

    virtual ~ITool(void)
    {
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// IConfiguration --
///
/// "Configuration" interface.
///
/// Abstract class.

struct IConfiguration : public ITool
{
    //Name : Configuration+'|'+"Win32"
    virtual string OutputDirectory(void)		const = 0;
    virtual string IntermediateDirectory(void)	const = 0;
    virtual string ConfigurationType(void)		const = 0;
    virtual string CharacterSet(void)			const = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
/// ICompilerTool --
///
/// "CompilerTool" interface.
///
/// Abstract class.

struct ICompilerTool : public ITool
{
    //string Name : "VCCLCompilerTool"
    virtual string Optimization(void)					const = 0;
    virtual string AdditionalIncludeDirectories(void)	const = 0;

    // Preprocessor definition:
    // LIB (Debug)		WIN32;_DEBUG;_LIB
    // LIB (Release)	WIN32;NDEBUG;_LIB
    // APP (Debug)		WIN32;_DEBUG;_CONSOLE
    // APP (Release)	WIN32;NDEBUG;_CONSOLE
    // DLL (Debug)		WIN32;_DEBUG;_WINDOWS;_USRDLL;XNCBI_EXPORTS 
    //                                    (project_name(uppercase)+"_EXPORTS")

    // DLL (Relese)		WIN32;NDEBUG;_WINDOWS;_USRDLL;XNCBI_EXPORTS
    //                                    (project_name(uppercase)+"_EXPORTS")

    // All configurations:
    virtual string PreprocessorDefinitions(void)		 const = 0;

    virtual string MinimalRebuild(void)					 const = 0;
    virtual string BasicRuntimeChecks(void)				 const = 0;
    virtual string RuntimeLibrary(void)					 const = 0;
    virtual string RuntimeTypeInfo(void)                 const = 0;
    virtual string UsePrecompiledHeader(void)			 const = 0;
    virtual string WarningLevel(void)					 const = 0;
    virtual string Detect64BitPortabilityProblems(void)  const = 0;
    virtual string DebugInformationFormat(void)			 const = 0;
    virtual string CompileAs(void)                       const = 0;

    //For release - EXE DLL LIB
    virtual string InlineFunctionExpansion(void)		 const = 0;
    virtual string OmitFramePointers(void)				 const = 0;
    virtual string StringPooling(void)					 const = 0;
    virtual string EnableFunctionLevelLinking(void)		 const = 0;

    //Latest additions
    virtual string OptimizeForProcessor(void)		     const = 0;
    virtual string StructMemberAlignment(void)		     const = 0; 
    virtual string CallingConvention(void)		         const = 0;
    virtual string IgnoreStandardIncludePath(void)	     const = 0;
    virtual string ExceptionHandling(void)		         const = 0;
    virtual string BufferSecurityCheck(void)		     const = 0;
    virtual string DisableSpecificWarnings(void)		 const = 0;
    virtual string UndefinePreprocessorDefinitions(void) const = 0;
    virtual string AdditionalOptions(void)		         const = 0;

    virtual string GlobalOptimizations(void)		     const = 0;
    virtual string FavorSizeOrSpeed(void)		         const = 0;
    virtual string BrowseInformation(void)		         const = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
/// ILinkerTool --
///
/// "LinkerTool" interface.
///
/// Abstract class.

struct ILinkerTool : public ITool
{
    //string Name : "VCLinkerTool"
    virtual string AdditionalOptions(void)			  const = 0;
    virtual string OutputFile(void)					  const = 0;
    virtual string LinkIncremental(void)			  const = 0;
    virtual string GenerateDebugInformation(void)	  const = 0;
    virtual string ProgramDatabaseFile(void)		  const = 0;
    virtual string SubSystem(void)					  const = 0;
    virtual string ImportLibrary(void)				  const = 0;
    virtual string TargetMachine(void)				  const = 0;

    //release - EXE DLL +=
    virtual string OptimizeReferences(void)			  const = 0;
    virtual string EnableCOMDATFolding(void)		  const = 0;

    //Latest additions
    virtual string IgnoreAllDefaultLibraries(void)	  const = 0;
    virtual string IgnoreDefaultLibraryNames(void)	  const = 0;
    virtual string AdditionalLibraryDirectories(void) const = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
/// ILibrarianTool --
///
/// "LibrarianTool" interface.
///
/// Abstract class.

struct ILibrarianTool : public ITool
{
    //string Name : "VCLibrarianTool"
    virtual string AdditionalOptions(void)			  const = 0;
    virtual string OutputFile(void)					  const = 0;
    virtual string IgnoreAllDefaultLibraries(void)	  const = 0;
    virtual string IgnoreDefaultLibraryNames(void)	  const = 0;
    
    //Latest additions
    virtual string AdditionalLibraryDirectories(void) const = 0;
};

/// Dummies - Name - only tools. Must present in msvc proj:
struct ICustomBuildTool : public ITool
{
    //Name : "VCCustomBuildTool"
};
struct IMIDLTool : public ITool
{
    //Name : "VCMIDLTool"
};
struct IPostBuildEventTool : public ITool
{
    //Name : "VCPostBuildEventTool"
};
struct IPreBuildEventTool : public ITool
{
    //Name : "VCPreBuildEventTool"
};
struct IPreLinkEventTool : public ITool
{
    //Name : "VCPreLinkEventTool"
};


/////////////////////////////////////////////////////////////////////////////
///
/// IResourceCompilerTool --
///
/// "ResourceCompilerTool" interface.
///
/// Abstract class.

struct IResourceCompilerTool : public ITool
{
    //Name : "VCResourceCompilerTool"
    //must be implemented for Dll
    virtual string PreprocessorDefinitions(void)		const = 0;
    virtual string Culture(void)						const = 0;
    virtual string AdditionalIncludeDirectories(void)	const = 0;
};


/// Dummies - Name - only tools. Must present in msvc proj:
struct IWebServiceProxyGeneratorTool : public ITool
{
    //Name : "VCWebServiceProxyGeneratorTool"
};
struct IXMLDataGeneratorTool : public ITool
{
    //Name : "VCXMLDataGeneratorTool"
};
struct IManagedWrapperGeneratorTool : public ITool
{
    //Name : "VCManagedWrapperGeneratorTool"
};
struct IAuxiliaryManagedWrapperGeneratorTool : public ITool
{
    //Name : "VCAuxiliaryManagedWrapperGeneratorTool"
};

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcTools --
///
/// Abstraction of MSVC 7.10 C++ project build tools.
///
/// Provides all tools interfaces. Tools implementation will depends upon
/// project general and project specific contexts.

class CMsvcTools
{
public:
    CMsvcTools(const CMsvcPrjGeneralContext& general_context,
               const CMsvcPrjProjectContext& project_context);

    ~CMsvcTools();

    IConfiguration      * Configuration	(void) const;
    ICompilerTool       * Compiler      (void) const;
    ILinkerTool         * Linker        (void) const;
    ILibrarianTool      * Librarian     (void) const;
    ICustomBuildTool	* CustomBuid    (void) const;
    IMIDLTool			* MIDL		    (void) const;
    IPostBuildEventTool * PostBuildEvent(void) const;
    IPreBuildEventTool	* PreBuildEvent (void) const;
    IPreLinkEventTool	* PreLinkEvent  (void) const;
    
    IResourceCompilerTool *
                          ResourceCompiler          (void) const;

    IWebServiceProxyGeneratorTool *
                          WebServiceProxyGenerator	(void) const;

    IXMLDataGeneratorTool *
                          XMLDataGenerator          (void) const;

    IManagedWrapperGeneratorTool *
                          ManagedWrapperGenerator	(void) const;

    IAuxiliaryManagedWrapperGeneratorTool *
                          AuxiliaryManagedWrapperGenerator(void) const;

private:
    auto_ptr<IConfiguration>        m_Configuration;
    auto_ptr<ICompilerTool>         m_Compiler;
    auto_ptr<ILinkerTool>           m_Linker;
    auto_ptr<ILibrarianTool>        m_Librarian;
    
    auto_ptr<ICustomBuildTool>      m_CustomBuid;
    auto_ptr<IMIDLTool>             m_MIDL;
    auto_ptr<IPostBuildEventTool>	m_PostBuildEvent;
    auto_ptr<IPreBuildEventTool>	m_PreBuildEvent;
    auto_ptr<IPreLinkEventTool>		m_PreLinkEvent;
    
    auto_ptr<IResourceCompilerTool>	m_ResourceCompiler;

    auto_ptr<IWebServiceProxyGeneratorTool>	
                                    m_WebServiceProxyGenerator;
    
    auto_ptr<IXMLDataGeneratorTool>	m_XMLDataGenerator;
    
    auto_ptr<IManagedWrapperGeneratorTool>	
                                    m_ManagedWrapperGenerator;
    
    auto_ptr<IAuxiliaryManagedWrapperGeneratorTool> 
                                    m_AuxiliaryManagedWrapperGenerator;

    //this is not a value-type class
    CMsvcTools();
    CMsvcTools(const CMsvcTools&);
    CMsvcTools& operator= (const CMsvcTools&);
};



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/01/26 19:25:41  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.3  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // PROJECT_CONTEXT_HEADER