#ifndef UNITYPE_HPP
#define UNITYPE_HPP

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
*   TYpe definition of 'SET OF' and 'SEQUENCE OF'
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/02/01 21:46:25  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.3  1999/12/03 21:42:14  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.2  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/type.hpp>

class CUniSequenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CUniSequenceDataType(const AutoPtr<CDataType>& elementType);

    void PrintASN(CNcbiOstream& out, int indent) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    CDataType* GetElementType(void)
        {
            return m_ElementType.get();
        }
    const CDataType* GetElementType(void) const
        {
            return m_ElementType.get();
        }
    void SetElementType(const AutoPtr<CDataType>& type);

    CTypeInfo* CreateTypeInfo(void);
    
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    const char* GetASNKeyword(void) const;

private:
    AutoPtr<CDataType> m_ElementType;
};

class CUniSetDataType : public CUniSequenceDataType {
    typedef CUniSequenceDataType CParent;
public:
    CUniSetDataType(const AutoPtr<CDataType>& elementType);

    CTypeInfo* CreateTypeInfo(void);
    
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    const char* GetASNKeyword(void) const;
};

#endif
