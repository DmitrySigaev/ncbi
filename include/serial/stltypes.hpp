#ifndef STLTYPES__HPP
#define STLTYPES__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  1999/09/01 17:38:03  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.17  1999/08/31 17:50:04  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.16  1999/07/20 18:22:57  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.15  1999/07/19 15:50:20  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/15 19:35:06  vasilche
* Implemented map<K, V>.
*
* Revision 1.13  1999/07/15 16:59:55  vasilche
* Fixed template use in typedef.
*
* Revision 1.12  1999/07/15 16:54:44  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.11  1999/07/13 20:18:08  vasilche
* Changed types naming.
*
* Revision 1.10  1999/07/01 17:55:22  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:38  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:46  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/16 20:35:25  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:20:08  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/11 19:15:50  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.4  1999/06/10 21:06:42  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/09 18:39:00  vasilche
* Modified templates to work on Sun.
*
* Revision 1.2  1999/06/04 20:51:39  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:31  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/memberid.hpp>
#include <serial/choiceptr.hpp>
#include <set>
#include <map>
#include <list>
#include <vector>

BEGIN_NCBI_SCOPE

class CStlOneArgTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlOneArgTemplate(const CTypeRef& dataType)
        : CParent("stl<X>"), m_DataType(dataType)
        { }

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType.Get();
        }

private:
    CTypeRef m_DataType;
};

class CStlTwoArgsTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlTwoArgsTemplate(const CTypeRef& keyType, const CTypeRef& valueType)
        : CParent("stl<X,Y>"), m_KeyType(keyType), m_ValueType(valueType)
        { }

    const CMemberId& GetKeyId(void) const
        {
            return m_KeyId;
        }
    void SetKeyId(const CMemberId& id)
        {
            m_KeyId = id;
        }
    const CMemberId& GetValueId(void) const
        {
            return m_ValueId;
        }
    void SetValueId(const CMemberId& id)
        {
            m_ValueId = id;
        }

    TTypeInfo GetKeyTypeInfo(void) const
        {
            return m_KeyType.Get();
        }
    TTypeInfo GetValueTypeInfo(void) const
        {
            return m_ValueType.Get();
        }

private:
    CMemberId m_KeyId;
    CMemberId m_ValueId;
    CTypeRef m_KeyType;
    CTypeRef m_ValueType;
};

template<typename Data>
class CStlClassInfoAutoPtr : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef auto_ptr<TDataType> TObjectType;

    CStlClassInfoAutoPtr(void)
        : CParent("auto_ptr<X>", GetTypeRef(static_cast<TDataType*>(0)).Get())
        { }
    
    TConstObjectPtr GetObjectPointer(TConstObjectPtr object) const
        {
            return static_cast<const TObjectType*>(object)->get();
        }
    void SetObjectPointer(TObjectPtr object, TObjectPtr data) const
        {
            static_cast<TObjectType*>(object)->reset(static_cast<TDataType*>(data));
        }

    virtual TConstObjectPtr GetDefault(void) const
        {
            static TObjectType def;
            return &def;
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoAutoPtr;
            return typeInfo;
        }
};

template<typename Data>
class CStlClassInfoChoiceAutoPtr : public CChoicePointerTypeInfo
{
    typedef CChoicePointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef auto_ptr<TDataType> TObjectType;

    CStlClassInfoChoiceAutoPtr(void)
        : CParent("auto_ptr<X>", GetTypeRef(static_cast<TDataType*>(0)).Get())
        { }
    
    TConstObjectPtr GetObjectPointer(TConstObjectPtr object) const
        {
            return static_cast<const TObjectType*>(object)->get();
        }
    void SetObjectPointer(TObjectPtr object, TObjectPtr data) const
        {
            static_cast<TObjectType*>(object)->reset(static_cast<TDataType*>(data));
        }

    virtual TConstObjectPtr GetDefault(void) const
        {
            static TObjectType def;
            return &def;
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoChoiceAutoPtr;
            return typeInfo;
        }
};

template<typename List>
class CStlOneArgTemplateImpl : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    typedef List TObjectType;
    typedef typename TObjectType::const_iterator TConstIterator;

    CStlOneArgTemplateImpl(const CTypeRef& dataType)
        : CParent(dataType)
        { }

    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& l1 = Get(object1);
            const TObjectType& l2 = Get(object2);
            if ( l1.size() != l2.size() )
                return false;
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            for ( TConstIterator i1 = l1.begin(), i2 = l2.begin();
                  i1 != l1.end(); ++i1, ++i2 ) {
                if ( !dataTypeInfo->Equals(&*i1, &*i2) )
                    return false;
            }
            return true;
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            const TObjectType& from = Get(src);
            Reserve(dst, from.size());
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            size_t index = 0;
            for ( TConstIterator i = from.begin(); i != from.end();
                  ++i, ++index ) {
                dataTypeInfo->Assign(AddEmpty(dst, index), &*i);
            }
        }

protected:
    virtual void CollectExternalObjects(COObjectList& objectList,
                                        TConstObjectPtr object) const
        {
            const TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                dataTypeInfo->CollectObjects(objectList, &*i);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CObjectOStream::Block block(out, l.size());
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                block.Next();
                out.WriteExternalObject(&*i, dataTypeInfo);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            if ( block.Fixed() )
                Reserve(object, block.GetSize());
            while ( block.Next() ) {
                in.ReadExternalObject(AddEmpty(object, block.GetIndex()),
                                      dataTypeInfo);
            }
        }

    virtual void Reserve(TObjectPtr object, size_t length) const = 0;
    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t index) const = 0;
};

template<typename Data>
class CStlClassInfoList : public CStlOneArgTemplateImpl< list<Data> >
{
    typedef CStlOneArgTemplateImpl<TObjectType> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoList(void)
        : CParent(GetTypeRef(static_cast<const TDataType*>(0)))
        { }
    CStlClassInfoList(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoList;
            return typeInfo;
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t ) const
        {
            Get(object).clear();
        }

    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t ) const
        {
            TObjectType& l = Get(object);
            l.push_back(TDataType());
            return &l.back();
        }
};

template<typename Data>
class CStlClassInfoVector : public CStlOneArgTemplateImpl< vector<Data> >
{
    typedef CStlOneArgTemplateImpl<TObjectType> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoVector(void)
        : CParent(GetTypeRef(static_cast<const TDataType*>(0)))
        { }
    CStlClassInfoVector(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoVector;
            return typeInfo;
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t length) const
        {
            Get(object).resize(length);
        }

    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t index) const
        {
            TObjectType& l = Get(object);
            if ( index >= l.size() )
                l.push_back(TDataType());
            return &(l[index]);
        }
};

class CStlClassInfoMapImpl : public CStlTwoArgsTemplate
{
    typedef CStlTwoArgsTemplate CParent;
public:

    CStlClassInfoMapImpl(const CTypeRef& keyType, const CTypeRef& valueType)
        : CParent(keyType, valueType)
        { }

protected:
    void CollectKeyValuePair(COObjectList& objectList,
                             TConstObjectPtr key, TConstObjectPtr value) const;
    void WriteKeyValuePair(CObjectOStream& out,
                           TConstObjectPtr key, TConstObjectPtr value) const;
    void ReadKeyValuePair(CObjectIStream& in,
                          TObjectPtr key, TObjectPtr value) const;
};

template<typename Data>
class CStlClassInfoSet : public CStlOneArgTemplateImpl< set<Data> >
{
    typedef CStlOneArgTemplateImpl<TObjectType> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoSet(void)
        : CParent(GetTypeRef(static_cast<const TDataType*>(0)))
        { }
    CStlClassInfoSet(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoSet;
            return typeInfo;
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t ) const
        {
            Get(object).clear();
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            while ( block.Next() ) {
                Data data;
                GetDataTypeInfo()->ReadData(in, &data);
                o.insert(data);
            }
        }
    virtual TObjectPtr AddEmpty(TObjectPtr , size_t ) const
        {
            return 0;
        }
};

template<typename Key, typename Value>
class CStlClassInfoMap : public CStlClassInfoMapImpl
{
    typedef CStlClassInfoMapImpl CParent;

public:
    typedef Key TKeyType;
    typedef Value TValueType;
    typedef map<TKeyType, TValueType> TObjectType;

    CStlClassInfoMap(void)
        : CParent(GetTypeRef(static_cast<const TKeyType*>(0)),
                  GetTypeRef(static_cast<const TValueType*>(0)))
        {
        }
    CStlClassInfoMap(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(keyType, dataType)
        {
        }

    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoMap;
            return typeInfo;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& o1 = Get(object1);
            const TObjectType& o2 = Get(object2);
            if ( o1.size() != o2.size() )
                return false;
            TTypeInfo keyTypeInfo = GetKeyTypeInfo();
            TTypeInfo valueTypeInfo = GetValueTypeInfo();
            for ( TObjectType::const_iterator i1 = o1.begin(), i2 = o2.begin();
                  i1 != o1.end(); ++i1, ++i2 ) {
                if ( !keyTypeInfo->Equals(&i1->first, &i2->first) ||
                     !valueTypeInfo->Equals(&i1->second, &i2->second) )
                    return false;
            }
            return true;
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            TObjectType& to = Get(dst);
            const TObjectType& from = Get(src);
            to.clear();
            for ( TObjectType::const_iterator i = from.begin();
                  i != from.end(); ++i ) {
                to.insert(*i);
            }
        }

protected:
    virtual void CollectExternalObjects(COObjectList& objectList,
                                        TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            for ( TObjectType::const_iterator i = o.begin();
                  i != o.end(); ++i ) {
                CollectKeyValuePair(objectList, &i->first, &i->second);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            CObjectOStream::Block block(out, o.size());
            for ( TObjectType::const_iterator i = o.begin();
                  i != o.end(); ++i ) {
                block.Next();
                WriteKeyValuePair(out, &i->first, &i->second);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            while ( block.Next() ) {
                TKeyType key;
                TValueType value;
                ReadKeyValuePair(in, &key, &value);
                o.insert(TObjectType::value_type(key, value));
            }
        }
};

template<typename Key, typename Value>
class CStlClassInfoMultiMap : public CStlClassInfoMapImpl
{
    typedef CStlClassInfoMapImpl CParent;

public:
    typedef Key TKeyType;
    typedef Value TValueType;
    typedef multimap<TKeyType, TValueType> TObjectType;

    CStlClassInfoMultiMap(void)
        : CParent(GetTypeRef(static_cast<const TKeyType*>(0)),
                  GetTypeRef(static_cast<const TValueType*>(0)))
        {
        }
    CStlClassInfoMultiMap(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(keyType, dataType)
        {
        }

    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoMap;
            return typeInfo;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& o1 = Get(object1);
            const TObjectType& o2 = Get(object2);
            if ( o1.size() != o2.size() )
                return false;
            TTypeInfo keyTypeInfo = GetKeyTypeInfo();
            TTypeInfo valueTypeInfo = GetValueTypeInfo();
            for ( TObjectType::const_iterator i1 = o1.begin(), i2 = o2.begin();
                  i1 != o1.end(); ++i1, ++i2 ) {
                if ( !keyTypeInfo->Equals(&i1->first, &i2->first) ||
                     !valueTypeInfo->Equals(&i1->second, &i2->second) )
                    return false;
            }
            return true;
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            TObjectType& to = Get(dst);
            const TObjectType& from = Get(src);
            to.clear();
            for ( TObjectType::const_iterator i = from.begin();
                  i != from.end(); ++i ) {
                to.insert(*i);
            }
        }

protected:
    virtual void CollectExternalObjects(COObjectList& objectList,
                                        TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            for ( TObjectType::const_iterator i = o.begin();
                  i != o.end(); ++i ) {
                CollectKeyValuePair(objectList, &i->first, &i->second);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            CObjectOStream::Block block(out, o.size());
            for ( TObjectType::const_iterator i = o.begin();
                  i != o.end(); ++i ) {
                block.Next();
                WriteKeyValuePair(out, &i->first, &i->second);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            while ( block.Next() ) {
                TKeyType key;
                TValueType value;
                ReadKeyValuePair(in, &key, &value);
                o.insert(TObjectType::value_type(key, value));
            }
        }
};

template<typename Char>
class CStlClassInfoCharVector : public CTypeInfo {
public:
    typedef vector<Char> TObjectType;
    CStlClassInfoCharVector(void)
        : CTypeInfo(typeid(TObjectType).name())
        {
        }

    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoCharVector;
            return typeInfo;
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }
    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            /*
              const TObjectType& o1 = Get(object1);
              const TObjectType& o2 = Get(object2);
              size_t length = o1.size();
              if ( length != o2.size() )
              return false;
              return memcmp(&o1.front(), &o2.front(), length) == 0;
            */
            return Get(object1) == Get(object2);
        }
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            Get(dst) = Get(src);
        }

private:
    static char* ToChar(Char* p)
        { return reinterpret_cast<char*>(p); }
    static const char* ToChar(const Char* p)
        { return reinterpret_cast<const char*>(p); }

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            size_t length = o.size();
            CObjectOStream::ByteBlock block(out, length);
            if ( length > 0 )
                block.Write(ToChar(&o.front()), length);
        }
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::ByteBlock block(in);
            if ( block.KnownLength() ) {
                size_t length = block.GetExpectedLength();
                o.resize(length);
                if ( block.Read(ToChar(&o.front()), length) != length )
                    THROW1_TRACE(runtime_error, "read fault");
            }
            else {
                // length is known -> copy via buffer
                Char buffer[1024];
                size_t count;
                o.clear();
                while ( (count = block.Read(ToChar(buffer),
                                            sizeof(buffer))) != 0 ) {
                    o.insert(o.end(), buffer, buffer + count);
                }
            }
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */
