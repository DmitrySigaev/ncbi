#ifndef MSVC_PRJ_GENERATOR_HEADER
#define MSVC_PRJ_GENERATOR_HEADER

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
#include "VisualStudioProject.hpp"
#include <app/project_tree_builder/msvc_project_context.hpp>

#include <corelib/ncbienv.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcProjectGenerator --
///
/// Generator MSVC 7.10 project (*.vcproj file).
///
/// Generates MSVC 7.10 C++ project from the project tree item and save this
/// project to the appropriate place in "compilers" branch of the build tree.

class CMsvcProjectGenerator
{
public:
    CMsvcProjectGenerator(const list<string>& configs);
    ~CMsvcProjectGenerator();
    
    bool Generate(const CProjItem& prj);

private:

    list<string> m_Configs;

    /// Helpers:
    static void CollectSources(const CProjItem&              project,
                               const CMsvcPrjProjectContext& context,
                               list<string>*                 rel_pathes);


    static void CollectHeaders(const CProjItem&              project,
                               const CMsvcPrjProjectContext& context,
                               list<string>*                 rel_pathes);


    static void CollectInlines(const CProjItem&              project,
                               const CMsvcPrjProjectContext& context,
                               list<string>*                 rel_pathes);

    /// Prohibited to.
    CMsvcProjectGenerator(void);
    CMsvcProjectGenerator(const CMsvcProjectGenerator&);
    CMsvcProjectGenerator& operator= (const CMsvcProjectGenerator&);

};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // MSVC_PRJ_GENERATOR_HEADER