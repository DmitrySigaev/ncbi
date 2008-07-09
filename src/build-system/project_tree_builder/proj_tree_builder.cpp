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
#include "proj_tree_builder.hpp"
#include "proj_builder_app.hpp"
#include "proj_src_resolver.hpp"
#include "msvc_prj_defines.hpp"

#include "proj_projects.hpp"
#include <algorithm>

#include "ptb_err_codes.hpp"

BEGIN_NCBI_SCOPE

const char* s_check_separator = " ____ ";

struct PLibExclude
{
    PLibExclude(const string& prj_name, const list<string>& excluded_lib_ids)
        : m_Prj(prj_name)
    {
        copy(excluded_lib_ids.begin(), excluded_lib_ids.end(), 
             inserter(m_ExcludedLib, m_ExcludedLib.end()) );
    }

    bool operator() (const string& lib_id) const
    {
        if (m_ExcludedLib.find(lib_id) != m_ExcludedLib.end()) {
            LOG_POST(Warning << "Project " << m_Prj << ": library excluded by request: " << lib_id);
            return true;
        }
        return false;
    }

private:
    string m_Prj;
    set<string> m_ExcludedLib;
};


//-----------------------------------------------------------------------------
CProjItem::TProjType SMakeProjectT::GetProjType(const string& base_dir,
                                                const string& projname,
                                                SMakeInInfo::TMakeinType type)
{
    string fname = "Makefile." + projname;

    string fname_base = CDirEntry::ConcatPath(base_dir, fname);
    string fname_app = CDirEntry::ConcatPath(base_dir, fname + ".app");
    string fname_lib = CDirEntry::ConcatPath(base_dir, fname + ".lib");
    string fname_dll = CDirEntry::ConcatPath(base_dir, fname + ".dll");
    string fname_msvc = CDirEntry::ConcatPath(base_dir, fname + ".msvcproj");

    switch (type) {
    case SMakeInInfo::eApp:
        if ( CDirEntry(fname_app).Exists()) {
            return CProjKey::eApp;
        }
        break;
    case SMakeInInfo::eLib:
        if ( CDirEntry(fname_lib).Exists()) {
            return CProjKey::eLib;
        }
        break;
    case SMakeInInfo::eDll:
        if ( CDirEntry(fname_dll).Exists()) {
            return CProjKey::eDll;
        }
        break;
    case SMakeInInfo::eMsvc:
        if ( CDirEntry(fname_msvc).Exists()) {
            return CProjKey::eMsvc;
        }
        break;

    default:
        break;
    }

    if ( CDirEntry(fname_lib).Exists() )
        return CProjKey::eLib;
    else if (CDirEntry(fname_dll).Exists() )
        return CProjKey::eDll;
    else if (CDirEntry(fname_app).Exists() )
        return CProjKey::eApp;
    else if (CDirEntry(fname_msvc).Exists() )
        return CProjKey::eMsvc;


    switch (type) {
    case SMakeInInfo::eApp:
        PTB_WARNING_EX(fname_app, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    case SMakeInInfo::eLib:
        PTB_WARNING_EX(fname_lib, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    case SMakeInInfo::eDll:
        PTB_WARNING_EX(fname_dll, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    case SMakeInInfo::eMsvc:
        PTB_WARNING_EX(fname_msvc, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    default:
        PTB_WARNING_EX(fname_base, ePTB_MissingMakefile,
                       "Makefile not found");
        break;
    }
    return CProjKey::eNoProj;
}


bool SMakeProjectT::IsMakeInFile(const string& name)
{
    return name == "Makefile.in";
}


bool SMakeProjectT::IsMakeLibFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".lib");
}

bool SMakeProjectT::IsMakeDllFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".dll");
}


bool SMakeProjectT::IsMakeAppFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".app");
}


bool SMakeProjectT::IsUserProjFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".msvcproj");
}


void SMakeProjectT::DoResolveDefs(CSymResolver& resolver, 
                                  TFiles& files,
                                  const set<string>& keys)
{
    set<string> defs_unresolved;
    map<string,string> defs_resolved;
    NON_CONST_ITERATE(CProjectTreeBuilder::TFiles, p, files) {

        CMsvcProjectMakefile msvc_prj(p->first + ".msvc");
        bool msvc_empty = msvc_prj.IsEmpty();

	    NON_CONST_ITERATE(CSimpleMakeFileContents::TContents, 
                          n, 
                          p->second.m_Contents) {
            
            const string& key    = n->first;
            list<string>& values = n->second;
		    if (keys.find(key) != keys.end()) {
                bool modified = false;
                list<string> new_vals;
                list<string>  redef_values;
                modified = msvc_prj.Redefine(values,redef_values);
                NON_CONST_ITERATE(list<string>, k, redef_values) {
//                NON_CONST_ITERATE(list<string>, k, values) {
                    //iterate all values and try to resolve 
                    const string& val = *k;
                    if( !CSymResolver::HasDefine(val) ) {
                        if (msvc_empty) {
                            new_vals.push_back(val);
                        } else {
                            msvc_prj.Append(new_vals,val);
                        }
                    } else {
                        list<string> resolved_def;
                        string val_define = FilterDefine(val);
	                    resolver.Resolve(val_define, &resolved_def, p->second);
	                    if ( resolved_def.empty() ) {
                            defs_unresolved.insert(val);
		                    new_vals.push_back(val); //not resolved - keep old val
                        } else {
                            defs_resolved[val] = NStr::Join( resolved_def, " ");
                            //was resolved
                            ITERATE(list<string>, l, resolved_def) {
                                const string& define = *l;
                                if ( IsConfigurableDefine(define) ) {
                                    string stripped = StripConfigurableDefine(define);
                                    string resolved_def_str;
                                    GetApp().GetSite().ResolveDefine(stripped, resolved_def_str);
                                    if ( !resolved_def_str.empty() ) {
                                        defs_resolved[define] = resolved_def_str;
                                        list<string> resolved_defs;
                                        NStr::Split(resolved_def_str, 
                                                    LIST_SEPARATOR, 
                                                    resolved_defs);
                                        if (msvc_empty) {
                                            copy(resolved_defs.begin(),
                                                resolved_defs.end(),
                                                back_inserter(new_vals));
                                        } else {
                                            msvc_prj.Append(new_vals,resolved_defs);
                                        }
                                    } else {
// configurable definitions could be described in terms of components
                                        list<string> components;
                                        GetApp().GetSite().GetComponents(stripped, &components);
                                        if (!components.empty()) {
                                            defs_resolved[define] = "Component= " + NStr::Join( components, ", ");
                                        } else {
                                            defs_unresolved.insert(define);
                                        }
                                        if (msvc_empty) {
                                            new_vals.push_back(define);
                                        } else {
                                            msvc_prj.Append(new_vals,define);
                                        }
                                    }

                                } else if (HasConfigurableDefine(define)) {
                                    string raw = ExtractConfigurableDefine(define);
                                    string stripped = StripConfigurableDefine(raw);
                                    string resolved_def_str;
                                    GetApp().GetSite().ResolveDefine(stripped, resolved_def_str);
                                    if (resolved_def_str == " ") {
                                        resolved_def_str.erase();
                                    }
                                    if (msvc_empty) {
                                        new_vals.push_back( NStr::Replace(define, raw, resolved_def_str));
                                    } else {
                                        msvc_prj.Append(new_vals,NStr::Replace(define, raw, resolved_def_str));
                                    }
                                } else {
                                    if (msvc_empty) {
                                        new_vals.push_back(define);
                                    } else {
                                        msvc_prj.Append(new_vals,define);
                                    }
                                }
                            }
		                    modified = true;
                        }
                    }
                }
                if (modified) {
                    msvc_prj.Redefine(new_vals,redef_values);
                    values = redef_values; // by ref!
                }
		    }
        }
    }

    if (!defs_resolved.empty()) {
        string s;
        for (map<string,string>::const_iterator r = defs_resolved.begin();
            r != defs_resolved.end(); ++r) {
            s += ' ';
            s += r->first;
            s += " = ";
            s += r->second;
            s += ";";
        }
        PTB_INFO("Resolved macro definitions: " << s);
    }
    if (!defs_unresolved.empty()) {
        string s;
        for (set<string>::const_iterator u = defs_unresolved.begin();
            u != defs_unresolved.end(); ++u) {
            s += ' ';
            s += *u;
        }
        PTB_WARNING_EX(kEmptyStr, ePTB_MacroUndefined,
                       "Unresolved macro definitions:" << s);
    }
}


string SMakeProjectT::GetOneIncludeDir(const string& flag, const string& token)
{
    size_t token_pos = flag.find(token);
    if (token_pos != NPOS && 
        token_pos + token.length() < flag.length()) {
        return flag.substr(token_pos + token.length()); 
    }
    return "";
}


void SMakeProjectT::CreateIncludeDirs(const list<string>& cpp_flags,
                                      const string&       source_base_dir,
                                      list<string>*       include_dirs)
{
    include_dirs->clear();
    ITERATE(list<string>, p, cpp_flags) {
        const string& flag = *p;
        string token("-I$(includedir)");

        // process -I$(includedir)
        string token_val;
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(includedir)");
        if ( !token_val.empty() ) {
            string dir = 
                CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include,
                                      token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(incdir)");
        if ( !token_val.empty() ) {
            string dir = CDirEntry::ConcatPath(GetApp().m_IncDir,token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }

        // process -I$(srcdir)
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(srcdir)");
        if ( !token_val.empty() )  {
            string dir = 
                CDirEntry::ConcatPath(source_base_dir,
                                      token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }
        
        // process defines like NCBI_C_INCLUDE
        if(CSymResolver::IsDefine(flag)) {
            string dir_all;
            GetApp().GetSite().ResolveDefine(CSymResolver::StripDefine(flag), dir_all);
            if ( !dir_all.empty() ) {
                list<string> dir_list;
                NStr::Split(dir_all, LIST_SEPARATOR, dir_list);
                ITERATE(list<string>, dir_item, dir_list) {
                    const string& dir = *dir_item;
                    if ( CDirEntry(dir).IsDir() ) {
                        include_dirs->push_back(dir);    
                    } else if (CDirEntry::IsAbsolutePath(dir)) {
                        LOG_POST(Warning << "In " << source_base_dir << ": "
                            << flag << " = " << dir << ": "
                            << dir << " not found");
                        include_dirs->push_back(dir);    
                    } else {
                        string d = 
                            CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, dir);
                        d = CDirEntry::NormalizePath(d);
                        d = CDirEntry::AddTrailingPathSeparator(d);
                        if ( CDirEntry(d).IsDir() ) {
                            include_dirs->push_back(d);    
                        } else {
                            LOG_POST(Warning << flag << " = " << dir << ": "
                                        << dir << " not found");
                        }
                    }
                }
            }
        }

        // process additional include dirs for LibChoices
        if(CSymResolver::IsDefine(flag)) {
            string sflag = CSymResolver::StripDefine(flag);
            list<string> libchoices_abs_includes ;
            GetApp().GetSite().GetLibChoiceIncludes(sflag, 
                                                    &libchoices_abs_includes);
            ITERATE(list<string>, n, libchoices_abs_includes) {
                const string& dir = *n;
                if ( !dir.empty() ) {
                    include_dirs->push_back(dir);    
                }
            }
        }
    }
    include_dirs->sort();
    include_dirs->unique();
}


void SMakeProjectT::CreateDefines(const list<string>& cpp_flags,
                                  list<string>*       defines)
{
    defines->clear();

    ITERATE(list<string>, p, cpp_flags) {
        const string& flag = *p;
        if ( NStr::StartsWith(flag, "-D") ) {
            defines->push_back(flag.substr(2));
        }
    }
}


void SMakeProjectT::Create3PartyLibs(const list<string>& libs_flags, 
                                     list<string>*       libs_list)
{
    libs_list->clear();
    ITERATE(list<string>, p, libs_flags) {
        const string& flag = *p;
        if ( IsConfigurableDefine(flag) ) {
            libs_list->push_back(StripConfigurableDefine(flag));    
        }
    }
}


void SMakeProjectT::AnalyzeMakeIn
    (const CSimpleMakeFileContents& makein_contents,
     TMakeInInfoList*               info)
{
    info->clear();
    CSimpleMakeFileContents::TContents::const_iterator p;

    p = makein_contents.m_Contents.find("LIB_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("EXPENDABLE_LIB_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
    }
    p = makein_contents.m_Contents.find("POTENTIAL_LIB_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Potential))); 
    }

    p = makein_contents.m_Contents.find("DLL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDll, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("EXPENDABLE_DLL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDll, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
    }
    p = makein_contents.m_Contents.find("POTENTIAL_DLL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDll, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Potential))); 
    }

    p = makein_contents.m_Contents.find("APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("EXPENDABLE_APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
    }
    p = makein_contents.m_Contents.find("POTENTIAL_APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Potential))); 
    }

    p = makein_contents.m_Contents.find("ASN_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eAsn, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("DTD_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eAsn, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("XSD_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eAsn, p->second,
            makein_contents.GetMakeType())); 
    }

    p = makein_contents.m_Contents.find("MSVC_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eMsvc, p->second,
            makein_contents.GetMakeType())); 
    }

    //TODO - DLL_PROJ
}


string SMakeProjectT::CreateMakeAppLibFileName
                (const string&            base_dir,
                 const string&            projname,
                 SMakeInInfo::TMakeinType type)
{
    CProjItem::TProjType proj_type = 
            SMakeProjectT::GetProjType(base_dir, projname, type);

    string fname = "Makefile." + projname;
    
    if (proj_type==CProjKey::eLib)
        return fname + ".lib";

    if (proj_type==CProjKey::eDll)
        return fname + ".dll";

    if (proj_type==CProjKey::eApp)
        return fname + ".app";

    if (proj_type==CProjKey::eMsvc)
        return fname + ".msvcproj";

    return "";
}


void SMakeProjectT::CreateFullPathes(const string&      dir, 
                                     const list<string> files,
                                     list<string>*      full_pathes)
{
    ITERATE(list<string>, p, files) {
        string full_path = CDirEntry::ConcatPath(dir, *p);
        full_pathes->push_back(full_path);
    }
}


void SMakeProjectT::ConvertLibDepends(const list<string>& depends,
                                      list<CProjKey>*     depends_ids)
{
    list<string> depends_libs;
    SMakeProjectT::ConvertLibDependsMacro(depends, depends_libs);

    depends_ids->clear();
    ITERATE(list<string>, p, depends_libs) {
        string id = *p;
        if(CSymResolver::IsDefine(id)) {
            string def;
            GetApp().GetSite().ResolveDefine(CSymResolver::StripDefine(id), def);
            list<string> resolved_def;
            NStr::Split(def, LIST_SEPARATOR, resolved_def);
            ITERATE(list<string>, r, resolved_def) {
                id = *r;
                depends_ids->push_back(CProjKey(CProjKey::eLib, id));
            }
        } else {
            depends_ids->push_back(CProjKey(CProjKey::eLib, id));
        }
    }
    depends_ids->sort();
    depends_ids->unique();
}

void SMakeProjectT::ConvertLibDependsMacro(const list<string>& depends, 
                                           list<string>& depends_libs)
{
    depends_libs.clear();
    ITERATE(list<string>, p, depends) {
        const string& id = *p;
        if (id[0] == '#') {
            break;
        }
        string lib = GetApp().GetSite().ProcessMacros(id,false);
        if (!lib.empty()) {
            depends_libs.push_back(lib);
        } else {
            depends_libs.push_back(id);
        }
    }
}


bool SMakeProjectT::IsConfigurableDefine(const string& define)
{
    return  NStr::StartsWith(define, "@")  &&
            NStr::EndsWith  (define, "@");

}


string SMakeProjectT::StripConfigurableDefine(const string& define)
{
    return IsConfigurableDefine(define) ? 
                define.substr(1, define.length() - 2): "";
}

bool   SMakeProjectT::HasConfigurableDefine(const string& define)
{
    return define.find("@") != string::npos;
}

string SMakeProjectT::ExtractConfigurableDefine (const string& define)
{
    string::size_type start, end;
    start = define.find("@");
    end = define.find("@",start+1);
    if (end == string::npos) {
        LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + define);
        return define;
    }
    return define.substr(start,end-start+1);
}

//-----------------------------------------------------------------------------
void SAppProjectT::CreateNcbiCToolkitLibs(const CSimpleMakeFileContents& makefile,
                                          list<string>* libs_list)
{
    CSimpleMakeFileContents::TContents::const_iterator k = 
    makefile.m_Contents.find("NCBI_C_LIBS");
    if (k == makefile.m_Contents.end()) {
        return;
    }
    const list<string>& values = k->second;

    ITERATE(list<string>, p, values) {
        const string& val = *p;
        if ( NStr::StartsWith(val, "-l") ) {
            string lib_id = val.substr(2);
            libs_list->push_back(lib_id);
        } else {
            libs_list->push_back(val);
        }
    }

    libs_list->sort();
    libs_list->unique();
}

CProjKey SAppProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makeapp , 
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    CProjectItemsTree::TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {
        /// FIXME: items may not be really missing here; they may just be
        /// excluded based on user preference
        /**
        PTB_WARNING_EX(applib_mfilepath, ePTB_MissingMakefile,
                       "Makefile not found");
                       **/
        return CProjKey();
    }
    
    const CSimpleMakeFileContents& makefile = m->second;
    string full_makefile_name = "Makefile." + proj_name + ".app";
    string full_makefile_path =
        CDirEntry::ConcatPath(applib_mfilepath, full_makefile_name);

    CSimpleMakeFileContents::TContents::const_iterator k = 
        makefile.m_Contents.find("SRC");
    if (k == makefile.m_Contents.end()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }

    //sources - relative  pathes from source_base_dir
    //We'll create relative pathes from them
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    //depends
    list<string> depends;
    k = makefile.m_Contents.find("LIB");
    if (k != makefile.m_Contents.end()) {
//        depends = k->second;
        ITERATE(list<string>, i, k->second) {
            depends.push_back(
                NStr::Replace(NStr::Replace(*i, "-dll", kEmptyStr),
                              "-static", kEmptyStr));
        }
    }
    //Adjust depends by information from msvc Makefile
    CMsvcProjectMakefile project_makefile( CDirEntry::ConcatPath(
        source_base_dir, CreateMsvcProjectMakefileName(proj_name, CProjKey::eApp)));

    list<string> added_depends;
    project_makefile.GetAdditionalLIB(SConfigInfo(), &added_depends);

    list<string> excluded_depends;
    project_makefile.GetExcludedLIB(SConfigInfo(), &excluded_depends);

    list<string> adj_depends(depends);
    copy(added_depends.begin(), 
         added_depends.end(), back_inserter(adj_depends));
    adj_depends.sort();
    adj_depends.unique();

    PLibExclude pred(proj_name, excluded_depends);
    EraseIf(adj_depends, pred);

    list<CProjKey> depends_ids;
    SMakeProjectT::ConvertLibDepends(adj_depends, &depends_ids);
    ///////////////////////////////////

    //requires
    list<string> requires;
    k = makefile.m_Contents.find("REQUIRES");
    if (k != makefile.m_Contents.end()) {
        project_makefile.Redefine(k->second,requires);        
    }

    //project name
    k = makefile.m_Contents.find("APP");
    if (k == makefile.m_Contents.end()  ||  k->second.empty()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "APP is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "APP is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }
    string proj_id = k->second.front();

    //LIBS
    list<string> libs_3_party;
    k = makefile.m_Contents.find("LIBS");
    if (k != makefile.m_Contents.end()) {
        const list<string> libs_flags = k->second;
        SMakeProjectT::Create3PartyLibs(libs_flags, &libs_3_party);
    }
    
    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = makefile.m_Contents.find("CPPFLAGS");
    if (k != makefile.m_Contents.end()) {
        const list<string> cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);
    }

    //NCBI_C_LIBS - Special case for NCBI C Toolkit
    k = makefile.m_Contents.find("NCBI_C_LIBS");
    list<string> ncbi_clibs;
    if (k != makefile.m_Contents.end()) {
        libs_3_party.push_back("NCBI_C_LIBS");
        CreateNcbiCToolkitLibs(makefile, &ncbi_clibs);
    }
    
    CProjItem project(CProjKey::eApp, 
                      proj_name, 
                      proj_id,
                      source_base_dir,
                      sources, 
                      depends_ids,
                      requires,
                      libs_3_party,
                      include_dirs,
                      defines,
                      maketype,
        IdentifySlnGUID(source_base_dir, CProjKey(CProjKey::eApp, proj_id)));
    //
    project.m_NcbiCLibs = ncbi_clibs;

    //DATATOOL_SRC
    k = makefile.m_Contents.find("DATATOOL_SRC");
    if ( k != makefile.m_Contents.end() ) {
        //Add depends from datatoool for ASN projects
        if (!GetApp().GetDatatoolId().empty()) {
            project.m_Depends.push_back(CProjKey(CProjKey::eApp, GetApp().GetDatatoolId()));
        }
        const list<string> datatool_src_list = k->second;
        ITERATE(list<string>, i, datatool_src_list) {

            const string& src = *i;
            //Will process .asn or .dtd files
            string source_file_path = 
                CDirEntry::ConcatPath(source_base_dir, src);
            source_file_path = CDirEntry::NormalizePath(source_file_path);
            if ( CDirEntry(source_file_path + ".asn").Exists() )
                source_file_path += ".asn";
            else if ( CDirEntry(source_file_path + ".dtd").Exists() )
                source_file_path += ".dtd";
            else if ( CDirEntry(source_file_path + ".xsd").Exists() )
                source_file_path += ".xsd";

            CDataToolGeneratedSrc data_tool_src;
            CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
            if ( !data_tool_src.IsEmpty() )
                project.m_DatatoolSources.push_back(data_tool_src);
        }
    }

// assemble check info
    string check_info;
    string check_dir = CDirEntry::CreateRelativePath(
        GetApp().GetProjectTreeInfo().m_Src, source_base_dir);
    NStr::ReplaceInPlace(check_dir,"\\","/");
    if (NStr::EndsWith(check_dir,'/')) {
        check_dir.erase(check_dir.size()-1,1);
    }
    string check_testname(proj_name);
    string check_appname(proj_id);

    string check_copy;
    k = makefile.m_Contents.find("CHECK_COPY");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        check_copy = k->second.front();
    }
    string check_timeout("200");
    k = makefile.m_Contents.find("CHECK_TIMEOUT");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        check_timeout = k->second.front();
    }
    string check_requires;
    k = makefile.m_Contents.find("CHECK_REQUIRES");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        check_requires = k->second.front();
    }
    string check_authors;
    k = makefile.m_Contents.find("CHECK_AUTHORS");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        check_authors = k->second.front();
    }
    k = makefile.m_Contents.find("CHECK_CMD");
    if ( k != makefile.m_Contents.end() ) {
        const list<string> check_cmd_list = k->second;
        string test_name("/CHECK_NAME");
        ITERATE(list<string>, i, check_cmd_list) {
            string check_cmd(*i), check_name;
            string::size_type  n = check_cmd.find(test_name);
            if (n != string::npos) {
                check_name = check_cmd.substr(n+test_name.size());
                check_cmd = check_cmd.substr(0,n);
            }
            CNcbiOstrstream check;
            check << check_dir
                << s_check_separator << check_testname
                << s_check_separator << check_appname
                << s_check_separator << check_cmd
                << s_check_separator << check_name
                << s_check_separator << check_copy
                << s_check_separator << check_timeout
                << s_check_separator << check_requires
                << s_check_separator << check_authors;
            project.m_CheckInfo.push_back( CNcbiOstrstreamToString(check) );
        }
    } else {
        string check_cmd(check_appname), check_name;
        CNcbiOstrstream check;
        check << check_dir
            << s_check_separator << check_testname
            << s_check_separator << check_appname
            << s_check_separator << check_cmd
            << s_check_separator << check_name
            << s_check_separator << check_copy
            << s_check_separator << check_timeout
            << s_check_separator << check_requires
            << s_check_separator << check_authors;
        project.m_CheckInfo.push_back( CNcbiOstrstreamToString(check) );
    }

    CProjKey proj_key(CProjKey::eApp, proj_id);
    tree->m_Projects[proj_key] = project;

    return proj_key;
}


//-----------------------------------------------------------------------------
CProjKey SLibProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makelib , 
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    TFiles::const_iterator m = makelib.find(applib_mfilepath);
    if (m == makelib.end()) {
        /// FIXME: items may not be really missing here; they may just be
        /// excluded based on user preference
        /**
        PTB_WARNING_EX(applib_mfilepath, ePTB_MissingMakefile,
                       "Makefile not found");
                       **/
        return CProjKey();
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    string full_makefile_name = "Makefile." + proj_name + ".lib";
    string full_makefile_path =
        CDirEntry::ConcatPath(applib_mfilepath, full_makefile_name);

    if (k == m->second.m_Contents.end()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }

    // sources - relative pathes from source_base_dir
    // We'll create relative pathes from them)
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    // depends - TODO
    list<CProjKey> depends_ids;
    k = m->second.m_Contents.find("ASN_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &depends_ids);
    }

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end()) {
        CMsvcProjectMakefile project_makefile( CDirEntry::ConcatPath(
            source_base_dir, CreateMsvcProjectMakefileName(proj_name, CProjKey::eLib)));
        project_makefile.Redefine(k->second,requires);        
    }

    //project name
    k = m->second.m_Contents.find("LIB");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "LIB is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "LIB is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }
    string proj_id = k->second.front();

    //LIBS
    list<string> libs_3_party;
    k = m->second.m_Contents.find("LIBS");
    if (k != m->second.m_Contents.end()) {
        const list<string> libs_flags = k->second;
        SMakeProjectT::Create3PartyLibs(libs_flags, &libs_3_party);
    }
    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = m->second.m_Contents.find("CPPFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string> cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);

    }
    
    string lib_or_dll;
    k = m->second.m_Contents.find("LIB_OR_DLL");
    if (k != m->second.m_Contents.end()) {
        lib_or_dll = k->second.front();
    }
    string dll_host;
    if (!lib_or_dll.empty() ||
        CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            list<string> dll_depends;
            k = m->second.m_Contents.find("DLL_LIB");
            if (k != m->second.m_Contents.end()) {
                ITERATE(list<string>, i, k->second) {
                    dll_depends.push_back(
                        NStr::Replace(NStr::Replace(*i, "-dll", kEmptyStr),
                              "-static", kEmptyStr));
                }
            }
            list<CProjKey> dll_depends_ids;
            SMakeProjectT::ConvertLibDepends(dll_depends, &dll_depends_ids);
            copy(dll_depends_ids.begin(), 
                    dll_depends_ids.end(), 
                    back_inserter(depends_ids));
            if (NStr::CompareNocase(lib_or_dll,"dll") == 0 ||
                NStr::CompareNocase(lib_or_dll,"both") == 0) {
#if 0
                if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
                    dll_host = proj_id;
                } else {
                    dll_host =  proj_name;
                }
#else
		dll_host = proj_id;
#endif
            }
        }
    }
    CProjKey proj_key(CProjKey::eLib, proj_id);
    tree->m_Projects[proj_key] = CProjItem(CProjKey::eLib,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends_ids,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines,
                                           maketype,
        IdentifySlnGUID(source_base_dir, proj_key));

    k = m->second.m_Contents.find("HEADER_EXPORT");
    if (k != m->second.m_Contents.end()) {
        (tree->m_Projects[proj_key]).m_ExportHeaders = k->second;
    }
    k = m->second.m_Contents.find("PACKAGE_EXPORT");
    if (k != m->second.m_Contents.end()) {
        (tree->m_Projects[proj_key]).m_ExportHeadersDest = k->second.front();
    }

    if (!dll_host.empty()) {
        tree->m_Projects[proj_key].m_DllHost = dll_host;
        CProjKey proj_dll(CProjKey::eDll, dll_host);
        CProjItem item_dll = tree->m_Projects[proj_dll];
        item_dll.m_ProjType = CProjKey::eDll;
#if 0
        item_dll.m_Name = dll_host;
        item_dll.m_ID = dll_host;
#else
        item_dll.m_Name = proj_name;
        item_dll.m_ID = proj_id;
#endif
        item_dll.m_SourcesBaseDir = source_base_dir;
        item_dll.m_MakeType = maketype;
        item_dll.m_HostedLibs.push_back(proj_id);
        item_dll.m_GUID  = IdentifySlnGUID(source_base_dir, proj_dll);
        tree->m_Projects[proj_dll] = item_dll;
    }
    return proj_key;
}

//-----------------------------------------------------------------------------
CProjKey SDllProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makedll , 
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    TFiles::const_iterator m = makedll.find(applib_mfilepath);
    if (m == makedll.end()) {

        LOG_POST(Info << "Dll Makefile not found: " << applib_mfilepath);
        return CProjKey();
    }
    CSimpleMakeFileContents::TContents::const_iterator k;

    //DLL
    k = m->second.m_Contents.find("DLL");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {
        LOG_POST(Info << "No DLL specified in Makefile." << proj_name
                      << ".dll  at " << applib_mfilepath);
        return CProjKey();
    }
    string proj_id = k->second.front();

    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = m->second.m_Contents.find("CPPFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string> cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);

    }

    list<CProjKey> depends_ids;
    k = m->second.m_Contents.find("DEPENDENCIES");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &depends_ids);
    }

    list<string> requires;
    requires.push_back("DLL");

    list<string> sources;
    list<string> libs_3_party;

    CProjKey proj_key(CProjKey::eDll, proj_id);
    tree->m_Projects[proj_key] = CProjItem(CProjKey::eDll,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends_ids,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines,
                                           maketype,
        IdentifySlnGUID(source_base_dir, proj_key));

    k = m->second.m_Contents.find("HOSTED_LIBS");
    if (k != m->second.m_Contents.end()) {
        tree->m_Projects[proj_key].m_HostedLibs = k->second;
    }
    return proj_key;
}

//-----------------------------------------------------------------------------
CProjKey SAsnProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makeapp, 
                                const TFiles& makelib, 
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    TAsnType asn_type = GetAsnProjectType(applib_mfilepath, makeapp, makelib);
    if (asn_type == eMultiple) {
        return SAsnProjectMultipleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree, maketype);
    }
    if(asn_type == eSingle) {
        return SAsnProjectSingleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree, maketype);
    }
    return CProjKey();
}


SAsnProjectT::TAsnType SAsnProjectT::GetAsnProjectType(const string& applib_mfilepath,
                                                       const TFiles& makeapp,
                                                       const TFiles& makelib)
{
    TFiles::const_iterator p = makeapp.find(applib_mfilepath);
    if ( p != makeapp.end() ) {
        const CSimpleMakeFileContents& fc = p->second;
        if (fc.m_Contents.find("ASN") != fc.m_Contents.end() )
            return eMultiple;
        else
            return eSingle;
    }

    p = makelib.find(applib_mfilepath);
    if ( p != makelib.end() ) {
        const CSimpleMakeFileContents& fc = p->second;
        if (fc.m_Contents.find("ASN") != fc.m_Contents.end() )
            return eMultiple;
        else
            return eSingle;
    }
    return eNoAsn;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectSingleT::DoCreate(const string& source_base_dir,
                                      const string& proj_name,
                                      const string& applib_mfilepath,
                                      const TFiles& makeapp, 
                                      const TFiles& makelib, 
                                      CProjectItemsTree* tree,
                                      EMakeFileType maketype)
{
    CProjItem::TProjType proj_type = 
        IsMakeLibFile( CDirEntry(applib_mfilepath).GetName()) ? CProjKey::eLib : CProjKey::eApp;
    
    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
            SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree, maketype) : 
            SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree, maketype);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "ASN project not found: " + proj_id.Id());
        return CProjKey();
    }
    CProjItem& project = p->second;

    //Add depends from datatoool for ASN projects
    if (!GetApp().GetDatatoolId().empty()) {
        project.m_Depends.push_back(CProjKey(CProjKey::eApp,
                                            GetApp().GetDatatoolId()));
    }

    //Will process .asn or .dtd files
    string source_file_path = CDirEntry::ConcatPath(source_base_dir, proj_name);
    if ( CDirEntry(source_file_path + ".asn").Exists() )
        source_file_path += ".asn";
    else if ( CDirEntry(source_file_path + ".dtd").Exists() )
        source_file_path += ".dtd";
    else if ( CDirEntry(source_file_path + ".xsd").Exists() )
        source_file_path += ".xsd";

    CDataToolGeneratedSrc data_tool_src;
    CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
    if ( !data_tool_src.IsEmpty() )
        project.m_DatatoolSources.push_back(data_tool_src);

    return proj_id;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectMultipleT::DoCreate(const string& source_base_dir,
                                        const string& proj_name,
                                        const string& applib_mfilepath,
                                        const TFiles& makeapp, 
                                        const TFiles& makelib, 
                                        CProjectItemsTree* tree,
                                        EMakeFileType maketype)
{
    CProjItem::TProjType proj_type = 
        IsMakeLibFile( CDirEntry(applib_mfilepath).GetName()) ? CProjKey::eLib : CProjKey::eApp;
    

    const TFiles& makefile = proj_type == CProjKey::eLib? makelib : makeapp;
    TFiles::const_iterator m = makefile.find(applib_mfilepath);
    if (m == makefile.end()) {

        LOG_POST(Info << "AsnProject Makefile not found: " << applib_mfilepath);
        return CProjKey();
    }
    const CSimpleMakeFileContents& fc = m->second;

    // ASN
    CSimpleMakeFileContents::TContents::const_iterator k = 
        fc.m_Contents.find("ASN");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Info << "No ASN specified in Makefile: project " << proj_name
                      << "  at " << applib_mfilepath);
        return CProjKey();
    }
    const list<string> asn_names = k->second;

    list<CDataToolGeneratedSrc> datatool_sources;
    ITERATE(list<string>, p, asn_names) {
        const string& asn = *p;
        
        // this dir
        string asn_path_abs = CDirEntry::NormalizePath(source_base_dir);
        asn_path_abs = CDirEntry::AddTrailingPathSeparator(asn_path_abs);
        asn_path_abs = CDirEntry::ConcatPath(asn_path_abs, asn);
        if ( CDirEntry(asn_path_abs + ".asn").Exists() )
            asn_path_abs += ".asn";
        else if ( CDirEntry(asn_path_abs + ".dtd").Exists() )
            asn_path_abs += ".dtd";
        else if ( CDirEntry(asn_path_abs + ".xsd").Exists() )
            asn_path_abs += ".xsd";
        else {
            // one level up
            string parent_dir_abs = ParentDir(source_base_dir);
            string asn_dir_abs = CDirEntry::ConcatPath(parent_dir_abs, asn);
            asn_dir_abs = CDirEntry::NormalizePath(asn_dir_abs);
            asn_dir_abs = CDirEntry::AddTrailingPathSeparator(asn_dir_abs);
        
            asn_path_abs = CDirEntry::ConcatPath(asn_dir_abs, asn);
            if ( CDirEntry(asn_path_abs + ".asn").Exists() )
                asn_path_abs += ".asn";
            else if ( CDirEntry(asn_path_abs + ".dtd").Exists() )
                asn_path_abs += ".dtd";
            else if ( CDirEntry(asn_path_abs + ".xsd").Exists() )
                asn_path_abs += ".xsd";
            else {
                PTB_ERROR_EX(asn_path_abs, ePTB_FileNotFound,
                            "ASN spec file not found");
            }
        }

        CDataToolGeneratedSrc data_tool_src;
        CDataToolGeneratedSrc::LoadFrom(asn_path_abs, &data_tool_src);
        if ( !data_tool_src.IsEmpty() )
            datatool_sources.push_back(data_tool_src);

    }

    // SRC
    k = fc.m_Contents.find("SRC");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Info << "No SRC specified in Makefile: project " << proj_name
                      << "  at " << applib_mfilepath);
        return CProjKey();
    }
    const list<string> src_list = k->second;
    list<string> sources;
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            sources.push_back(src);
    }

    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
        SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree, maketype) :
        SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree, maketype);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "ASN project not found: " << proj_id.Id()
                       << " at " << applib_mfilepath);
        return CProjKey();
    }
    CProjItem& project = p->second;

    // Adjust created proj item
    //SRC - 
    project.m_Sources.clear();
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            project.m_Sources.push_front(src);    
    }
    project.m_Sources.remove(proj_name);
    project.m_Sources.push_back(proj_name + "__");
    project.m_Sources.push_back(proj_name + "___");
    ITERATE(list<string>, p, asn_names) {
        const string& asn = *p;
        if (asn == proj_name)
            continue;
        string src(1, CDirEntry::GetPathSeparator());
        src += "..";
        src += CDirEntry::GetPathSeparator();
        src += asn;
        src += CDirEntry::GetPathSeparator();
        src += asn;

        project.m_Sources.remove(asn);
        project.m_Sources.push_back(src + "__");
        project.m_Sources.push_back(src + "___");
    }

    project.m_DatatoolSources = datatool_sources;

    //Add depends from datatoool for ASN projects
    if (!GetApp().GetDatatoolId().empty()) {
        project.m_Depends.push_back(CProjKey(CProjKey::eApp, 
                                            GetApp().GetDatatoolId()));
    }

    return proj_id;
}

//-----------------------------------------------------------------------------
CProjKey SMsvcProjectT::DoCreate(const string&      source_base_dir,
                                 const string&      proj_name,
                                 const string&      applib_mfilepath,
                                 const TFiles&      makemsvc, 
                                 CProjectItemsTree* tree,
                                 EMakeFileType maketype)
{
    TFiles::const_iterator m = makemsvc.find(applib_mfilepath);
    if (m == makemsvc.end()) {

        LOG_POST(Info << "MsvcProject Makefile not found: " << applib_mfilepath);
        return CProjKey();
    }

    // VCPROJ - will map to src
    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("VCPROJ");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Info << "No VCPROJ specified in Makefile: project " << proj_name
                      << "  at " << applib_mfilepath);
        return CProjKey();
    }
    string vcproj_file;
    list<string> sources;
    ITERATE(list<string>, s, k->second) {
        string d = GetApp().ProcessLocationMacros( *s );
        vcproj_file = d;
        if (CDirEntry::IsAbsolutePath(d)) {
            d = CDirEntry::CreateRelativePath( source_base_dir, d);
        }
        sources.push_back( d );
        break;
    }

    // depends - 
    list<CProjKey> depends_ids;
    k = m->second.m_Contents.find("LIB_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eLib, *p));
        }
    }
    k = m->second.m_Contents.find("APP_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eApp, *p));
        }
    }
    k = m->second.m_Contents.find("DLL_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eDll, *p));
        }
    }
    k = m->second.m_Contents.find("MSVC_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eMsvc, *p));
        }
    }

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end()) {
        CMsvcProjectMakefile project_makefile( CDirEntry::ConcatPath(
            source_base_dir, CreateMsvcProjectMakefileName(proj_name, CProjKey::eMsvc)));
        project_makefile.Redefine(k->second,requires);        
    }

    //project id
    k = m->second.m_Contents.find("MSVC_PROJ");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST(Info << "No MSVC_PROJ specified in Makefile: project " << proj_name
                      << "  at " << applib_mfilepath);
        return CProjKey();
    }
    string proj_id = k->second.front();

    list<string> libs_3_party;
    list<string> include_dirs;
    list<string> defines;

    CMsvcProjectMakefile project_makefile
                       ((CDirEntry::ConcatPath( source_base_dir, 
                           CreateMsvcProjectMakefileName(proj_name, 
                                                         CProjKey::eMsvc))));
    CProjKey proj_key(CProjKey::eMsvc, proj_id);
    tree->m_Projects[proj_key] = CProjItem(CProjKey::eMsvc,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends_ids,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines,
                                           maketype,
        IdentifySlnGUID(vcproj_file, proj_key));
//                                           project_makefile.GetGUID());
    return proj_key;
}
//-----------------------------------------------------------------------------
void 
CProjectTreeBuilder::BuildOneProjectTree(const IProjectFilter* filter,
                                         const string&         root_src_path,
                                         CProjectItemsTree*    tree)
{
    SMakeFiles subtree_makefiles;

    ProcessDir(root_src_path, 
               true,
               filter,
               &subtree_makefiles, eMakeType_Undefined);

    // Resolve macrodefines
    list<string> metadata_files;
    GetApp().GetMetaDataFiles(&metadata_files);
    CSymResolver resolver;
    resolver += GetApp().GetSite().GetMacros();
    ITERATE(list<string>, p, metadata_files) {
	    resolver += CSymResolver(CDirEntry::ConcatPath(root_src_path, *p));
	}
    ResolveDefs(resolver, subtree_makefiles);

    // Build projects tree
    CProjectItemsTree::CreateFrom(root_src_path,
                                  subtree_makefiles.m_In, 
                                  subtree_makefiles.m_Lib, 
                                  subtree_makefiles.m_Dll, 
                                  subtree_makefiles.m_App,
                                  subtree_makefiles.m_User, tree);
}


void 
CProjectTreeBuilder::BuildProjectTree(const IProjectFilter* filter,
                                      const string&         root_src_path,
                                      CProjectItemsTree*    tree)
{
    // Build subtree
    CProjectItemsTree target_tree;
    BuildOneProjectTree(filter, root_src_path, &target_tree);

    // Analyze subtree dependencies
    list<CProjKey> external_depends;
    target_tree.GetExternalDepends(&external_depends);

    // We have to add more projects to the target tree
    if ( !external_depends.empty()  &&  !filter->PassAll() ) {

        list<CProjKey> depends_to_resolve = external_depends;

        while ( !depends_to_resolve.empty() ) {
            bool modified = false;
            ITERATE(list<CProjKey>, p, depends_to_resolve) {
                // id of the project we have to resolve
                const CProjKey& prj_id = *p;
                CProjectItemsTree::TProjects::const_iterator n = 
                               GetApp().GetWholeTree().m_Projects.find(prj_id);

                if (n != GetApp().GetWholeTree().m_Projects.end()) {
                    //insert this project into the target_tree
                    target_tree.m_Projects[prj_id] = n->second;
                    modified = true;
                } else {
                    /// FIXME: is this needed?
                    _TRACE("Project not found: " + prj_id.Id());
                }
            }
    
            if (!modified) {
                //done - no projects has been added to target_tree
                AddDatatoolSourcesDepends(&target_tree);
                *tree = target_tree;
                return;
            } else {
                //continue resolving dependencies
                target_tree.GetExternalDepends(&depends_to_resolve);
            }
        }
    }

    AddDatatoolSourcesDepends(&target_tree);
    *tree = target_tree;
}


void CProjectTreeBuilder::ProcessDir(const string&         dir_name, 
                                     bool                  is_root,
                                     const IProjectFilter* filter,
                                     SMakeFiles*           makefiles,
                                     EMakeFileType         maketype)
{
    // Node - Makefile.in should present
    // this is true if and only if there are also Makefile.*.lib or
    // Makefile.*.app project makefiles to process
    string node_path = 
        CDirEntry::ConcatPath(dir_name, 
                              GetApp().GetProjectTreeInfo().m_TreeNode);
    if ( !is_root  &&  !CDirEntry(node_path).Exists() ) {
        CDir::TGetEntriesFlags flags = CDir::fIgnoreRecursive;
        CDir::TEntries entries =
            CDir(dir_name).GetEntries("Makefile.*.lib", flags);
        if (entries.empty()) {
            entries = CDir(dir_name).GetEntries("Makefile.*.app", flags);
        }
        if ( !entries.empty() ) {
            PTB_WARNING_EX(node_path, ePTB_MissingMakefile,
                           "Makefile.in missing");
        }
        return;
    }
    if (!is_root &&
        CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        // on UNIX the build tree is already configured,
        // we check if this particular subtree is enabled (ie, exists) there
        string subtree =
            CDirEntry::CreateRelativePath(
                GetApp().GetProjectTreeInfo().m_Src, dir_name);
        subtree = CDirEntry::ConcatPath(CDirEntry(GetApp().m_Solution).GetDir(), subtree);
        if (!CDirEntry(subtree).Exists()) {
            PTB_INFO_EX(subtree, ePTB_NoError,
                        "skipped missing subtree");
            return;
        }
    }
    
    bool weak=false;
    bool process_projects = !is_root && filter->CheckProject(dir_name,&weak);
    if (!process_projects && !weak && !is_root) {
        return;
    }
    
    // Process Makefile.in
    map<string, EMakeFileType> subprojects;
    map<string, EMakeFileType> appprojects;
    map<string, EMakeFileType> libprojects;
    map<string, EMakeFileType> dllprojects;

    if ( process_projects || weak ) {
        ProcessMakeInFile(node_path, makefiles, maketype);
        TFiles::const_iterator p = makefiles->m_In.find(node_path);
        if (p != makefiles->m_In.end()) {
        const CSimpleMakeFileContents& makefile = p->second;
        CSimpleMakeFileContents::TContents::const_iterator k;
        int j;
        string subproj[] = {"SUB_PROJ","EXPENDABLE_SUB_PROJ","POTENTIAL_SUB_PROJ",""};
        EMakeFileType subtype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            subtype[2] = eMakeType_Excluded;
        }
        for (j=0; !subproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(subproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    subprojects[*i] = max(maketype, subtype[j]);
                }
            }
        }
        if ( process_projects ) {
        string appproj[] = {"APP_PROJ","EXPENDABLE_APP_PROJ","POTENTIAL_APP_PROJ",""};
        EMakeFileType apptype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            apptype[2] = eMakeType_Excluded;
        }
        for (j=0; !appproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(appproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    appprojects["Makefile." + *i + ".app"] = max(maketype, apptype[j]);
                }
            }
        }
        string libproj[] = {"LIB_PROJ","EXPENDABLE_LIB_PROJ","POTENTIAL_LIB_PROJ",
                            "ASN_PROJ","DTD_PROJ","XSD_PROJ",""};
        EMakeFileType libtype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential,
            eMakeType_Undefined, eMakeType_Undefined, eMakeType_Undefined};
        if (filter->ExcludePotential()) {
            libtype[2] = eMakeType_Excluded;
        }
        for (j=0; !libproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(libproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    libprojects["Makefile." + *i + ".lib"] = max(maketype, libtype[j]);
                }
            }
        }
        string dllproj[] = {"DLL_PROJ","EXPENDABLE_DLL_PROJ","POTENTIAL_DLL_PROJ",""};
        EMakeFileType dlltype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            dlltype[2] = eMakeType_Excluded;
        }
        for (j=0; !dllproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(dllproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    dllprojects["Makefile." + *i + ".dll"] = max(maketype, dlltype[j]);
                }
            }
        }
        }
        }
    }

    // Process Makefile.*.lib
    if ( process_projects && !libprojects.empty()) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.lib");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (libprojects.find(name) != libprojects.end() &&
                SMakeProjectT::IsMakeLibFile(name) )
	            ProcessMakeLibFile(dir_entry->GetPath(), makefiles, libprojects[name]);

        }
    }
    // Process Makefile.*.dll
    if ( process_projects && !dllprojects.empty()) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.dll");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (dllprojects.find(name) != dllprojects.end() &&
                SMakeProjectT::IsMakeDllFile(name) )
	            ProcessMakeDllFile(dir_entry->GetPath(), makefiles, dllprojects[name]);

        }
    }
    // Process Makefile.*.app
    if ( process_projects && !appprojects.empty() ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.app");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (appprojects.find(name) != appprojects.end() &&
                SMakeProjectT::IsMakeAppFile(name) )
	            ProcessMakeAppFile(dir_entry->GetPath(), makefiles, appprojects[name]);

        }
    }
    // Process Makefile.*.msvcproj
    if ( process_projects ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.msvcproj");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            if ( SMakeProjectT::IsUserProjFile(dir_entry->GetName()) )
	            ProcessUserProjFile(dir_entry->GetPath(), makefiles, maketype);

        }
    }

    // Convert subprojects to subdirs
    map<string, EMakeFileType> subprojects_dirs;
//    if ( is_root || (!process_projects && weak) ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("*");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            string name  = dir_entry->GetName();
            if ( name == "."  ||  name == ".." ||  name == "CVS" ||  name == ".svn" ||
                 name == string(1,CDir::GetPathSeparator()) ) {
                continue;
            }
            if ( dir_entry->IsDir() ) {
                if (subprojects.find(name) != subprojects.end()) {
                    subprojects_dirs[dir_entry->GetPath()] = subprojects[name];
                } else {
                    subprojects_dirs[dir_entry->GetPath()] =
                        is_root ? eMakeType_Undefined : eMakeType_Excluded;
                }
            }
        }
/*
    } else {
        // for non-root only subprojects
        map<string, EMakeFileType>::const_iterator p;
        for (p = subprojects.begin(); p != subprojects.end(); ++p) {
            const string& subproject = p->first;
            string subproject_dir = 
                CDirEntry::ConcatPath(dir_name, subproject);
            subprojects_dirs[subproject_dir] = p->second;
        }
    }
*/

    // Process subproj ( e.t. subdirs )
    map<string, EMakeFileType>::const_iterator p;
    for (p = subprojects_dirs.begin(); p != subprojects_dirs.end(); ++p) {
        const string& subproject_dir = p->first;
        ProcessDir(subproject_dir, false, filter, makefiles, p->second);
    }

}


void CProjectTreeBuilder::ProcessMakeInFile(const string& file_name, 
                                            SMakeFiles*   makefiles,
                                            EMakeFileType type)
{
    CSimpleMakeFileContents fc(file_name, type);
    if ( !fc.m_Contents.empty() ) {
	    makefiles->m_In[file_name] = fc;
        PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}


void CProjectTreeBuilder::ProcessMakeLibFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type)
{
    CSimpleMakeFileContents fc(file_name, type);
    if ( !fc.m_Contents.empty()  ) {
        string unmet;
        if ( GetApp().IsAllowedProjectTag(fc, unmet) ) {
	        makefiles->m_Lib[file_name] = fc;
            PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	    } else {
            PTB_WARNING(file_name, "ignored; unmet dependency: " << unmet);
	    }
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}

void CProjectTreeBuilder::ProcessMakeDllFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type)
{
    string s = "MakeDll : " + file_name + "   ";

    CSimpleMakeFileContents fc(file_name, type);
    if ( !fc.m_Contents.empty()  ) {
        string unmet;
        if ( GetApp().IsAllowedProjectTag(fc, unmet) ) {
	        makefiles->m_Dll[file_name] = fc;
	    } else {
            LOG_POST(Info << s << "rejected, proj_tag= " << unmet);
	    }
	} else {
        LOG_POST(Info << s << "rejected (is empty)");
	}
}


void CProjectTreeBuilder::ProcessMakeAppFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type)
{
    CSimpleMakeFileContents fc(file_name, type);
    if ( !fc.m_Contents.empty() ) {
        string unmet;
        if ( GetApp().IsAllowedProjectTag(fc, unmet) ) {
	        makefiles->m_App[file_name] = fc;
            PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	    } else {
            PTB_WARNING(file_name, "ignored; unmet dependency: " << unmet);
	    }
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}


void CProjectTreeBuilder::ProcessUserProjFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type)
{
    CSimpleMakeFileContents fc(file_name, type);
    if ( !fc.m_Contents.empty() ) {
        string unmet;
        if ( GetApp().IsAllowedProjectTag(fc, unmet) ) {
    	    makefiles->m_User[file_name] = fc;
            PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	    } else {
            PTB_WARNING(file_name, "ignored; unmet dependency: " << unmet);
	    }
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}


//recursive resolving
void CProjectTreeBuilder::ResolveDefs(CSymResolver& resolver, 
                                      SMakeFiles&   makefiles)
{
    {{
        _TRACE("*** Resolving macrodefinitions in App projects ***");
        //App
        set<string> keys;
        keys.insert("LIB");
        keys.insert("LIBS");
        keys.insert("NCBI_C_LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_App, keys);
    }}

    {{
        _TRACE("*** Resolving macrodefinitions in Lib projects ***");
        //Lib
        set<string> keys;
        keys.insert("LIB");
        keys.insert("LIBS");
        keys.insert("SRC");
        keys.insert("DLL_LIB");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_Lib, keys);
    }}

    {{
        _TRACE("*** Resolving macrodefinitions in Msvc projects ***");
        set<string> keys;
        keys.insert("DLL_DEP");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_User, keys);
    }}
}


//analyze modules
void s_CollectDatatoolIds(const CProjectItemsTree& tree,
                          map<string, CProjKey>*   datatool_ids)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {
        const CProjKey&  project_id = p->first;
        const CProjItem& project    = p->second;
        ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *n;
            string src_abs_path = 
                CDirEntry::ConcatPath(src.m_SourceBaseDir, src.m_SourceFile);
            string src_rel_path = 
                CDirEntry::CreateRelativePath
                                 (GetApp().GetProjectTreeInfo().m_Src, 
                                  src_abs_path);
            (*datatool_ids)[src_rel_path] = project_id;
        }
    }
}


void CProjectTreeBuilder::AddDatatoolSourcesDepends(CProjectItemsTree* tree)
{
    //datatool src rel path / project ID

    // 1. Collect all projects with datatool-generated-sources
    map<string, CProjKey> whole_datatool_ids;
    s_CollectDatatoolIds(GetApp().GetWholeTree(), &whole_datatool_ids);



    // 2. Extent tree to accomodate more ASN projects if necessary
    bool tree_extented = false;
    map<string, CProjKey> datatool_ids;

    do {
        
        tree_extented = false;
        s_CollectDatatoolIds(*tree, &datatool_ids);

        NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
//            const CProjKey&  project_id = p->first;
            CProjItem& project          = p->second;
            ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
                const CDataToolGeneratedSrc& src = *n;
                ITERATE(list<string>, i, src.m_ImportModules) {
                    const string& module = *i;
                    map<string, CProjKey>::const_iterator j = 
                        datatool_ids.find(module);
                    if (j == datatool_ids.end()) {
                        j = whole_datatool_ids.find(module);
                        if (j != whole_datatool_ids.end()) {
                            const CProjKey& depends_id = j->second;
                            tree->m_Projects[depends_id] = 
                                GetApp().GetWholeTree().m_Projects.find(depends_id)->second;
                            tree_extented = true;
                        }
                    }
                }
            }
        }
    } while( tree_extented );


    // 3. Finally - generate depends
    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
        const CProjKey&  project_id = p->first;
        CProjItem& project          = p->second;
        ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *n;
            ITERATE(list<string>, i, src.m_ImportModules) {
                const string& module = *i;
                map<string, CProjKey>::const_iterator j = 
                    datatool_ids.find(module);
                if (j != datatool_ids.end()) {
                    const CProjKey& depends_id = j->second;
                    if (depends_id != project_id) {
                        project.m_Depends.push_back(depends_id);
                        project.m_Depends.sort();
                        project.m_Depends.unique();
                    }
                }
            }
        }
    }

}


END_NCBI_SCOPE
