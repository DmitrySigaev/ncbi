#ifndef CORELIB___NCBIDIAG_P__HPP
#define CORELIB___NCBIDIAG_P__HPP

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
 * Author:  Vyacheslav Kononenko
 *
 *
 */

/// @file ncbidiag_p.hpp
///
///   Defines NCBI C++ service classes and functions for diagnostic APIs,
///   classes, and macros.
///
///   More elaborate documentation could be found in:
///     http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/
///            programming_manual/diag.html

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbiutil.hpp>
#include <string>
#include <vector>
#include <istream>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
/// Forward declaration

class CNcbiDiag;

enum EDiagFilterAction {
    eDiagFilter_None,
    eDiagFilter_Accept,
    eDiagFilter_Reject
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrMatcher --
///
/// Abstract class that matches a char* string.
///

class CDiagStrMatcher
{
public:
    virtual ~CDiagStrMatcher();

    /// Return true if str matches for this object
    virtual bool Match(const char* str) const = 0;

    // Print state
    virtual void Print(ostream& out) const = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrEmptyMatcher --
///
/// Specialization of CDiagStrMatcher that matches an empty string.
///

class CDiagStrEmptyMatcher : public CDiagStrMatcher
{
public:
    /// Return true if str is empty
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrStringMatcher --
///
/// Specialization of CDiagStrMatcher that macthes a given string.
///

class CDiagStrStringMatcher : public CDiagStrMatcher
{
public:
    // Constructor - set the pattern to match
    CDiagStrStringMatcher(const string& pattern)
        : m_Pattern(pattern)
    {}

    /// Return true if str is equal to pattern
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;

private:
    string m_Pattern;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrPathMatcher --
///
/// Specialization of CDiagStrMatcher that macthes a path to the source file
///

class CDiagStrPathMatcher : public CDiagStrMatcher
{
public:
    // Constructor - set the pattern to match
    CDiagStrPathMatcher(const string& pattern)
        : m_Pattern(pattern)
    {}

    /// Return true if str is equal to pattern
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;

private:
    string m_Pattern;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagMatcher --
///
/// Matcher of CNcbiDiag for module, class and function name if any
///

class CDiagMatcher
{
public:
    // Takes ownership of module, nclass and function.
    // If NULL pointer passed as CDiagStrMatcher*, then it accepts anything
    CDiagMatcher(CDiagStrMatcher*  file,
                 CDiagStrMatcher*  module, 
                 CDiagStrMatcher*  nclass, 
                 CDiagStrMatcher*  func,
                 EDiagFilterAction action)
        : m_File(file), m_Module(module), m_Class(nclass), m_Function(func), 
          m_Action(action)
    {}

    // Check if the filter accepts a filename
    EDiagFilterAction MatchFile(const char* file) const;

    // Match whole set
    EDiagFilterAction Match(const char* module,
                            const char* nclass,
                            const char* function) const;

    // Print state
    void Print(ostream& out) const;

private:
    AutoPtr<CDiagStrMatcher> m_File;
    AutoPtr<CDiagStrMatcher> m_Module;
    AutoPtr<CDiagStrMatcher> m_Class;
    AutoPtr<CDiagStrMatcher> m_Function;
    EDiagFilterAction        m_Action;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagFilter --
///
/// Checks if SDiagMessage should be passed.
/// Contains 0..N CDiagMatcher objects
///

class CDiagFilter
{
public:
    // 'tors
    CDiagFilter()   {}
    ~CDiagFilter()  { Clean(); }

    // Check if the filter accepts the pass
    EDiagFilterAction CheckFile(const char* file) const;

    // Check if the filter accepts the message
    EDiagFilterAction Check(const CNcbiDiag& message) const;

    // Check if the filter accepts the exception
    // NOTE: iterate through all exceptions and return TRUE
    //       if any exception matches
    EDiagFilterAction Check(const CException& ex) const;

    // Fill the filter from a string
    void Fill(const char* filter_string);

    // Print state
    void Print(ostream& out) const;

private:
    // Check if the filter accepts the location
    EDiagFilterAction x_Check(const char* module,
                              const char* nclass,
                              const char* function) const;

    // CSyntaxParser can insert CDiagStrMatcher and clean
    friend class CDiagSyntaxParser;

    // Insert a new matcher into the end of list
    // Take ownership of the matcher
    void InsertMatcher(CDiagMatcher *matcher)
    { m_Matchers.push_back(matcher); }

    // Remove and destroy all matchers, if any
    void Clean()  { m_Matchers.clear(); }

    typedef vector< AutoPtr<CDiagMatcher> >  TMatchers;
    TMatchers m_Matchers;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagLexParser --
///
/// Lexical parser
///

class CDiagLexParser
{
public:
    // Symbols
    enum ESymbol {
        eDone,           // to mark that symbol was processed
        eExpl,           // ! processed
        ePath,           // path started with /
        eId,             // id and ?
        eDoubleColon,    // ::
        ePars,           // ()
        eEnd             // end of stream
    };

    // Constructor
    CDiagLexParser();

    // Takes next lexical symbol from the stream
    ESymbol Parse(istream& in);

    // Returns id or ? string when eId is returned from parse()
    const string& GetId() const  { return m_Str; }

    // Returns current position
    int GetPos() const { return m_Pos; }

private:
    string  m_Str;
    int     m_Pos;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagSyntaxParser --
///
/// Parses string and fills CDiagFilter with CDiagStrMatchers
///

class CDiagSyntaxParser
{
public:
    // Error info type
    typedef pair<const char*, int> TErrorInfo;


    // Constructor
    CDiagSyntaxParser();

    // Parses stream and fills CDiagFilter to with CDiagStrMatchers
    void Parse(istream& in, CDiagFilter& to);

private:
    enum EInto {
        eModule,
        eFunction
    };

    // Puts CDiagMessageMatcher into CDiagFilter
    void x_PutIntoFilter(CDiagFilter& to, EInto into);

    // Creates a CDiagStrMatcher by string
    static CDiagStrMatcher* x_CreateMatcher(const string& str);

    typedef vector< AutoPtr<CDiagStrMatcher> >  TMatchers;
    TMatchers                 m_Matchers;
    AutoPtr<CDiagStrMatcher>  m_FileMatcher;             
    int                       m_Pos;
    bool                      m_Negative;
};


END_NCBI_SCOPE


/*
 * ==========================================================================
 *
 * $Log$
 * Revision 1.1  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * ==========================================================================
 */

#endif  /* CORELIB___NCBIDIAG_P__HPP */
