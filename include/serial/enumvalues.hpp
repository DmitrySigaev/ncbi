#ifndef ENUMVALUES__HPP
#define ENUMVALUES__HPP

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
*   Description of enumerated data type values (named integers)
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <util/lightstr.hpp>
#include <list>
#include <map>
#include <memory>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CEnumeratedTypeValues
{
public:
    typedef list< pair<string, TEnumValueType> > TValues;
    typedef map<CLightString, TEnumValueType> TNameToValue;
    typedef map<TEnumValueType, const string*> TValueToName;

    CEnumeratedTypeValues(const char* name, bool isInteger);
    CEnumeratedTypeValues(const string& name, bool isInteger);
    ~CEnumeratedTypeValues(void);

    const string& GetName(void) const
        {
            return m_Name;
        }
    /// Get ASN.1 module name
    const string& GetModuleName(void) const
        {
            return m_ModuleName;
        }
    /// Set ASN.1 module name
    void SetModuleName(const string& name);

    /// Check whether the type is defined as INTEGER in ASN.1 spec
    bool IsInteger(void) const
        {
            return m_Integer;
        }
    /// Get the list of name-value pairs
    const TValues& GetValues(void) const
        {
            return m_Values;
        }

    /// Add name-value pair
    void AddValue(const string& name, TEnumValueType value);
    /// Add name-value pair
    void AddValue(const char* name, TEnumValueType value);

    /// Find numeric value by the name of the enum
    ///
    /// @param name
    ///   Name of enum value
    /// @return
    ///   Numeric value, if found; otherwise, throws an exception
    TEnumValueType FindValue(const CLightString& name) const;
    
    /// Check whether enum with this name is defined
    ///
    /// @param name 
    ///   Name of enum value
    /// @return
    ///   TRUE, if it is defined
    bool IsValidName(const CLightString& name) const;

    /// Find name of the enum by its numeric value
    ///
    /// @param value
    ///   Numeric value
    /// @param allowBadValue
    ///   When TRUE, and the name is not found, return empty string;
    ///   otherwise, throw an exception
    /// @return
    ///   Name of the enum
    const string& FindName(TEnumValueType value, bool allowBadValue) const;

    /// Get name-to-value map
    const TNameToValue& NameToValue(void) const;
    /// Get value-to-name map
    const TValueToName& ValueToName(void) const;

private:
    string m_Name;
    string m_ModuleName;

    bool m_Integer;
    TValues m_Values;
    mutable auto_ptr<TNameToValue> m_NameToValue;
    mutable auto_ptr<TValueToName> m_ValueToName;
};


/* @} */


END_NCBI_SCOPE

#endif  /* ENUMVALUES__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2006/12/07 18:59:30  gouriano
* Reviewed doxygen groupping, added documentation
*
* Revision 1.13  2006/10/12 15:08:23  gouriano
* Some header files moved into impl
*
* Revision 1.12  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.11  2005/10/24 20:27:18  gouriano
* Added option to write named integers by value only
*
* Revision 1.10  2004/05/24 15:12:10  gouriano
* added method to acces list of values in CEnumeratedTypeValues
*
* Revision 1.9  2004/04/30 13:28:39  gouriano
* Remove obsolete function declarations
*
* Revision 1.8  2003/04/15 14:15:10  siyan
* Added doxygen support
*
* Revision 1.7  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.6  2001/01/05 20:10:34  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.5  2000/12/15 15:37:59  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.4  2000/11/07 17:25:11  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.3  2000/09/18 20:00:01  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.2  2000/06/01 19:06:55  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/
