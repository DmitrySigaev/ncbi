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

#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_src_resolver.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <set>
#include <algorithm>

BEGIN_NCBI_SCOPE


struct PLibExclude
{
    PLibExclude(const list<string>& excluded_lib_ids)
    {
        copy(excluded_lib_ids.begin(), excluded_lib_ids.end(), 
             inserter(m_ExcludedLib, m_ExcludedLib.end()) );
    }

    bool operator() (const string& lib_id) const
    {
        return m_ExcludedLib.find(lib_id) != m_ExcludedLib.end();
    }

private:
    set<string> m_ExcludedLib;
};


//-----------------------------------------------------------------------------
CProjKey::CProjKey(void)
:m_Type(eNoProj)
{
}


CProjKey::CProjKey(TProjType type, const string& project_id)
:m_Type(type),
 m_Id  (project_id)
{
}


CProjKey::CProjKey(const CProjKey& key)
:m_Type(key.m_Type),
 m_Id  (key.m_Id)
{
}


CProjKey& CProjKey::operator= (const CProjKey& key)
{
    if (this != &key) {
        m_Type = key.m_Type;
        m_Id   = key.m_Id;
    }
    return *this;
}


CProjKey::~CProjKey(void)
{
}


bool CProjKey::operator< (const CProjKey& key) const
{
    if (m_Type < key.m_Type)
        return true;
    else if (m_Type > key.m_Type)
        return false;
    else
        return m_Id < key.m_Id;
}


bool CProjKey::operator== (const CProjKey& key) const
{
    return m_Type == key.m_Type && m_Id == key.m_Id;
}


bool CProjKey::operator!= (const CProjKey& key) const
{
    return !(*this == key);
}


CProjKey::TProjType CProjKey::Type(void) const
{
    return m_Type;
}


const string& CProjKey::Id(void) const
{
    return m_Id;
}


//-----------------------------------------------------------------------------
CProjItem::CProjItem(void)
{
    Clear();
}


CProjItem::CProjItem(const CProjItem& item)
{
    SetFrom(item);
}


CProjItem& CProjItem::operator= (const CProjItem& item)
{
    if (this != &item) {
        Clear();
        SetFrom(item);
    }
    return *this;
}

CProjItem::CProjItem(TProjType type,
                     const string& name,
                     const string& id,
                     const string& sources_base,
                     const list<string>&   sources, 
                     const list<CProjKey>& depends,
                     const list<string>&   requires,
                     const list<string>&   libs_3_party,
                     const list<string>&   include_dirs,
                     const list<string>&   defines)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir (sources_base),
    m_Sources (sources), 
    m_Depends (depends),
    m_Requires(requires),
    m_Libs3Party (libs_3_party),
    m_IncludeDirs(include_dirs),
    m_Defines (defines)
{
}

CProjItem::~CProjItem(void)
{
    Clear();
}


void CProjItem::Clear(void)
{
    m_ProjType = CProjKey::eNoProj;
}


void CProjItem::SetFrom(const CProjItem& item)
{
    m_Name           = item.m_Name;
    m_ID		     = item.m_ID;
    m_ProjType       = item.m_ProjType;
    m_SourcesBaseDir = item.m_SourcesBaseDir;
    m_Sources        = item.m_Sources;
    m_Depends        = item.m_Depends;
    m_Requires       = item.m_Requires;
    m_Libs3Party     = item.m_Libs3Party;
    m_IncludeDirs    = item.m_IncludeDirs;
    m_DatatoolSources= item.m_DatatoolSources;
    m_Defines        = item.m_Defines;
    m_NcbiCLibs      = item.m_NcbiCLibs;
}


//-----------------------------------------------------------------------------
CProjectItemsTree::CProjectItemsTree(void)
{
    Clear();
}


CProjectItemsTree::CProjectItemsTree(const string& root_src)
{
    Clear();
    m_RootSrc = root_src;
}


CProjectItemsTree::CProjectItemsTree(const CProjectItemsTree& projects)
{
    SetFrom(projects);
}


CProjectItemsTree& 
CProjectItemsTree::operator= (const CProjectItemsTree& projects)
{
    if (this != &projects) {
        Clear();
        SetFrom(projects);
    }
    return *this;
}


CProjectItemsTree::~CProjectItemsTree(void)
{
    Clear();
}


void CProjectItemsTree::Clear(void)
{
    m_RootSrc.erase();
    m_Projects.clear();
}


void CProjectItemsTree::SetFrom(const CProjectItemsTree& projects)
{
    m_RootSrc  = projects.m_RootSrc;
    m_Projects = projects.m_Projects;
}


void CProjectItemsTree::CreateFrom(const string& root_src,
                                   const TFiles& makein, 
                                   const TFiles& makelib, 
                                   const TFiles& makeapp , 
                                   CProjectItemsTree* tree)
{
    tree->m_Projects.clear();
    tree->m_RootSrc = root_src;

    ITERATE(TFiles, p, makein) {

        const string& fc_path = p->first;
        const CSimpleMakeFileContents& fc_makein = p->second;

        string source_base_dir;
        CDirEntry::SplitPath(fc_path, &source_base_dir);

        SMakeProjectT::TMakeInInfoList list_info;
        SMakeProjectT::AnalyzeMakeIn(fc_makein, &list_info);
        ITERATE(SMakeProjectT::TMakeInInfoList, i, list_info) {

            const SMakeProjectT::SMakeInInfo& info = *i;

            //Iterate all project_name(s) from makefile.in 
            ITERATE(list<string>, n, info.m_ProjNames) {

                //project id will be defined latter
                const string proj_name = *n;
        
                string applib_mfilepath = 
                    CDirEntry::ConcatPath(source_base_dir,
                    SMakeProjectT::CreateMakeAppLibFileName(source_base_dir, 
                                                            proj_name));
                if ( applib_mfilepath.empty() )
                    continue;
            
                if (info.m_Type == SMakeProjectT::SMakeInInfo::eApp) {

                    SAsnProjectT::TAsnType asn_type = 
                        SAsnProjectT::GetAsnProjectType(applib_mfilepath,
                                                        makeapp,
                                                        makelib);

                    if (asn_type == SAsnProjectT::eMultiple) {
                        SAsnProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, makelib, tree);
                    } else {
                        SAppProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, tree);
                    }
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eLib) {

                    SAsnProjectT::TAsnType asn_type = 
                        SAsnProjectT::GetAsnProjectType(applib_mfilepath,
                                                        makeapp,
                                                        makelib);

                    if (asn_type == SAsnProjectT::eMultiple) {
                        SAsnProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, makelib, tree);
                    } else {
                        SLibProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makelib, tree);
                    }
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eAsn) {

                    SAsnProjectT::DoCreate(source_base_dir, 
                                       proj_name, 
                                       applib_mfilepath, 
                                       makeapp, makelib, tree);
                }
            }
        }
    }

    {{
        // REQUIRES tags in Makefile.in(s)
        ITERATE(TFiles, p, makein) {

            string makein_dir;
            CDirEntry::SplitPath(p->first, &makein_dir);
            const CSimpleMakeFileContents& makein_contents = p->second;
            NON_CONST_ITERATE(TProjects, n, tree->m_Projects) {

                CProjItem& project = n->second;
                if ( IsSubdir(makein_dir, project.m_SourcesBaseDir) ) {

                    CSimpleMakeFileContents::TContents::const_iterator k = 
                        makein_contents.m_Contents.find("REQUIRES");
                    if ( k != makein_contents.m_Contents.end() ) {

                        const list<string> requires = k->second;
                        copy(requires.begin(), 
                             requires.end(), 
                             back_inserter(project.m_Requires));
                        
                        project.m_Requires.sort();
                        project.m_Requires.unique();
                    }
                }
            }
        }
    }}
}


void CProjectItemsTree::GetInternalDepends(list<CProjKey>* depends) const
{
    depends->clear();

    set<CProjKey> depends_set;

    ITERATE(TProjects, p, m_Projects) {
        const CProjItem& proj_item = p->second;
        ITERATE(list<CProjKey>, n, proj_item.m_Depends) {
            depends_set.insert(*n);
        }
    }

    copy(depends_set.begin(), depends_set.end(), back_inserter(*depends));
}


void 
CProjectItemsTree::GetExternalDepends(list<CProjKey>* external_depends) const
{
    external_depends->clear();

    list<CProjKey> depends;
    GetInternalDepends(&depends);
    ITERATE(list<CProjKey>, p, depends) {
        const CProjKey& depend_id = *p;
        if (m_Projects.find(depend_id) == m_Projects.end())
            external_depends->push_back(depend_id);
    }
}

#if 0
void CProjectItemsTree::GetDepends(const CProjKey& proj_id,
                                   list<CProjKey>* depends) const
{
    TProjects::const_iterator p = m_Projects.find(proj_id);

    if (p != m_Projects.end()) {
        const CProjItem& project = p->second;
        ITERATE(list<CProjKey>, n, project.m_Depends) {
            const CProjKey& depend_id = *n;
            if (find(depends->begin(), 
                     depends->end(), 
                     depend_id) != depends->end()) {
                GetDepends(proj_id, depends);
            } else {
                LOG_POST(Error << "Cyclic dependencies found : " + depend_id.Id());
            }
        }
    }
}


void CProjectItemsTree::CreateBuildOrder(const CProjKey& proj_id, 
                                         list<CProjKey>* projects) const
{
    list<CProjKey> project_depends;
    GetDepends(proj_id, &project_depends);
    
    typedef map<CProjKey, int> TProjWeights;
    TProjWeights proj_weights;
    ITERATE(TProjects, p, m_Projects) {
        
    }
}
#endif
//-----------------------------------------------------------------------------
CProjItem::TProjType SMakeProjectT::GetProjType(const string& base_dir,
                                                const string& projname)
{
    string fname = "Makefile." + projname;
    
    if ( CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".lib")).Exists() )
        return CProjKey::eLib;
    else if (CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".app")).Exists() )
        return CProjKey::eApp;

    LOG_POST(Error << "No .lib or .app projects for : " + projname +
                      " in directory: " + base_dir);
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


bool SMakeProjectT::IsMakeAppFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".app");
}


void SMakeProjectT::DoResolveDefs(CSymResolver& resolver, 
                                  TFiles& files,
                                  const set<string>& keys)
{
    NON_CONST_ITERATE(CProjectTreeBuilder::TFiles, p, files) {
	    NON_CONST_ITERATE(CSimpleMakeFileContents::TContents, 
                          n, 
                          p->second.m_Contents) {
            
            const string& key    = n->first;
            list<string>& values = n->second;
		    if (keys.find(key) != keys.end()) {
                list<string> new_vals;
                bool modified = false;
                NON_CONST_ITERATE(list<string>, k, values) {
                    //iterate all values and try to resolve 
                    const string& val = *k;
                    if( !CSymResolver::IsDefine(val) ) {
                        new_vals.push_back(val);
                    } else {
                        list<string> resolved_def;
                        string val_define = FilterDefine(val);
	                    resolver.Resolve(val_define, &resolved_def);
	                    if ( resolved_def.empty() )
		                    new_vals.push_back(val); //not resolved - keep old val
                        else {
                            //was resolved
		                    copy(resolved_def.begin(), 
			                     resolved_def.end(), 
			                     back_inserter(new_vals));
		                    modified = true;
                        }
                    }
                }
                if (modified)
                    values = new_vals; // by ref!
		    }
        }
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
        const string token("-I$(includedir)");

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
            string dir = GetApp().GetSite().ResolveDefine
                                             (CSymResolver::StripDefine(flag));
            if ( !dir.empty() && CDirEntry(dir).IsDir() ) {
                include_dirs->push_back(dir);    
            }
        }
    }
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
        if ( NStr::StartsWith(flag, "@") &&
             NStr::EndsWith(flag, "@") ) {
            libs_list->push_back(string(flag, 1, flag.length() - 2));    
        }
    }
}


void SMakeProjectT::AnalyzeMakeIn
    (const CSimpleMakeFileContents& makein_contents,
     TMakeInInfoList*               info)
{
    info->clear();

    CSimpleMakeFileContents::TContents::const_iterator p = 
        makein_contents.m_Contents.find("LIB_PROJ");

    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second)); 
    }

    p = makein_contents.m_Contents.find("APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second)); 
    }

    p = makein_contents.m_Contents.find("ASN_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eAsn, p->second)); 
    }

    //TODO - DLL_PROJ
}


string SMakeProjectT::CreateMakeAppLibFileName
                (const string&            base_dir,
                 const string&            projname)
{
    CProjItem::TProjType proj_type = 
            SMakeProjectT::GetProjType(base_dir, projname);

    string fname = "Makefile." + projname;
    fname += proj_type==CProjKey::eLib? ".lib": ".app";
    return fname;
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


void SMakeProjectT::ConvertLibDepends(const list<string>& depends_libs, 
                                      list<CProjKey>*     depends_ids)
{
    depends_ids->clear();
    ITERATE(list<string>, p, depends_libs)
    {
        const string& id = *p;
        depends_ids->push_back(CProjKey(CProjKey::eLib, id));
    }
}


//-----------------------------------------------------------------------------
void SAppProjectT::CreateNcbiCToolkitLibs(const string& applib_mfilepath,
                                          const TFiles& makeapp,
                                          list<string>* libs_list)
{
    libs_list->clear();
    CProjectItemsTree::TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {

        LOG_POST(Info << "No Makefile.*.app for Makefile.in :"
                  + applib_mfilepath);
        return;
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
    m->second.m_Contents.find("NCBI_C_LIBS");
    if (k == m->second.m_Contents.end()) {
        return;
    }
    const list<string>& values = k->second;

    ITERATE(list<string>, p, values) {
        const string& val = *p;
        if ( NStr::StartsWith(val, "-l") ) {
            string lib_id = val.substr(2);
            libs_list->push_back(lib_id);
        }
        if ( NStr::StartsWith(val, "@") &&
             NStr::EndsWith  (val, "@") ) {
            string lib_define = val.substr(1, val.length() - 2);
            string lib_id_str = GetApp().GetSite().ResolveDefine(lib_define);
            if ( !lib_id_str.empty() ) {
                list<string> lib_ids;
                NStr::Split(lib_id_str, LIST_SEPARATOR, lib_ids);
                copy(lib_ids.begin(), lib_ids.end(), back_inserter(*libs_list));
            } else {
                LOG_POST(Error << "No define in local site : "
                          + lib_define);
            }
        }
    }

    libs_list->sort();
    libs_list->unique();
}

CProjKey SAppProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makeapp , 
                                CProjectItemsTree* tree)
{
    CProjectItemsTree::TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {

        LOG_POST(Info << "No Makefile.*.app for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Warning << "No SRC key in Makefile.*.app :"
                  + applib_mfilepath);
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
    k = m->second.m_Contents.find("LIB");
    if (k != m->second.m_Contents.end())
        depends = k->second;
    //Adjust depends by information from msvc Makefile
    CMsvcProjectMakefile project_makefile
                       ((CDirEntry::ConcatPath
                          (source_base_dir, 
                           CreateMsvcProjectMakefileName(proj_name, 
                                                         CProjKey::eApp))));
    list<string> added_depends;
    project_makefile.GetAdditionalLIB(SConfigInfo(), &added_depends);

    list<string> excluded_depends;
    project_makefile.GetExcludedLIB(SConfigInfo(), &excluded_depends);

    list<string> adj_depends(depends);
    copy(added_depends.begin(), 
         added_depends.end(), back_inserter(adj_depends));
    adj_depends.sort();
    adj_depends.unique();

    PLibExclude pred(excluded_depends);
    EraseIf(adj_depends, pred);

    list<CProjKey> depends_ids;
    SMakeProjectT::ConvertLibDepends(adj_depends, &depends_ids);
    ///////////////////////////////////

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end())
        requires = k->second;

    //project name
    k = m->second.m_Contents.find("APP");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST(Error << "No APP key or empty in Makefile.*.app :"
                  + applib_mfilepath);
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

    //NCBI_C_LIBS - Special case for NCBI C Toolkit
    k = m->second.m_Contents.find("NCBI_C_LIBS");
    list<string> ncbi_clibs;
    if (k != m->second.m_Contents.end()) {
        libs_3_party.push_back("NCBI_C_LIBS");
        CreateNcbiCToolkitLibs(applib_mfilepath, makeapp, &ncbi_clibs);
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
                      defines);
    //
    project.m_NcbiCLibs = ncbi_clibs;

    //DATATOOL_SRC
    k = m->second.m_Contents.find("DATATOOL_SRC");
    if ( k != m->second.m_Contents.end() ) {
        //Add depends from datatoool for ASN projects
        project.m_Depends.push_back(CProjKey(CProjKey::eApp, GetApp().GetDatatoolId()));
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

            CDataToolGeneratedSrc data_tool_src;
            CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
            if ( !data_tool_src.IsEmpty() )
                project.m_DatatoolSources.push_back(data_tool_src);
        }
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
                                CProjectItemsTree* tree)
{
    TFiles::const_iterator m = makelib.find(applib_mfilepath);
    if (m == makelib.end()) {

        LOG_POST(Info << "No Makefile.*.lib for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Warning << "No SRC key in Makefile.*.lib :"
                  + applib_mfilepath);
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
    if (k != m->second.m_Contents.end())
        requires = k->second;

    //project name
    k = m->second.m_Contents.find("LIB");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST(Error << "No LIB key or empty in Makefile.*.lib :"
                  + applib_mfilepath);
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
                                           defines);
    return proj_key;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makeapp, 
                                const TFiles& makelib, 
                                CProjectItemsTree* tree)
{
    TAsnType asn_type = GetAsnProjectType(applib_mfilepath, makeapp, makelib);
    if (asn_type == eMultiple) {
        return SAsnProjectMultipleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree);
    }
    if(asn_type == eSingle) {
        return SAsnProjectSingleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree);
    }

    LOG_POST(Error << "Unsupported ASN project" + NStr::IntToString(asn_type));
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

    LOG_POST(Error << "Can not define ASN project: " + applib_mfilepath);
    return eNoAsn;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectSingleT::DoCreate(const string& source_base_dir,
                                      const string& proj_name,
                                      const string& applib_mfilepath,
                                      const TFiles& makeapp, 
                                      const TFiles& makelib, 
                                      CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        SMakeProjectT::GetProjType(source_base_dir, proj_name);
    
    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
            SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree) : 
            SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "Can not find ASN project with id : " + proj_id.Id());
        return CProjKey();
    }
    CProjItem& project = p->second;

    //Add depends from datatoool for ASN projects
    project.m_Depends.push_back(CProjKey(CProjKey::eApp,
                                         GetApp().GetDatatoolId()));

    //Will process .asn or .dtd files
    string source_file_path = CDirEntry::ConcatPath(source_base_dir, proj_name);
    if ( CDirEntry(source_file_path + ".asn").Exists() )
        source_file_path += ".asn";
    else if ( CDirEntry(source_file_path + ".dtd").Exists() )
        source_file_path += ".dtd";

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
                                        CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        SMakeProjectT::GetProjType(source_base_dir, proj_name);
    

    const TFiles& makefile = proj_type == CProjKey::eLib? makelib : makeapp;
    TFiles::const_iterator m = makefile.find(applib_mfilepath);
    if (m == makefile.end()) {

        LOG_POST(Info << "No Makefile.*.lib/app  for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }
    const CSimpleMakeFileContents& fc = m->second;

    // ASN
    CSimpleMakeFileContents::TContents::const_iterator k = 
        fc.m_Contents.find("ASN");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Error << "No ASN key in multiple ASN  project:"
                  + applib_mfilepath);
        return CProjKey();
    }
    const list<string> asn_names = k->second;

    string parent_dir_abs = ParentDir(source_base_dir);
    list<CDataToolGeneratedSrc> datatool_sources;
    ITERATE(list<string>, p, asn_names) {
        const string& asn = *p;
        // one level up
        string asn_dir_abs = CDirEntry::ConcatPath(parent_dir_abs, asn);
        asn_dir_abs = CDirEntry::NormalizePath(asn_dir_abs);
        asn_dir_abs = CDirEntry::AddTrailingPathSeparator(asn_dir_abs);
    
        string asn_path_abs = CDirEntry::ConcatPath(asn_dir_abs, asn);
        if ( CDirEntry(asn_path_abs + ".asn").Exists() )
            asn_path_abs += ".asn";
        else if ( CDirEntry(asn_dir_abs + ".dtd").Exists() )
            asn_path_abs += ".dtd";

        CDataToolGeneratedSrc data_tool_src;
        CDataToolGeneratedSrc::LoadFrom(asn_path_abs, &data_tool_src);
        if ( !data_tool_src.IsEmpty() )
            datatool_sources.push_back(data_tool_src);

    }

    // SRC
    k = fc.m_Contents.find("SRC");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Error << "No SRC key in multiple ASN  project:"
                  + applib_mfilepath);
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
                               proj_name, applib_mfilepath, makelib, tree) :
        SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "Can not find ASN project with id : " +proj_id.Id());
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

        project.m_Sources.push_back(src + "__");
        project.m_Sources.push_back(src + "___");
    }

    project.m_DatatoolSources = datatool_sources;

    //Add depends from datatoool for ASN projects
    project.m_Depends.push_back(CProjKey(CProjKey::eApp, 
                                         GetApp().GetDatatoolId()));

    return proj_id;
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
               &subtree_makefiles);

    // Resolve macrodefines
    list<string> metadata_files;
    GetApp().GetMetaDataFiles(&metadata_files);
    CSymResolver resolver;
    ITERATE(list<string>, p, metadata_files) {
	    resolver += CSymResolver(CDirEntry::ConcatPath(root_src_path, *p));
	}
    ResolveDefs(resolver, subtree_makefiles);

    // Build projects tree
    CProjectItemsTree::CreateFrom(root_src_path,
                                  subtree_makefiles.m_In, 
                                  subtree_makefiles.m_Lib, 
                                  subtree_makefiles.m_App, tree);
}


void 
CProjectTreeBuilder::BuildProjectTree(const IProjectFilter* filter,
                                      const string&         root_src_path,
                                      CProjectItemsTree*    tree)
{
    // Bulid subtree
    CProjectItemsTree target_tree;
    BuildOneProjectTree(filter, root_src_path, &target_tree);

    // Analyze subtree depends
    list<CProjKey> external_depends;
    target_tree.GetExternalDepends(&external_depends);

    // We have to add more projects to the target tree
    if ( !external_depends.empty()  &&  !filter->PassAll() ) {
        // Get whole project tree
        CProjectItemsTree whole_tree;
        CProjectDummyFilter pass_all_filter;
        BuildOneProjectTree(&pass_all_filter, root_src_path, &whole_tree);

        list<CProjKey> depends_to_resolve = external_depends;

        while ( !depends_to_resolve.empty() ) {
            bool modified = false;
            ITERATE(list<CProjKey>, p, depends_to_resolve) {
                // id of project we have to resolve
                const CProjKey& prj_id = *p;
                CProjectItemsTree::TProjects::const_iterator n = 
                                            whole_tree.m_Projects.find(prj_id);
            
                if (n != whole_tree.m_Projects.end()) {
                    //insert this project to target_tree
                    target_tree.m_Projects[prj_id] = n->second;
                    modified = true;
                } else {
                    LOG_POST (Error << "No project with id :" + prj_id.Id());
                }
            }

            if (!modified) {
                //we done - no more projects was added to target_tree
                *tree = target_tree;
                return;
            } else {
                //continue resolving dependences
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
                                     SMakeFiles*           makefiles)
{
    // Do not collect makefile from root directory
    CDir dir(dir_name);
    CDir::TEntries contents = dir.GetEntries("*");
    ITERATE(CDir::TEntries, i, contents) {
        string name  = (*i)->GetName();
        if ( name == "."  ||  name == ".."  ||  
             name == string(1,CDir::GetPathSeparator()) ) {
            continue;
        }
        string path = (*i)->GetPath();

        if ( (*i)->IsFile()  &&  
             !is_root        &&  
             filter->CheckProject(CDirEntry(path).GetDir()) ) {

            if ( SMakeProjectT::IsMakeInFile(name) )
	            ProcessMakeInFile(path, makefiles);
            else if ( SMakeProjectT::IsMakeLibFile(name) )
	            ProcessMakeLibFile(path, makefiles);
            else if ( SMakeProjectT::IsMakeAppFile(name) )
	            ProcessMakeAppFile(path, makefiles);
        } 
        else if ( (*i)->IsDir() ) {

            ProcessDir(path, false, filter, makefiles);
        }
    }
}


void CProjectTreeBuilder::ProcessMakeInFile(const string& file_name, 
                                            SMakeFiles*   makefiles)
{
    LOG_POST(Info << "Processing MakeIn: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_In[file_name] = fc;
}


void CProjectTreeBuilder::ProcessMakeLibFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST(Info << "Processing MakeLib: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_Lib[file_name] = fc;
}


void CProjectTreeBuilder::ProcessMakeAppFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST(Info << "Processing MakeApp: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_App[file_name] = fc;
}


//recursive resolving
void CProjectTreeBuilder::ResolveDefs(CSymResolver& resolver, 
                                      SMakeFiles&   makefiles)
{
    {{
        //App
        set<string> keys;
        keys.insert("LIB");
        keys.insert("LIBS");
        keys.insert("NCBI_C_LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_App, keys);
    }}

    {{
        //Lib
        set<string> keys;
        keys.insert("LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_Lib, keys);
    }}
}


//analyze modules
void CProjectTreeBuilder::AddDatatoolSourcesDepends(CProjectItemsTree* tree)
{
    //datatool src rel path / project ID
    map<string, CProjKey> datatool_ids;
    ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
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
            datatool_ids[src_rel_path] = project_id;
        }
    }
    
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


//-----------------------------------------------------------------------------
void CCyclicDepends::FindCycles(const TProjects& tree,
                                TDependsCycles*  cycles)
{
    cycles->clear();

    ITERATE(TProjects, p, tree) {
        // Look throgh all projects in tree.
        const CProjKey& project_id = p->first;
        // If this proj_id was already reported in some cycle, 
        // it's no need to test it again.
        if ( !IsInAnyCycle(project_id, *cycles) ) {
            // Analyze for cycles
            AnalyzeProjItem(project_id, tree, cycles);
        }
    }
}


bool CCyclicDepends::IsInAnyCycle(const CProjKey&       proj_id,
                                  const TDependsCycles& cycles)
{
    ITERATE(TDependsCycles, p, cycles) {
        const TDependsChain& cycle = *p;
        if (find(cycle.begin(), cycle.end(), proj_id) != cycle.end())
            return true;
    }
    return false;
}


void CCyclicDepends::AnalyzeProjItem(const CProjKey&  proj_id,
                                     const TProjects& tree,
                                     TDependsCycles*  cycles)
{
    TProjects::const_iterator p = tree.find(proj_id);
    if (p == tree.end()) {
        LOG_POST( Error << "Unknown project: " << proj_id.Id() );
        return;
    }
    
    const CProjItem& project = p->second;
    // No depends - no cycles
    if ( project.m_Depends.empty() )
        return;

    TDependsChains chains;
    ITERATE(list<CProjKey>, n, project.m_Depends) {

        // Prepare initial state of depends chains
        // one depend project in each chain
        const CProjKey& depend_id = *n;
        if ( !IsInAnyCycle(depend_id, *cycles) ) {
            TDependsChain one_chain;
            one_chain.push_back(depend_id);
            chains.push_back(one_chain);
        }
    }
    // Try to extend this chains
    TDependsChain cycle_found;
    bool cycles_found = ExtendChains(proj_id, tree, &chains, &cycle_found);
    if ( cycles_found ) {
        // Report chains as a cycles
        cycles->insert(cycle_found);
    }
}


bool CCyclicDepends::ExtendChains(const CProjKey&  proj_id, 
                                  const TProjects& tree,
                                  TDependsChains*  chains,
                                  TDependsChain*   cycle_found)
{
    for (TDependsChains::iterator p = chains->begin(); 
          p != chains->end();  ) {
        // Iterate through all chains
        TDependsChain& one_chain = *p;
        // we'll consider last element.
        const CProjKey& depend_id = one_chain.back();
        TProjects::const_iterator n = tree.find(depend_id);
        if (n == tree.end()) {
            //LOG_POST( Error << "Unknown project: " << depend_id.Id() );
            return false;
        }
        const CProjItem& depend_project = n->second;
        if ( depend_project.m_Depends.empty() ) {
            // If nobody depends from this project - remove this chain
            p = chains->erase(p);
        } else {
            // We'll create new chains 
            // by adding depend_project dependencies to old_chain
            TDependsChain old_chain = one_chain;
            p = chains->erase(p);

            ITERATE(list<CProjKey>, k, depend_project.m_Depends) {
                const CProjKey& new_depend_id = *k;
                // add each new depends to the end of the old_chain.
                TDependsChain new_chain = old_chain;
                new_chain.push_back(new_depend_id);
                p = chains->insert(p, new_chain);
                ++p;
            }
        }
    }
    // No chains - no cycles
    if ( chains->empty() )
        return false;
    // got cycles in chains - we done
    if ( IsCyclic(proj_id, *chains, cycle_found) )
        return true;
    // otherwise - continue search.
    return ExtendChains(proj_id, tree, chains, cycle_found);
}


bool CCyclicDepends::IsCyclic(const CProjKey&       proj_id, 
                              const TDependsChains& chains,
                              TDependsChain*        cycle_found)
{
    // First iteration - we'll try to find project to
    // consider inside depends chains. If we found - we have a cycle.
    ITERATE(TDependsChains, p, chains) {
        const TDependsChain& one_chain = *p;
        if (find(one_chain.begin(), 
                 one_chain.end(), 
                 proj_id) != one_chain.end()) {
            *cycle_found = one_chain;
            return true;
        }
    }

    // We look into all chais
    ITERATE(TDependsChains, p, chains) {
        TDependsChain one_chain = *p;
        // remember original size of chain
        size_t orig_size = one_chain.size();
        // remove all non-unique elements
        one_chain.sort();
        one_chain.unique();
        // if size of the chain is altered - we have a cycle.
        if (one_chain.size() != orig_size) {
            *cycle_found = one_chain;
            return true;
        }
    }
    
    // Got nothing - no cycles
    return false;
}


//-----------------------------------------------------------------------------
CProjectTreeFolders::CProjectTreeFolders(const CProjectItemsTree& tree)
:m_RootParent("/", NULL)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {

        const CProjKey&  project_id = p->first;
        const CProjItem& project    = p->second;
        
        TPath path;
        CreatePath(GetApp().GetProjectTreeInfo().m_Src, 
                   project.m_SourcesBaseDir, 
                   &path);
        SProjectTreeFolder* folder = FindOrCreateFolder(path);
        folder->m_Name = path.back();
        folder->m_Projects.insert(project_id);
    }
}


SProjectTreeFolder* 
CProjectTreeFolders::CreateFolder(SProjectTreeFolder* parent,
                                  const string&       folder_name)
{
    m_Folders.push_back(SProjectTreeFolder(folder_name, parent));
    SProjectTreeFolder* inserted = &(m_Folders.back());

    parent->m_Siblings.insert
        (SProjectTreeFolder::TSiblings::value_type(folder_name, inserted));
    
    return inserted;

}

SProjectTreeFolder* 
CProjectTreeFolders::FindFolder(const TPath& path)
{
    SProjectTreeFolder& folder_i = m_RootParent;
    ITERATE(TPath, p, path) {
        const string& node = *p;
        SProjectTreeFolder::TSiblings::iterator n = 
            folder_i.m_Siblings.find(node);
        if (n == folder_i.m_Siblings.end())
            return NULL;
        folder_i = *(n->second);
    }
    return &folder_i;
}


SProjectTreeFolder* 
CProjectTreeFolders::FindOrCreateFolder(const TPath& path)
{
    SProjectTreeFolder* folder_i = &m_RootParent;
    ITERATE(TPath, p, path) {
        const string& node = *p;
        SProjectTreeFolder::TSiblings::iterator n = folder_i->m_Siblings.find(node);
        if (n == folder_i->m_Siblings.end()) {
            folder_i = CreateFolder(folder_i, node);
        } else {        
            folder_i = n->second;
        }
    }
    return folder_i;
}


void CProjectTreeFolders::CreatePath(const string& root_src_dir, 
                                     const string& project_base_dir,
                                     TPath*        path)
{
    path->clear();
    
    string rel_dir = 
        CDirEntry::CreateRelativePath(root_src_dir, project_base_dir);
    string sep(1, CDirEntry::GetPathSeparator());
    NStr::Split(rel_dir, sep, *path);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2004/03/01 17:59:35  gorelenk
 * Added implementation of class CCyclicDepends member-functions.
 *
 * Revision 1.20  2004/02/27 18:08:25  gorelenk
 * Changed implementation of CProjectTreeBuilder::ProcessDir.
 *
 * Revision 1.19  2004/02/26 21:29:04  gorelenk
 * Changed implementations of member-functions of class CProjectTreeBuilder:
 * BuildProjectTree, BuildOneProjectTree and ProcessDir because of use
 * IProjectFilter* filter instead of const string& subtree_to_build.
 *
 * Revision 1.18  2004/02/24 17:20:11  gorelenk
 * Added implementation of member-function CreateNcbiCToolkitLibs
 * of struct SAppProjectT.
 * Changed implementation of member-function DoCreate
 * of struct SAppProjectT.
 *
 * Revision 1.17  2004/02/23 20:58:41  gorelenk
 * Fixed double properties apperience in generated MSVC projects.
 *
 * Revision 1.16  2004/02/20 22:53:59  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.15  2004/02/18 23:35:40  gorelenk
 * Added special processing for NCBI_C_LIBS tag.
 *
 * Revision 1.14  2004/02/13 16:02:24  gorelenk
 * Modified procedure of depends resolve.
 *
 * Revision 1.13  2004/02/11 16:46:29  gorelenk
 * Cnanged LOG_POST message.
 *
 * Revision 1.12  2004/02/10 21:20:44  gorelenk
 * Added support of DATATOOL_SRC tag.
 *
 * Revision 1.11  2004/02/10 18:17:05  gorelenk
 * Added support of depends fine-tuning.
 *
 * Revision 1.10  2004/02/06 23:15:00  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.9  2004/02/04 23:58:29  gorelenk
 * Added support of include dirs and 3-rd party libs.
 *
 * Revision 1.8  2004/02/03 17:12:55  gorelenk
 * Changed implementation of classes CProjItem and CProjectItemsTree.
 *
 * Revision 1.7  2004/01/30 20:44:22  gorelenk
 * Initial revision.
 *
 * Revision 1.6  2004/01/28 17:55:50  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:30  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */
