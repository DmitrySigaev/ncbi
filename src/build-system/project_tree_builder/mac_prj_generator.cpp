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
 * Author:  Andrei Gourianov
 *
 */

#include <ncbi_pch.hpp>

#include "mac_prj_generator.hpp"

#include "proj_builder_app.hpp"
#include "msvc_project_context.hpp"


#include "msvc_prj_utils.hpp"
#include "msvc_prj_defines.hpp"
#include "msvc_prj_files_collector.hpp"
#include "ptb_err_codes.hpp"

#include <algorithm>

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
#  include <serial/objostrxml.hpp>
#  include <serial/serial.hpp>
#endif

BEGIN_NCBI_SCOPE

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)

// 
// 1 - use human-friendly names (still works with XCode, but generates lots of warnings)
// 0 - use XCode-friendly hexadecimal ids
#define USE_VERBOSE_NAMES 0

/////////////////////////////////////////////////////////////////////////////

bool s_ProjId_less(const CProjItem* x, const CProjItem* y)
{
    return NStr::CompareNocase(
        CMacProjectGenerator::GetProjId(*x),
        CMacProjectGenerator::GetProjId(*y)) < 0;
}

bool s_ProjItem_less(const CProjItem& x, const CProjItem& y)
{
    ITERATE( list<CProjKey>, i, x.m_Depends) {
        if (y.m_ID == i->Id()) {
            return true;
        }
    }
    return false;
}

bool s_String_less(const CRef<CArray::C_E>& x, const CRef<CArray::C_E>& y)
{
    return NStr::CompareNocase(x->GetString(), y->GetString()) < 0;
}


/////////////////////////////////////////////////////////////////////////////

CMacProjectGenerator::CMacProjectGenerator(
    const list<SConfigInfo>& configs, const CProjectItemsTree& projects_tree)
    :m_Configs(configs), m_Projects_tree(projects_tree)
{
}

CMacProjectGenerator::~CMacProjectGenerator(void)
{
}

void CMacProjectGenerator::Generate(const string& solution)
{
    m_SolutionDir = CDirEntry::IsAbsolutePath(solution) ? 
        solution : CDirEntry::ConcatPath( CDir::GetCwd(), solution);
    string solution_name = CDirEntry(m_SolutionDir).GetName();
    m_SolutionDir = CDirEntry(m_SolutionDir).GetDir(CDirEntry::eIfEmptyPath_Empty);

    m_OutputDir = CDirEntry::AddTrailingPathSeparator(
        CDirEntry::ConcatPath( CDirEntry::ConcatPath(
            GetApp().GetProjectTreeInfo().m_Compilers,
            GetApp().GetRegSettings().m_CompilersSubdir),
            GetApp().GetBuildType().GetTypeStr()));
    m_OutputDir = GetRelativePath(m_OutputDir);
    
    CRef<CPlist> xproj( new CPlist);
    CDict& dict_root = xproj->SetPlistObject().SetDict();
    
    xproj->SetAttlist().SetVersion("1.0");
    AddString( dict_root, "archiveVersion", "1");
// 39  42  44
    AddString( dict_root, "objectVersion", GetApp().GetRegSettings().m_Version);
    CRef<CDict> dict_objects( AddDict( dict_root, "objects"));
    string configs_root( CreateBuildConfigurations( *dict_objects));

    CRef<CArray> file_groups( new CArray);
    CRef<CArray> targets( new CArray);
    CRef<CArray> all_dependencies( new CArray);
    CRef<CArray> app_dependencies( new CArray);
    CRef<CArray> lib_dependencies( new CArray);
    
    // set GUIDs
    list<const CProjItem*> all_projects;
    ITERATE(CProjectItemsTree::TProjects, p, m_Projects_tree.m_Projects) {
        const CProjItem& prj(p->second);
        prj.m_GUID = GetUUID();
        all_projects.push_back(&prj);
    }
    all_projects.sort(s_ProjId_less);
    ITERATE(list<const CProjItem*>, p, all_projects) {
        const CProjItem& prj(**p);

#if USE_VERBOSE_NAMES
        string proj_dependency(GetProjDependency(prj));
#else
        string proj_dependency(prj.m_GUID);
#endif
        string explicit_type( GetExplicitType( prj));

        CProjectFileCollector prj_files( prj, m_Configs, m_SolutionDir+m_OutputDir);
        if (!prj_files.CheckProjectConfigs()) {
            continue;
        }
        if (prj.m_MakeType == eMakeType_ExcludedByReq) {
            PTB_WARNING_EX(prj.m_ID, ePTB_ProjectExcluded,
                           "Excluded due to unmet requirements");
            continue;
        }
        bool excluded = (prj.m_MakeType == eMakeType_Excluded);

        prj_files.DoCollect();
        if (!excluded) {
            if (prj.m_ProjType == CProjKey::eLib || prj.m_ProjType == CProjKey::eDll) {
                AddString( *lib_dependencies, proj_dependency);
            } else if (prj.m_ProjType == CProjKey::eApp) {
                AddString( *app_dependencies, proj_dependency);
            } else {
                continue;
            }
            AddString( *all_dependencies, proj_dependency);
        }

        if (prj.m_ProjType == CProjKey::eApp) {
            string app_type = prj_files.GetProjectContext().GetMsvcProjectMakefile().GetLinkerOpt("APP_TYPE",SConfigInfo());
            if (app_type == "application") {
                prj.m_IsBundle = true;
                explicit_type = GetExplicitType( prj);
            }
        }
        
        CRef<CArray> build_phases( new CArray);
        CRef<CArray> build_files( new CArray);

        // project file groups
        AddString( *file_groups,
            CreateProjectFileGroups(prj, prj_files, *dict_objects, *build_files));

        // project script phase
        string proj_script(
            CreateProjectScriptPhase(prj, prj_files, *dict_objects));
        if (!proj_script.empty()) {
            AddString( *build_phases, proj_script);
        }
        // project build phase
        string proj_build(
            CreateProjectBuildPhase(prj, *dict_objects, build_files));
        if (!proj_build.empty()) {
            AddString( *build_phases, proj_build);
        }
        // project custom script phase
        string proj_cust_script(
            CreateProjectCustomScriptPhase(prj, prj_files, *dict_objects));
        if (!proj_cust_script.empty()) {
            AddString( *build_phases, proj_cust_script);
        }
        // project target and dependencies
#if USE_VERBOSE_NAMES
        string proj_product(  GetProjProduct(  prj));
#else
        string proj_product(  GetUUID());
#endif
        string proj_target(
            CreateProjectTarget( prj, prj_files, *dict_objects, build_phases,
                                 proj_product));
        if (!proj_target.empty()) {
            AddString( *targets, proj_target);
        }

        // project dependency key
        {
            CRef<CDict> dict_dep( AddDict( *dict_objects, proj_dependency));
            AddString( *dict_dep, "isa", "PBXTargetDependency");
            AddString( *dict_dep, "target", proj_target);
        }
        // project product
        {
            CRef<CDict> dict_product( AddDict( *dict_objects, proj_product));
            AddString( *dict_product, "explicitFileType", explicit_type);
            if (prj.m_ProjType == CProjKey::eDll) {
                AddString( *dict_product, "isa", "PBXLibraryReference");
            } else if (prj.m_ProjType == CProjKey::eApp) {
                AddString( *dict_product, "isa", "PBXFileReference");
                AddString( *dict_product, "path", prj.m_ID);
            } else if (prj.m_ProjType == CProjKey::eLib) {
                AddString( *dict_product, "isa", "PBXFileReference");
                AddString( *dict_product, "path", string("lib") + prj.m_ID + string(".a"));
            }
//            AddString( *dict_product, "refType", "3");
            AddString( *dict_product, "sourceTree", "BUILT_PRODUCTS_DIR");
        }
    }

// collect file groups
#if USE_VERBOSE_NAMES
    string source_files("Source_Files");
    string root_group("Main_Group");
#else
    string source_files(GetUUID());
    string root_group( GetUUID());    
#endif

    file_groups->Set().sort(s_String_less);
    AddGroupDict( *dict_objects, source_files, file_groups, "Sources");
    CRef<CArray> main_groups( new CArray);
    AddString( *main_groups, source_files);
    AddGroupDict( *dict_objects, root_group, main_groups, "NCBI C++ Toolkit");

    targets->Set().sort(s_String_less);
    lib_dependencies->Set().sort(s_String_less);
    app_dependencies->Set().sort(s_String_less);
    all_dependencies->Set().sort(s_String_less);
// aggregate targets
    InsertString( *targets,
        AddAggregateTarget("BUILD_LIBS", *dict_objects, lib_dependencies));
    InsertString( *targets,
        AddAggregateTarget("BUILD_APPS", *dict_objects, app_dependencies));
    InsertString( *targets,
        AddAggregateTarget("BUILD_ALL",  *dict_objects, all_dependencies));
    InsertString( *targets,
        AddConfigureTarget(solution_name,  *dict_objects));

// root object
    AddString( dict_root, "rootObject",
        CreateRootObject(configs_root, *dict_objects, targets, root_group));

// save project
    Save(solution_name, *xproj);
}

void CMacProjectGenerator::Save(const string& solution_name, CPlist& xproj)
{
    string solution_dir(m_SolutionDir);
    solution_dir = CDirEntry::ConcatPath(solution_dir, solution_name);
    solution_dir += ".xcodeproj";
    CDir(solution_dir).CreatePath();
    string solution_file( CDirEntry::ConcatPath(solution_dir, "project.pbxproj"));
    auto_ptr<CObjectOStream> out(CObjectOStream::Open(solution_file, eSerial_Xml));
    CObjectOStreamXml *ox = dynamic_cast<CObjectOStreamXml*>(out.get());
    ox->SetReferenceDTD(true);
    ox->SetDTDPublicId("-//Apple//DTD PLIST 1.0//EN");
    ox->SetDTDFilePrefix("http://www.apple.com/DTDs/");
    ox->SetDTDFileName("PropertyList-1.0");
    ox->SetEncoding(eEncoding_UTF8);
    *out << xproj;
}

string CMacProjectGenerator::CreateProjectFileGroups(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects, CArray& build_files)
{
    CDllSrcFilesDistr& dll_src( GetApp().GetDllFilesDistr());
    CProjKey proj_key(prj.m_ProjType, prj.m_ID);

    string proj_id(        GetProjId( prj) );
#if USE_VERBOSE_NAMES
    string proj_src(       proj_id + "_src");
    string proj_include(   proj_id + "_include");
    string proj_specs(     proj_id + "_specs");
    string src_group_name( proj_id + "_sources");
#else
    string proj_src(       GetUUID());
    string proj_include(   GetUUID());
    string proj_specs(     GetUUID());
    string src_group_name( GetUUID());
#endif

    CRef<CArray> proj_cpps(  new CArray);
    CRef<CArray> proj_hpps(  new CArray);
    CRef<CArray> specs( new CArray);

    map<string, CRef<CArray> > hosted_cpps;
    map<string, CRef<CArray> > hosted_hpps;
    map<string, CRef<CArray> > hosted_srcs;
    ITERATE( list<string>, hosted_lib, prj.m_HostedLibs) {
        hosted_cpps[ *hosted_lib] = new CArray;
        hosted_hpps[ *hosted_lib] = new CArray;
        hosted_srcs[ *hosted_lib] = new CArray;
    }

    // for each source file in project
    ITERATE ( list<string>, f, prj_files.GetSources()) {
        string src( AddFile( dict_objects, *f));
        if (!src.empty()) {
            bool added=false;
            if (prj.m_ProjType == CProjKey::eDll) {
                CProjKey hosted_key = dll_src.GetSourceLib( *f, proj_key);
                if (!hosted_key.Id().empty()) {
                    AddString( *hosted_cpps[ hosted_key.Id() ], src);
                    added = true;
                }
            }
            if (!added) {
                AddString( *proj_cpps, src);
            }
            AddString( build_files, AddFileSource( dict_objects, src));
        }
    }
    // for each header file in project
    ITERATE ( list<string>, f, prj_files.GetHeaders()) {
        string src( AddFile( dict_objects, *f));
        if (!src.empty()) {
            bool added=false;
            if (prj.m_ProjType == CProjKey::eDll) {
                CProjKey hosted_key = dll_src.GetHeaderLib( *f, proj_key);
                if (!hosted_key.Id().empty()) {
                    AddString( *hosted_hpps[ hosted_key.Id() ], src);
                    added = true;
                } else {
                    CProjKey hosted_inl = dll_src.GetInlineLib( *f, proj_key);
                    if (!hosted_inl.Id().empty()) {
                        AddString( *hosted_hpps[ hosted_inl.Id() ], src);
                        added = true;
                    }
                }
            }
            if (!added) {
                AddString( *proj_hpps, src);
            }
        }
    }
    // dataspecs
    ITERATE ( list<string>, f, prj_files.GetDataSpecs()) {
        string src( AddFile( dict_objects, *f));
        if (!src.empty()) {
            AddString( *specs,src);
        }
    }

    string source_files(  "Source Files");
    string header_files(  "Header Files");
    string datatool_files("Datatool Files");
    CRef<CArray> prj_sources( new CArray);
    ITERATE( list<string>, hosted_lib, prj.m_HostedLibs) {
        CRef<CArray>& cpps = hosted_cpps[ *hosted_lib];
        CRef<CArray>& hpps = hosted_hpps[ *hosted_lib];
        CRef<CArray>& srcs = hosted_srcs[ *hosted_lib];
#if USE_VERBOSE_NAMES
        string hosted_src(   *hosted_lib + "_hosted_src");
        string hosted_inc(   *hosted_lib + "_hosted_include");
        string hosted_group( *hosted_lib + "_hosted_sources");
#else
        string hosted_src(   GetUUID());
        string hosted_inc(   GetUUID());
        string hosted_group( GetUUID());
#endif
        if (!cpps->Get().empty()) {
            cpps->Set().sort(s_String_less);
            AddString( *srcs, hosted_src);
            AddGroupDict( dict_objects, hosted_src, cpps, source_files);
        }
        if (!hpps->Get().empty()) {
            hpps->Set().sort(s_String_less);
            AddString( *srcs, hosted_inc);
            AddGroupDict( dict_objects, hosted_inc, hpps, header_files);
        }
        AddGroupDict( dict_objects, hosted_group, srcs, *hosted_lib);
        AddString( *prj_sources, hosted_group);
    }
    if (!prj.m_HostedLibs.empty()) {
        prj_sources->Set().sort(s_String_less);
    }
    if (!proj_cpps->Get().empty()) {
        proj_cpps->Set().sort(s_String_less);
        AddString( *prj_sources, proj_src);
        AddGroupDict( dict_objects, proj_src,     proj_cpps, source_files);
    }
    if (!proj_hpps->Get().empty()) {
        proj_hpps->Set().sort(s_String_less);
        AddString( *prj_sources, proj_include);
        AddGroupDict( dict_objects, proj_include, proj_hpps, header_files);
    }
    if (!specs->Get().empty()) {
        AddString( *prj_sources, proj_specs);
        AddGroupDict( dict_objects, proj_specs,   specs, datatool_files);
    }
    AddGroupDict( dict_objects, src_group_name, prj_sources, GetTargetName(prj));
    return src_group_name;
}

string CMacProjectGenerator::CreateProjectScriptPhase(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    string script;
    CRef<CArray> inputs(  new CArray);
    CRef<CArray> outputs( new CArray);
    // configurable files
    ITERATE ( list<string>, f, prj_files.GetConfigurableSources()) {
        string outfile(GetRelativePath( *f));
        AddString( *outputs, outfile);
        CDirEntry ent(outfile);
        string infile(CDirEntry::ConcatPath( ent.GetDir(), ent.GetBase()));
        infile += ".$CONFIGURATION";
        infile += ent.GetExt();
        script += "cmp -s " + infile + " " + outfile + "\n";
        script += "if test $? -ne 0; then\n";
        script += "cp -p " + infile + " " + outfile + "\n";
        script += "fi\n";
    }
    // datatool
    if (!prj.m_DatatoolSources.empty()) {
#if 0
        ITERATE ( list<string>, f, prj_files.GetSources()) {
            if (prj_files.IsProducedByDatatool(prj,*f)) {
                string outfile(GetRelativePath( *f));
                AddString( *outputs, outfile);
            }
        }
#endif
        string pch_name = GetApp().GetMetaMakefile().GetDefaultPch();
        ITERATE ( list<string>, f, prj_files.GetDataSpecs()) {
            CDirEntry entry(*f);
            string spec_base( CDirEntry(GetRelativePath(*f)).GetDir() + entry.GetBase());
            AddString( *inputs, GetRelativePath(*f));
            AddString( *inputs, spec_base + ".def");
            AddString( *outputs, spec_base + ".files");
            script += "echo Using datatool to create a C++ objects from " + entry.GetName() + "\n";
            script += m_OutputDir + GetApp().GetDatatoolPathForApp() +
                " " + GetApp().GetDatatoolCommandLine() + " -pch " + pch_name;
            script += " -m " + GetRelativePath( entry.GetPath(), &GetApp().GetProjectTreeInfo().m_Src);
            string imports( prj_files.GetDataSpecImports(*f));
            if (!imports.empty()) {
#ifdef PSEUDO_XCODE
                NStr::ReplaceInPlace(imports, "\\", "/");
#endif
                script += " -M \"" + imports + "\"";
            }
            script += " -oc " + entry.GetBase();
            script += " -od " + spec_base + ".def";
            script += " -or " + GetRelativePath( entry.GetDir(), &GetApp().GetProjectTreeInfo().m_Src);
            script += " -oR " + GetRelativePath( GetApp().GetProjectTreeInfo().m_Root) + "\n";
        }
    }
    if (!script.empty()) {
#if USE_VERBOSE_NAMES
        string proj_script(   GetProjId(       prj) + "_script");
#else
        string proj_script(   GetUUID());
#endif
        CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
        AddArray(  *dict_script, "files");
        AddArray(  *dict_script, "inputPaths",  inputs);
        AddArray(  *dict_script, "outputPaths", outputs);
        AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
        AddString( *dict_script, "shellPath", "/bin/sh");
        AddString( *dict_script, "shellScript", script);
        return proj_script;
    }
    return kEmptyStr;
}

string CMacProjectGenerator::CreateProjectCustomScriptPhase(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    SCustomScriptInfo info;
    prj_files.GetProjectContext().GetMsvcProjectMakefile().GetCustomScriptInfo(info);

    if (!info.m_Script.empty()) {
#if USE_VERBOSE_NAMES
        string proj_script(   GetProjId(       prj) + "_cust_script");
#else
        string proj_script(   GetUUID());
#endif
        CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
        string script_loc( prj.m_SourcesBaseDir);

        CRef<CArray> inputs(  new CArray);
        CRef<CArray> outputs( new CArray);
        list<string> in_list;
        NStr::Split(info.m_Input, LIST_SEPARATOR, in_list);
        ITERATE( list<string>, i, in_list) {
            AddString( *inputs,
                GetRelativePath(CDirEntry::ConcatPath(script_loc,*i)));
        }
        list<string> out_list;
        NStr::Split(info.m_Output, LIST_SEPARATOR, out_list);
        ITERATE( list<string>, o, out_list) {
            AddString( *outputs,
                GetRelativePath(CDirEntry::ConcatPath(script_loc,*o)));
        }
        AddArray(  *dict_script, "files");
        AddArray(  *dict_script, "inputPaths",  inputs);
        AddArray(  *dict_script, "outputPaths", outputs);
        AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
        if (info.m_Shell.empty()) {
            info.m_Shell = "/bin/sh";
        }
        AddString( *dict_script, "shellPath", info.m_Shell);
        AddString( *dict_script, "shellScript",
            GetRelativePath(CDirEntry::ConcatPath(script_loc,info.m_Script)));
        return proj_script;
    }
    return kEmptyStr;
}

string CMacProjectGenerator::CreateProjectBuildPhase(
    const CProjItem& prj,
    CDict& dict_objects, CRef<CArray>& build_files)
{
#if USE_VERBOSE_NAMES
    string proj_build(    GetProjBuild( prj));
#else
    string proj_build(    GetUUID());
#endif
    CRef<CDict> dict_build( AddDict( dict_objects, proj_build));
    AddArray( *dict_build, "files", build_files);
    AddString( *dict_build, "isa", "PBXSourcesBuildPhase");
    return proj_build;
}

string CMacProjectGenerator::CreateProjectTarget(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects, CRef<CArray>& build_phases,
    const string& product_id)
{
#if USE_VERBOSE_NAMES
    string proj_target(   GetProjTarget(   prj));
#else
    string proj_target(   GetUUID());
#endif
    string target_name(   GetTargetName(   prj));
    string product_type(  GetProductType(  prj));
    CRef<CArray> dependencies( new CArray);
    string configs_prj(
        CreateProjectBuildConfigurations( prj, prj_files, dict_objects));
    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildConfigurationList",configs_prj);
    AddArray( *dict_target, "buildPhases", build_phases);
    ITERATE( list<CProjKey>, d, prj.m_Depends) {
        CProjectItemsTree::TProjects::const_iterator
            dp = m_Projects_tree.m_Projects.find( *d);
        if ( dp != m_Projects_tree.m_Projects.end() &&
            (dp->first.Id() != prj.m_ID || dp->first.Type() != prj.m_ProjType)) {

            if (dp->first.Type() == CProjKey::eLib &&
                GetApp().GetSite().Is3PartyLib(dp->first.Id())) {
                    continue;
            }
#if USE_VERBOSE_NAMES
            AddString( *dependencies, GetProjDependency(dp->second));
#else
            AddString( *dependencies, dp->second.m_GUID);
#endif
        }
    }
    AddArray(  *dict_target, "dependencies", dependencies);
    AddString( *dict_target, "isa", "PBXNativeTarget");
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "productName", target_name);
    AddString( *dict_target, "productReference", product_id);
    AddString( *dict_target, "productType", product_type);
    return proj_target;
}

string CMacProjectGenerator::CreateBuildConfigurations(CDict& dict_objects)
{
    CRef<CArray> build_settings( new CArray);
#if USE_VERBOSE_NAMES
    string bld_cfg(      "Build_Configuration_");
    string bld_cfg_list( "Build_Configurations");
#else
    string bld_cfg_list( GetUUID());
#endif

    ITERATE(list<SConfigInfo>, cfg, m_Configs) {
        if (cfg->m_rtType == SConfigInfo::rtSingleThreaded ||
            cfg->m_rtType == SConfigInfo::rtSingleThreadedDebug ||
            cfg->m_rtType == SConfigInfo::rtUnknown) {
            continue;
        }
#if USE_VERBOSE_NAMES
        string bld_cfg_name(bld_cfg + cfg->m_Name);
#else
        string bld_cfg_name( GetUUID());
#endif
        CreateBuildSettings( *AddDict( dict_objects, bld_cfg_name), *cfg);
        AddString( *build_settings, bld_cfg_name);
    }

    CRef<CDict> configs( AddDict( dict_objects, bld_cfg_list));
    AddArray(  *configs, "buildConfigurations", build_settings);
    AddString( *configs, "defaultConfigurationIsVisible", "0");
    AddString( *configs, "defaultConfigurationName", "ReleaseDLL");
    AddString( *configs, "isa", "XCConfigurationList");
    return bld_cfg_list;
}

string CMacProjectGenerator::CreateProjectBuildConfigurations(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    CRef<CArray> build_settings( new CArray);
    string proj_id(      GetProjId( prj) );
#if USE_VERBOSE_NAMES
    string bld_cfg(      proj_id + "_Build_Configuration_");
    string bld_cfg_list( proj_id + "_Build_Configurations");
#else
    string bld_cfg_list( GetUUID());
#endif

    ITERATE(list<SConfigInfo>, cfg, prj_files.GetEnabledConfigs()) {
        if (cfg->m_rtType == SConfigInfo::rtSingleThreaded ||
            cfg->m_rtType == SConfigInfo::rtSingleThreadedDebug ||
            cfg->m_rtType == SConfigInfo::rtUnknown) {
            continue;
        }
#if USE_VERBOSE_NAMES
        string bld_cfg_name(bld_cfg + cfg->m_Name);
#else
        string bld_cfg_name( GetUUID());
#endif
        CreateProjectBuildSettings( prj, prj_files,
            *AddDict( dict_objects, bld_cfg_name), *cfg);
        AddString( *build_settings, bld_cfg_name);
    }

    CRef<CDict> configs( AddDict( dict_objects, bld_cfg_list));
    AddArray(  *configs, "buildConfigurations", build_settings);
    AddString( *configs, "defaultConfigurationIsVisible", "0");
    AddString( *configs, "defaultConfigurationName", "ReleaseDLL");
    AddString( *configs, "isa", "XCConfigurationList");
    return bld_cfg_list;
}

string CMacProjectGenerator::CreateAggregateBuildConfigurations(
    const string& target_name, CDict& dict_objects)
{
    CRef<CArray> build_settings( new CArray);
    string proj_id(      target_name );
#if USE_VERBOSE_NAMES
    string bld_cfg(      target_name + "_Build_Configuration_");
    string bld_cfg_list( target_name + "_Build_Configurations");
#else
    string bld_cfg_list( GetUUID());
#endif

    ITERATE(list<SConfigInfo>, cfg, m_Configs) {
        if (cfg->m_rtType == SConfigInfo::rtSingleThreaded ||
            cfg->m_rtType == SConfigInfo::rtSingleThreadedDebug ||
            cfg->m_rtType == SConfigInfo::rtUnknown) {
            continue;
        }
#if USE_VERBOSE_NAMES
        string bld_cfg_name(bld_cfg + cfg->m_Name);
#else
        string bld_cfg_name( GetUUID());
#endif
        CreateAggregateBuildSettings( target_name,
            *AddDict( dict_objects, bld_cfg_name), *cfg);
        AddString( *build_settings, bld_cfg_name);
    }

    CRef<CDict> configs( AddDict( dict_objects, bld_cfg_list));
    AddArray(  *configs, "buildConfigurations", build_settings);
    AddString( *configs, "defaultConfigurationIsVisible", "0");
    AddString( *configs, "defaultConfigurationName", "ReleaseDLL");
    AddString( *configs, "isa", "XCConfigurationList");
    return bld_cfg_list;
}

void CMacProjectGenerator::CreateBuildSettings(CDict& dict_cfg, const SConfigInfo& cfg)
{
    string tmp_str;
    list<string> tmp_list;

    CRef<CDict> settings( AddDict( dict_cfg, "buildSettings"));
    AddString( dict_cfg, "isa", "XCBuildConfiguration");
    AddString( dict_cfg, "name", cfg.m_Name);

    tmp_str = GetApp().GetMetaMakefile().GetCompilerOpt("ARCHS", cfg);
    tmp_list.clear();
    NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    string def_arch( CMsvc7RegSettings::GetMsvcPlatformName());
    if (find(tmp_list.begin(), tmp_list.end(), def_arch) == tmp_list.end()) {
        tmp_list.push_front(def_arch);
    }
    CRef<CArray> archs( AddArray( *settings, "ARCHS"));
    ITERATE( list<string>, a, tmp_list) {
        AddString( *archs, *a);
    }

    AddCompilerSetting( *settings, cfg, "GCC_WARN_ABOUT_RETURN_TYPE");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_UNUSED_VARIABLE");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_EFFECTIVE_CPLUSPLUS_VIOLATIONS");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_HIDDEN_VIRTUAL_FUNCTIONS");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_NON_VIRTUAL_DESTRUCTOR");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_PEDANTIC");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_SHADOW");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_SIGN_COMPARE");

    AddCompilerSetting( *settings, cfg, "GCC_DYNAMIC_NO_PIC");
    AddCompilerSetting( *settings, cfg, "GCC_ENABLE_FIX_AND_CONTINUE");
    AddCompilerSetting( *settings, cfg, "GCC_MODEL_TUNING");
    AddCompilerSetting( *settings, cfg, "GCC_ENABLE_CPP_EXCEPTIONS");
    AddCompilerSetting( *settings, cfg, "GCC_ENABLE_CPP_RTTI");

    AddCompilerSetting( *settings, cfg, "COPY_PHASE_STRIP");
    AddCompilerSetting( *settings, cfg, "GCC_GENERATE_DEBUGGING_SYMBOLS");
    AddCompilerSetting( *settings, cfg, "GCC_OPTIMIZATION_LEVEL");
    AddCompilerSetting( *settings, cfg, "DEBUG_INFORMATION_FORMAT");
    AddCompilerSetting( *settings, cfg, "GCC_DEBUGGING_SYMBOLS");

#if 0
// moved into project settings
    tmp_str = GetApp().GetMetaMakefile().GetCompilerOpt("GCC_PREPROCESSOR_DEFINITIONS", cfg);
    tmp_list.clear();
    NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    CRef<CArray> preproc( AddArray( *settings, "GCC_PREPROCESSOR_DEFINITIONS"));
    ITERATE( list<string>, a, tmp_list) {
        AddString( *preproc, *a);
    }
#endif

    AddCompilerSetting( *settings, cfg, "SDKROOT");
    AddCompilerSetting( *settings, cfg, "FRAMEWORK_SEARCH_PATHS");

    AddLinkerSetting( *settings, cfg, "DEAD_CODE_STRIPPING");
    AddLinkerSetting( *settings, cfg, "PREBINDING");
//    AddLinkerSetting( *settings, cfg, "ZERO_LINK");
    if (cfg.m_rtType == SConfigInfo::rtMultiThreadedDebugDLL ||
        cfg.m_rtType == SConfigInfo::rtMultiThreadedDLL) {
        AddString( *settings, "STANDARD_C_PLUS_PLUS_LIBRARY_TYPE", "dynamic");
    } else {
        AddString( *settings, "STANDARD_C_PLUS_PLUS_LIBRARY_TYPE", "static");
    }
}

void CMacProjectGenerator::CreateProjectBuildSettings(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_cfg, const SConfigInfo& cfg)
{
    const CMsvcMetaMakefile& metamake( GetApp().GetMetaMakefile());
    bool dll_build = GetApp().GetBuildType().GetType() == CBuildType::eDll;
    string def_lib_path(m_OutputDir + "lib/$(CONFIGURATION)");
    string libs_out(m_OutputDir + "lib/$(CONFIGURATION)");
    string bins_out(m_OutputDir + "bin/$(CONFIGURATION)");
    string proj_dir("$(PROJECT_DIR)/");
//    string temp_dir("$(BUILD_DIR)/$(CONFIGURATION)");
    string temp_dir("$(OBJROOT)/$(CONFIGURATION)");
    string objroot(m_OutputDir + "build");

    CRef<CArray> lib_paths(new CArray);
    AddString(*lib_paths, def_lib_path);
    list<string> prj_lib_dirs;
    if (prj_files.GetLibraryDirs(prj_lib_dirs, cfg)) {
        ITERATE ( list<string>, f, prj_lib_dirs) {
            AddString( *lib_paths, GetRelativePath( *f));
        }
    }
    
    CRef<CDict> settings( AddDict( dict_cfg, "buildSettings"));
    AddString( dict_cfg, "isa", "XCBuildConfiguration");
    AddString( dict_cfg, "name", cfg.m_Name);

    if (prj.m_ProjType == CProjKey::eLib) {

        AddLibrarianSetting( *settings, cfg, "GCC_ENABLE_SYMBOL_SEPARATION");
        AddLibrarianSetting( *settings, cfg, "GCC_INLINES_ARE_PRIVATE_EXTERN");
        AddLibrarianSetting( *settings, cfg, "GCC_SYMBOLS_PRIVATE_EXTERN");

        AddString( *settings, "CONFIGURATION_BUILD_DIR", libs_out);
        AddString( *settings, "CONFIGURATION_TEMP_DIR", temp_dir);
        AddString( *settings, "INSTALL_PATH", proj_dir + libs_out);
        AddString( *settings, "OBJROOT", objroot);

    } else if (prj.m_ProjType == CProjKey::eDll) {

        string bld_out(libs_out);
        if (prj.m_IsBundle) {
            bld_out = bins_out;
        }
        AddString( *settings, "GCC_SYMBOLS_PRIVATE_EXTERN", "NO");

        AddString( *settings, "CONFIGURATION_BUILD_DIR", bld_out);
        AddString( *settings, "CONFIGURATION_TEMP_DIR", temp_dir);
        AddString( *settings, "INSTALL_PATH", proj_dir + bld_out);
        AddString( *settings, "OBJROOT", objroot);

        AddArray( *settings, "LIBRARY_SEARCH_PATHS", lib_paths);
        AddString( *settings, "EXECUTABLE_PREFIX", "lib");

    } else if (prj.m_ProjType == CProjKey::eApp) {

        AddString( *settings, "CONFIGURATION_BUILD_DIR", bins_out);
        AddString( *settings, "CONFIGURATION_TEMP_DIR", temp_dir);
        AddString( *settings, "INSTALL_PATH", proj_dir + bins_out);
        AddString( *settings, "OBJROOT", objroot);

        AddArray( *settings, "LIBRARY_SEARCH_PATHS", lib_paths);
    }
    AddString( *settings, "MACH_O_TYPE", GetMachOType(prj));

// library dependencies
    if (prj.m_ProjType == CProjKey::eDll || prj.m_ProjType == CProjKey::eApp) {
        list<CProjItem> ldlibs;
        ITERATE( list<CProjKey>, d, prj.m_Depends) {
            CProjectItemsTree::TProjects::const_iterator
                dp = m_Projects_tree.m_Projects.find( *d);
            if ( dp != m_Projects_tree.m_Projects.end() &&
                (dp->first.Id() != prj.m_ID || dp->first.Type() != prj.m_ProjType) &&
                (dp->first.Type() == CProjKey::eLib || dp->first.Type() == CProjKey::eDll)) {

                if (dp->first.Type() == CProjKey::eLib &&
                    GetApp().GetSite().Is3PartyLib(dp->first.Id())) {
                        continue;
                }
                ldlibs.push_back(dp->second);
            }
        }
        ldlibs.sort(s_ProjItem_less);
        string ldlib;
        ITERATE( list<CProjItem>, d, ldlibs) {
            ldlib += string(" -l") + GetTargetName(*d);
        }
        string add;
        add = prj_files.GetProjectContext().GetMsvcProjectMakefile().GetLinkerOpt("OTHER_LDFLAGS",cfg);
        if (!add.empty()) {
            ldlib += " " + add;
        }
        add = prj_files.GetProjectContext().AdditionalLinkerOptions(cfg);
        if (!add.empty()) {
            ldlib += " " + add;
        }
        add = metamake.GetLinkerOpt("OTHER_LDFLAGS", cfg);
        if (!add.empty()) {
            ldlib += " " + add;
        }
        AddString( *settings, "OTHER_LDFLAGS", ldlib);
    }

    CRef<CArray> inc_dirs( AddArray( *settings, "HEADER_SEARCH_PATHS"));
    list<string> prj_inc_dirs;
    if (prj_files.GetIncludeDirs(prj_inc_dirs, cfg)) {
        ITERATE ( list<string>, f, prj_inc_dirs) {
/*
            if (CSymResolver::HasDefine(*f)) {
                continue;
            }
*/
            AddString( *inc_dirs, GetRelativePath( *f));
        }
    }
    AddString( *settings, "PRODUCT_NAME", GetTargetName(prj));

// preprocessor definitions    
    list<string> tmp_list = prj.m_Defines;
    string tmp_str = metamake.GetCompilerOpt("GCC_PREPROCESSOR_DEFINITIONS", cfg);
    NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    if (dll_build) {
        tmp_str = GetApp().GetConfig().Get(CMsvc7RegSettings::GetMsvcSection(), "DllBuildDefine");
        NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    }

    tmp_list.sort();
    tmp_list.unique();
    CRef<CArray> preproc( AddArray( *settings, "GCC_PREPROCESSOR_DEFINITIONS"));
    ITERATE( list<string>, a, tmp_list) {
        AddString( *preproc, *a);
    }

// precompiled header
    if (metamake.IsPchEnabled()) {
        string nofile = CDirEntry::ConcatPath(prj.m_SourcesBaseDir,"aanofile");
        string pch_name = metamake.GetUsePchThroughHeader(
            prj.m_ID, nofile, GetApp().GetProjectTreeInfo().m_Src);
        if (!pch_name.empty()) {
            string pch_path;
            // find header (MacOS requires? path)
            if (prj_files.GetIncludeDirs(prj_inc_dirs, cfg)) {
                ITERATE ( list<string>, f, prj_inc_dirs) {
                    if (pch_path.empty()) {
                        string t = CDirEntry::ConcatPath( *f, pch_name);
                        if (CDirEntry(t).IsFile()) {
                            pch_path = t;
                            break;
                        }
                    }
                }
            }
            if (pch_path.empty()) {
                pch_path = CDirEntry::ConcatPath(
                    GetApp().GetProjectTreeInfo().m_Include, pch_name);
            }
            AddString( *settings, "GCC_PRECOMPILE_PREFIX_HEADER", "YES");
            AddString( *settings, "GCC_PREFIX_HEADER", GetRelativePath( pch_path));
// for some unknown reason, when I define NCBI_USE_PCH
// xutil library does not compile
            string tmp(metamake.GetPchUsageDefine());
            if (!tmp.empty()) {
                AddString( *preproc, tmp);
            }
        }
    }
}

void CMacProjectGenerator::CreateAggregateBuildSettings(
    const string& target_name, CDict& dict_cfg, const SConfigInfo& cfg)
{
    CRef<CDict> settings( AddDict( dict_cfg, "buildSettings"));
    AddString( dict_cfg, "isa", "XCBuildConfiguration");
    AddString( dict_cfg, "name", cfg.m_Name);
    AddString( *settings, "PRODUCT_NAME", target_name);
}

string CMacProjectGenerator::AddAggregateTarget(
    const string& target_name, CDict& dict_objects, CRef<CArray>& dependencies)
{
#if USE_VERBOSE_NAMES
    string proj_target( target_name + "_target");
#else
    string proj_target( GetUUID());
#endif
    string configs_prj(
        CreateAggregateBuildConfigurations( target_name, dict_objects));
    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildConfigurationList", configs_prj);
    AddArray(  *dict_target, "buildPhases");
    AddString( *dict_target, "comments", NStr::UIntToString(dependencies->Get().size()) + " targets");
    AddArray(  *dict_target, "dependencies", dependencies);
    AddString( *dict_target, "isa", "PBXAggregateTarget");
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "productName", target_name);
    return proj_target;
}

string CMacProjectGenerator::AddConfigureTarget(
    const string& solution_name, CDict& dict_objects)
{
    string target_name("CONFIGURE");
#if USE_VERBOSE_NAMES
    string proj_target( target_name + "_target");
    string proj_script( target_name + "_script");
#else
    string proj_target( GetUUID());
    string proj_script( GetUUID());
#endif

    string configs_prj(
        CreateAggregateBuildConfigurations( target_name, dict_objects));

    string script;
    script += "export PTB_PATH=" + m_OutputDir + "../static/bin/ReleaseDLL\n";
    script += "export SLN_PATH=" + m_OutputDir + solution_name + "\n";
    script += "export TREE_ROOT=" + GetRelativePath( GetApp().m_Root) + "\n";
    script += "export BUILD_TREE_ROOT=" + GetRelativePath(
        CDirEntry::AddTrailingPathSeparator( CDirEntry::ConcatPath(
            GetApp().GetProjectTreeInfo().m_Compilers,
            GetApp().GetRegSettings().m_CompilersSubdir))) + "\n";
    script += m_OutputDir + "UtilityProjects/configure_";
    script += solution_name + ".sh";
    CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
    AddArray(  *dict_script, "files");
    AddArray(  *dict_script, "inputPaths");
    AddArray(  *dict_script, "outputPaths");
    AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
    AddString( *dict_script, "shellPath", "/bin/sh");
    AddString( *dict_script, "shellScript", script);

    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildConfigurationList", configs_prj);
    CRef<CArray> build_phases(AddArray(  *dict_target, "buildPhases"));
    AddString( *build_phases, proj_script);
    AddArray(  *dict_target, "dependencies");
    AddString( *dict_target, "isa", "PBXAggregateTarget");
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "productName", target_name);
    return proj_target;
}

string CMacProjectGenerator::CreateRootObject(
    const string& configs_root, CDict& dict_objects, CRef<CArray>& targets,
    const string& root_group)
{
#if USE_VERBOSE_NAMES
    string root_name("ROOT_OBJECT");
#else
    string root_name( GetUUID());
#endif
    CRef<CDict> root_obj( AddDict( dict_objects, root_name));
    AddString( *root_obj, "buildConfigurationList", configs_root);
    AddString( *root_obj, "compatibilityVersion", "Xcode 3.0");
    AddString( *root_obj, "hasScannedForEncodings", "1");
    AddString( *root_obj, "isa", "PBXProject");
    AddString( *root_obj, "mainGroup", root_group);
    AddString( *root_obj, "projectDirPath", "");
    AddString( *root_obj, "projectRoot", "");
    AddArray(  *root_obj, "targets", targets);
    return root_name;
}

string CMacProjectGenerator::GetUUID(void)
{
    static Uint4 uuid = 1;
    char buffer[64];
    ::sprintf(buffer, "ABCDABCDABCDABCD%08X", uuid++);
    return buffer;
}

string CMacProjectGenerator::AddFile(CDict& dict, const string& name)
{
    string filetype;
    CDirEntry entry(name);
    string ext = entry.GetExt();
    if ( ext == ".cpp" || ext == ".c") {
        filetype = string("sourcecode") + ext + ext;
    } else if (ext == ".hpp" || ext == ".inl") {
        filetype = "sourcecode.cpp.h";
    } else if (ext == ".h") {
        filetype = "sourcecode.c.h";
    } else if (ext == ".xsd") {
        filetype = "text.xml";
    } else {
        filetype = "text";
    }
    
    string base_name = entry.GetName();
#if USE_VERBOSE_NAMES
    static size_t counter = 0;
    string name_id = "FILE" + NStr::UIntToString(counter++);
#else
    string name_id( GetUUID());
#endif
    CRef<CDict> file( AddDict( dict, name_id));
    AddString( *file, "isa", "PBXFileReference");
    if (!filetype.empty()) {
        AddString( *file, "lastKnownFileType", filetype);
    }
    AddString( *file, "name", base_name);
    AddString( *file, "path", GetRelativePath(name));
    AddString( *file, "sourceTree", "SOURCE_ROOT");
    return name_id;
}

string CMacProjectGenerator::AddFileSource(CDict& dict, const string& name)
{
#if USE_VERBOSE_NAMES
    string name_ref = "SRC_" + name;
#else
    string name_ref( GetUUID());
#endif
    CRef<CDict> file( AddDict( dict, name_ref));
    AddString( *file, "fileRef", name);
    AddString( *file, "isa", "PBXBuildFile");
    return name_ref;
}

void  CMacProjectGenerator::AddGroupDict(
    CDict& dict, const string& key, CRef<CArray>& children, const string& name)
{
    CRef<CDict> group( AddDict( dict, key));
    AddArray(  *group, "children", children);
    AddString( *group, "isa", "PBXGroup");
    AddString( *group, "name", name);
    AddString( *group, "refType", "4");
}


void CMacProjectGenerator::AddString(CArray& ar, const string& value)
{
    CRef<CArray::C_E> e(new CArray::C_E);
    e->SetString( value);
    ar.Set().push_back(e);
}

void CMacProjectGenerator::InsertString(CArray& ar, const string& value)
{
    CRef<CArray::C_E> e(new CArray::C_E);
    e->SetString( value);
    ar.Set().push_front(e);
}
    
void CMacProjectGenerator::AddString(
    CDict& dict, const string& key, const string& value)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    e->SetPlistObject().SetString(value);
    dict.Set().push_back(e);
}

CRef<CArray> CMacProjectGenerator::AddArray(
    CDict& dict, const string& key)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    CArray& a = e->SetPlistObject().SetArray();
    dict.Set().push_back(e);
    return CRef<CArray>(&a);
}

void CMacProjectGenerator::AddArray(
    CDict& dict, const string& key, CRef<CArray>& array)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    e->SetPlistObject().SetArray(*array);
    dict.Set().push_back(e);
}

CRef<CDict> CMacProjectGenerator::AddDict(
    CDict& dict, const string& key)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    CDict& d = e->SetPlistObject().SetDict();
    dict.Set().push_back(e);
    return CRef<CDict>(&d);
}

void CMacProjectGenerator::AddCompilerSetting(CDict& settings,
    const SConfigInfo& cfg, const string& key)
{
    string value = GetApp().GetMetaMakefile().GetCompilerOpt(key,cfg);
    if (!value.empty()) {
        AddString( settings, key, value);
    }
}
void CMacProjectGenerator::AddLinkerSetting(CDict& settings,
    const SConfigInfo& cfg, const string& key)
{
    string value = GetApp().GetMetaMakefile().GetLinkerOpt(key,cfg);
    if (!value.empty()) {
        AddString( settings, key, value);
    }
}
void CMacProjectGenerator::AddLibrarianSetting(CDict& settings,
    const SConfigInfo& cfg, const string& key)
{
    string value = GetApp().GetMetaMakefile().GetLibrarianOpt(key,cfg);
    if (!value.empty()) {
        AddString( settings, key, value);
    }
}

string CMacProjectGenerator::GetRelativePath(const string& name, const string* from) const
{
    if (!from) {
        from = &m_SolutionDir;
    }
    if (!SameRootDirs(name, *from)) {
        return name;
    }
    string file_path;
    try {
        file_path = CDirEntry::CreateRelativePath(*from, name);
//  On REAL MacOS, it is not needed
#ifdef PSEUDO_XCODE
        NStr::ReplaceInPlace(file_path, "\\", "/");
#endif
    } catch (CFileException&) {
        file_path = name;
    }
    return file_path;
}

string CMacProjectGenerator::GetProjId( const CProjItem& prj)
{
    string id(prj.m_ID);
    if (prj.m_ProjType == CProjKey::eLib) {
        id += "_ar";
    } else if (prj.m_ProjType == CProjKey::eDll) {
        id += "_dylib";
    } else if (prj.m_ProjType == CProjKey::eApp) {
        id += "_exe";
    }
    return id;
}
string CMacProjectGenerator::GetProjTarget(const CProjItem& prj)
{
    return GetProjId(prj) + "_target";
}
string CMacProjectGenerator::GetProjBuild(const CProjItem& prj)
{
    return GetProjId(prj) + "_build";
}
string CMacProjectGenerator::GetProjProduct(const CProjItem& prj)
{
    return GetProjId(prj) + "_product";
}
string CMacProjectGenerator::GetProjHeaders(const CProjItem& prj)
{
    return GetProjId(prj) + "_headers";
}
string CMacProjectGenerator::GetProjDependency(  const CProjItem& prj)
{
    return GetProjId(prj) + "_dependency";
}
string CMacProjectGenerator::GetTargetName( const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return /*string("lib") +*/ prj.m_ID;
    }
    return prj.m_ID;
}

string CMacProjectGenerator::GetMachOType(  const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return "staticlib";
    } else if (prj.m_ProjType == CProjKey::eDll) {
/*
        if (prj.m_IsBundle) {
            return "mh_bundle";
        }
*/
        return "mh_dylib";
    } else if (prj.m_ProjType == CProjKey::eApp) {
/*
        if (prj.m_IsBundle) {
            return "mh_bundle";
        }
*/
//        return "mh_executable";
        return "mh_execute";
    }
    return "";
}

string CMacProjectGenerator::GetProductType( const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return "com.apple.product-type.library.static";
    } else if (prj.m_ProjType == CProjKey::eDll) {
        return "com.apple.product-type.library.dynamic";
    } else if (prj.m_ProjType == CProjKey::eApp) {
        if (prj.m_IsBundle) {
            return "com.apple.product-type.application";
        }
        return "com.apple.product-type.tool";
    }
    return "";
}
string CMacProjectGenerator::GetExplicitType( const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return "archive.ar";
    } else if (prj.m_ProjType == CProjKey::eDll) {
        return "compiled.mach-o.dylib";
    } else if (prj.m_ProjType == CProjKey::eApp) {
        if (prj.m_IsBundle) {
            return "wrapper.application";
        }
        return "compiled.mach-o.executable";
    }
    return "";
}


#endif


END_NCBI_SCOPE
