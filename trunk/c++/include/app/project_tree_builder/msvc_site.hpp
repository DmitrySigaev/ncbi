#ifndef MSVC_SITE_HEADER
#define MSVC_SITE_HEADER
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
#include <set>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
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
    string       m_IncludeDir;
    string       m_LibPath;
    list<string> m_Libs;

    bool IsEmpty(void) const
    {
        return m_Libs.empty();
    }
    void Clear(void)
    {
        m_IncludeDir.erase();
        m_LibPath.erase();
        m_Libs.clear();
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

    bool IsProvided(const string& thing) const;

    bool IsImplicitExclude(const string& node) const;

    void GetLibInfo(const string& lib, 
                    const SConfigInfo& config, SLibInfo* libinfo) const;

private:
    const CNcbiRegistry& m_Registry;
    
    set<string> m_NotProvidedThing;
    set<string> m_ImplicitExcludeNodes;

    /// Prohibited to:
    CMsvcSite(void);
    CMsvcSite(const CMsvcSite&);
    CMsvcSite& operator= (const CMsvcSite&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/01/28 17:55:06  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:25:41  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */

#endif // MSVC_SITE_HEADER