#ifndef PROJECT_TREE_BUILDER__FILE_CONTENTS__HPP
#define PROJECT_TREE_BUILDER__FILE_CONTENTS__HPP

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

#include <string>
#include <list>

#include <corelib/ncbistre.hpp>
#include <app/project_tree_builder/proj_utils.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

enum EMakeFileType {
    eMakeType_Undefined  = 0,
    eMakeType_Expendable = 1,
    eMakeType_Potential  = 2,
    eMakeType_Excluded   = 3,
    eMakeType_ExcludedByReq = 4
};

string MakeFileTypeAsString(EMakeFileType type);

/////////////////////////////////////////////////////////////////////////////
///
/// CSimpleMakeFileContents --
///
/// Abstraction of makefile contents.
///
/// Container for key/values pairs with included makefiles parsing support.

class CSimpleMakeFileContents
{
public:
    CSimpleMakeFileContents(void);
    CSimpleMakeFileContents(const CSimpleMakeFileContents& contents);

    CSimpleMakeFileContents& operator= (
	    const CSimpleMakeFileContents& contents);

    CSimpleMakeFileContents(const string& file_path, EMakeFileType type);

    ~CSimpleMakeFileContents(void);

    /// Key-Value(s) pairs
    typedef map< string, list<string> > TContents;
    TContents m_Contents;

    static void LoadFrom(const string& file_path, CSimpleMakeFileContents* fc);
    void AddDefinition( const string& key, const string& value);
    bool GetValue(const string& key, string& value) const;
    
    EMakeFileType GetMakeType(void) const
    {
        return m_Type;
    }

    /// Debug dump
    void Dump(CNcbiOfstream& ostr) const;

private:
    void Clear(void);

    void SetFrom(const CSimpleMakeFileContents& contents);

    struct SParser;
    friend struct SParser;

    struct SParser
    {
        SParser(CSimpleMakeFileContents* fc);

        void StartParse(void);
        void AcceptLine(const string& line);
        void EndParse(void);

        bool      m_Continue;
        SKeyValue m_CurrentKV;

        CSimpleMakeFileContents* m_FileContents;

    private:
        SParser(void);
        SParser(const SParser&);
        SParser& operator= (const SParser&);
    };

    void AddReadyKV(const SKeyValue& kv);
    EMakeFileType m_Type;
};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__FILE_CONTENTS__HPP
