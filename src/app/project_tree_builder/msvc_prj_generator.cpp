#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_project_context.hpp>


#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/msvc_prj_files_collector.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


static 
void s_CreateDatatoolCustomBuildInfo(const CProjItem&              prj,
                                     const CMsvcPrjProjectContext& context,
                                     const CDataToolGeneratedSrc&  src,                                   
                                     SCustomBuildInfo*             build_info);

//-----------------------------------------------------------------------------
CMsvcProjectGenerator::CMsvcProjectGenerator(const list<SConfigInfo>& configs)
    :m_Configs(configs)
{
}


CMsvcProjectGenerator::~CMsvcProjectGenerator(void)
{
}


bool CMsvcProjectGenerator::Generate(const CProjItem& prj)
{
    CMsvcPrjProjectContext project_context(prj);
    CVisualStudioProject xmlprj;
    
    {{
        // Attributes:
        xmlprj.SetAttlist().SetProjectType (MSVC_PROJECT_PROJECT_TYPE);
        xmlprj.SetAttlist().SetVersion     (MSVC_PROJECT_VERSION);
        xmlprj.SetAttlist().SetName        (project_context.ProjectName());
        xmlprj.SetAttlist().SetKeyword     (MSVC_PROJECT_KEYWORD_WIN32);
    }}
    
    {{
        // Platforms
         CRef<CPlatform> platform(new CPlatform(""));
         platform->SetAttlist().SetName(MSVC_PROJECT_PLATFORM);
         xmlprj.SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<SConfigInfo>, p , m_Configs) {

        const SConfigInfo& cfg_info = *p;
        // Check config availability
        if ( !project_context.IsConfigEnabled(cfg_info) ) {
            LOG_POST(Info << "Configuration "
                          << cfg_info.m_Name
                          << " disabled in project "
                          << project_context.ProjectId());
            continue;
        }

        // Contexts:
        
        CMsvcPrjGeneralContext general_context(cfg_info, project_context);

        // MSVC Tools
        CMsvcTools msvc_tool(general_context, project_context);

        CRef<CConfiguration> conf(new CConfiguration());

#define BIND_TOOLS(tool, msvctool, X) \
                  tool->SetAttlist().Set##X(msvctool->##X())

        {{
            //Configuration

            //Name
            BIND_TOOLS(conf, msvc_tool.Configuration(), Name);
            
            //OutputDirectory
            BIND_TOOLS(conf, msvc_tool.Configuration(), OutputDirectory);

            //IntermediateDirectory
            BIND_TOOLS(conf, msvc_tool.Configuration(), IntermediateDirectory);

            //ConfigurationType
            BIND_TOOLS(conf, msvc_tool.Configuration(), ConfigurationType);

            //CharacterSet
            BIND_TOOLS(conf, msvc_tool.Configuration(), CharacterSet);
        }}

        
        {{
            //Compiler
            CRef<CTool> tool(new CTool("")); 

            //Name
            BIND_TOOLS(tool, msvc_tool.Compiler(), Name);
            
            //Optimization
            BIND_TOOLS(tool, msvc_tool.Compiler(), Optimization);

            //AdditionalIncludeDirectories - more dirs are coming from makefile
            BIND_TOOLS(tool, 
                       msvc_tool.Compiler(), AdditionalIncludeDirectories);

            //PreprocessorDefinitions
            BIND_TOOLS(tool, msvc_tool.Compiler(), PreprocessorDefinitions);
            
            //MinimalRebuild
            BIND_TOOLS(tool, msvc_tool.Compiler(), MinimalRebuild);

            //BasicRuntimeChecks
            BIND_TOOLS(tool, msvc_tool.Compiler(), BasicRuntimeChecks);

            //RuntimeLibrary
            BIND_TOOLS(tool, msvc_tool.Compiler(), RuntimeLibrary);

            //RuntimeTypeInfo
            BIND_TOOLS(tool, msvc_tool.Compiler(), RuntimeTypeInfo);

            //UsePrecompiledHeader
            BIND_TOOLS(tool, msvc_tool.Compiler(), UsePrecompiledHeader);

            //WarningLevel
            BIND_TOOLS(tool, msvc_tool.Compiler(), WarningLevel);

            //Detect64BitPortabilityProblems
            BIND_TOOLS(tool, 
                       msvc_tool.Compiler(), Detect64BitPortabilityProblems);

            //DebugInformationFormat
            BIND_TOOLS(tool, msvc_tool.Compiler(), DebugInformationFormat);

            //CompileAs
            BIND_TOOLS(tool, msvc_tool.Compiler(), CompileAs);
            
            //InlineFunctionExpansion
            BIND_TOOLS(tool, msvc_tool.Compiler(), InlineFunctionExpansion);

            //OmitFramePointers
            BIND_TOOLS(tool, msvc_tool.Compiler(), OmitFramePointers);

            //StringPooling
            BIND_TOOLS(tool, msvc_tool.Compiler(), StringPooling);

            //EnableFunctionLevelLinking
            BIND_TOOLS(tool, msvc_tool.Compiler(), EnableFunctionLevelLinking);

            //OptimizeForProcessor
            BIND_TOOLS(tool, msvc_tool.Compiler(), OptimizeForProcessor);
            
            //StructMemberAlignment 
            BIND_TOOLS(tool, msvc_tool.Compiler(), StructMemberAlignment);
            
            //CallingConvention
            BIND_TOOLS(tool, msvc_tool.Compiler(), CallingConvention);
            
            //IgnoreStandardIncludePath
            BIND_TOOLS(tool, msvc_tool.Compiler(), IgnoreStandardIncludePath);
            
            //ExceptionHandling
            BIND_TOOLS(tool, msvc_tool.Compiler(), ExceptionHandling);
            
            //BufferSecurityCheck
            BIND_TOOLS(tool, msvc_tool.Compiler(), BufferSecurityCheck);
            
            //DisableSpecificWarnings
            BIND_TOOLS(tool, msvc_tool.Compiler(), DisableSpecificWarnings);
            
            //UndefinePreprocessorDefinitions
            BIND_TOOLS(tool, 
                       msvc_tool.Compiler(), UndefinePreprocessorDefinitions);
            
            //AdditionalOptions
            BIND_TOOLS(tool, msvc_tool.Compiler(), AdditionalOptions);
            
            //GlobalOptimizations
            BIND_TOOLS(tool, msvc_tool.Compiler(), GlobalOptimizations);
            
            //FavorSizeOrSpeed
            BIND_TOOLS(tool, msvc_tool.Compiler(), FavorSizeOrSpeed);
            
            //BrowseInformation
            BIND_TOOLS(tool, msvc_tool.Compiler(), BrowseInformation);

            conf->SetTool().push_back(tool);
        }}

        {{
            //Linker
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.Linker(), Name);

            //AdditionalOptions
            BIND_TOOLS(tool, msvc_tool.Linker(), AdditionalOptions);
            
            //OutputFile
            BIND_TOOLS(tool, msvc_tool.Linker(), OutputFile);

            //LinkIncremental
            BIND_TOOLS(tool, msvc_tool.Linker(), LinkIncremental);
            
            //GenerateDebugInformation
            BIND_TOOLS(tool, msvc_tool.Linker(), GenerateDebugInformation);
            
            //ProgramDatabaseFile
            BIND_TOOLS(tool, msvc_tool.Linker(), ProgramDatabaseFile);

            //SubSystem
            BIND_TOOLS(tool, msvc_tool.Linker(), SubSystem);

            //ImportLibrary
            BIND_TOOLS(tool, msvc_tool.Linker(), ImportLibrary);

            //TargetMachine
            BIND_TOOLS(tool, msvc_tool.Linker(), TargetMachine);

            //OptimizeReferences
            BIND_TOOLS(tool, msvc_tool.Linker(), OptimizeReferences);

            //EnableCOMDATFolding
            BIND_TOOLS(tool, msvc_tool.Linker(), EnableCOMDATFolding);

            //IgnoreAllDefaultLibraries
            BIND_TOOLS(tool, msvc_tool.Linker(), IgnoreAllDefaultLibraries);
            
            //IgnoreDefaultLibraryNames
            BIND_TOOLS(tool, msvc_tool.Linker(), IgnoreDefaultLibraryNames);
            
            //AdditionalLibraryDirectories
            BIND_TOOLS(tool, msvc_tool.Linker(), AdditionalLibraryDirectories);

            conf->SetTool().push_back(tool);
        }}

        {{
            //Librarian
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.Librarian(), Name);

            //AdditionalOptions
            BIND_TOOLS(tool, msvc_tool.Librarian(), AdditionalOptions);

            //OutputFile
            BIND_TOOLS(tool, msvc_tool.Librarian(), OutputFile);

            //IgnoreAllDefaultLibraries
            BIND_TOOLS(tool, msvc_tool.Librarian(), IgnoreAllDefaultLibraries);

            //IgnoreDefaultLibraryNames
            BIND_TOOLS(tool, msvc_tool.Librarian(), IgnoreDefaultLibraryNames);

            //AdditionalLibraryDirectories
            BIND_TOOLS(tool, 
                       msvc_tool.Librarian(), AdditionalLibraryDirectories);

            conf->SetTool().push_back(tool);
        }}

        {{
            //CustomBuildTool
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.CustomBuid(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //MIDL
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.MIDL(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //PostBuildEvent
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.PostBuildEvent(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //PreBuildEvent
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), Name);
            //CommandLine
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), CommandLine);

            conf->SetTool().push_back(tool);
        }}
        {{
            //PreLinkEvent
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.PreLinkEvent(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //ResourceCompiler
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.ResourceCompiler(), Name);
             
            //AdditionalIncludeDirectories
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       AdditionalIncludeDirectories);

            //AdditionalOptions
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       AdditionalOptions);

            //Culture
            BIND_TOOLS(tool, msvc_tool.ResourceCompiler(), Culture);

            //PreprocessorDefinitions
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       PreprocessorDefinitions);
            


            conf->SetTool().push_back(tool);
        }}

        {{
            //WebServiceProxyGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.WebServiceProxyGenerator(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //XMLDataGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.XMLDataGenerator(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //ManagedWrapperGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, msvc_tool.ManagedWrapperGenerator(), Name);

            conf->SetTool().push_back(tool);
        }}
        {{
            //AuxiliaryManagedWrapperGenerator
            CRef<CTool> tool(new CTool(""));

            //Name
            BIND_TOOLS(tool, 
                       msvc_tool.AuxiliaryManagedWrapperGenerator(),
                       Name);

            conf->SetTool().push_back(tool);
        }}

        xmlprj.SetConfigurations().SetConfiguration().push_back(conf);
    }
    {{
        //References
        xmlprj.SetReferences("");
    }}

    // Collect all source, header, inline, resource files
    CMsvcPrjFilesCollector collector(project_context, prj);

    {{
        //Files - source files
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Source Files");
        filter->SetAttlist().SetFilter
            ("cpp;c;cxx;def;odl;idl;hpj;bat;asm;asmx");

        ITERATE(list<string>, p, collector.SourceFiles()) {
            //Include collected source files
            CRef< CFFile > file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }
        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Files - header files
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Header Files");
        filter->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

        ITERATE(list<string>, p, collector.HeaderFiles()) {
            //Include collected header files
            CRef<CFFile> file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }
        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Files - inline files
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Inline Files");
        filter->SetAttlist().SetFilter("inl");

        ITERATE(list<string>, p, collector.InlineFiles()) {
            //Include collected inline files
            CRef< CFFile > file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }
        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Resource Files - header files - empty
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Resource Files");
        filter->SetAttlist().SetFilter
            ("rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx");

        ITERATE(list<string>, p, collector.ResourceFiles()) {
            //Include collected header files
            CRef<CFFile> file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }

        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Custom Build files
        const list<SCustomBuildInfo>& info_list = 
            project_context.GetCustomBuildInfo();

        if ( !info_list.empty() ) {
            CRef<CFilter> filter(new CFilter());
            filter->SetAttlist().SetName("Custom Build Files");
            filter->SetAttlist().SetFilter("");
            
            ITERATE(list<SCustomBuildInfo>, p, info_list) { 
                const SCustomBuildInfo& build_info = *p;
                AddCustomBuildFileToFilter(filter, 
                                           m_Configs, 
                                           project_context.ProjectDir(),
                                           build_info);
            }

            xmlprj.SetFiles().SetFilter().push_back(filter);
        }
    }}
    {{
        //Datatool files
        if ( !prj.m_DatatoolSources.empty() ) {
            
            CRef<CFilter> filter(new CFilter());
            filter->SetAttlist().SetName("Datatool Files");
            filter->SetAttlist().SetFilter("");

            ITERATE(list<CDataToolGeneratedSrc>, p, prj.m_DatatoolSources) {

                const CDataToolGeneratedSrc& src = *p;
                SCustomBuildInfo build_info;
                s_CreateDatatoolCustomBuildInfo(prj, 
                                              project_context, 
                                              src, 
                                              &build_info);
                AddCustomBuildFileToFilter(filter, 
                                           m_Configs, 
                                           project_context.ProjectDir(), 
                                           build_info);
            }

            xmlprj.SetFiles().SetFilter().push_back(filter);
        }
    }}

    {{
        //Globals
        xmlprj.SetGlobals("");
    }}


    string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                project_context.ProjectName());
    project_path += MSVC_PROJECT_FILE_EXT;

    SaveIfNewer(project_path, xmlprj);

    return true;

}

static 
void s_CreateDatatoolCustomBuildInfo(const CProjItem&              prj,
                                     const CMsvcPrjProjectContext& context,
                                     const CDataToolGeneratedSrc&  src,                                   
                                     SCustomBuildInfo*             build_info)
{
    build_info->Clear();

    //SourceFile
    build_info->m_SourceFile = 
        CDirEntry::ConcatPath(src.m_SourceBaseDir, src.m_SourceFile);

    //CommandLine
    //exe location - path is supposed to be relative encoded
    string tool_exe_location("");
    if (prj.m_ProjType == CProjKey::eApp)
        tool_exe_location = GetApp().GetDatatoolPathForApp();
    else if (prj.m_ProjType == CProjKey::eLib)
        tool_exe_location = GetApp().GetDatatoolPathForLib();
    else if (prj.m_ProjType == CProjKey::eDll)
        tool_exe_location = GetApp().GetDatatoolPathForApp();
    else
        return;
    //command line
    string tool_cmd_prfx = GetApp().GetDatatoolCommandLine();
    tool_cmd_prfx += " -or ";
    tool_cmd_prfx += 
        CDirEntry::CreateRelativePath(GetApp().GetProjectTreeInfo().m_Src,
                                      src.m_SourceBaseDir);
    tool_cmd_prfx += " -oR ";
    tool_cmd_prfx += CDirEntry::CreateRelativePath(context.ProjectDir(),
                                         GetApp().GetProjectTreeInfo().m_Root);

    string tool_cmd(tool_cmd_prfx);
    if ( !src.m_ImportModules.empty() ) {
        tool_cmd += " -M \"";
        tool_cmd += NStr::Join(src.m_ImportModules, " ");
        tool_cmd += '"';
    }
    build_info->m_CommandLine = 
        "@echo on\n" + tool_exe_location + " " + tool_cmd;

    //Description
    build_info->m_Description = 
        "Using datatool to create a C++ object from ASN/DTD $(InputPath)";

    //Outputs
    string data_tool_src_base;
    CDirEntry::SplitPath(src.m_SourceFile, NULL, &data_tool_src_base);
    string outputs = "$(InputDir)" + data_tool_src_base + "__.cpp;";
    outputs += "$(InputDir)" + data_tool_src_base + "___.cpp;";
    ITERATE(list<string>, p, src.m_GeneratedCppLocal) {
        const string& src_cpp = *p;
        outputs += "$(InputDir)" + src_cpp + ".cpp;";
        outputs += "$(InputDir)" + src_cpp + "_.cpp;";
    }
    ITERATE(list<string>, p, src.m_GeneratedHpp) {
        const string& src_hpp = *p;
        string src_hpp_abs = 
            CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, 
                                  src_hpp);
        string src_hpp_rel = 
            CDirEntry::CreateRelativePath(prj.m_SourcesBaseDir, src_hpp_abs);

        outputs += "$(InputDir)" + src_hpp_rel + ".hpp;";
        outputs += "$(InputDir)" + src_hpp_rel + "_.hpp;";
    }
    build_info->m_Outputs = outputs;

    //Additional Dependencies
    //build_info->m_AdditionalDependencies = tool_exe_location;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2004/03/15 22:22:01  gorelenk
 * Changed implementation of s_CreateDatatoolCustomBuildInfo.
 *
 * Revision 1.26  2004/03/10 16:39:56  gorelenk
 * Changed LOG_POST inside CMsvcProjectGenerator::Generate.
 *
 * Revision 1.25  2004/03/08 23:35:04  gorelenk
 * Changed implementation of CMsvcProjectGenerator::Generate.
 *
 * Revision 1.24  2004/03/05 20:31:54  gorelenk
 * Files collecting functionality was moved to separate class :
 * CMsvcPrjFilesCollector.
 *
 * Revision 1.23  2004/03/05 18:07:22  gorelenk
 * Changed implementation of s_CreateDatatoolCustomBuildInfo .
 *
 * Revision 1.22  2004/02/25 20:19:36  gorelenk
 * Changed implementation of function s_IsInsideDatatoolSourceDir.
 *
 * Revision 1.21  2004/02/25 19:42:35  gorelenk
 * Changed implementation of function s_IsProducedByDatatool.
 *
 * Revision 1.20  2004/02/24 21:03:06  gorelenk
 * Added checking of config availability to implementation of
 * member-function Generate of class CMsvcProjectGenerator.
 *
 * Revision 1.19  2004/02/23 20:42:57  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.18  2004/02/20 22:53:26  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.17  2004/02/13 23:11:10  gorelenk
 * Changed implementation of function s_CreateDatatoolCustomBuildInfo - added
 * list of output files to generated SCustomBuildInfo.
 *
 * Revision 1.16  2004/02/13 20:31:20  gorelenk
 * Changed source relative path for datatool input files.
 *
 * Revision 1.15  2004/02/12 16:27:57  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.14  2004/02/10 18:09:12  gorelenk
 * Added definitions of functions SaveIfNewer and PromoteIfDifferent
 * - support for file overwriting only if it was changed.
 *
 * Revision 1.13  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.12  2004/02/05 00:02:08  gorelenk
 * Added support of user site and  Makefile defines.
 *
 * Revision 1.11  2004/02/03 17:20:47  gorelenk
 * Changed implementation of method Generate of class CMsvcProjectGenerator.
 *
 * Revision 1.10  2004/01/30 20:46:55  gorelenk
 * Added support of ASN projects.
 *
 * Revision 1.9  2004/01/29 15:48:58  gorelenk
 * Support of projects fitering transfered to CProjBulderApp class
 *
 * Revision 1.8  2004/01/28 17:55:48  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.7  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.6  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
