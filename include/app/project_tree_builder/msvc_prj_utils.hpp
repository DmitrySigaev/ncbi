#ifndef MSVC_PRJ_UTILS_HEADER
#define MSVC_PRJ_UTILS_HEADER

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


#include <corelib/ncbireg.hpp>

#include <app/project_tree_builder/msvc71_project__.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Creates CVisualStudioProject class instance from file.
///
/// @param file_path
///   Path to file load from.
/// @return
///   Created on heap CVisualStudioProject instance or NULL
///   if failed.
CVisualStudioProject * LoadFromXmlFile(const string& file_path);


/// Save CVisualStudioProject class instance to file.
///
/// @param file_path
///   Path to file project will be saved to.
/// @param project
///   Project to save.
void SaveToXmlFile  (const string&               file_path, 
                     const CVisualStudioProject& project);

/// Save CVisualStudioProject class instance to file only if no such file 
//  or contents of this file will be different from already present file.
///
/// @param file_path
///   Path to file project will be saved to.
/// @param project
///   Project to save.
void SaveIfNewer    (const string&               file_path, 
                     const CVisualStudioProject& project);

/// Consider promotion candidate to present 
void PromoteIfDifferent(const string& present_path, 
                        const string& candidate_path);


/// Generate pseudo-GUID.
string GenerateSlnGUID(void);

/// Get extension for source file without extension.
///
/// @param file_path
///   Source file full path withour extension.
/// @return
///   Extension of source file (".cpp" or ".c") 
///   if such file exist. Empty string string if there is no
///   such file.
string SourceFileExt(const string& file_path);


/////////////////////////////////////////////////////////////////////////////
///
/// SConfigInfo --
///
/// Abstraction of configuration informaion.
///
/// Configuration name, debug/release flag, runtime library 
/// 

struct SConfigInfo
{
    SConfigInfo(void);
    SConfigInfo(const string& name, 
                bool debug, 
                const string& runtime_library);

    string m_Name;
    bool   m_Debug;
    string m_RuntimeLibrary;
};

/////////////////////////////////////////////////////////////////////////////
///
/// SCustomBuildInfo --
///
/// Abstraction of custom build source file.
///
/// Information for custom buil source file 
/// (not *.c, *.cpp, *.midl, *.rc, etc.)
/// MSVC does not know how to buil this file and
/// we provide information how to do it.
/// 

struct SCustomBuildInfo
{
    string m_SourceFile;
    string m_CommandLine;
    string m_Description;
    string m_Outputs;
    string m_AdditionalDependencies;

    bool IsEmpty(void) const
    {
        return m_SourceFile.empty() || m_CommandLine.empty();
    }
    void Clear(void)
    {
        m_SourceFile.erase();
        m_CommandLine.erase();
        m_Description.erase();
        m_Outputs.erase();
        m_AdditionalDependencies.erase();
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvc7RegSettings --
///
/// Abstraction of [msvc7] section in app registry.
///
/// Settings for generation of msvc 7.10 projects
/// 

class CMsvc7RegSettings
{
public:
    CMsvc7RegSettings(void);

    string            m_Version;
    list<SConfigInfo> m_ConfigInfo;
    string            m_ProjectEngineName;
    string            m_Encoding;
    string            m_CompilersSubdir;
    string            m_MakefilesExt;
    string            m_MetaMakefile;
    list<string>      m_NotProvidedRequests;

private:
    CMsvc7RegSettings(const CMsvc7RegSettings&);
    CMsvc7RegSettings& operator= (const CMsvc7RegSettings&);
};


/// Is abs_dir a parent of abs_parent_dir.
bool IsSubdir(const string& abs_parent_dir, const string& abs_dir);


/// Erase if predicate is true
template <class C, class P> 
void EraseIf(C& cont, const P& pred)
{
    for (C::iterator p = cont.begin(); p != cont.end(); )
    {
        if ( pred(*p) )
            p = cont.erase(p);
        else
            ++p;
    }
}

/// Get option fron registry from  
///     [<section>.debug.<ConfigName>] section for debug configuratios
///  or [<section>.release.<ConfigName>] for release configurations
///
/// if no such option then try      
///     [<section>.debug]
/// or  [<section>.release]
///
/// if no such option then finally try
///     [<section>]
///
string GetOpt(const CNcbiRegistry& registry, 
              const string&        section, 
              const string&        opt, 
              const SConfigInfo&   config);

/// return <config>|Win32 as needed by MSVC compiler
string ConfigName(const string& config);


/// Common function shared by 
/// CMsvcMasterProjectGenerator and CMsvcProjectGenerator
void AddCustomBuildFileToFilter(CRef<CFilter>&          filter, 
                                const list<SConfigInfo> configs,
                                const SCustomBuildInfo& build_info);

/// Checks if 2 dirs has the same root
bool SameRootDirs(const string& dir1, const string& dir2);

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/02/10 18:08:16  gorelenk
 * Added declaration of functions SaveIfNewer and PromoteIfDifferent
 * - support for file overwriting only if it was changed.
 *
 * Revision 1.9  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.8  2004/02/05 16:26:43  gorelenk
 * Function GetComponents moved to class CMsvcSite member.
 *
 * Revision 1.7  2004/02/04 23:17:58  gorelenk
 * Added declarations of functions GetComponents and SameRootDirs.
 *
 * Revision 1.6  2004/02/03 17:09:46  gorelenk
 * Changed declaration of class CMsvc7RegSettings.
 * Added declaration of function GetComponents.
 *
 * Revision 1.5  2004/01/28 17:55:05  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.4  2004/01/26 19:25:41  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.3  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // MSVC_PRJ_UTILS_HEADER