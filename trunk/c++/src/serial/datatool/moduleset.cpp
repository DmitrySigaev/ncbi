/*  $Id$
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
* Author: Eugene Vasilchenko
*
* File Description:
*   Set of modules: equivalent of ASN.1 source file
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2000/11/29 17:42:45  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependancy.
*
* Revision 1.28  2000/11/15 20:34:55  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.27  2000/11/14 21:41:25  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.26  2000/11/08 17:02:51  vasilche
* Added generation of modular DTD files.
*
* Revision 1.25  2000/09/26 17:38:26  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.24  2000/08/25 15:59:22  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.23  2000/07/10 17:32:00  vasilche
* Macro arguments made more clear.
* All old ASN stuff moved to serialasn.hpp.
* Changed prefix of enum info functions to GetTypeInfo_enum_.
*
* Revision 1.22  2000/06/16 20:01:30  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.21  2000/05/24 20:09:29  vasilche
* Implemented DTD generation.
*
* Revision 1.20  2000/04/07 19:26:29  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.19  2000/03/15 21:24:12  vasilche
* Error diagnostic about ambiguous types made more clear.
*
* Revision 1.18  2000/02/01 21:48:03  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.17  1999/12/30 21:33:40  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.16  1999/12/28 18:55:59  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.15  1999/12/21 17:18:36  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.14  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.13  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <typeinfo>
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/fileutil.hpp>

BEGIN_NCBI_SCOPE

CFileModules::CFileModules(const string& name)
    : m_SourceFileName(name)
{
}

void CFileModules::AddModule(const AutoPtr<CDataTypeModule>& module)
{
    module->SetModuleContainer(this);
    CDataTypeModule*& mptr = m_ModulesByName[module->GetName()];
    if ( mptr ) {
        ERR_POST(GetSourceFileName() << ": duplicate module: " <<
                 module->GetName());
    }
    else {
        mptr = module.get();
        m_Modules.push_back(module);
    }
}

bool CFileModules::Check(void) const
{
    bool ok = true;
    iterate ( TModules, mi, m_Modules ) {
        if ( !(*mi)->Check() )
            ok = false;
    }
    return ok;
}

bool CFileModules::CheckNames(void) const
{
    bool ok = true;
    iterate ( TModules, mi, m_Modules ) {
        if ( !(*mi)->CheckNames() )
            ok = false;
    }
    return ok;
}

void CFileModules::PrintASN(CNcbiOstream& out) const
{
    iterate ( TModules, mi, m_Modules ) {
        (*mi)->PrintASN(out);
    }
    m_LastComments.PrintASN(out, 0, CComments::eMultiline);
}

void CFileModules::PrintDTD(CNcbiOstream& out) const
{
    iterate ( TModules, mi, m_Modules ) {
        (*mi)->PrintDTD(out);
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

void CFileModules::PrintDTDModular(void) const
{
    iterate ( TModules, mi, m_Modules ) {
        string fileNameBase = (*mi)->GetDTDFileNameBase();
        {
            string fileName = fileNameBase + ".mod";
            CNcbiOfstream out(fileName.c_str());
            (*mi)->PrintDTD(out);
            if ( !out )
                ERR_POST(Fatal << "Cannot write to file "<<fileName);
        }
        {
            string fileName = fileNameBase + ".dtd";
            CNcbiOfstream out(fileName.c_str());
            (*mi)->PrintDTDModular(out);
            if ( !out )
                ERR_POST(Fatal << "Cannot write to file "<<fileName);
        }
    }
}

const string& CFileModules::GetSourceFileName(void) const
{
    return m_SourceFileName;
}

string CFileModules::GetFileNamePrefix(void) const
{
    if ( MakeFileNamePrefixFromSourceFileName() ) {
        if ( m_PrefixFromSourceFileName.empty() ) {
            m_PrefixFromSourceFileName = DirName(m_SourceFileName);
            if ( m_PrefixFromSourceFileName.empty() ||
                 m_PrefixFromSourceFileName[0] == '.' ||
                 m_PrefixFromSourceFileName[0] == '/' ) {
                // path absent or non local
                m_PrefixFromSourceFileName.erase();
                return GetModuleContainer().GetFileNamePrefix();
            }
        }
        _TRACE("file " << m_SourceFileName << ": \"" << m_PrefixFromSourceFileName << "\"");
        if ( UseAllFileNamePrefixes() ) {
            return Path(GetModuleContainer().GetFileNamePrefix(),
                        m_PrefixFromSourceFileName);
        }
        else {
            return m_PrefixFromSourceFileName;
        }
    }
    return GetModuleContainer().GetFileNamePrefix();
}

CDataType* CFileModules::ExternalResolve(const string& moduleName,
                                         const string& typeName,
                                         bool allowInternal) const
{
    // find module definition
    TModulesByName::const_iterator mi = m_ModulesByName.find(moduleName);
    if ( mi == m_ModulesByName.end() ) {
        // no such module
        THROW1_TRACE(CModuleNotFound,
                     "module not found: "+moduleName+" for type "+typeName);
    }
    return mi->second->ExternalResolve(typeName, allowInternal);
}

CDataType* CFileModules::ResolveInAnyModule(const string& typeName,
                                            bool allowInternal) const
{
    CResolvedTypeSet types(typeName);
    iterate ( TModules, i, m_Modules ) {
        try {
            types.Add((*i)->ExternalResolve(typeName, allowInternal));
        }
        catch ( CAmbiguiousTypes& exc ) {
            types.Add(exc);
        }
        catch ( CTypeNotFound& /* ignored */ ) {
        }
    }
    return types.GetType();
}

void CFileSet::AddFile(const AutoPtr<CFileModules>& moduleSet)
{
    moduleSet->SetModuleContainer(this);
    m_ModuleSets.push_back(moduleSet);
}

void CFileSet::PrintASN(CNcbiOstream& out) const
{
    iterate ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintASN(out);
    }
}

void CFileSet::PrintDTD(CNcbiOstream& out) const
{
#if 0
    out <<
        "<!-- ======================== -->\n"
        "<!-- NCBI DTD                 -->\n"
        "<!-- NCBI ASN.1 mapped to XML -->\n"
        "<!-- ======================== -->\n"
        "\n"
        "<!-- Entities used to give specificity to #PCDATA -->\n"
        "<!ENTITY % INTEGER '#PCDATA'>\n"
        "<!ENTITY % ENUM 'EMPTY'>\n"
        "<!ENTITY % BOOLEAN 'EMPTY'>\n"
        "<!ENTITY % NULL 'EMPTY'>\n"
        "<!ENTITY % REAL '#PCDATA'>\n"
        "<!ENTITY % OCTETS '#PCDATA'>\n"
        "<!-- ============================================ -->\n"
        "\n";
#endif
    iterate ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintDTD(out);
    }
}

void CFileSet::PrintDTDModular(void) const
{
    iterate ( TModuleSets, i, m_ModuleSets ) {
        (*i)->PrintDTDModular();
    }
}

CDataType* CFileSet::ExternalResolve(const string& module, const string& name,
                                     bool allowInternal) const
{
    CResolvedTypeSet types(module, name);
    iterate ( TModuleSets, i, m_ModuleSets ) {
        try {
            types.Add((*i)->ExternalResolve(module, name, allowInternal));
        }
        catch ( CAmbiguiousTypes& exc ) {
            types.Add(exc);
        }
        catch ( CTypeNotFound& /* ignored */ ) {
        }
    }
    return types.GetType();
}

CDataType* CFileSet::ResolveInAnyModule(const string& name,
                                        bool allowInternal) const
{
    CResolvedTypeSet types(name);
    iterate ( TModuleSets, i, m_ModuleSets ) {
        try {
            types.Add((*i)->ResolveInAnyModule(name, allowInternal));
        }
        catch ( CAmbiguiousTypes& exc ) {
            types.Add(exc);
        }
        catch ( CTypeNotFound& /* ignored */ ) {
        }
    }
    return types.GetType();
}

bool CFileSet::Check(void) const
{
    bool ok = true;
    iterate ( TModuleSets, mi, m_ModuleSets ) {
        if ( !(*mi)->Check() )
            ok = false;
    }
    return ok;
}

bool CFileSet::CheckNames(void) const
{
    bool ok = true;
    iterate ( TModuleSets, mi, m_ModuleSets ) {
        if ( !(*mi)->CheckNames() )
            ok = false;
    }
    return ok;
}

END_NCBI_SCOPE
