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
#include <app/project_tree_builder/msvc_prj_files_collector.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/configurable_file.hpp>

BEGIN_NCBI_SCOPE


static void s_CollectRelPathes(const string&        path_from,
                               const list<string>&  abs_dirs,
                               list<string>*        rel_pathes);

// sort files by base file name, ignoring path and extension
bool s_FileName_less(const string& x, const string& y)
{
    string base_x, base_y;
    CDirEntry::SplitPath(x, NULL, &base_x);
    CDirEntry::SplitPath(y, NULL, &base_y);
    return NStr::CompareNocase(base_x, base_y) < 0;
}

//-----------------------------------------------------------------------------

CMsvcPrjFilesCollector::CMsvcPrjFilesCollector
                                (const CMsvcPrjProjectContext& project_context,
                                 const list<SConfigInfo>&      project_configs,
                                 const CProjItem&              project)
    : m_Context(&project_context),
      m_Configs(&project_configs),
      m_Project(&project)
{
    CollectSources();
    CollectHeaders();
    CollectInlines();
    CollectResources();
}


CMsvcPrjFilesCollector::~CMsvcPrjFilesCollector(void)
{
}


const list<string>& CMsvcPrjFilesCollector::SourceFiles(void) const
{
    return m_SourceFiles;
}


const list<string>& CMsvcPrjFilesCollector::HeaderFiles(void) const
{
    return m_HeaderFiles;
}


const list<string>& CMsvcPrjFilesCollector::InlineFiles(void) const
{
    return m_InlineFiles;
}


// source files helpers -------------------------------------------------------

const list<string>& CMsvcPrjFilesCollector::ResourceFiles(void) const
{
    return m_ResourceFiles;
}

struct PSourcesExclude
{
    PSourcesExclude(const list<string>& excluded_sources)
    {
        copy(excluded_sources.begin(), excluded_sources.end(), 
             inserter(m_ExcludedSources, m_ExcludedSources.end()) );
    }

    bool operator() (const string& src) const
    {
        string src_base;
        CDirEntry::SplitPath(src, NULL, &src_base);
        return m_ExcludedSources.find(src_base) != m_ExcludedSources.end();
    }

private:
    set<string> m_ExcludedSources;
};


static bool s_IsProducedByDatatool(const string&    src_path_abs,
                                   const CProjItem& project)
{
    if ( project.m_DatatoolSources.empty() )
        return false;

    string src_base;
    CDirEntry::SplitPath(src_path_abs, NULL, &src_base);

    // guess name.asn file name from name__ or name___
    string asn_base;
    if ( NStr::EndsWith(src_base, "___") ) {
        asn_base = src_base.substr(0, src_base.length() -3);
    } else if ( NStr::EndsWith(src_base, "__") ) {
        asn_base = src_base.substr(0, src_base.length() -2);
    } else {
        return false;
    }
    string asn_name = asn_base + ".asn";
    string dtd_name = asn_base + ".dtd";

    //try to find this name in datatool generated sources container
    ITERATE(list<CDataToolGeneratedSrc>, p, project.m_DatatoolSources) {
        const CDataToolGeneratedSrc& asn = *p;
        if ((asn.m_SourceFile == asn_name) || (asn.m_SourceFile == dtd_name))
            return true;
    }
    return false;
}


static bool s_IsInsideDatatoolSourceDir(const string& src_path_abs)
{
    string dir_name;
    CDirEntry::SplitPath(src_path_abs, &dir_name);

    //This files must be inside datatool src dir
    CDir dir(dir_name);
    if ( dir.GetEntries("*.module").empty() ) 
        return false;
    if ( dir.GetEntries("*.asn").empty() &&
         dir.GetEntries("*.dtd").empty()  ) 
        return false;

    return true;
}


void 
CMsvcPrjFilesCollector::CollectSources(void)
{
    m_SourceFiles.clear();

    list<string> sources;
    ITERATE(list<string>, p, m_Project->m_Sources) {

        const string& src_rel = *p;
        string src_path = 
            CDirEntry::ConcatPath(m_Project->m_SourcesBaseDir, src_rel);
        src_path = CDirEntry::NormalizePath(src_path);

        sources.push_back(src_path);
    }

    list<string> included_sources;
    m_Context->GetMsvcProjectMakefile().GetAdditionalSourceFiles //TODO
                                            (SConfigInfo(),&included_sources);

    ITERATE(list<string>, p, included_sources) {
        sources.push_back(CDirEntry::NormalizePath
                                        (CDirEntry::ConcatPath
                                              (m_Project->m_SourcesBaseDir, *p)));
    }

    list<string> excluded_sources;
    m_Context->GetMsvcProjectMakefile().GetExcludedSourceFiles //TODO
                                            (SConfigInfo(), &excluded_sources);
    PSourcesExclude pred(excluded_sources);
    EraseIf(sources, pred);

    ITERATE(list<string>, p, sources) {

        const string& abs_path = *p; // whithout ext.

        string ext = SourceFileExt(abs_path);
        if ( NStr::EndsWith(ext, ".in") ) {
            // Special case: skip configurable file generation
            // if configurations was not specified
            if ( m_Configs->empty() ) {
                m_SourceFiles.push_back(
                    CDirEntry::CreateRelativePath(m_Context->ProjectDir(),
                                                  abs_path));
            } else {
                // configurable source file
                string orig_ext = NStr::Replace(ext, ".in", "");
                string dst_path;
                CDirEntry::SplitPath(abs_path, NULL, &dst_path, NULL);
                dst_path = CDirEntry::MakePath(m_Context->ProjectDir(), dst_path);

                // Create configurable file for each enabled configuration
                ITERATE(list<SConfigInfo>, p , *m_Configs) {
                    const SConfigInfo& cfg_info = *p;
                    string file_dst_path;
                    file_dst_path = dst_path + "." +
                                    ConfigurableFileSuffix(cfg_info.m_Name)+
                                    orig_ext;
                    CreateConfigurableFile(abs_path + ext, file_dst_path,
                                           cfg_info.m_Name);
                }
                dst_path += ".@config@" + orig_ext;
                m_SourceFiles.push_back(
                    CDirEntry::CreateRelativePath(m_Context->ProjectDir(),
                                                  dst_path));
                }
        }
        else if ( !ext.empty() ) {
            // add ext to file
            string source_file_abs_path = abs_path + ext;
            m_SourceFiles.push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                              source_file_abs_path));
        } 
        else if ( s_IsProducedByDatatool(abs_path, *m_Project) ||
                  s_IsInsideDatatoolSourceDir(abs_path) ) {
            // .cpp file extension
            m_SourceFiles.push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                              abs_path + ".cpp"));
        } else {
            LOG_POST(Warning <<"Can not resolve/find source file : " + abs_path);
        }
    }
    m_SourceFiles.sort(s_FileName_less);
}


// header files helpers -------------------------------------------------------
void 
CMsvcPrjFilesCollector::CollectHeaders(void)
{
    m_HeaderFiles.clear();
    s_CollectRelPathes(m_Context->ProjectDir(), m_Context->IncludeDirsAbs(),
                       &m_HeaderFiles);
    m_HeaderFiles.sort(s_FileName_less);
}


// inline files helpers -------------------------------------------------------

void 
CMsvcPrjFilesCollector::CollectInlines(void)
{
    m_InlineFiles.clear();
    s_CollectRelPathes(m_Context->ProjectDir(), m_Context->InlineDirsAbs(),
                       &m_InlineFiles);
    m_InlineFiles.sort(s_FileName_less);
}


// resource files helpers -------------------------------------------------------

void 
CMsvcPrjFilesCollector::CollectResources(void)
{
    m_ResourceFiles.clear();

    // resources from msvc makefile - first priority
    list<string> included_sources;
    m_Context->GetMsvcProjectMakefile().GetResourceFiles
                                            (SConfigInfo(),&included_sources);
    list<string> sources;
    ITERATE(list<string>, p, included_sources) {
        sources.push_back(CDirEntry::NormalizePath
                                        (CDirEntry::ConcatPath
                                              (m_Project->m_SourcesBaseDir, *p)));
    }

    ITERATE(list<string>, p, sources) {

        const string& abs_path = *p; // with ext.
        m_ResourceFiles.push_back(
            CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                          abs_path));
    }
    if ( m_ResourceFiles.empty() ) {
        // if there is no makefile resources - use defaults
        string default_rc;
        if (m_Project->m_ProjType == CProjKey::eApp) {
            default_rc = GetApp().GetSite().GetAppDefaultResource();
        }
        if ( !default_rc.empty() ) {
            string abs_path = GetApp().GetProjectTreeInfo().m_Compilers;
            abs_path = 
                CDirEntry::ConcatPath(abs_path, 
                                    GetApp().GetRegSettings().m_CompilersSubdir);
            abs_path = CDirEntry::ConcatPath(abs_path, default_rc);
            abs_path = CDirEntry::NormalizePath(abs_path);
            m_ResourceFiles.push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                            abs_path));
        }
    }
    m_ResourceFiles.sort(s_FileName_less);
}



//-----------------------------------------------------------------------------
// Collect all files from specified dirs having specified exts
static void s_CollectRelPathes(const string&        path_from,
                               const list<string>&  abs_dirs,
                               list<string>*        rel_pathes)
{
    rel_pathes->clear();

    list<string> pathes;
    ITERATE(list<string>, n, abs_dirs) {
        string value(*n), pdir, base, ext;
        if (value.empty()) {
            continue;
        }
        
        SIZE_TYPE negation_pos = value.find('!');
        bool remove = negation_pos != NPOS;
        if (remove) {
            value = NStr::Replace(value, "!", kEmptyStr);
            if (value.empty() ||
                value[value.length()-1] == CDirEntry::GetPathSeparator()) {
                continue;
            }
        }
        CDirEntry::SplitPath(value, &pdir, &base, &ext);
        CDir dir(pdir);
        if ( !dir.Exists() )
            continue;
        CDir::TEntries contents = dir.GetEntries(base + ext);
        ITERATE(CDir::TEntries, i, contents) {
            if ( (*i)->IsFile() ) {
                string path  = (*i)->GetPath();
                if ( NStr::EndsWith(path, ext, NStr::eNocase) ) {
                    if (remove) {
                        pathes.remove(path);
                    } else {
                        pathes.push_back(path);
                    }
                }
            }
        }
    }

    ITERATE(list<string>, p, pathes)
        rel_pathes->push_back(CDirEntry::CreateRelativePath(path_from, *p));
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/11/17 19:52:14  gouriano
 * Corrected to take into account DTD_PROJ
 *
 * Revision 1.9  2004/10/21 14:48:06  gouriano
 * Sort project files alphabetically
 *
 * Revision 1.8  2004/10/12 16:18:26  ivanov
 * Added configurable file support
 *
 * Revision 1.7  2004/10/12 13:27:35  gouriano
 * Added possibility to specify which headers to include into project
 *
 * Revision 1.6  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/05/19 14:25:53  gorelenk
 * Changed implementation of CMsvcPrjFilesCollector::CollectResources -
 * default resource will be added omly if there are no excplicit resource
 * in msvc makefile.
 *
 * Revision 1.4  2004/05/17 14:38:39  gorelenk
 * Changed CMsvcPrjFilesCollector::CollectResources - implemented addition of
 * default resource to 'app' projects.
 *
 * Revision 1.3  2004/05/10 19:54:34  gorelenk
 * Changed s_CollectRelPathes .
 *
 * Revision 1.2  2004/03/05 20:32:48  gorelenk
 * Added implementation of CMsvcPrjFilesCollector member-functions.
 *
 * Revision 1.1  2004/03/05 18:04:55  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
