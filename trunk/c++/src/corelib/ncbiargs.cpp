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
 *      parsed values -- CArgs,             CArgValue
 *      exceptions    -- CArgException, ARG_THROW()
 *      constraints   -- CArgAllow;  CArgAllow_{Strings,Integers,Doubles}
 *
 */

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  Include the private header
//

#define NCBIARGS__CPP
#include "ncbiargs_p.hpp"



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  Constants
//

static const string s_AutoHelp("h");
static const string s_ExtraName("....");




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArgException::
//

CArgException::CArgException(const string& what)
    THROWS_NONE
: runtime_error(what)
{
    return;
}


CArgException::CArgException(const string& what, const string& attr)
    THROWS_NONE
: runtime_error(what + ":  `" + attr + "'")
{
    return;
}


CArgException::CArgException
(const string& name, const string& what, const string& attr)
    THROWS_NONE
: runtime_error("Argument \"" + (name.empty() ? s_ExtraName : name) + "\". " +
                what + ":  `" + attr + "'")
{
    return;
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArg_***::   classes representing various types of argument value
//
//    CArgValue
//
//       CArg_NoValue     : CArgValue
//
//       CArg_String      : CArgValue
//          CArg_Integer     : CArg_String
//          CArg_Double      : CArg_String
//          CArg_Boolean     : CArg_String
//          CArg_InputFile   : CArg_String
//          CArg_OutputFile  : CArg_String
//    


///////////////////////////////////////////////////////
//  CArgValue::

CArgValue::CArgValue(const string& name)
    : m_Name(name)
{
    if ( !CArgDescriptions::VerifyName(m_Name, true) ) {
        ARG_THROW2("CArgValue::  Invalid argument name", m_Name);
    }
}


CArgValue::~CArgValue(void)
{
    return;
}



///////////////////////////////////////////////////////
//  Overload the comparison operator -- to handle "CRef<CArgValue>" elements
//  in "CArgs::m_Args" stored as "set< CRef<CArgValue> >"
//

inline bool operator< (const CRef<CArgValue>& x, const CRef<CArgValue>& y)
{
    return x->GetName() < y->GetName();
}



///////////////////////////////////////////////////////
//  CArg_NoValue::

inline CArg_NoValue::CArg_NoValue(const string& name)
    : CArgValue(name)
{
    return;
}


bool CArg_NoValue::HasValue(void) const
{
    return false;
}


#define THROW_CArg_NoValue \
   ARG_THROW("Attempt to use unassigned optional argument", "NULL")

const string& CArg_NoValue::AsString    (void) const { THROW_CArg_NoValue; }
int           CArg_NoValue::AsInteger   (void) const { THROW_CArg_NoValue; }
double        CArg_NoValue::AsDouble    (void) const { THROW_CArg_NoValue; }
bool          CArg_NoValue::AsBoolean   (void) const { THROW_CArg_NoValue; }
CNcbiIstream& CArg_NoValue::AsInputFile (void) const { THROW_CArg_NoValue; }
CNcbiOstream& CArg_NoValue::AsOutputFile(void) const { THROW_CArg_NoValue; }
void          CArg_NoValue::CloseFile   (void) const { THROW_CArg_NoValue; }



///////////////////////////////////////////////////////
//  CArg_String::

inline CArg_String::CArg_String(const string& name, const string& value)
    : CArgValue(name),
      m_String(value)
{
    return;
}


bool CArg_String::HasValue(void) const
{
    return true;
}


const string& CArg_String::AsString(void) const
{
    return m_String;
}


int CArg_String::AsInteger(void) const
{ ARG_THROW("Attempt to cast to a wrong (Integer) type", AsString()); }
double CArg_String::AsDouble(void) const
{ ARG_THROW("Attempt to cast to a wrong (Double) type", AsString()); }
bool CArg_String::AsBoolean(void) const
{ ARG_THROW("Attempt to cast to a wrong (Boolean) type", AsString()); }
CNcbiIstream& CArg_String::AsInputFile(void) const
{ ARG_THROW("Attempt to cast to a wrong (InputFile) type", AsString()); }
CNcbiOstream& CArg_String::AsOutputFile(void) const
{ ARG_THROW("Attempt to cast to a wrong (OutputFile) type", AsString()); }
void CArg_String::CloseFile(void) const
{ ARG_THROW("Attempt to close an argument of non-file type", AsString()); }



///////////////////////////////////////////////////////
//  CArg_Integer::

inline CArg_Integer::CArg_Integer(const string& name, const string& value)
    : CArg_String(name, value)
{
    try {
        m_Integer = NStr::StringToInt(value);
    } catch (exception& _DEBUG_ARG(e)) {
        _TRACE(e.what());
        ARG_THROW("Integer value expected", value);
    }
}


int CArg_Integer::AsInteger(void) const
{
    return m_Integer;
}



///////////////////////////////////////////////////////
//  CArg_Double::

inline CArg_Double::CArg_Double(const string& name, const string& value)
    : CArg_String(name, value)
{
    try {
        m_Double = NStr::StringToDouble(value);
    } catch (exception& _DEBUG_ARG(e)) {
        _TRACE(e.what());
        ARG_THROW("Floating point value expected", value);
    }
}


double CArg_Double::AsDouble(void) const
{
    return m_Double;
}



///////////////////////////////////////////////////////
//  CArg_Boolean::

inline CArg_Boolean::CArg_Boolean(const string& name, bool value)
    : CArg_String(name, NStr::BoolToString(value))
{
    m_Boolean = value;
}


inline CArg_Boolean::CArg_Boolean(const string& name, const string& value)
    : CArg_String(name, value)
{
    try {
        m_Boolean = NStr::StringToBool(value);
    } catch (exception& _DEBUG_ARG(e)) {
        _TRACE(e.what());
        ARG_THROW("Boolean value expected", value);
    }
}


bool CArg_Boolean::AsBoolean(void) const
{
    return m_Boolean;
}



///////////////////////////////////////////////////////
//  CArg_InputFile::

void CArg_InputFile::x_Open(void) const
{
    if ( m_InputFile )
        return;

    if (AsString() == "-") {
        m_InputFile  = &cin;
        m_DeleteFlag = false;
    } else if ( !AsString().empty() ) {
        m_InputFile  = new CNcbiIfstream(AsString().c_str(),
                                         IOS_BASE::in | m_OpenMode);
        if (!m_InputFile  ||  !*m_InputFile) {
            delete m_InputFile;
            m_InputFile = 0;
        } else {
            m_DeleteFlag = true;
        }
    }

    if ( !m_InputFile ) {
        ARG_THROW("CArg_InputFile::  cannot open for reading", AsString());
    }
}


CArg_InputFile::CArg_InputFile(const string& name, const string& value,
                               IOS_BASE::openmode openmode,
                               bool               delay_open)
: CArg_String(name, value),
  m_OpenMode(openmode),
  m_InputFile(0),
  m_DeleteFlag(true)
{
    if ( !delay_open )
        x_Open();
}


CArg_InputFile::~CArg_InputFile(void)
{
    if (m_InputFile  &&  m_DeleteFlag)
        delete m_InputFile;
}


CNcbiIstream& CArg_InputFile::AsInputFile(void) const
{
    x_Open();
    return *m_InputFile;
}


void CArg_InputFile::CloseFile(void) const
{
    if ( !m_InputFile ) {
        ERR_POST(Warning <<
                 CArgException(GetName(),
                               "CArg_InputFile::CloseFile -- file not opened",
                               AsString()).what());
        return;
    }

    if ( m_DeleteFlag ) {
        delete m_InputFile;
        m_InputFile = 0;
    }
}



///////////////////////////////////////////////////////
//  CArg_OutputFile::

void CArg_OutputFile::x_Open(void) const
{
    if ( m_OutputFile )
        return;

    if (AsString() == "-") {
        m_OutputFile = &cout;
        m_DeleteFlag = false;
    } else if ( !AsString().empty() ) {
        m_OutputFile = new CNcbiOfstream(AsString().c_str(),
                                         IOS_BASE::out | m_OpenMode);
        if (!m_OutputFile  ||  !*m_OutputFile) {
            delete m_OutputFile;
            m_OutputFile = 0;
        } else {
            m_DeleteFlag = true;
        }
    }

    if ( !m_OutputFile ) {
        ARG_THROW("CArg_OutputFile::  cannot open for writing", AsString());
    }
}


CArg_OutputFile::CArg_OutputFile(const string& name, const string& value,
                                 IOS_BASE::openmode openmode,
                                 bool               delay_open)
    : CArg_String(name, value),
      m_OpenMode(openmode),
      m_OutputFile(0),
      m_DeleteFlag(true)
{
    if ( !delay_open )
        x_Open();
}


CArg_OutputFile::~CArg_OutputFile(void)
{
    if (m_OutputFile  &&  m_DeleteFlag)
        delete m_OutputFile;
}


CNcbiOstream& CArg_OutputFile::AsOutputFile(void) const
{
    x_Open();
    return *m_OutputFile;
}


void CArg_OutputFile::CloseFile(void) const
{
    if ( !m_OutputFile ) {
        ERR_POST(Warning <<
                 CArgException(GetName(),
                               "CArg_OutputFile::CloseFile -- file not opened",
                               AsString()).what());
        return;
    }

    if ( m_DeleteFlag ) {
        delete m_OutputFile;
        m_OutputFile = 0;
    }
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArgDesc***::   abstract base classes for argument descriptions
//
//    CArgDesc
//
//    CArgDescMandatory  : CArgDesc
//    CArgDescOptional   : virtual CArgDescMandatory
//    CArgDescDefault    : virtual CArgDescOptional
//
//    CArgDescSynopsis
//


///////////////////////////////////////////////////////
//  CArgDesc::

CArgDesc::CArgDesc(const string& name, const string& comment)
    : m_Name(name), m_Comment(comment)
{
    if ( !CArgDescriptions::VerifyName(m_Name) ) {
        ARG_THROW2("Invalid argument name", m_Name);
    }
}


CArgDesc::~CArgDesc(void)
{
    return;
}


void CArgDesc::VerifyDefault(void) const
{
    return;
}


void CArgDesc::SetConstraint(CArgAllow* constraint)
{
    ARG_THROW("Attempt to set constraint for a no-value argument",
              constraint ? constraint->GetUsage() : kEmptyStr);
}


const CArgAllow* CArgDesc::GetConstraint(void) const
{
    return 0;
}


string CArgDesc::GetUsageConstraint(void) const
{
    const CArgAllow* constraint = GetConstraint();
    return constraint ? constraint->GetUsage() : kEmptyStr;
}



///////////////////////////////////////////////////////
//  Overload the comparison operator -- to handle "AutoPtr<CArgDesc>" elements
//  in "CArgs::m_Args" stored as "set< AutoPtr<CArgDesc> >"
//

inline bool operator< (const AutoPtr<CArgDesc>& x, const AutoPtr<CArgDesc>& y)
{
    return x->GetName() < y->GetName();
}



///////////////////////////////////////////////////////
//  CArgDescMandatory::

CArgDescMandatory::CArgDescMandatory(const string&            name,
                                     const string&            comment,
                                     CArgDescriptions::EType  type,
                                     CArgDescriptions::TFlags flags)
    : CArgDesc(name, comment),
      m_Type(type), m_Flags(flags)
{
    // verify if "flags" "type" are matching
    switch ( type ) {
    case CArgDescriptions::eBoolean:
    case CArgDescriptions::eOutputFile:
        return;
    case CArgDescriptions::eInputFile:
        if((flags &
            ~(CArgDescriptions::fPreOpen | CArgDescriptions::fBinary)) == 0)
            return;
    case CArgDescriptions::k_EType_Size:
        _TROUBLE;
        ARG_THROW("Invalid argument type", "k_EType_Size");
    default:
        if (flags == 0)
            return;
    }

    ARG_THROW("Argument type/flags mismatch",
              "(type=" + CArgDescriptions::GetTypeName(type) +
              ", flags=" + NStr::UIntToString(flags) + ")");

}


CArgDescMandatory::~CArgDescMandatory(void)
{
    return;
}


string CArgDescMandatory::GetUsageCommentAttr(void) const
{
    // Print type name
    string str = CArgDescriptions::GetTypeName(GetType());

    // Print constraint info, if any
    string constr = GetUsageConstraint();
    if ( !constr.empty() ) {
        str += ", ";
        str += constr;
    }
    return str;
}


CArgValue* CArgDescMandatory::ProcessArgument(const string& value) const
{
    // Process according to the argument type
    CRef<CArgValue> arg_value;
    switch ( GetType() ) {
    case CArgDescriptions::eString:
        arg_value = new CArg_String(GetName(), value);
        break;
    case CArgDescriptions::eBoolean:
        arg_value = new CArg_Boolean(GetName(), value);
        break;
    case CArgDescriptions::eInteger:
        arg_value = new CArg_Integer(GetName(), value);
        break;
    case CArgDescriptions::eDouble:
        arg_value = new CArg_Double(GetName(), value);
        break;
    case CArgDescriptions::eInputFile: {
        bool delay_open = (GetFlags() & CArgDescriptions::fPreOpen) == 0;
        IOS_BASE::openmode openmode = (IOS_BASE::openmode) 0;
        if (GetFlags() & CArgDescriptions::fBinary)
            openmode |= IOS_BASE::binary;
        arg_value = new CArg_InputFile(GetName(), value, openmode, delay_open);
        break;
    }
    case CArgDescriptions::eOutputFile: {
        bool delay_open = (GetFlags() & CArgDescriptions::fPreOpen) == 0;
        IOS_BASE::openmode openmode = (IOS_BASE::openmode) 0;
        if (GetFlags() & CArgDescriptions::fBinary)
            openmode |= IOS_BASE::binary;
        if (GetFlags() & CArgDescriptions::fAppend)
            openmode |= IOS_BASE::app;
        arg_value = new CArg_OutputFile(GetName(), value, openmode,delay_open);
        break;
    }
    case CArgDescriptions::k_EType_Size: {
        _TROUBLE;
        ARG_THROW("Unknown argument type", NStr::IntToString((int) GetType()));
    }
    } /* switch GetType() */


    // Check against additional (user-defined) constraints, if any imposed
    if ( m_Constraint ) {
        bool err = false;
        try {
            if ( !m_Constraint->Verify(value) )
                err = true;
        } catch (...) {
            err = true;
        }
        if ( err ) {
            ARG_THROW("Illegal value, expected " + m_Constraint->GetUsage(),
                      value);
        }
    }

    return arg_value.Release();
}


CArgValue* CArgDescMandatory::ProcessDefault(void) const
{
    ARG_THROW("Required argument missing", GetUsageCommentAttr());
}


void CArgDescMandatory::SetConstraint(CArgAllow* constraint)
{
    m_Constraint = constraint;
}


const CArgAllow* CArgDescMandatory::GetConstraint(void) const
{
    return m_Constraint;
}




///////////////////////////////////////////////////////
//  CArgDescOptional::


CArgDescOptional::CArgDescOptional(const string& name, const string& comment,
                                   CArgDescriptions::EType  type,
                                   CArgDescriptions::TFlags flags)
    : CArgDescMandatory(name, comment, type, flags)
{
    return;
}


CArgDescOptional::~CArgDescOptional(void)
{
    return;
}


CArgValue* CArgDescOptional::ProcessDefault(void) const
{
    return new CArg_NoValue(GetName());
}




///////////////////////////////////////////////////////
//  CArgDescDefault::


CArgDescDefault::CArgDescDefault(const string& name, const string& comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            default_value)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDescOptional(name, comment, type, flags),
      m_DefaultValue(default_value)
{
    return;
}


CArgDescDefault::~CArgDescDefault(void)
{
    return;
}


CArgValue* CArgDescDefault::ProcessDefault(void) const
{
    return ProcessArgument(GetDefaultValue());
}


void CArgDescDefault::VerifyDefault(void) const
{
    if (GetType() == CArgDescriptions::eInputFile  ||
        GetType() == CArgDescriptions::eOutputFile) {
        return;
    }

    // Process, then immediately delete
    CRef<CArgValue> arg_value(ProcessArgument(GetDefaultValue()));
}



///////////////////////////////////////////////////////
//  CArgDescSynopsis::


CArgDescSynopsis::CArgDescSynopsis(const string& synopsis)
    : m_Synopsis(synopsis)
{
    for (string::const_iterator it = m_Synopsis.begin();
         it != m_Synopsis.end();  ++it) {
        if (*it != '_'  &&  !isalnum(*it)) {
            ARG_THROW2("Argument synopsis must be alphanumeric", m_Synopsis);
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArgDesc_***::   classes for argument descriptions
//    CArgDesc_Flag    : CArgDesc
//
//    CArgDesc_Key     : virtual CArgDescMandatory
//    CArgDesc_KeyOpt  : CArgDesc_Key, virtual CArgDescOptional
//    CArgDesc_KeyDef  : CArgDesc_Key, CArgDescDefault
//
//    CArgDesc_Pos     : virtual CArgDescMandatory
//    CArgDesc_PosOpt  : CArgDesc_Pos, virtual CArgDescOptional
//    CArgDesc_PosDef  : CArgDesc_Pos, CArgDescDefault
//



///////////////////////////////////////////////////////
//  CArgDesc_Flag::


CArgDesc_Flag::CArgDesc_Flag(const string& name, const string& comment,
                             bool set_value)
    : CArgDesc(name, comment),
      m_SetValue(set_value)
{
    return;
}


CArgDesc_Flag::~CArgDesc_Flag(void)
{
    return;
}


string CArgDesc_Flag::GetUsageSynopsis(bool /*name_only*/) const
{
    return "-" + GetName();
}


string CArgDesc_Flag::GetUsageCommentAttr(void) const
{
    return kEmptyStr;
}


CArgValue* CArgDesc_Flag::ProcessArgument(const string& /*value*/) const
{
    if ( m_SetValue ) {
        return new CArg_Boolean(GetName(), true);
    } else {
        return new CArg_NoValue(GetName());
    }
}


CArgValue* CArgDesc_Flag::ProcessDefault(void) const
{
    if ( m_SetValue ) {
        return new CArg_NoValue(GetName());
    } else {
        return new CArg_Boolean(GetName(), true);
    }
}



///////////////////////////////////////////////////////
//  CArgDesc_Pos::


CArgDesc_Pos::CArgDesc_Pos(const string&            name,
                           const string&            comment,
                           CArgDescriptions::EType  type,
                           CArgDescriptions::TFlags flags)
    : CArgDescMandatory(name, comment, type, flags)
{
    return;
}


CArgDesc_Pos::~CArgDesc_Pos(void)
{
    return;
}


string CArgDesc_Pos::GetUsageSynopsis(bool /*name_only*/) const
{
    return GetName().empty() ? s_ExtraName : GetName();
}



///////////////////////////////////////////////////////
//  CArgDesc_PosOpt::


CArgDesc_PosOpt::CArgDesc_PosOpt(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags)
    : CArgDescMandatory (name, comment, type, flags),
      CArgDescOptional  (name, comment, type, flags),
      CArgDesc_Pos      (name, comment, type, flags)
{
    return;
}


CArgDesc_PosOpt::~CArgDesc_PosOpt(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgDesc_PosDef::


CArgDesc_PosDef::CArgDesc_PosDef(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            default_value)
    : CArgDescMandatory (name, comment, type, flags),
      CArgDescOptional  (name, comment, type, flags),
      CArgDescDefault   (name, comment, type, flags, default_value),
      CArgDesc_PosOpt   (name, comment, type, flags)
{
    return;
}


CArgDesc_PosDef::~CArgDesc_PosDef(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgDesc_Key::


CArgDesc_Key::CArgDesc_Key(const string&            name,
                           const string&            comment,
                           CArgDescriptions::EType  type,
                           CArgDescriptions::TFlags flags,
                           const string&            synopsis)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDesc_Pos     (name, comment, type, flags),
      CArgDescSynopsis(synopsis)
{
    return;
}


CArgDesc_Key::~CArgDesc_Key(void)
{
    return;
}


inline string s_KeyUsageSynopsis(const string& name, const string& synopsis,
                                 bool name_only)
{
    if ( name_only ) {
        return '-' + name;
    } else {
        return '-' + name + ' ' + synopsis;
    }
}


string CArgDesc_Key::GetUsageSynopsis(bool name_only) const
{
    return s_KeyUsageSynopsis(GetName(), GetSynopsis(), name_only);
}



///////////////////////////////////////////////////////
//  CArgDesc_KeyOpt::


CArgDesc_KeyOpt::CArgDesc_KeyOpt(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            synopsis)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDescOptional (name, comment, type, flags),
      CArgDesc_PosOpt  (name, comment, type, flags),
      CArgDescSynopsis(synopsis)
{
    return;
}


CArgDesc_KeyOpt::~CArgDesc_KeyOpt(void)
{
    return;
}


string CArgDesc_KeyOpt::GetUsageSynopsis(bool name_only) const
{
    return s_KeyUsageSynopsis(GetName(), GetSynopsis(), name_only);
}



///////////////////////////////////////////////////////
//  CArgDesc_KeyDef::


CArgDesc_KeyDef::CArgDesc_KeyDef(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            synopsis,
                                 const string&            default_value)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDescOptional (name, comment, type, flags),
      CArgDesc_PosDef  (name, comment, type, flags, default_value),
      CArgDescSynopsis(synopsis)
{
    return;
}


CArgDesc_KeyDef::~CArgDesc_KeyDef(void)
{
    return;
}


string CArgDesc_KeyDef::GetUsageSynopsis(bool name_only) const
{
    return s_KeyUsageSynopsis(GetName(), GetSynopsis(), name_only);
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  Aux.functions to figure out various arg. features
//
//    s_IsPositional(arg)
//    s_IsOptional(arg)
//    s_IsFlag(arg)
//

inline bool s_IsKey(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDescSynopsis*> (&arg) != 0);
}


inline bool s_IsPositional(const CArgDesc& arg)
{
    return dynamic_cast<const CArgDesc_Pos*> (&arg) &&  !s_IsKey(arg);
}


inline bool s_IsOptional(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDescOptional*> (&arg) != 0);
}


inline bool s_IsFlag(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDesc_Flag*> (&arg) != 0);
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CArgs::
//


CArgs::CArgs(void)
{
    m_nExtra = 0;
}


CArgs::~CArgs(void)
{
    return;
}


static string s_ComposeNameExtra(size_t idx)
{
    return '#' + NStr::UIntToString(idx);
}


CArgs::TArgsCI CArgs::x_Find(const string& name) const
{
    return m_Args.find(CRef<CArgValue> (new CArg_NoValue(name)));
}


bool CArgs::Exist(const string& name) const
{
    return (x_Find(name) != m_Args.end());
}


const CArgValue& CArgs::operator[] (const string& name) const
{
    TArgsCI arg = x_Find(name);
    if (arg == m_Args.end()) {
        // Special diagnostics for "extra" args
        if (!name.empty()  &&  name[0] == '#') {
            size_t idx;
            try {
                idx = NStr::StringToUInt(name.c_str() + 1);
            } catch (...) {
                idx = kMax_UInt;
            }
            if (idx == kMax_UInt) {
                ARG_THROW2("CArgs:: argument of invalid name requested", name);
            }
            if (m_nExtra == 0) {
                ARG_THROW2("CArgs::  no \"extra\" (unnamed positional) args "
                           "provided, cannot get", s_ComposeNameExtra(idx));
            }
            if (idx == 0  ||  idx >= m_nExtra) {
                ARG_THROW2("CArgs::  \"extra\" (unnamed positional) arg is "
                           "out-of-range (#1..#" + NStr::UIntToString(m_nExtra)
                           + ")", s_ComposeNameExtra(idx));
            }
        }
        // Diagnostics for all other argument classes
        ARG_THROW2("CArgs::  undescribed argument requested", name);
    }

    // Found arg with name "name"
    return **arg;
}


const CArgValue& CArgs::operator[] (size_t idx) const
{
    return (*this)[s_ComposeNameExtra(idx)];
}


string& CArgs::Print(string& str) const
{
    for (TArgsCI arg = m_Args.begin();  arg != m_Args.end();  ++arg) {
        // Arg. name
        const string& arg_name = (*arg)->GetName();
        str += arg_name;

        // Arg. value, if any
        const CArgValue& arg_value = (*this)[arg_name];
        if ( arg_value ) {
            str += " = `";
            str += arg_value.AsString();
            str += "'\n";
        } else {
            str += ":  <not assigned>\n";
        }
    }
    return str;
}


void CArgs::Add(CArgValue* arg)
{
    // special case:  add an "extra" arg (generate virtual name for it)
    bool is_extra = false;
    if ( arg->GetName().empty() ) {
        arg->m_Name = s_ComposeNameExtra(m_nExtra + 1);
        is_extra = true;
    }

    // check-up
    _ASSERT(CArgDescriptions::VerifyName(arg->GetName(), true));
    if ( Exist(arg->GetName()) ) {
        ARG_THROW2("CArgs::Add()  argument with this name already defined",
                   arg->GetName());
    }

    // add
    m_Args.insert(arg);

    if ( is_extra ) {
        m_nExtra++;
    }
}




///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//  CArgDescriptions::
//


CArgDescriptions::CArgDescriptions(bool auto_help)
    : m_nExtra(0),
      m_nExtraOpt(0),
      m_AutoHelp(auto_help)
{
    SetUsageContext("NCBI_PROGRAM", kEmptyStr);
    if ( m_AutoHelp ) {
        AddFlag(s_AutoHelp,
                "Print this USAGE message;  ignore other arguments");
    }
}


CArgDescriptions::~CArgDescriptions(void)
{
    return;
}


const string& CArgDescriptions::GetTypeName(EType type)
{
    static const string s_TypeName[k_EType_Size] = {
        "String",
        "Boolean",
        "Integer",
        "Real",
        "File_In",
        "File_Out"
    };

    if (type == k_EType_Size) {
        _TROUBLE;  ARG_THROW2("Invalid argument type", "k_EType_Size");
    }
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
        (new CArgDesc_Key(name, comment, type, flags, synopsis));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddOptionalKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_KeyOpt> arg
        (new CArgDesc_KeyOpt(name, comment, type, flags, synopsis));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddDefaultKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 const string& default_value,
 TFlags        flags)
{
    auto_ptr<CArgDesc_KeyDef> arg
        (new CArgDesc_KeyDef(name, comment, type, flags, synopsis,
                             default_value));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddFlag
(const string& name,
 const string& comment,
 bool          set_value)
{
    auto_ptr<CArgDesc_Flag> arg
        (new CArgDesc_Flag(name, comment, set_value));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddPositional
(const string& name,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_Pos> arg
        (new CArgDesc_Pos(name, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddOptionalPositional
(const string& name,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_PosOpt> arg
        (new CArgDesc_PosOpt(name, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddDefaultPositional
(const string& name,
 const string& comment,
 EType         type,
 const string& default_value,
 TFlags        flags)
{
    auto_ptr<CArgDesc_PosDef> arg
        (new CArgDesc_PosDef(name, comment, type, flags, default_value));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddExtra
(unsigned      n_mandatory,
 unsigned      n_optional,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    if (!n_mandatory  &&  !n_optional) {
        ARG_THROW2("AddExtra(0,0, ...)", "Zero # of extra arguments (?!)");
    }
    if (n_mandatory > 4096) {
        ARG_THROW2("AddExtra(N, ...)", "Too many mandatory extra arguments");
    }

    m_nExtra    = n_mandatory;
    m_nExtraOpt = n_optional;

    auto_ptr<CArgDesc_Pos> arg
        (m_nExtra ?
         new CArgDesc_Pos   (kEmptyStr, comment, type, flags) :
         new CArgDesc_PosOpt(kEmptyStr, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::SetConstraint(const string& name, CArgAllow* constraint)
{
    CRef<CArgAllow> safe_delete(constraint);

    TArgsI it = x_Find(name);
    if (it == m_Args.end()) {
        ARG_THROW2("Attempt to set constraint for undescribed argument", name);
    }
    (*it)->SetConstraint(constraint);
}


bool CArgDescriptions::Exist(const string& name) const
{
    return (x_Find(name) != m_Args.end());
}


void CArgDescriptions::Delete(const string& name)
{
    {{ // ...from the list of all args
        TArgsI it = x_Find(name);
        if (it == m_Args.end()) {
            ARG_THROW2("Cannot delete non-existing arg. description", name);
        }
        m_Args.erase(it);

        // take special care of the extra args
        if ( name.empty() ) {
            m_nExtra = 0;
            m_nExtraOpt = 0;
            return;
        }
    }}

    {{ // ...from the list of key/flag args
        TKeyFlagArgs::iterator it =
            find(m_KeyFlagArgs.begin(), m_KeyFlagArgs.end(), name);
        _ASSERT(it != m_KeyFlagArgs.end());
        _ASSERT(find(it, m_KeyFlagArgs.end(), name) == m_KeyFlagArgs.end());
        m_KeyFlagArgs.erase(it);
    }}

    {{ // ...from the list of positional args' positions
        TPosArgs::iterator it =
            find(m_PosArgs.begin(), m_PosArgs.end(), name);
        _ASSERT(it != m_PosArgs.end());
        _ASSERT(find(it, m_PosArgs.end(), name) == m_PosArgs.end());
        m_PosArgs.erase(it);
    }}
}


// Fake class to hold only a name -- to find in "m_Args"
class CArgDesc_NameOnly : public CArgDesc
{
public:
    CArgDesc_NameOnly(const string& name) :
        CArgDesc(name, kEmptyStr) {}
private:
    virtual string GetUsageSynopsis(bool/*name_only*/) const{return kEmptyStr;}
    virtual string GetUsageCommentAttr(void) const {return kEmptyStr;}
    virtual CArgValue* ProcessArgument(const string&) const {return 0;}
    virtual CArgValue* ProcessDefault(void) const {return 0;}
};

CArgDescriptions::TArgsCI CArgDescriptions::x_Find(const string& name) const
{
    return m_Args.find(AutoPtr<CArgDesc> (new CArgDesc_NameOnly(name)));
}

CArgDescriptions::TArgsI CArgDescriptions::x_Find(const string& name)
{
    return m_Args.find(AutoPtr<CArgDesc> (new CArgDesc_NameOnly(name)));
}



void CArgDescriptions::x_PreCheck(void) const
{
    // Check for the consistency of positional args
    if ( m_nExtra ) {
        for (TPosArgs::const_iterator name = m_PosArgs.begin();
             name != m_PosArgs.end();  ++name) {
            TArgsCI arg_it = x_Find(*name);
            _ASSERT(arg_it != m_Args.end());
            CArgDesc& arg = **arg_it;

            if (dynamic_cast<const CArgDesc_PosOpt*> (&arg)) {
                ARG_THROW2("Cannot have both optional named and required "
                           "unnamed positional args simultaneously. Name of "
                           "the offending argument", arg.GetName());
            }
        }
    }

    // Check for the validity of default values
    string arg_msg;
    for (TArgsCI it = m_Args.begin();  it != m_Args.end();  ++it) {
        CArgDesc& arg = **it;

        if (dynamic_cast<CArgDescDefault*> (&arg) == 0) {
            continue;
        }

        try {
            arg.VerifyDefault();
            continue;
        } catch (exception& e) {
            arg_msg = e.what();
        }
        ARG_THROW1("(Invalid default value) " + arg_msg);
    }
}


void CArgDescriptions::x_CheckAutoHelp(const string& arg) const
{
    _ASSERT(m_AutoHelp);
    if (arg.compare('-' + s_AutoHelp) == 0) {
        throw CArgHelpException();
    }
}


// (return TRUE if "arg2" was used)
bool CArgDescriptions::x_CreateArg
(const string& arg1,
 bool have_arg2, const string& arg2,
 unsigned* n_plain, CArgs& args)
    const
{
    // Argument name
    string name;

    // Check if to start processing the args as positional
    if (*n_plain == kMax_UInt) {
        // Check for the "--" delimiter
        if (arg1.compare("--") == 0) {
            *n_plain = 0;  // pos.args started
            return false;
        }
        // Check if argument has not a key/flag syntax
        if ((arg1.length() > 1)  &&  arg1[0] == '-') {
            // Extract name of flag or key
            name = arg1.substr(1);
            if ( !VerifyName(name) ) {
                *n_plain = 0;  // pos.args started
            }
        } else {
            *n_plain = 0;  // pos.args started
        }
    }

    // Whether the value of "arg2" is used
    bool arg2_used = false;

    // Extract name of positional argument
    if (*n_plain != kMax_UInt) {
        if (*n_plain < m_PosArgs.size()) {
            name = m_PosArgs[*n_plain];  // named positional argument
        } else {
            name = kEmptyStr;  // unnamed (extra) positional argument
        }
        (*n_plain)++;

        // Check for too many positional arguments
        if (kMax_UInt - m_nExtraOpt > m_nExtra + m_PosArgs.size()  &&
            *n_plain > m_PosArgs.size() + m_nExtra + m_nExtraOpt) {
            ARG_THROW2("Too many positional arguments (" +
                       NStr::UIntToString(*n_plain) +
                       "), the offending value", arg1);
        }
    }

    // Get arg. description
    TArgsCI it = x_Find(name);
    if (it == m_Args.end()) {
        if ( name.empty() ) {
            ARG_THROW2("Unexpected extra argument, at position #",
                      NStr::UIntToString(*n_plain));
        } else {
            ARG_THROW2("Unknown argument, with name", name);
        }
    }
    _ASSERT(*it);

    const CArgDesc& arg = **it;

    // Get argument value
    const string* value;
    if ( s_IsKey(arg) ) {
        // <key> <value> arg  -- advance from the arg.name to the arg.value
        if ( !have_arg2 ) {
            ARG_THROW3(arg1, "Value is missing", kEmptyStr);
        }
        value = &arg2;
        arg2_used = true;
    } else {
        value = &arg1;
    }

    // Process the "raw" argument value into "CArgValue"
    CRef<CArgValue> arg_value(arg.ProcessArgument(*value));

    // Add the argument value to "args"
    args.Add(arg_value);

    // Success (also indicate whether one or two "raw" args have been used)
    return arg2_used;
}


void CArgDescriptions::x_PostCheck(CArgs& args, unsigned n_plain) const
{
    // Check if all mandatory unnamed positional arguments are provided
    if (m_PosArgs.size() < n_plain  &&  n_plain < m_PosArgs.size() + m_nExtra){
        ARG_THROW1("Too few (" + NStr::UIntToString(n_plain) +
                   "unnamed positional arguments. Must define at least " +
                   NStr::UIntToString(m_nExtra));
    }

    // Compose an ordered list of args
    list<const CArgDesc*> def_args;
    iterate (TKeyFlagArgs, it, m_KeyFlagArgs) {
        const CArgDesc& arg = **x_Find(*it);
        def_args.push_back(&arg);
    }
    iterate (TPosArgs, it, m_PosArgs) {
        const CArgDesc& arg = **x_Find(*it);
        def_args.push_back(&arg);
    }

    // Set default values (if available) for the arguments not defined
    // in the command line.
    iterate (list<const CArgDesc*>, it, def_args) {
        const CArgDesc& arg = **it;

        // Nothing to do if defined in the command-line
        if ( args.Exist(arg.GetName()) ) {
            continue;
        }

        // Use default argument value
        CRef<CArgValue> arg_value(arg.ProcessDefault());

        // Add the value to "args"
        args.Add(arg_value);
    }
}


void CArgDescriptions::SetUsageContext
(const string& usage_name,
 const string& usage_description,
 bool          usage_sort_args,
 SIZE_TYPE     usage_width)
{
    m_UsageName        = usage_name;
    m_UsageDescription = usage_description;
    m_UsageSortArgs    = usage_sort_args;

    const SIZE_TYPE kMinUsageWidth = 30;
    if (usage_width < kMinUsageWidth) {
        usage_width = kMinUsageWidth;
        ERR_POST(Warning <<
                 "CArgDescriptions::SetUsageContext() -- usage_width=" <<
                 usage_width << " adjusted to " << kMinUsageWidth);
    }
    m_UsageWidth = usage_width;
}


bool CArgDescriptions::VerifyName(const string& name, bool extended)
{
    if ( name.empty() )
        return true;

    string::const_iterator it = name.begin();
    if (extended  &&  *it == '#') {
        for (++it;  it != name.end();  ++it) {
            if ( !isdigit(*it) ) {
                return false;
            }
        }
    } else {
        for ( ;  it != name.end();  ++it) {
            if (!isalnum(*it)  &&  *it != '_')
                return false;
        }
    }

    return true;
}


void CArgDescriptions::x_AddDesc(CArgDesc& arg)
{
    const string& name = arg.GetName();

    if ( Exist(name) ) {
        ARG_THROW2("Argument with this name already exists, cannot add", name);
    }

    if (s_IsKey(arg)  ||  s_IsFlag(arg)) {
        _ASSERT(find(m_KeyFlagArgs.begin(), m_KeyFlagArgs.end(), name)
                == m_KeyFlagArgs.end());
        m_KeyFlagArgs.push_back(name);
    } else if ( !name.empty() ) {
        _ASSERT(find(m_PosArgs.begin(), m_PosArgs.end(), name)
                == m_PosArgs.end());
        if ( s_IsOptional(arg) ) {
            m_PosArgs.push_back(name);
        } else {
            TPosArgs::iterator it;
            for (it = m_PosArgs.begin();  it != m_PosArgs.end();  ++it) {
                if ( s_IsOptional(**x_Find(*it)) )
                    break;
            }
            m_PosArgs.insert(it, name);
        }
    }

    m_Args.insert(&arg);
}



///////////////////////////////////////////////////////
//  CArgDescriptions::PrintUsage()


static void s_PrintSynopsis(string& str, const CArgDesc& arg,
                            SIZE_TYPE* pos, SIZE_TYPE width)
{
    string s;
    if ( s_IsOptional(arg) ) {
        s += " [" + arg.GetUsageSynopsis() + ']';
    } else if ( s_IsPositional(arg) ) {
        s += " <" + arg.GetUsageSynopsis() + '>';
    } else {
        s += ' ' + arg.GetUsageSynopsis();
    }

    if (*pos + s.length() > width) {
        const static string s_NewLine("\n   ");
        str += s_NewLine;
        *pos = s_NewLine.length();
    }
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

        SIZE_TYPE endl_pos = pos + width;
        if (endl_pos > s.length()) {
            endl_pos = s.length();
        }

        // explicit '\n'?
        SIZE_TYPE break_pos;
        for (break_pos = pos;
             break_pos < endl_pos  &&  s[break_pos] != '\n';
             break_pos++) {
            continue;
        }
        if (break_pos < endl_pos) {
            _ASSERT(s[break_pos] == '\n');
            str.append(s, pos, break_pos - pos);
            pos = break_pos + 1;
            continue;
        }

        // last line?
        if (s.length() <= endl_pos) {
            str.append(s, pos, NPOS);
            break;
        }

        // break a line off the string
        for (break_pos = endl_pos - 1;
             break_pos != pos  &&  !isspace(s[break_pos]);
             break_pos--) {
            continue;
        }
        if (break_pos == pos  ||  break_pos + 4 < endl_pos) {
            str.append(s, pos, width - 1);
            str += '-';
            pos += width - 1;
        } else {
            str.append(s, pos, break_pos - pos);
            pos = break_pos + 1;
        }
    }
}


static void s_PrintComment(string& str, const CArgDesc& arg, SIZE_TYPE width)
{
    // Print synopsis
    str += "\n ";
    str += arg.GetUsageSynopsis(true/*name_only*/);

    // Print type (and value constraint, if any)
    string attr = arg.GetUsageCommentAttr();
    if ( !attr.empty() ) {
        str += " <";
        str += attr;
        str += '>';
    }

    // Print description
    s_PrintCommentBody(str, arg.GetComment(), width);

    // Print default value, if any
    const CArgDescDefault* dflt = dynamic_cast<const CArgDescDefault*> (&arg);
    if ( dflt ) {
        s_PrintCommentBody
            (str, "Default = `" + dflt->GetDefaultValue() + '\'', width);
    }
}


string& CArgDescriptions::PrintUsage(string& str) const
{
    typedef list<const CArgDesc*> TList;
    typedef TList::iterator       TListI;
    typedef TList::const_iterator TListCI;

    TList args;

    args.push_front(0);
    TListI it_pos = args.begin();

    // Keys and Flags
    if ( m_UsageSortArgs ) {
        // Alphabetically ordered,
        // mandatory keys to go first, then flags, then optional keys
        TListI& it_opt_keys = it_pos; 
        args.push_front(0);
        TListI it_flags = args.begin();
        args.push_front(0);
        TListI it_keys  = args.begin();

        for (TArgsCI it = m_Args.begin();  it != m_Args.end();  ++it) {
            const CArgDesc* arg = it->get();

            if (dynamic_cast<const CArgDesc_KeyOpt*> (arg)  ||
                dynamic_cast<const CArgDesc_KeyDef*> (arg)) {
                args.insert(it_opt_keys, arg);
            } else if (dynamic_cast<const CArgDesc_Key*> (arg)) {
                args.insert(it_keys, arg);
            } else if (dynamic_cast<const CArgDesc_Flag*> (arg)) {
                if (s_AutoHelp.compare(arg->GetName()) == 0)
                    args.push_front(arg);
                else
                    args.insert(it_flags, arg);
            }
        }
        args.erase(it_keys);
        args.erase(it_flags);
    } else {
        // Unsorted, just the order they were described by user
        for (TKeyFlagArgs::const_iterator name = m_KeyFlagArgs.begin();
             name != m_KeyFlagArgs.end();  ++name) {
            TArgsCI it = x_Find(*name);
            _ASSERT(it != m_Args.end());

            args.insert(it_pos, it->get());
        }
    }

    // Positional
    for (TPosArgs::const_iterator name = m_PosArgs.begin();
         name != m_PosArgs.end();  ++name) {
        TArgsCI it = x_Find(*name);
        _ASSERT(it != m_Args.end());
        const CArgDesc* arg = it->get();

        // Mandatory args to go first, then go optional ones
        if (dynamic_cast<const CArgDesc_PosOpt*> (arg)) {
            args.push_back(arg);
        } else if (dynamic_cast<const CArgDesc_Pos*> (arg)) {
            args.insert(it_pos, arg);
        }
    }
    args.erase(it_pos);

    // Extra
    {{
        TArgsCI it = x_Find(kEmptyStr);
        if (it != m_Args.end()) {
            args.push_back(it->get());
        }
    }}


    // Do Printout
    TListCI it;

    // SYNOPSYS
    str += "USAGE\n  ";
    str += m_UsageName;
    SIZE_TYPE pos = 3 + m_UsageName.length();

    for (it = args.begin();  it != args.end();  ++it) {
        s_PrintSynopsis(str, **it, &pos, m_UsageWidth);
    }

    // DESCRIPTION
    str += "\n\nDESCRIPTION   ";
    if ( m_UsageDescription.empty() ) {
        str += " -- none";
    } else {
        s_PrintCommentBody(str, m_UsageDescription, m_UsageWidth);
    }

    // REQUIRED & OPTIONAL ARGUMENTS
    string s_req;
    string s_opt;
    for (it = args.begin();  it != args.end();  ++it) {
        s_PrintComment((s_IsOptional(**it) || s_IsFlag(**it)) ? s_opt : s_req,
                       **it, m_UsageWidth);
    }
    if ( !s_req.empty() ) {
        str += "\n\nREQUIRED ARGUMENTS";
        str += s_req;
    }
    if ( !s_opt.empty() ) {
        str += "\n\nOPTIONAL ARGUMENTS";
        str += s_opt;
    }

    // # of extra arguments
    if (m_nExtra  ||  (m_nExtraOpt != 0  &&  m_nExtraOpt != kMax_UInt)) {
        string str_extra = "\nNOTE:  Specify ";
        if ( m_nExtra ) {
            str_extra += "at least ";
            str_extra += NStr::UIntToString(m_nExtra);
            if (m_nExtraOpt != kMax_UInt) {
                str_extra += ", and ";
            }
        }
        if (m_nExtraOpt != kMax_UInt) {
            str_extra += "no more than ";
            str_extra += NStr::UIntToString(m_nExtra + m_nExtraOpt);
        }
        str_extra += " arguments in \"....\"";
        s_PrintCommentBody(str, str_extra, m_UsageWidth);
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
    // Check for "-h" flag
    if ( m_AutoHelp ) {
        for (int i = 1;  i < argc;  i++) {
            x_CheckAutoHelp(argv[i]);
        }
    }
    // Create new "CArgs" to fill up, and parse cmd.-line args into it
    auto_ptr<CArgs> args(new CArgs());
    unsigned n_plain = kMax_UInt;
    for (int i = 1;  i < argc;  i++) {
        bool have_arg2 = (i + 1 < argc);
        if ( x_CreateArg(argv[i], have_arg2,
                         have_arg2 ? (string)argv[i+1] : kEmptyStr,
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
    // Check for "-h" flag
    if ( m_AutoHelp ) {
        for (SIZE_TYPE i = 1;  i < argc;  i++) {
            x_CheckAutoHelp(argv[i]);
        }
    }
    // Create new "CArgs" to fill up, and parse cmd.-line args into it
    auto_ptr<CArgs> args(new CArgs());
    unsigned n_plain = kMax_UInt;
    for (SIZE_TYPE i = 1;  i < argc;  i++) {
        bool have_arg2 = (i + 1 < argc);
        if ( x_CreateArg(argv[i], have_arg2,
                         have_arg2 ? (string)argv[i+1] : kEmptyStr,
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
//   CArgAllow_Symbols::
//   CArgAllow_String::
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
//  s_IsSymbol() -- check if the symbol belongs to one of standard character
//                  classes from <ctype.h>, or to user-defined symbol set
//

inline bool s_IsAllowedSymbol(unsigned char                   ch,
                              CArgAllow_Symbols::ESymbolClass symbol_class,
                              const string&                   symbol_set)
{
    switch ( symbol_class ) {
    case CArgAllow_Symbols::eAlnum:   return isalnum(ch) != 0;
    case CArgAllow_Symbols::eAlpha:   return isalpha(ch) != 0;
    case CArgAllow_Symbols::eCntrl:   return iscntrl(ch) != 0;
    case CArgAllow_Symbols::eDigit:   return isdigit(ch) != 0;
    case CArgAllow_Symbols::eGraph:   return isgraph(ch) != 0;
    case CArgAllow_Symbols::eLower:   return islower(ch) != 0;
    case CArgAllow_Symbols::ePrint:   return isprint(ch) != 0;
    case CArgAllow_Symbols::ePunct:   return ispunct(ch) != 0;
    case CArgAllow_Symbols::eSpace:   return isspace(ch) != 0;
    case CArgAllow_Symbols::eUpper:   return isupper(ch) != 0;
    case CArgAllow_Symbols::eXdigit:  return isxdigit(ch) != 0;
    case CArgAllow_Symbols::eUser:
        return symbol_set.find_first_of(ch) != NPOS;
    }
    _TROUBLE;  return false;
}


static string s_GetUsageSymbol(CArgAllow_Symbols::ESymbolClass symbol_class,
                               const string&                   symbol_set)
{
    switch ( symbol_class ) {
    case CArgAllow_Symbols::eAlnum:   return "alphanumeric";
    case CArgAllow_Symbols::eAlpha:   return "alphabetic";
    case CArgAllow_Symbols::eCntrl:   return "control symbol";
    case CArgAllow_Symbols::eDigit:   return "decimal";
    case CArgAllow_Symbols::eGraph:   return "graphical symbol";
    case CArgAllow_Symbols::eLower:   return "lower case";
    case CArgAllow_Symbols::ePrint:   return "printable";
    case CArgAllow_Symbols::ePunct:   return "punctuation";
    case CArgAllow_Symbols::eSpace:   return "space";
    case CArgAllow_Symbols::eUpper:   return "upper case";
    case CArgAllow_Symbols::eXdigit:  return "hexadecimal";
    case CArgAllow_Symbols::eUser:
        return "'" + NStr::PrintableString(symbol_set) + "'";
    }
    _TROUBLE;  return kEmptyStr;
}



///////////////////////////////////////////////////////
//  CArgAllow_Symbols::
//

CArgAllow_Symbols::CArgAllow_Symbols(ESymbolClass symbol_class)
    : CArgAllow(),
      m_SymbolClass(symbol_class)
{
    return;
}


CArgAllow_Symbols::CArgAllow_Symbols(const string& symbol_set)
    : CArgAllow(),
      m_SymbolClass(eUser), m_SymbolSet(symbol_set)
{
    return;
}


bool CArgAllow_Symbols::Verify(const string& value) const
{
    if (value.length() != 1)
        return false;

    return s_IsAllowedSymbol(value[0], m_SymbolClass, m_SymbolSet);
}


string CArgAllow_Symbols::GetUsage(void) const
{
    return "one symbol: " + s_GetUsageSymbol(m_SymbolClass, m_SymbolSet);
}


CArgAllow_Symbols::~CArgAllow_Symbols(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgAllow_String::
//

CArgAllow_String::CArgAllow_String(ESymbolClass symbol_class)
    : CArgAllow_Symbols(symbol_class)
{
    return;
}


CArgAllow_String::CArgAllow_String(const string& symbol_set)
    : CArgAllow_Symbols(symbol_set)
{
    return;
}


bool CArgAllow_String::Verify(const string& value) const
{
    for (string::const_iterator it = value.begin();  it != value.end(); ++it) {
        if ( !s_IsAllowedSymbol(*it, m_SymbolClass, m_SymbolSet) )
            return false;
    }
    return true;
}


string CArgAllow_String::GetUsage(void) const
{
    return "to contain only symbols: " +
        s_GetUsageSymbol(m_SymbolClass, m_SymbolSet);
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


bool CArgAllow_Strings::Verify(const string& value) const
{
    return (m_Strings.find(value) != m_Strings.end());
}


string CArgAllow_Strings::GetUsage(void) const
{
    if ( m_Strings.empty() ) {
        return "ERROR:  Constraint with no values allowed(?!)";
    }

    string str;
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
    return str;
}


CArgAllow_Strings::~CArgAllow_Strings(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgAllow_Integers::
//

CArgAllow_Integers::CArgAllow_Integers(int x_min, int x_max)
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
    int val = NStr::StringToInt(value);
    return (m_Min <= val  &&  val <= m_Max);
}


string CArgAllow_Integers::GetUsage(void) const
{
    return NStr::IntToString(m_Min) + ".." + NStr::IntToString(m_Max);
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
    return NStr::DoubleToString(m_Min) + ".." + NStr::DoubleToString(m_Max);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.34  2002/04/11 21:08:01  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.33  2001/07/24 23:33:14  vakatov
 * Use _DEBUG_ARG() to get rid of the compiler warnings in Release mode
 *
 * Revision 1.32  2001/03/16 16:40:18  vakatov
 * Moved <corelib/ncbi_limits.h> to the header
 *
 * Revision 1.31  2001/01/22 23:07:14  vakatov
 * CArgValue::AsInteger() to return "int" (rather than "long")
 *
 * Revision 1.30  2001/01/03 17:45:35  vakatov
 * + <ncbi_limits.h>
 *
 * Revision 1.29  2000/12/24 00:13:00  vakatov
 * Radically revamped NCBIARGS.
 * Introduced optional key and posit. args without default value.
 * Added new arg.value constraint classes.
 * Passed flags to be detected by HasValue() rather than AsBoolean.
 * Simplified constraints on the number of mandatory and optional extra args.
 * Improved USAGE info and diagnostic messages. Etc...
 *
 * Revision 1.26  2000/11/29 00:18:13  vakatov
 * s_ProcessArgument() -- avoid nested quotes in the exception message
 *
 * Revision 1.25  2000/11/29 00:07:28  vakatov
 * Flag and key args not to be sorted in alphabetical order by default; see
 * "usage_sort_args" in SetUsageContext().
 *
 * Revision 1.24  2000/11/24 23:28:33  vakatov
 * CArgValue::  added CloseFile()
 * CArgValue::  get rid of "change_mode" feature in AsInput/OutputFile()
 *
 * Revision 1.23  2000/11/22 22:04:31  vakatov
 * Added special flag "-h" and special exception CArgHelpException to
 * force USAGE printout in a standard manner
 *
 * Revision 1.22  2000/11/17 22:04:30  vakatov
 * CArgDescriptions::  Switch the order of optional args in methods
 * AddOptionalKey() and AddPlain(). Also, enforce the default value to
 * match arg. description (and constraints, if any) at all times.
 *
 * Revision 1.21  2000/11/13 20:31:07  vakatov
 * Wrote new test, fixed multiple bugs, ugly "features", and the USAGE.
 *
 * Revision 1.20  2000/11/09 21:02:58  vakatov
 * USAGE to show default value for optional arguments
 *
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
