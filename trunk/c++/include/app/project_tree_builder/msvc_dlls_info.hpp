#ifndef MSVC_DLLS_INDO_HEADER
#define MSVC_DLLS_INDO_HEADER

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
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <corelib/ncbireg.hpp>
#include <list>
#include <set>
#include <app/project_tree_builder/proj_tree.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE


class CMsvcDllsInfo
{
public:
    CMsvcDllsInfo(const CNcbiRegistry& registry);
    ~CMsvcDllsInfo(void);
    
    void GetDllsList      (list<string>*      dlls_ids) const;
    void GetBuildConfigs  (list<SConfigInfo>* config)   const;
    string GetBuildDefine (void) const;

    struct SDllInfo
    {
        list<string> m_Hosting;
        list<string> m_Depends;
        string       m_DllDefine;

        bool IsEmpty(void) const;
        void Clear  (void);
    };
    void GetDllInfo(const string& dll_id, SDllInfo* dll_info) const;

    bool   IsDllHosted(const string& lib_id) const;
    string GetDllHost (const string& lib_id) const; 

private:
    const CNcbiRegistry& m_Registry;

    //no value-type semantics
    CMsvcDllsInfo(void);
    CMsvcDllsInfo(const CMsvcDllsInfo&);
    CMsvcDllsInfo& operator= (const CMsvcDllsInfo&);

};


void FilterOutDllHostedProjects(const CProjectItemsTree& tree_src, 
                                CProjectItemsTree*       tree_dst);

void CreateDllBuildTree(const CProjectItemsTree& tree_src, 
                        CProjectItemsTree*       tree_dst);


void CreateDllsList(const CProjectItemsTree& tree_src,
                    list<string>*            dll_ids);


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/03/10 16:36:03  gorelenk
 * Added definition of functions FilterOutDllHostedProjects,
 * CreateDllBuildTree and CreateDllsList.
 *
 * Revision 1.3  2004/03/08 23:27:40  gorelenk
 * Added declarations of member-functions  IsDllHosted,
 * GetDllHost, GetLibPrefixes to class CMsvcDllsInfo.
 *
 * Revision 1.2  2004/03/03 22:17:33  gorelenk
 * Added declaration of class CMsvcDllsInfo.
 *
 * Revision 1.1  2004/03/03 17:08:32  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */


#endif //MSVC_DLLS_INDO_HEADER