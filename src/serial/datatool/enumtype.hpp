#ifndef ENUMTYPE_HPP
#define ENUMTYPE_HPP

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
*   Enumerated type definition
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "type.hpp"
#include <list>

class CEnumDataType : public CDataType {
    typedef CDataType CParent;
public:
    class Value {
    public:
        Value()
            {
            }
        Value(const string& i, int v)
            : id(i), value(v)
            {
            }
        string id;
        int value;
    };
    typedef list<Value> TValues;

    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;

    void AddValue(const string& name, long value);

    void PrintASN(CNcbiOstream& out, int indent) const;

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

private:
    struct SEnumCInfo {
        string enumName;
        string cType;
        string valuePrefix;
        
        SEnumCInfo(const string& name, const string& type, const string& prefix)
            : enumName(name), cType(type), valuePrefix(prefix)
            {
            }
    };
    
    string DefaultEnumName(void) const;
    SEnumCInfo GetEnumCInfo(void) const;
public:

    void GenerateCode(CClassCode& code,
                      CTypeStrings* tType) const;

    CTypeInfo* CreateTypeInfo(void);
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual void GenerateCode(CClassCode& code) const;

private:
    TValues m_Values;
};

class CIntEnumDataType : public CEnumDataType {
    typedef CEnumDataType CParent;
public:
    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;
};

#endif
