
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

#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


CSimpleMakeFileContents::CSimpleMakeFileContents(void)
{
}


CSimpleMakeFileContents::CSimpleMakeFileContents
    (const CSimpleMakeFileContents& contents)
{
    SetFrom(contents);
}


CSimpleMakeFileContents& CSimpleMakeFileContents::operator=
    (const CSimpleMakeFileContents& contents)
{
    if (this != &contents) {
        SetFrom(contents);
    }
    return *this;
}


CSimpleMakeFileContents::CSimpleMakeFileContents(const string& file_path)
{
    LoadFrom(file_path, this);
}


CSimpleMakeFileContents::~CSimpleMakeFileContents(void)
{
}


void CSimpleMakeFileContents::Clear(void)
{
    m_Contents.clear();
}


void CSimpleMakeFileContents::SetFrom(const CSimpleMakeFileContents& contents)
{
    m_Contents = contents.m_Contents;
}


void CSimpleMakeFileContents::LoadFrom(const string&            path, 
                                       CSimpleMakeFileContents* fc)
{
    CSimpleMakeFileContents::SParser parser(fc);
    fc->Clear();

    CNcbiIfstream ifs(path.c_str());
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, path);

    parser.StartParse();

    string strline;
    while ( getline(ifs, strline) )
	    parser.AcceptLine(strline);

    parser.EndParse();
}


void CSimpleMakeFileContents::Dump(CNcbiOfstream& ostr) const
{
    ITERATE(TContents, p, m_Contents) {
	    ostr << p->first << " = ";
	    ITERATE(list<string>, m, p->second) {
		    ostr << *m << " ";
	    }
	    ostr << endl;
    }
}


CSimpleMakeFileContents::SParser::SParser(CSimpleMakeFileContents* fc)
    :m_FileContents(fc)
{
}


void CSimpleMakeFileContents::SParser::StartParse(void)
{
    m_Continue  = false;
    m_CurrentKV = SKeyValue();
}



//------------------------------------------------------------------------------
// helpers ---------------------------------------------------------------------

static bool s_WillContinue(const string& line)
{
    return NStr::EndsWith(line, "\\");
}


static void s_StripContinueStr(string* str)
{
    str->erase(str->length() -1, 1); // delete last '\'
}


static bool s_SplitKV(const string& line, string* key, string* value)
{
    if ( !NStr::SplitInTwo(line, "=", *key, *value) )
	    return false;

    *key = NStr::TruncateSpaces(*key); // only for key - preserve sp for vals
    if ( s_WillContinue(*value) ) 
	    s_StripContinueStr(value);		

    return true;
}


static bool s_IsKVString(const string& str)
{
    return str.find("=") != NPOS;
}


static bool s_IsCommented(const string& str)
{
    return NStr::StartsWith(str, "#");
}



void CSimpleMakeFileContents::SParser::AcceptLine(const string& line)
{
    string strline = NStr::TruncateSpaces(line);
    if ( s_IsCommented(strline) )
	    return;

    if (m_Continue) {
	    m_Continue = s_WillContinue(strline);
	    if ( strline.empty() ) {
		    //fix for ill-formed makefiles:
		    m_FileContents->AddReadyKV(m_CurrentKV);
		    return;
	    } else if ( s_IsKVString(strline) ) {
		    //fix for ill-formed makefiles:
		    m_FileContents->AddReadyKV(m_CurrentKV);
		    m_Continue = false; // guard 
		    AcceptLine(strline.c_str()); 
	    }
	    if (m_Continue)
		    s_StripContinueStr(&strline);
	    m_CurrentKV.m_Value += strline;
	    return;
	    
    } else {
	    // may be only k=v string
	    if ( s_IsKVString(strline) ) {
		    m_FileContents->AddReadyKV(m_CurrentKV);
		    m_Continue = s_WillContinue(strline);
		    s_SplitKV(strline, &m_CurrentKV.m_Key, &m_CurrentKV.m_Value);
		    return;			
	    }
    }
}


void CSimpleMakeFileContents::SParser::EndParse(void)
{
    m_FileContents->AddReadyKV(m_CurrentKV);
    m_Continue = false;
    m_CurrentKV = SKeyValue();
}


void CSimpleMakeFileContents::AddReadyKV(const SKeyValue& kv)
{
    if ( kv.m_Key.empty() ) 
	    return;

    list<string> values;
    NStr::Split(kv.m_Value, " \t\n\r", values);

    m_Contents[kv.m_Key] = values;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/01/22 17:57:53  gorelenk
 * first version
 *
 * ===========================================================================
 */
