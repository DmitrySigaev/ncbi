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
 * Authors:  Denis Vakatov, Anton Butanayev
 *
 * File Description:
 *   Command-line arguments' processing:
 *      descriptions  -- CArgDescriptions,  CArgDesc
 *      constraints   -- CArgAllow;  CArgAllow_{Strings,Integers,Doubles}
 *      parsed values -- CArgs,             CArgValue
 *      exceptions    -- CArgException, ARG_THROW()
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.19  2000/11/08 17:48:37  butanaev
 * There was no minus in optional key synopsis, fixed.
 *
 * Revision 1.18  2000/11/01 20:37:15  vasilche
 * Fixed detection of heap objects.
 * Removed ECanDelete enum and related constructors.
 * Disabled sync_with_stdio ad the beginning of AppMain.
 *
 * Revision 1.17  2000/10/30 22:26:29  vakatov
 * Get rid of "s_IsAvailableExtra()" -- not used anymore
 *
 * Revision 1.16  2000/10/20 22:33:37  vakatov
 * CArgAllow_Integers, CArgAllow_Doubles:  swap min/max, if necessary
 *
 * Revision 1.15  2000/10/20 22:23:28  vakatov
 * CArgAllow_Strings customization;  MSVC++ fixes;  better diagnostic messages
 *
 * Revision 1.14  2000/10/20 20:25:55  vakatov
 * Redesigned/reimplemented the user-defined arg.value constraints
 * mechanism (CArgAllow-related classes and methods). +Generic clean-up.
 *
 * Revision 1.13  2000/10/13 16:26:30  vasilche
 * Added heuristic for detection of CObject allocation in heap.
 *
 * Revision 1.12  2000/10/11 21:03:49  vakatov
 * Cleanup to avoid 64-bit to 32-bit values truncation, etc.
 * (reported by Forte6 Patch 109490-01)
 *
 * Revision 1.11  2000/10/06 21:56:45  butanaev
 * Added Allow() function. Added classes CArgAllowValue, CArgAllowIntInterval.
 *
 * Revision 1.10  2000/09/29 17:10:54  butanaev
 * Got rid of IsDefaultValue(), added IsProvided().
 *
 * Revision 1.9  2000/09/28 21:00:14  butanaev
 * fPreOpen with opposite meaning took over fDelayOpen.
 * IsDefaultValue() added which returns true if no
 * value for an optional argument was provided in cmd. line.
 *
 *
 * Revision 1.7  2000/09/28 19:38:00  butanaev
 * stdin and stdout now referenced as '-' Example: ./test_ncbiargs -if - -of -
 *
 * Revision 1.6  2000/09/22 21:25:59  butanaev
 * Fixed bug in handling default arg values.
 *
 * Revision 1.5  2000/09/19 21:19:58  butanaev
 * Added possibility to change file open mode on the fly
 *
 * Revision 1.4  2000/09/18 19:39:02  vasilche
 * Added CreateArgs() from CNcbiArguments.
 *
 * Revision 1.3  2000/09/12 15:00:30  butanaev
 * Fixed bug with stdin, stdout caused compilation errors on IRIX.
 *
 * Revision 1.2  2000/09/06 18:56:04  butanaev
 * Added stdin, stdout support. Fixed bug in PrintOut.
 *
 * Revision 1.1  2000/08/31 23:54:49  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//  CArgException::

CArgException::CArgException(const string& what)
    THROWS_NONE
: runtime_error(what)
{
    return;
}


CArgException::CArgException(const string& what, const string& arg_value)
    THROWS_NONE
: runtime_error(what + ":  " + arg_value)
{
    return;
}


// ARG_THROW("What", "Value")
#define ARG_THROW(what, value)  THROW_TRACE(CArgException, (what, value))



///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//  Internal classes:
//    CArg_***::


///////////////////////////////////////////////////////
//  CArgValue::

CArgValue::CArgValue(const string& value, bool is_default)
{
    m_String         = value;
    m_IsDefaultValue = is_default;
}


CArgValue::~CArgValue(void)
{
    return;
}


long CArgValue::AsInteger(void) const
{
    ARG_THROW("Attempt to cast to a wrong (Integer) type", AsString());
}


double CArgValue::AsDouble(void) const
{
    ARG_THROW("Attempt to cast to a wrong (Double) type", AsString());
}


bool CArgValue::AsBoolean(void) const
{
    ARG_THROW("Attempt to cast to a wrong (Boolean) type", AsString());
}


CNcbiIstream& CArgValue::AsInputFile(EFlags /*changeModeTo*/) const
{
    ARG_THROW("Attempt to cast to a wrong (InputFile) type", AsString());
}


CNcbiOstream& CArgValue::AsOutputFile(EFlags /*changeModeTo*/) const
{
    ARG_THROW("Attempt to cast to a wrong (OutputFile) type", AsString());
}


///////////////////////////////////////////////////////
//  CArg_String::

class CArg_String : public CArgValue
{
public:
    CArg_String(const string& value, bool is_default);
};

inline CArg_String::CArg_String(const string& value, bool is_default)
    : CArgValue(value, is_default)
{
    return;
}


///////////////////////////////////////////////////////
//  CArg_Alnum::

class CArg_Alnum : public CArgValue
{
public:
    CArg_Alnum(const string& value, bool is_default);
};


inline CArg_Alnum::CArg_Alnum(const string& value, bool is_default)
    : CArgValue(value, is_default)
{
    for (string::const_iterator it = value.begin();  it != value.end(); ++it) {
        if ( !isalnum(*it) ) {
            ARG_THROW("CArg_Alnum::  not an alphanumeric string", value);
        }
    }
}


///////////////////////////////////////////////////////
//  CArg_Integer::

class CArg_Integer : public CArgValue
{
public:
    CArg_Integer(const string& value, bool is_default);
    virtual long AsInteger(void) const;
private:
    long m_Integer;
};


inline CArg_Integer::CArg_Integer(const string& value, bool is_default)
    : CArgValue(value, is_default)
{
    try {
        m_Integer = NStr::StringToLong(value);
    } catch (exception& e) {
        _TRACE(e.what());
        ARG_THROW("CArg_Integer::  not an integer value", value);
    }
}


long CArg_Integer::AsInteger(void) const
{
    return m_Integer;
}


///////////////////////////////////////////////////////
//  CArg_Double::

class CArg_Double : public CArgValue
{
public:
    CArg_Double(const string& value, bool is_default);
    virtual double AsDouble(void) const;
private:
    double m_Double;
};


inline CArg_Double::CArg_Double(const string& value, bool is_default)
    : CArgValue(value, is_default)
{
    try {
        m_Double = NStr::StringToDouble(value);
    } catch (exception& e) {
        _TRACE(e.what());
        ARG_THROW("CArg_Double::  not a floating point value", value);
    }
}


double CArg_Double::AsDouble(void) const
{
    return m_Double;
}


///////////////////////////////////////////////////////
//  CArg_Boolean::

class CArg_Boolean : public CArgValue
{
public:
    CArg_Boolean(bool value, bool is_default);
    CArg_Boolean(const string& value, bool is_default);
    virtual bool AsBoolean(void) const;
private:
    bool m_Boolean;
};


inline CArg_Boolean::CArg_Boolean(bool value, bool is_default)
    : CArgValue( NStr::BoolToString(value), is_default )
{
    m_Boolean = value;
}


inline CArg_Boolean::CArg_Boolean(const string& value, bool is_default)
    : CArgValue(value, is_default)
{
    try {
        m_Boolean = NStr::StringToBool(value);
    } catch (exception& e) {
        _TRACE(e.what());
        ARG_THROW("CArg_Boolean::  not a boolean value", value);
    }
}


bool CArg_Boolean::AsBoolean(void) const
{
    return m_Boolean;
}


///////////////////////////////////////////////////////
//  CArg_InputFile::

class CArg_InputFile : public CArgValue
{
public:
    CArg_InputFile(const string&       value,
                   IOS_BASE::openmode  openmode,
                   bool                delay_open, bool is_default);
    ~CArg_InputFile();
    virtual CNcbiIstream& AsInputFile(EFlags changeModeTo = fUnchanged) const;

private:
    void Open(void) const;
    mutable IOS_BASE::openmode  m_OpenMode;
    mutable CNcbiIstream*       m_InputFile;
    mutable bool                m_DeleteFlag;
};


void CArg_InputFile::Open(void) const
{
    if ( m_InputFile )
        return;

    if (AsString() == "-") {
        m_InputFile  = &cin;
        m_DeleteFlag = false;
    } else {
        m_InputFile  = new CNcbiIfstream(AsString().c_str(),
                                         IOS_BASE::in | m_OpenMode);
        m_DeleteFlag = true;
    }

    if ( !*m_InputFile ) {
        ARG_THROW("CArg_InputFile::  cannot open for reading", AsString());
    }
}


CArg_InputFile::CArg_InputFile(const string&      value,
                               IOS_BASE::openmode openmode,
                               bool               delay_open,
                               bool               is_default)
: CArgValue(value, is_default),
  m_OpenMode(openmode),
  m_InputFile(0),
  m_DeleteFlag(true)
{
    if ( !delay_open )
        Open();
}


CArg_InputFile::~CArg_InputFile()
{
    if (m_InputFile  &&  m_DeleteFlag)
        delete m_InputFile;
}


CNcbiIstream& CArg_InputFile::AsInputFile(EFlags changeModeTo) const
{
    if (changeModeTo != fUnchanged  &&  m_InputFile) {
        ARG_THROW("Cannot change open mode in non-deffered open file argument",
                  AsString());
    }

    if (changeModeTo == fToText)
        m_OpenMode &= ~CArgDescriptions::fBinary;
    if (changeModeTo == fToBinary)
        m_OpenMode |= CArgDescriptions::fBinary;

    Open();
    return *m_InputFile;
}


///////////////////////////////////////////////////////
//  CArg_OutputFile::

class CArg_OutputFile : public CArgValue
{
public:
    CArg_OutputFile(const string&      value,
                    IOS_BASE::openmode openmode,
                    bool               delay_open,
                    bool               is_default);
    ~CArg_OutputFile();

    virtual CNcbiOstream& AsOutputFile(EFlags changeModeTo = fUnchanged) const;

private:
    void Open(void) const;
    mutable IOS_BASE::openmode  m_OpenMode;
    mutable CNcbiOstream*       m_OutputFile;
    mutable bool                m_DeleteFlag;
};


void CArg_OutputFile::Open(void) const
{
    if ( m_OutputFile )
        return;

    if (AsString() == "-") {
        m_OutputFile = &cout;
        m_DeleteFlag = false;
    } else {
        m_OutputFile = new CNcbiOfstream(AsString().c_str(),
                                         IOS_BASE::out | m_OpenMode);
        m_DeleteFlag = true;
    }

    if ( !*m_OutputFile ) {
        ARG_THROW("CArg_OutputFile::  cannot open for writing", AsString());
    }
}


CArg_OutputFile::CArg_OutputFile(const string&      value,
                                 IOS_BASE::openmode openmode,
                                 bool               delay_open,
                                 bool               is_default)
    : CArgValue(value, is_default),
      m_OpenMode(openmode),
      m_OutputFile(0),
      m_DeleteFlag(true)
{
    if ( !delay_open )
        Open();
}


CArg_OutputFile::~CArg_OutputFile()
{
    if (m_OutputFile  &&  m_DeleteFlag)
        delete m_OutputFile;
}


CNcbiOstream& CArg_OutputFile::AsOutputFile(EFlags changeModeTo) const
{
    if (changeModeTo != fUnchanged  &&  !m_OutputFile) {
        ARG_THROW("Cannot change open mode in non-deffered open file argument",
                  AsString());
    }

    if (changeModeTo == fToText)
        m_OpenMode &= ~CArgDescriptions::fBinary;
    if (changeModeTo == fToBinary)
        m_OpenMode |= CArgDescriptions::fBinary;

    Open();
    return *m_OutputFile;
}



///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//

CArgDesc::~CArgDesc(void)
{
    return;
}


///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//  Internal classes:
//    CArgDesc_Flag::
//    CArgDesc_Plain::
//    CArgDesc_Key::
//    CArgDesc_OptionalKey::


//
//  arg_flag
//

class CArgDesc_Flag : public CArgDesc
{
public:
    // 'ctors
    CArgDesc_Flag(const string& comment);
    virtual ~CArgDesc_Flag(void);

    virtual string GetUsageSynopsis   (const string& name,
                                       bool optional=false) const;
    virtual string GetUsageCommentAttr(bool optional=false) const;
    virtual string GetUsageCommentBody(void) const;
    virtual string GetUsageConstraint(void) const;
    virtual CArgValue* ProcessArgument(const string& value,
                                       bool          is_default = false) const;
    virtual void SetConstraint(CArgAllow* constraint);

    const string& GetComment(void) const { return m_Comment; }

private:
    string m_Comment;  // help (what this arg. is about)
};


inline CArgDesc_Flag::CArgDesc_Flag(const string& comment)
    : m_Comment(comment)
{
    return;
}


CArgDesc_Flag::~CArgDesc_Flag(void)
{
    return;
}


string CArgDesc_Flag::GetUsageSynopsis(const string& name, bool /*optional*/)
    const
{
    return "-" + name;
}


string CArgDesc_Flag::GetUsageCommentAttr(bool /*optional*/) const
{
    return "Flag";
}


string CArgDesc_Flag::GetUsageCommentBody(void) const
{
    return m_Comment;
}


string CArgDesc_Flag::GetUsageConstraint(void) const
{
    return NcbiEmptyString;
}


CArgValue* CArgDesc_Flag::ProcessArgument(const string& /*value*/,
                                          bool          is_default) const
{
    return new CArg_Boolean(true, is_default);
}


void CArgDesc_Flag::SetConstraint(CArgAllow* constraint)
{
    ARG_THROW("Attempt to add constraint to a flag argument",
              constraint ? constraint->GetUsage() : NcbiEmptyString);
}



//
//   arg_plain := <value>
//

class CArgDesc_Plain : public CArgDesc_Flag
{
public:
    // 'ctors
    CArgDesc_Plain(const string&            comment,
                   CArgDescriptions::EType  type,
                   CArgDescriptions::TFlags flags,
                   const string&            default_value = NcbiEmptyString);
    virtual ~CArgDesc_Plain(void);

    virtual string GetUsageSynopsis   (const string& name,
                                       bool optional=false) const;
    virtual string GetUsageCommentAttr(bool optional=false) const;
    virtual string GetUsageConstraint(void) const;
    virtual CArgValue* ProcessArgument(const string& value,
                                       bool          is_default = false) const;
    virtual void SetConstraint(CArgAllow* constraint);

    CArgDescriptions::EType  GetType   (void) const { return m_Type; }
    CArgDescriptions::TFlags GetFlags  (void) const { return m_Flags; }
    const string&            GetDefault(void) const { return m_DefaultValue; }
private:
    CArgDescriptions::EType  m_Type;
    CArgDescriptions::TFlags m_Flags;
    string                   m_DefaultValue;
    CRef<CArgAllow>          m_Constraint;
};


CArgDesc_Plain::CArgDesc_Plain
(const string&            comment,
 CArgDescriptions::EType  type,
 CArgDescriptions::TFlags flags,
 const string&            default_value)
    : CArgDesc_Flag(comment),
      m_Type(type),
      m_Flags(flags),
      m_DefaultValue(default_value)
{
    // verify if "flags" "type" are matching
    switch ( type ) {
    case CArgDescriptions::eOutputFile:
        return;
    case CArgDescriptions::eInputFile:
        if((flags &
            ~(CArgDescriptions::fPreOpen | CArgDescriptions::fBinary)) == 0)
            return;
    default:
        if (flags == 0)
            return;
    }

    ARG_THROW("Argument type/flags mismatch",
              "(type=" + CArgDescriptions::GetTypeName(type) +
              ", flags=" + NStr::UIntToString(flags) + ")");
}


CArgDesc_Plain::~CArgDesc_Plain(void)
{
    return;
}


string CArgDesc_Plain::GetUsageSynopsis(const string& name, bool optional)
    const
{
    string tmp = name;
    if( tmp.empty() )
        tmp = "...";

    if ( optional ) {
        return "[" + tmp + "]";
    } else {
        return "<" + tmp + ">";
    }
}


string CArgDesc_Plain::GetUsageCommentAttr(bool optional) const
{
    if ( optional ) {
        return CArgDescriptions::GetTypeName(GetType());
    } else {
        return CArgDescriptions::GetTypeName(GetType()) + ", optional";
    }
}


string CArgDesc_Plain::GetUsageConstraint(void) const
{
    if ( !m_Constraint )
        return NcbiEmptyString;

    return m_Constraint->GetUsage();
}



CArgValue* CArgDesc_Plain::ProcessArgument(const string& value,
                                           bool          is_default) const
{
    // Check against additional (user-defined) constraints, if any imposed
    if (m_Constraint.NotEmpty()  &&  !m_Constraint->Verify(value)) {
        ARG_THROW("Illegal value, must be " + m_Constraint->GetUsage(), value);
    }

    // Process according to the argument type
    switch ( GetType() ) {
    case CArgDescriptions::eString:
        return new CArg_String(value, is_default);
    case CArgDescriptions::eAlnum:
        return new CArg_Alnum(value, is_default);
    case CArgDescriptions::eBoolean:
        return new CArg_Boolean(value, is_default);
    case CArgDescriptions::eInteger:
        return new CArg_Integer(value, is_default);
    case CArgDescriptions::eDouble:
        return new CArg_Double(value, is_default);
    case CArgDescriptions::eInputFile: {
        bool delay_open = (GetFlags() & CArgDescriptions::fPreOpen) == 0;
        IOS_BASE::openmode openmode = 0;
        if (GetFlags() & CArgDescriptions::fBinary)
            openmode |= IOS_BASE::binary;
        return new CArg_InputFile(value, openmode, delay_open, is_default);
    }
    case CArgDescriptions::eOutputFile: {
        bool delay_open = (GetFlags() & CArgDescriptions::fPreOpen) == 0;
        IOS_BASE::openmode openmode = 0;
        if (GetFlags() & CArgDescriptions::fBinary)
            openmode |= IOS_BASE::binary;
        if (GetFlags() & CArgDescriptions::fAppend)
            openmode |= IOS_BASE::app;
        return new CArg_OutputFile(value, openmode, delay_open, is_default);
    }
    } /* switch GetType() */

    // Something is very wrong...
    _TROUBLE;
    ARG_THROW("Unknown argument type #", NStr::IntToString((int) GetType()));
}


void CArgDesc_Plain::SetConstraint(CArgAllow* constraint)
{
    m_Constraint = constraint;
}



//
//  arg_key := [-<key> <value>]
//

class CArgDesc_OptionalKey : public CArgDesc_Plain
{
public:
    // 'ctors
    CArgDesc_OptionalKey(const string&            synopsis,
                         const string&            comment,
                         CArgDescriptions::EType  type,
                         CArgDescriptions::TFlags flags,
                         const string&            default_value);
    virtual ~CArgDesc_OptionalKey(void);

    virtual string GetUsageSynopsis   (const string& name,
                                       bool optional=false) const;
    virtual string GetUsageCommentAttr(bool optional=false) const;

    const string& GetSynopsis(void) const { return m_Synopsis; }
private:
    string m_Synopsis;  // one-word synopsis
};


CArgDesc_OptionalKey::CArgDesc_OptionalKey
(const string&            synopsis,
 const string&            comment,
 CArgDescriptions::EType  type,
 CArgDescriptions::TFlags flags,
 const string&            default_value)
    : CArgDesc_Plain(comment, type, flags, default_value),
      m_Synopsis(synopsis)
{
    // verify synopsis
    for (string::const_iterator it = m_Synopsis.begin();
         it != m_Synopsis.end();  ++it) {
        if (*it != '_'  &&  !isalnum(*it)) {
            ARG_THROW("Argument synopsis must be alpha-num", m_Synopsis);
        }
    }
}


CArgDesc_OptionalKey::~CArgDesc_OptionalKey(void)
{
    return;
}


string CArgDesc_OptionalKey::GetUsageSynopsis(const string& name,
                                              bool /*optional*/) const
{
    return "[-" + name + " " + GetSynopsis() + "]";
}


string CArgDesc_OptionalKey::GetUsageCommentAttr(bool /*optional*/) const
{
    return CArgDesc_Plain::GetUsageCommentAttr();
}



//
//  arg_key := -<key> <value>
//

class CArgDesc_Key : public CArgDesc_OptionalKey
{
public:
    // 'ctors
    CArgDesc_Key(const string&            synopsis,
                 const string&            comment,
                 CArgDescriptions::EType  type,
                 CArgDescriptions::TFlags flags);
    virtual ~CArgDesc_Key(void);

    virtual string GetUsageSynopsis   (const string& name,
                                       bool optional=false) const;
    virtual string GetUsageCommentAttr(bool optional=false) const;
private:
    // prohibit GetDefault!
    const string& GetDefault(void) const { _TROUBLE;  return NcbiEmptyString; }
};


inline CArgDesc_Key::CArgDesc_Key
(const string&            synopsis,
 const string&            comment,
 CArgDescriptions::EType  type,
 CArgDescriptions::TFlags flags)
    : CArgDesc_OptionalKey(synopsis, comment, type, flags, NcbiEmptyString)
{
    return;
}


CArgDesc_Key::~CArgDesc_Key(void)
{
    return;
}


string CArgDesc_Key::GetUsageSynopsis(const string& name, bool /*optional*/)
    const
{
    return "-" + name + " " + GetSynopsis();
}


string CArgDesc_Key::GetUsageCommentAttr(bool /*optional*/) const
{
    return CArgDescriptions::GetTypeName(GetType()) + ", mandatory";
}



///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//  CArgs::


CArgs::CArgs(void)
{
    m_nExtra = 0;
}


CArgs::~CArgs(void)
{
    return;
}


static string s_ComposeNameExtra(unsigned idx)
{
    static const string s_PoundSign = "#";
    return s_PoundSign + NStr::UIntToString(idx);
}


void CArgs::Add(const string& name, CArgValue* arg)
{
    // special case:  add an extra arg (generate virtual name for it)
    if ( name.empty() ) {
        Add(s_ComposeNameExtra(++m_nExtra), arg);
        return;
    }

    // check-up
    if ( !CArgDescriptions::VerifyName(name) ) {
        ARG_THROW("CArgs::  invalid argument name", name);
    }
    if ( Exist(name) ) {
        ARG_THROW("CArgs::  argument with this name already exists", name);
    }

    // add
    m_Args[name] = arg;
}


bool CArgs::Exist(const string& name) const
{
    return (m_Args.find(name) != m_Args.end());
}


bool CArgs::IsProvided(const string& name) const
{
  return Exist(name) && ! (*this)[name].m_IsDefaultValue;
}


const CArgValue& CArgs::operator [](const string& name) const
{
    TArgs::const_iterator arg = m_Args.find(name);
    if (arg == m_Args.end()) {
        ARG_THROW("CArgs::  undefined argument requested", name);
    }
    return *arg->second;
}


const CArgValue& CArgs::operator [](unsigned idx) const
{
    static const string s_PoundSign = "#";
    if (idx >= m_nExtra) {
        ARG_THROW("CArgs::  \"extra\" position argument index overrun",
                  NStr::UIntToString(idx));
    }
    return (*this)[ s_ComposeNameExtra(idx) ];
}


void CArgs::Print(string& str) const
{
    for (TArgsCI arg = m_Args.begin();  arg != m_Args.end();  ++arg) {
        if (arg->first[0] != '#') {
            str += '-';
        }
        str += arg->first;
        str += ' ';
        str += arg->second->AsString();
        str += '\n';
    }
}



///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//  CArgDescriptions::
//

CArgDescriptions::CArgDescriptions(void)
    : m_Args(),
      m_PlainArgs(),
      m_Constraint(eEqual),
      m_ConstrArgs(0)
{
    SetUsageContext("PROGRAM", NcbiEmptyString);
}


CArgDescriptions::~CArgDescriptions(void)
{
    return;
}


const string& CArgDescriptions::GetTypeName(EType type)
{
    static const string s_TypeName[N_ARG_TYPE] = {
        "String",
        "AlNum",
        "Boolean",
        "Integer",
        "Double",
        "InpFile",
        "OutFile"
    };

    return s_TypeName[(int) type];
}


void CArgDescriptions::AddKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_Key> arg
        (new CArgDesc_Key(synopsis, comment, type, flags));

    x_AddDesc(name, *arg);
    arg.release();
}


void CArgDescriptions::AddOptionalKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 TFlags        flags,
 const string& default_value)
{
    auto_ptr<CArgDesc_OptionalKey> arg
        (new CArgDesc_OptionalKey
         (synopsis, comment, type, flags, default_value));

    x_AddDesc(name, *arg);
    arg.release();
}


void CArgDescriptions::AddFlag
(const string& name,
 const string& comment)
{
    auto_ptr<CArgDesc_Flag> arg
        (new CArgDesc_Flag(comment));

    x_AddDesc(name, *arg);
    arg.release();
}


void CArgDescriptions::AddPlain
(const string& name,
 const string& comment,
 EType         type,
 TFlags        flags,
 const string& default_value)
{
    auto_ptr<CArgDesc_Plain> arg
        (new CArgDesc_Plain(comment, type, flags, default_value));

    x_AddDesc(name, *arg);
    _ASSERT(find(m_PlainArgs.begin(), m_PlainArgs.end(), name)
            == m_PlainArgs.end());
    m_PlainArgs.push_back(name);
    arg.release();
}


void CArgDescriptions::AddExtra
(const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_Plain> arg
        (new CArgDesc_Plain(comment, type, flags));

    x_AddDesc(NcbiEmptyString, *arg);
    arg.release();
}


void CArgDescriptions::SetConstraint(EConstraint policy, unsigned num_args)
{
    m_Constraint = policy;
    m_ConstrArgs = num_args;

    if (m_Constraint == eAny  &&  m_ConstrArgs) {
        ERR_POST(Warning << "CArgDescriptions::SetConstraint(eAny, non-zero)");
    }
}


void CArgDescriptions::SetConstraint(const string& name,
                                     CArgAllow*    constraint)
{
    CRef<CArgAllow> safe_delete(constraint);

    TArgsI it = m_Args.find(name);
    if (it == m_Args.end()) {
        ARG_THROW("Attempt to set constraint for undescribed argument", name);
    }
    it->second->SetConstraint(constraint);
}


bool CArgDescriptions::Exist(const string& name) const
{
    return (m_Args.find(name) != m_Args.end());
}


void CArgDescriptions::Delete(const string& name)
{
    {{ // ...from the list of all args
        TArgs::iterator it = m_Args.find(name);
        if (it == m_Args.end()) {
            ARG_THROW("Cannot delete non-existing argument description", name);
        }
        m_Args.erase(it);
    }}

    {{ // ...from the list of plain arg positions
        TPlainArgs::iterator it =
            find(m_PlainArgs.begin(), m_PlainArgs.end(), name);
        _ASSERT(it != m_PlainArgs.end());
        _ASSERT(find(it, m_PlainArgs.end(), name) == m_PlainArgs.end());
        m_PlainArgs.erase(it);
    }}
}


inline bool s_IsTag(const string& value)
{
    return (value.length() > 1)  &&  value[0] == '-'  &&  value[1] != '-';
}



struct CArgContext {
    CArgContext(void) { plain_idx = 0; }
    unsigned plain_idx;
};


void CArgDescriptions::x_PreCheck(void) const
{
    bool     has_extra     = (m_Args.find(NcbiEmptyString) != m_Args.end());
    unsigned n_policy_args = m_ConstrArgs ?
        m_ConstrArgs : (unsigned) m_PlainArgs.size();
    const char* err_msg = 0;
    switch ( m_Constraint ) {
    case eAny:
        if ( !has_extra ) {
            err_msg = "Must describe \"extra\" arguments";
        }
        break;
    case eLessOrEqual:
    case eEqual:
        if (m_PlainArgs.size() > n_policy_args) {
            err_msg = "Too many \"plain\" arguments described";
        } else if (has_extra  &&  m_PlainArgs.size() == n_policy_args) {
            err_msg = "The described \"extra\" arguments would never be used";
        } else if (!has_extra  &&  m_PlainArgs.size() < n_policy_args) {
            err_msg = "Too few \"plain\" (and no \"extra\") args described";
        }
        break;
    case eMoreOrEqual:
        if (!has_extra  &&  m_PlainArgs.size() < n_policy_args) {
            err_msg = "Too few \"plain\" (and no \"extra\") args described";
        }
        break;
    }
    if ( !err_msg )
        return;  // okay

    // trouble
    static const char* s_PolicyStr[] = {
        "eAny", "eLessOrEqual",  "eEqual", "eMoreOrEqual" };

    THROW1_TRACE(CArgException,
                 string("CArgDescriptions::CreateArgs() inconsistency:  "
                        "policy=") +  s_PolicyStr[(int) m_Constraint]
                 + ", policy_args="
                 + (m_ConstrArgs ?
                    NStr::UIntToString(m_ConstrArgs) : string("0<plain_args>"))
                 + ", plain_args=" + NStr::UIntToString(m_PlainArgs.size())
                 + ", extra_args=" + NStr::BoolToString(has_extra) + " -- "
                 + err_msg + "!");
}


// (return TRUE if "arg2" was used)
bool CArgDescriptions::x_CreateArg
(const string& arg1,
 bool have_arg2, const string& arg2,
 unsigned* n_plain, CArgs& args)
    const
{
    bool arg2_used = false;

    // Extract arg. tag
    string tag;
    if ( s_IsTag(arg1) ) {
        // <key> <value>, or <flag>
        if ( *n_plain ) {
            ARG_THROW("Flag or key where a plain argument should be", arg1);
        }
        tag = arg1.substr(1);
        if (tag.empty()  ||  !VerifyName(tag)) {
            ARG_THROW("Illegal argument tag", arg1);
        }
    } else if (*n_plain < m_PlainArgs.size()) {
        // pos.arg -- plain
        tag = m_PlainArgs[*n_plain];
        (*n_plain)++;
    } else {
        // pos.arg -- extra
        tag = NcbiEmptyString;
        (*n_plain)++;
    }

    // Check for too many plain/extra arguments
    if ((m_Constraint == eEqual  ||  m_Constraint == eLessOrEqual)  &&
        *n_plain > (m_ConstrArgs ? m_ConstrArgs : m_PlainArgs.size())) {
        ARG_THROW("Too many positional arguments (" +
                  NStr::UIntToString(*n_plain) + ")", arg1);
    }

    // Get arg. description
    TArgsCI it = m_Args.find(tag);
    if (it == m_Args.end()) {
        if ( tag.empty() ) {
            ARG_THROW("Unexpected plain (extra) argument, at position #",
                      NStr::UIntToString(*n_plain));
        } else {
            ARG_THROW("Unknown argument, with tag", tag);
        }
    }
    _ASSERT(it->second.get());
    const CArgDesc* desc = it->second.get();

    // Get argument value
    const string* value;
    if ( dynamic_cast<const CArgDesc_OptionalKey*> (desc) ) {
        // <key> <value> arg  -- advance from the arg.tag to the arg.value
        if ( !have_arg2 ) {
            ARG_THROW("Missing argument value, at tag", arg1);
        }
        value = &arg2;
        arg2_used = true;
    } else {
        value = &arg1;
    }

    // Process argument value and add it to the "args"
    args.Add(tag, desc->ProcessArgument(*value));
    return arg2_used;
}


void CArgDescriptions::x_PostCheck(CArgs& args, unsigned n_plain) const
{
    // Check if all mandatory position arguments are passed in
    unsigned n_policy_args = m_ConstrArgs ?
        m_ConstrArgs : (unsigned) m_PlainArgs.size();
    switch ( m_Constraint ) {
    case eAny:
        break;
    case eLessOrEqual:
        _ASSERT(n_plain <= n_policy_args);  // (due to checks in x_CreateArg)
        break;
    case eEqual:
        _ASSERT(n_plain <= n_policy_args);  // (due to checks in x_CreateArg)
        if (n_plain != n_policy_args) {
            ARG_THROW("Too few position arguments specified, must be exactly",
                      NStr::UIntToString(n_policy_args));
        }
        break;
    case eMoreOrEqual:
        if (n_plain < n_policy_args) {
            ARG_THROW("Too few position arguments specified, must be at least",
                      NStr::UIntToString(n_policy_args));
        }
        break;
    }

    // Check if all mandatory "<key> <value>" arguments are passed in
    for (TArgsCI it = m_Args.begin();  it != m_Args.end();  ++it) {
        CArgDesc_Key* arg = dynamic_cast<CArgDesc_Key*> (it->second.get());
        if (arg  &&  !args.Exist(it->first))
            ARG_THROW("Must specify mandatory argument",
                      arg->GetUsageSynopsis(it->first, false));

        // Default optional arguments (if not provided in a command line)
        CArgDesc_OptionalKey* optArg =
            dynamic_cast<CArgDesc_OptionalKey*>(it->second.get());
        if (optArg  &&  !args.Exist(it->first)) {
            args.Add(it->first,
                     optArg->ProcessArgument(optArg->GetDefault(), true));
        }
    }
}


void CArgDescriptions::SetUsageContext
(const string& usage_name,
 const string& usage_description,
 SIZE_TYPE    usage_width)
{
    m_UsageName        = usage_name;
    m_UsageDescription = usage_description;

    const SIZE_TYPE kMinUsageWidth = 30;
    if (usage_width < kMinUsageWidth) {
        usage_width = kMinUsageWidth;
        ERR_POST(Warning <<
                 "CArgDescriptions::SetUsageContext() -- usage_width=" <<
                 usage_width << " adjusted to " << kMinUsageWidth);
    }
    m_UsageWidth = usage_width;
}


bool CArgDescriptions::VerifyName(const string& name)
{
    for(string::const_iterator it = name.begin();  it != name.end();  ++it) {
        if(!isalnum(*it)  &&  *it != '#') {
            return false;
        }
    }
    return true;
}


void CArgDescriptions::x_AddDesc(const string& name, CArgDesc& arg)
{
    if ( !VerifyName(name) ) {
        ARG_THROW("Invalid argument name", name);
    }

    if (m_Args.find(name) != m_Args.end()) {
        ARG_THROW("Argument with this name already exists, cannot add", name);
    }

    m_Args[name] = &arg;
}



///////////////////////////////////////////////////////
//  CArgDescriptions::PrintUsage()


class CArgUsage {
public:
    const string*   m_Name;
    const CArgDesc* m_Desc;
    bool            m_Optional;
    CArgUsage(void) : m_Name(0), m_Desc(0), m_Optional(false) {}
    CArgUsage(const string& name, const CArgDesc& desc, bool optional = false)
        : m_Name(&name), m_Desc(&desc), m_Optional(optional) {}
};


static void s_PrintSynopsis(string& str, const CArgUsage& arg,
                            SIZE_TYPE* pos, SIZE_TYPE width)
{
    const static string s_NewLine("\n   ");

    string s = arg.m_Desc->GetUsageSynopsis(*arg.m_Name, arg.m_Optional);

    if (*pos + s.length() > width) {
        str += s_NewLine;
        *pos = s_NewLine.length();
    }
    str  += " ";
    str  += s;
    *pos += s.length();
}


static void s_PrintCommentBody(string& str, const string& s, SIZE_TYPE width)
{
    const static string s_NewLine("\n   ");
    width -= s_NewLine.length();
    SIZE_TYPE pos = 0;

    for (;;) {
        // find first non-space
        pos = s.find_first_not_of("\t\n\r ", pos);
        if (pos == NPOS)
            break;

        // start new line
        str += s_NewLine;

        // last line?
        if (s.length() <= pos + width) {
            str.append(s, pos, NPOS);
            break;
        }

        // break a line off the string
        SIZE_TYPE break_pos = s.find_last_of("\t\n\r ", pos, pos + width);
        if (break_pos == NPOS  ||  (break_pos - pos) + 5 < width) {
            str.append(s, pos, width - 1);
            str += '-';
            pos += width - 1;
        } else {
            str.append(s, pos, break_pos - pos);
            pos = break_pos + 1;
        }
    }
}


static void s_PrintComment(string& str, const CArgUsage& arg, SIZE_TYPE width)
{
    // print synopsis, plus type and other attributes
    str += "\n ";
    str += arg.m_Desc->GetUsageSynopsis(*arg.m_Name, arg.m_Optional);
    str += "    ";
    str += arg.m_Desc->GetUsageCommentAttr(arg.m_Optional);

    // print constraint info, if any
    string constr = arg.m_Desc->GetUsageConstraint();
    if ( !constr.empty() ) {
        str += constr.insert(0, "\n    * constraint:  ");
    }

    // print description
    s_PrintCommentBody(str, arg.m_Desc->GetUsageCommentBody(), width);
}


inline bool s_IsOptionalPlain
(unsigned                      n,
 CArgDescriptions::EConstraint constraint,
 unsigned                      n_constraint)
{
    return
        (constraint == CArgDescriptions::eAny  ||
         constraint == CArgDescriptions::eLessOrEqual  ||
         (constraint == CArgDescriptions::eMoreOrEqual  &&
          n_constraint != 0  &&  n >= n_constraint));
}


string& CArgDescriptions::PrintUsage(string& str) const
{
    typedef list<CArgUsage>       TList;
    typedef TList::iterator       TListI;
    typedef TList::const_iterator TListCI;

    TList args;

    // Keys and Flags
    {{
        args.push_front(CArgUsage());
        TListI it_keys  = args.begin();
        args.push_front(CArgUsage());
        TListI it_flags = args.begin();

        for (TArgsCI it = m_Args.begin();  it != m_Args.end();  ++it) {
            const CArgDesc* desc = it->second.get();

            if (dynamic_cast<const CArgDesc_OptionalKey*> (desc)) {
                args.insert(it_keys, CArgUsage(it->first, *desc));
            } else if (dynamic_cast<const CArgDesc_Flag*> (desc)  &&
                       !dynamic_cast<const CArgDesc_Plain*> (desc)) {
                args.insert(it_flags, CArgUsage(it->first, *desc));
            }
        }

        args.erase(it_flags);
        args.erase(it_keys);
    }}

    // Plain
    for (unsigned n = 0;  n < m_PlainArgs.size();  n++) {
        TArgsCI it = m_Args.find(m_PlainArgs[n]);
        _ASSERT(it != m_Args.end());

        args.push_back(CArgUsage
                       (it->first, *it->second,
                        s_IsOptionalPlain(n, m_Constraint, m_ConstrArgs)));
    }

    // Extra
    {{
        TArgsCI it = m_Args.find(NcbiEmptyString);
        if (it != m_Args.end()) {
            args.push_back(CArgUsage
                           (it->first, *it->second,
                            s_IsOptionalPlain((unsigned) m_PlainArgs.size(),
                                              m_Constraint, m_ConstrArgs)));
        }
    }}


    // Do Printout
    TListCI it;

    // SYNOPSYS
    str.erase();
    str += "SYNOPSYS\n   ";
    str += m_UsageName;
    SIZE_TYPE pos = 3 + m_UsageName.length();

    for (it = args.begin();  it != args.end();  ++it) {
        s_PrintSynopsis(str, *it, &pos, m_UsageWidth);
    }

    // DESCRIPTION
    str += "\n\nDESCRIPTION   ";
    if ( m_UsageDescription.empty() ) {
        str += " -- none";
    } else {
        s_PrintCommentBody(str, m_UsageDescription, m_UsageWidth);
    }

    // OPERANDS
    str += "\n\nOPERANDS   ";

    for (it = args.begin();  it != args.end();  ++it) {
        s_PrintComment(str, *it, m_UsageWidth);
    }

    str += "\n";
    return str;
}



#ifdef NO_INCLASS_TMPL

///////////////////////////////////////////////////////
//  CArgDescriptions::CreateArgs()

CArgs* CArgDescriptions::CreateArgs(int argc, const char* argv[])
    const
{
    // Pre-processing consistency checks
    x_PreCheck();
    // Create new "CArgs" to fill up, and parse cmd.-line args into it
    auto_ptr<CArgs> args(new CArgs());
    unsigned n_plain = 0;
    for (int i = 1;  i < argc;  i++) {
        bool have_arg2 = (i + 1 < argc);
        if ( x_CreateArg(argv[i], have_arg2,
                         have_arg2 ? (string)argv[i+1] : NcbiEmptyString,
                         &n_plain, *args) )
            i++;
    }
    // Post-processing consistency checks
    x_PostCheck(*args, n_plain);
    return args.release();
}

CArgs* CArgDescriptions::CreateArgs(SIZE_TYPE argc, const CNcbiArguments& argv)
    const
{
    // Pre-processing consistency checks
    x_PreCheck();
    // Create new "CArgs" to fill up, and parse cmd.-line args into it
    auto_ptr<CArgs> args(new CArgs());
    unsigned n_plain = 0;
    for (SIZE_TYPE i = 1;  i < argc;  i++) {
        bool have_arg2 = (i + 1 < argc);
        if ( x_CreateArg(argv[i], have_arg2,
                         have_arg2 ? (string)argv[i+1] : NcbiEmptyString,
                         &n_plain, *args) )
            i++;
    }
    // Post-processing consistency checks
    x_PostCheck(*args, n_plain);
    return args.release();
}

#endif /* NO_INCLASS_TMPL */


CArgs* CArgDescriptions::CreateArgs(const CNcbiArguments& args) const
{
    return CreateArgs(args.Size(), args);
}



///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// CArgAllow::
//   CArgAllow_Strings::
//   CArgAllow_Integers::
//   CArgAllow_Doubles::
//


///////////////////////////////////////////////////////
//  CArgAllow::
//

CArgAllow::~CArgAllow(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgAllow_Strings::
//

CArgAllow_Strings::CArgAllow_Strings(void)
    : CArgAllow()
{
    return;
}


CArgAllow_Strings* CArgAllow_Strings::Allow(const string& value)
{
    m_Strings.insert(value);
    return this;
}


bool CArgAllow_Strings::Verify(const string &value) const
{
    return (m_Strings.find(value) != m_Strings.end());
}


string CArgAllow_Strings::GetUsage(void) const
{
    if ( m_Strings.empty() ) {
        return "ERROR:  Constraint with no values allowed(?!)";
    }

    string str = "{";
    set<string>::const_iterator it = m_Strings.begin();
    for (;;) {
        str += "`";
        str += *it;

        ++it;
        if (it == m_Strings.end()) {
            str += "'";
            break;
        }

        str += "', ";
    }

    str += "}";
    return str;
}



///////////////////////////////////////////////////////
//  CArgAllow_Integers::
//

CArgAllow_Integers::CArgAllow_Integers(long x_min, long x_max)
    : CArgAllow()
{
    if (x_min <= x_max) {
        m_Min = x_min;
        m_Max = x_max;
    } else {
        m_Min = x_max;
        m_Max = x_min;
    }
}


bool CArgAllow_Integers::Verify(const string& value) const
{
    long val = NStr::StringToLong(value);
    return (m_Min <= val  &&  val <= m_Max);
}


string CArgAllow_Integers::GetUsage(void) const
{
    return
        NStr::IntToString(m_Min) +
        " <= X <= " +
        NStr::IntToString(m_Max);
}



///////////////////////////////////////////////////////
//  CArgAllow_Doubles::
//

CArgAllow_Doubles::CArgAllow_Doubles(double x_min, double x_max)
    : CArgAllow()
{
    if (x_min <= x_max) {
        m_Min = x_min;
        m_Max = x_max;
    } else {
        m_Min = x_max;
        m_Max = x_min;
    }
}


bool CArgAllow_Doubles::Verify(const string& value) const
{
    double val = NStr::StringToDouble(value);
    return (m_Min <= val  &&  val <= m_Max);
}


string CArgAllow_Doubles::GetUsage(void) const
{
    return
        NStr::DoubleToString(m_Min) +
        " <= X <= " +
        NStr::DoubleToString(m_Max);
}


END_NCBI_SCOPE
