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
#include <app/project_tree_builder/resolver.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CSymResolver::CSymResolver(void)
{
    Clear();
}


CSymResolver::CSymResolver(const CSymResolver& resolver)
{
    SetFrom(resolver);
}


CSymResolver::CSymResolver(const string& file_path)
{
    LoadFrom(file_path, this);
}


CSymResolver& CSymResolver::operator= (const CSymResolver& resolver)
{
    if (this != &resolver) {
	    Clear();
	    SetFrom(resolver);
    }
    return *this;
}


CSymResolver::~CSymResolver(void)
{
    Clear();
}


string CSymResolver::StripDefine(const string& define)
{
    return string(define, 2, define.length() - 3);
}


void CSymResolver::Resolve(const string& define, list<string>* resolved_def)
{
    resolved_def->clear();

    if ( !HasDefine(define) ) {
	    resolved_def->push_back(define);
	    return;
    }

    string data(define);
    string::size_type start, end;
    start = data.find("$(");
    end = data.find(")", start);
    if (end == string::npos) {
        LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + define);
	    resolved_def->push_back(define);
	    return;
    }
    string raw_define = data.substr(start,end-start+1);
    string str_define = StripDefine( raw_define );

    CSimpleMakeFileContents::TContents::const_iterator m =
        m_Cache.find(str_define);

    if (m != m_Cache.end()) {
	    *resolved_def = m->second;
    } else {
        ITERATE(CSimpleMakeFileContents::TContents, p, m_Data.m_Contents) {
	        if (p->first == str_define) {
                ITERATE(list<string>, n, p->second) {
                    list<string> new_resolved_def;
                    Resolve(*n, &new_resolved_def);
                    copy(new_resolved_def.begin(),
                        new_resolved_def.end(),
                        back_inserter(*resolved_def));
                }
	        }
        }
        m_Cache[str_define] = *resolved_def;
    }

    if ( !IsDefine(define) && resolved_def->size() == 1 ) {
        data = NStr::Replace(data, raw_define, resolved_def->front());
        resolved_def->clear();
        resolved_def->push_back(data);
    }
}

void CSymResolver::Resolve(const string& define, list<string>* resolved_def,
                           const CSimpleMakeFileContents& mdata)
{
    resolved_def->clear();

    if ( !HasDefine(define) ) {
	    resolved_def->push_back(define);
	    return;
    }

    string data(define);
    string::size_type start, end;
    start = data.find("$(");
    end = data.find(")", start);
    if (end == string::npos) {
        LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + define);
	    resolved_def->push_back(define);
	    return;
    }
    string raw_define = data.substr(start,end-start+1);
    string str_define = StripDefine( raw_define );

    ITERATE(CSimpleMakeFileContents::TContents, p, mdata.m_Contents) {
	    if (p->first == str_define) {
            ITERATE(list<string>, n, p->second) {
                list<string> new_resolved_def;
                Resolve(*n, &new_resolved_def, mdata);
                copy(new_resolved_def.begin(),
                    new_resolved_def.end(),
                    back_inserter(*resolved_def));
            }
	    }
    }

    if ( !IsDefine(define) && resolved_def->size() == 1 ) {
        data = NStr::Replace(data, raw_define, resolved_def->front());
        resolved_def->clear();
        resolved_def->push_back(data);
    }
}


CSymResolver& CSymResolver::operator+= (const CSymResolver& src)
{
    // Clear cache for resolved defines
    m_Cache.clear();
    
    // Add contents of src
    copy(src.m_Data.m_Contents.begin(), 
         src.m_Data.m_Contents.end(), 
         inserter(m_Data.m_Contents, m_Data.m_Contents.end()));

    return *this;
}

bool CSymResolver::IsDefine(const string& param)
{
    return NStr::StartsWith(param, "$(")  &&  NStr::EndsWith(param, ")");
}

bool CSymResolver::HasDefine(const string& param)
{
    return (param.find("$(") != string::npos && param.find(")") != string::npos );
}


void CSymResolver::LoadFrom(const string& file_path, 
                            CSymResolver * resolver)
{
    resolver->Clear();
    CSimpleMakeFileContents::LoadFrom(file_path, &resolver->m_Data);
}

void CSymResolver::AddDefinition(const string& key, const string& value)
{
    m_Data.AddDefinition(key, value);
}

bool CSymResolver::IsEmpty(void) const
{
    return m_Data.m_Contents.empty();
}


void CSymResolver::Clear(void)
{
    m_Data.m_Contents.clear();
    m_Cache.clear();
}


void CSymResolver::SetFrom(const CSymResolver& resolver)
{
    m_Data  = resolver.m_Data;
    m_Cache = resolver.m_Cache;
}


//-----------------------------------------------------------------------------
// Filter opt defines like $(SRC_C:.core_%)           to $(SRC_C).
// or $(OBJMGR_LIBS:dbapi_driver=dbapi_driver-static) to $(OBJMGR_LIBS)
string FilterDefine(const string& define)
{
    if ( !CSymResolver::IsDefine(define) )
        return define;

    string res;
    for(string::const_iterator p = define.begin(); p != define.end(); ++p) {
        char ch = *p;
        if ( !(ch == '$'   || 
               ch == '('   || 
               ch == '_'   || 
               isalpha((unsigned char) ch) || 
               isdigit((unsigned char) ch) ) )
            break;
        res += ch;
    }
    res += ')';
    return res;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2005/09/15 18:24:37  gouriano
 * Recognize and process local (within a single makefile) macros
 *
 * Revision 1.11  2005/07/06 19:12:20  gouriano
 * Recognize and process macros inside a larger string
 *
 * Revision 1.10  2005/06/03 16:27:50  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.9  2004/07/20 13:38:40  gouriano
 * Added conditional macro definition
 *
 * Revision 1.8  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/02/17 16:18:33  gorelenk
 * Added digit as an allowed symbol for makefile defines.
 *
 * Revision 1.6  2004/02/10 18:16:00  gorelenk
 * Fixed recursive resolving procedure.
 *
 * Revision 1.5  2004/02/04 23:57:22  gorelenk
 * Added definition of function FilterDefine.
 *
 * Revision 1.4  2004/01/28 17:55:50  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.3  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */
 
