#ifndef PROJ_BUILDER_APP_HEADER
#define PROJ_BUILDER_APP_HEADER

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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <app/project_tree_builder/proj_utils.hpp>
#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CProjBulderApp --
///
/// Abstraction of proj_tree_builder application.
///
/// Singleton implementation.

class CProjBulderApp : public CNcbiApplication
{
public:
    

    /// ShortCut for enums
    int EnumOpt(const string& enum_name, const string& enum_val) const;

    /// Singleton
    friend CProjBulderApp& GetApp(void);


private:
    CProjBulderApp(void);

    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);

    /// Parse program arguments.
    void ParseArguments(void);

    /// Solution to build.
    string m_Solution;


    //TFiles m_FilesMakeIn;
    //TFiles m_FilesMakeLib;
    //TFiles m_FilesMakeApp;

    typedef map<string, CSimpleMakeFileContents> TFiles;
    void DumpFiles(const TFiles& files, const string& filename) const;
    
    auto_ptr<CMsvc7RegSettings> m_MsvcRegSettings;
    auto_ptr<CMsvcSite>         m_MsvcSite;
    auto_ptr<CMsvcMetaMakefile> m_MsvcMetaMakefile;

    auto_ptr<SProjectTreeInfo>  m_ProjectTreeInfo;

public:

    void    GetMetaDataFiles    (list<string>*      files)   const;


    const CMsvc7RegSettings& GetRegSettings(void);
    
    const CMsvcSite&         GetSite(void);

    const CMsvcMetaMakefile& GetMetaMakefile(void);

    const SProjectTreeInfo&  GetProjectTreeInfo(void);

    string GetDatatoolId          (void) const;
    string GetDatatoolPathForApp  (void) const;
    string GetDatatoolPathForLib  (void) const;
    string GetDatatoolCommandLine (void) const;
    
private:
    void    GetBuildConfigs     (list<SConfigInfo>* configs) const;
};


/// access to App singleton

CProjBulderApp& GetApp(void);


/////////////////////////////////////////////////////////////////////////////
///
/// CProjBulderAppException --
///
/// Exception class.
///
/// proj_tree_builder specific exceptions.

class CProjBulderAppException : public CException
{
public:
    enum EErrCode {
        eFileCreation,
        eEnumValue,
        eFileOpen,
        eProjectType,
        eBuildConfiguration,
        eMetaMakefile,
        eConfigureDefinesPath
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eFileCreation:
            return "Can not create file";
        case eEnumValue:
            return "Bad or missing enum value";
        case eFileOpen:
            return "Can not open file";
        case eProjectType:
            return "Unknown project type";
        case eBuildConfiguration:
            return "Unknown build configuration";
        case eMetaMakefile:
            return "Bad or missed metamakefile";
        case eConfigureDefinesPath:
            return "Path to configure defines file is empty";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CProjBulderAppException, CException);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/02/05 16:24:57  gorelenk
 * + eConfigureDefinesPath in CProjBulderAppException class.
 *
 * Revision 1.8  2004/01/30 20:41:34  gorelenk
 * Added support of ASN projects
 *
 * Revision 1.7  2004/01/28 17:55:06  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.6  2004/01/26 19:25:42  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.5  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif
