#ifndef PROJECT_TREE_BUILDER_MSVC_SITE__HPP
#define PROJECT_TREE_BUILDER_MSVC_SITE__HPP
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
#include <app/project_tree_builder/ptb_registry.hpp>
#include <set>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// SLibInfo --
///
/// Abstraction of lib description in site.
///
/// Provides information about 
/// additional include dir, library path and libs list.

struct SLibInfo
{
    list<string> m_IncludeDir;
    list<string> m_LibDefines;
    string       m_LibPath;
    list<string> m_Libs;
    list<string> m_StdLibs;
    list<string> m_Macro;
    list<string> m_Files;

    bool IsEmpty(void) const
    {
        return m_IncludeDir.empty() &&
               m_LibDefines.empty() &&
               m_LibPath.empty()    && 
               m_Libs.empty()       &&
               m_StdLibs.empty()    &&
               m_Macro.empty()      &&
               m_Files.empty();
    }
    void Clear(void)
    {
        m_IncludeDir.clear();
        m_LibDefines.clear();
        m_LibPath.erase();
        m_Libs.clear();
        m_StdLibs.clear();
        m_Macro.clear();
        m_Files.clear();
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcSite --
///
/// Abstraction of user site for building of C++ projects.
///
/// Provides information about libraries availability as well as implicit
/// exclusion of some branches from project tree.

class CMsvcSite
{
public:
    CMsvcSite(const CNcbiRegistry& registry);

    // Is REQUIRES provided?
    bool IsProvided(const string& thing) const;

    /// Get components from site
    void GetComponents(const string& entry, list<string>* components) const;

    /// Is section present in site registry?
    bool IsDescribed(const string& section) const;

    string ProcessMacros(string data, bool preserve_unresolved=true) const;
    // Get library (LIBS) description
    void GetLibInfo(const string& lib, 
                    const SConfigInfo& config, SLibInfo* libinfo) const;
    // Is this lib available in certain config
    bool IsLibEnabledInConfig(const string&      lib, 
                              const SConfigInfo& config) const;
    
    // Resolve define (now from CPPFLAGS)
    bool ResolveDefine(const string& define, string& resolved) const;

    // Configure related:
    // Path from tree root to file where configure defines must be.
    string GetConfigureDefinesPath(void) const;
    // What we have to define:
    void   GetConfigureDefines    (list<string>* defines) const;

    // Lib Choices related:
    enum ELibChoice {
        eUnknown,
        eLib,
        e3PartyLib
    };
    struct SLibChoice
    {
        SLibChoice(void);

        SLibChoice(const CMsvcSite& site,
                   const string&    lib,
                   const string&    lib_3party);

        ELibChoice m_Choice;
        string     m_LibId;
        string     m_3PartyLib;
    };
    bool IsLibWithChoice            (const string& lib_id) const;
    bool Is3PartyLibWithChoice      (const string& lib3party_id) const;

    ELibChoice GetChoiceForLib      (const string& lib_id) const;
    ELibChoice GetChoiceFor3PartyLib(const string& lib3party_id,
                                     const SConfigInfo& cfg_info) const;

    void GetLibChoiceIncludes(const string& cpp_flags_define,
                              list<string>* abs_includes) const;
    void GetLibChoiceIncludes(const string& cpp_flags_define,
                              const SConfigInfo& cfg_info,
                              list<string>* abs_includes) const;
    void GetLibInclude(const string& lib_id,
                       const SConfigInfo& cfg_info,
                       list<string>* includes) const;

    SLibChoice GetLibChoiceForLib   (const string& lib_id) const;
    SLibChoice GetLibChoiceFor3PartyLib   (const string& lib3party_id) const;


    
    string GetAppDefaultResource(void) const;


    void   GetThirdPartyLibsToInstall    (list<string>* libs) const;
    string GetThirdPartyLibsBinPathSuffix(void) const;
    string GetThirdPartyLibsBinSubDir    (void) const;
    void   GetStandardFeatures           (list<string>& features) const;
    
    void ProcessMacros (const list<SConfigInfo>& configs);
    const CSymResolver& GetMacros(void) const
    {
        return m_Macros;
    }

    static bool IsLibOk(const SLibInfo& lib_info, bool silent = false);
private:
    CPtbRegistry m_Registry;
    CSimpleMakeFileContents m_UnixMakeDef;
    
    set<string> m_ProvidedThing;
    set<string> m_NotProvidedThing;

    list<SLibChoice> m_LibChoices;

    CSymResolver m_Macros;

    /// cache of directories and their existence
    typedef map<string, bool> TDirectoryExistenceMap;
    static TDirectoryExistenceMap sm_DirExists;
    static bool x_DirExists(const string& dir_name);

    /// Prohibited to:
    CMsvcSite(void);
    CMsvcSite(const CMsvcSite&);
    CMsvcSite& operator= (const CMsvcSite&);
};

END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER_MSVC_SITE__HPP
