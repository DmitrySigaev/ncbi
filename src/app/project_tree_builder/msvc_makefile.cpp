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

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CMsvcMetaMakefile::CMsvcMetaMakefile(const string& file_path)
{
    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (ifs) {
        //read registry
        m_MakeFile.Read(ifs);
        //and remember dir from where it was loaded
        CDirEntry::SplitPath(file_path, &m_MakeFileBaseDir);
    }
}


bool CMsvcMetaMakefile::IsEmpty(void) const
{
    return m_MakeFile.Empty();
}


string CMsvcMetaMakefile::GetCompilerOpt(const string& opt, 
                                         const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "Compiler", opt, config);
}


string CMsvcMetaMakefile::GetLinkerOpt(const string& opt, 
                                       const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "Linker", opt, config);
}


string CMsvcMetaMakefile::GetLibrarianOpt(const string& opt, 
                                          const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "Librarian", opt, config);
}


//-----------------------------------------------------------------------------
string CreateMsvcProjectMakefileName(const string&        project_name,
                                     CProjItem::TProjType type)
{
    string name("Makefile.");
    
    name += project_name + '.';
    
    switch (type) {
    case CProjKey::eApp:
        name += "app.";
        break;
    case CProjKey::eLib:
        name += "lib.";
        break;
    default:
        NCBI_THROW(CProjBulderAppException, 
                   eProjectType, 
                   NStr::IntToString(type));
        break;
    }

    name += "msvc";
    return name;
}


string CreateMsvcProjectMakefileName(const CProjItem& project)
{
    return CreateMsvcProjectMakefileName(project.m_Name, 
                                         project.m_ProjType);
}


//-----------------------------------------------------------------------------
CMsvcProjectMakefile::CMsvcProjectMakefile(const string& file_path)
    :CMsvcMetaMakefile(file_path)
{
}


bool CMsvcProjectMakefile::IsExcludeProject(bool default_val) const
{
    string val = m_MakeFile.GetString("Common", "ExcludeProject", "");

    if ( val.empty() )
        return default_val;

    return val != "FALSE";
}


void CMsvcProjectMakefile::GetAdditionalSourceFiles(const SConfigInfo& config,
                                                    list<string>* files) const
{
    string files_string = 
        GetOpt(m_MakeFile, "AddToProject", "SourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}


void CMsvcProjectMakefile::GetAdditionalLIB(const SConfigInfo& config, 
                                            list<string>*      lib_ids) const
{
    string lib_string = 
        GetOpt(m_MakeFile, "AddToProject", "LIB", config);
    
    NStr::Split(lib_string, LIST_SEPARATOR, *lib_ids);
}


void CMsvcProjectMakefile::GetExcludedSourceFiles(const SConfigInfo& config,  
                                                  list<string>* files) const
{
    string files_string = 
        GetOpt(m_MakeFile, 
               "ExcludedFromProject", "SourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}


void CMsvcProjectMakefile::GetExcludedLIB(const SConfigInfo& config, 
                                          list<string>*      lib_ids) const
{
    string lib_string = 
        GetOpt(m_MakeFile, 
               "ExcludedFromProject", "LIB", config);
    
    NStr::Split(lib_string, LIST_SEPARATOR, *lib_ids);
}


void CMsvcProjectMakefile::GetAdditionalIncludeDirs(const SConfigInfo& config,  
                                                    list<string>* dirs) const
{
    string dirs_string = 
        GetOpt(m_MakeFile, "AddToProject", "IncludeDirs", config);
    
    NStr::Split(dirs_string, LIST_SEPARATOR, *dirs);
}


void 
CMsvcProjectMakefile::GetCustomBuildInfo(list<SCustomBuildInfo>* info) const
{
    info->clear();

    string source_files_str = 
        m_MakeFile.GetString("CustomBuild", "SourceFiles", "");
    
    list<string> source_files;
    NStr::Split(source_files_str, LIST_SEPARATOR, source_files);

    ITERATE(list<string>, p, source_files){
        const string& source_file = *p;
        
        SCustomBuildInfo build_info;
        string source_file_path_abs = 
            CDirEntry::ConcatPath(m_MakeFileBaseDir, source_file);
        build_info.m_SourceFile = 
            CDirEntry::NormalizePath(source_file_path_abs);
        build_info.m_CommandLine = 
            m_MakeFile.GetString(source_file, "CommandLine", "");
        build_info.m_Description = 
            m_MakeFile.GetString(source_file, "Description", "");
        build_info.m_Outputs = 
            m_MakeFile.GetString(source_file, "Outputs", "");
        build_info.m_AdditionalDependencies = 
            m_MakeFile.GetString(source_file, "AdditionalDependencies", "");

        if ( !build_info.IsEmpty() )
            info->push_back(build_info);
    }
}


void CMsvcProjectMakefile::GetResourceFiles(const SConfigInfo& config, 
                                            list<string>*      files) const
{
    string files_string = 
        GetOpt(m_MakeFile, "AddToProject", "ResourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}


//-----------------------------------------------------------------------------
string GetCompilerOpt(const CMsvcMetaMakefile&    meta_file, 
                      const CMsvcProjectMakefile& project_file,
                      const string&               opt,
                      const SConfigInfo&          config)
{
   string val = project_file.GetCompilerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetCompilerOpt(opt, config);
}


string GetLinkerOpt(const CMsvcMetaMakefile&    meta_file, 
                    const CMsvcProjectMakefile& project_file,
                    const string&               opt,
                    const SConfigInfo&          config)
{
   string val = project_file.GetLinkerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLinkerOpt(opt, config);
}


string GetLibrarianOpt(const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       const SConfigInfo&          config)
{
   string val = project_file.GetLibrarianOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLibrarianOpt(opt, config);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/02/23 18:50:31  gorelenk
 * Added implementation of GetResourceFiles member-function
 * of class CMsvcProjectMakefile.
 *
 * Revision 1.6  2004/02/20 22:53:25  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.5  2004/02/12 16:27:56  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.4  2004/02/10 18:02:46  gorelenk
 * + GetAdditionalLIB.
 * + GetExcludedLIB - customization of depends.
 *
 * Revision 1.3  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.2  2004/01/28 17:55:48  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */
