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


#include "VisualStudioProject.hpp"

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

/// Creates CVisualStudioProject class instanse from file.
///
/// @param file_path
///   Path to file load from.
/// @return
///   Created on heap CVisualStudioProject instance or NULL
///   if failed.
CVisualStudioProject * LoadFromXmlFile(const string& file_path);


/// Save CVisualStudioProject class instanse to file.
///
/// @param file_path
///   Path to file project will be saved to.
/// @param project
///   Project to save.
void SaveToXmlFile  (const string&               file_path, 
                     const CVisualStudioProject& project);

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

private:
    CMsvc7RegSettings(const CMsvc7RegSettings&);
    CMsvc7RegSettings& operator= (const CMsvc7RegSettings&);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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