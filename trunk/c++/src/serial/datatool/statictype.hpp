#ifndef STATICTYPE_HPP
#define STATICTYPE_HPP

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
*   Predefined types: INTEGER, BOOLEAN, VisibleString etc.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2000/01/10 19:46:46  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.5  1999/12/03 21:42:13  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.4  1999/12/01 17:36:27  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/18 17:13:07  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.2  1999/11/15 19:36:19  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "type.hpp"

class CStaticDataType : public CDataType {
    typedef CDataType CParent;
public:
    void PrintASN(CNcbiOstream& out, int indent) const;

    void GetFullCType(CTypeStrings& tType, CClassCode& code) const;
    virtual string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const = 0;
};

class CNullDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    void GetFullCType(CTypeStrings& tType, CClassCode& code) const;
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CBoolDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CRealDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CStringDataType(void);

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    void GetFullCType(CTypeStrings& tType, CClassCode& code) const;
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CStringStoreDataType : public CStringDataType {
    typedef CStringDataType CParent;
public:
    CStringStoreDataType(void);

    const CTypeInfo* GetTypeInfo(void);
    void GetFullCType(CTypeStrings& tType, CClassCode& code) const;
    virtual const char* GetASNKeyword(void) const;
};

class CBitStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual const char* GetASNKeyword(void) const;
};

class COctetStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    void GetFullCType(CTypeStrings& tType, CClassCode& code) const;
    const char* GetASNKeyword(void) const;
};

class CIntDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);
    string GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

#endif
