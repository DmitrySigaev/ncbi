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
* Revision 1.19  2000/09/01 13:16:18  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.18  2000/08/15 20:04:05  vasilche
* Fixed typo.
*
* Revision 1.17  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.16  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.15  2000/04/06 16:10:59  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.14  2000/03/29 15:55:28  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.13  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.12  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.11  1999/12/28 18:55:51  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.10  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.9  1999/07/20 18:23:11  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.8  1999/07/01 17:55:32  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.7  1999/06/30 16:04:57  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.6  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.5  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.4  1999/06/15 16:19:49  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.3  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.2  1999/06/07 19:30:26  vasilche
* More bug fixes
*
* Revision 1.1  1999/06/04 20:51:46  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objlist.hpp>
#include <serial/typeinfo.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

COObjectList::COObjectList(void)
{
}

COObjectList::~COObjectList(void)
{
}

void COObjectList::Clear(void)
{
    m_Objects.clear();
    m_ObjectsByPtr.clear();
}

CWriteObjectInfo& COObjectList::RegisterObject(TTypeInfo typeInfo)
{
    TObjectIndex newIndex = m_Objects.size();
    _ASSERT(m_Objects.empty() || m_Objects.back().IsWritten());
    m_Objects.push_back(CConstObjectInfo(0, typeInfo,
                                         CConstObjectInfo::eNonCObject));
    CWriteObjectInfo& info = m_Objects.back();
    MarkObjectWritten(info);
    info.m_Index = newIndex;
    return info;
}

CWriteObjectInfo& COObjectList::RegisterObject(TConstObjectPtr object,
                                               TTypeInfo typeInfo)
{
    _TRACE("COObjectList::RegisterObject("<<NStr::PtrToString(object)<<", "<<
           typeInfo->GetName() << ") size: " << typeInfo->GetSize() <<
           ", end: " << NStr::PtrToString(typeInfo->EndOf(object)));
    typeInfo = typeInfo->GetRealTypeInfo(object);

    const CObject* cObject = typeInfo->GetCObjectPtr(object);

    if ( cObject ) {
        // special case with cObjects
        if ( cObject->ReferencedOnlyOnce() ) {
            // unique reference -> do not remember pointer
            // in debug mode check for absence of other references
#if _DEBUG
            pair<TObjectsByPtr::iterator, bool> ins =
                m_ObjectsByPtr.insert(TObjectsByPtr::value_type(object, CWriteObjectInfo::eObjectIndexNotWritten));
            if ( !ins.second ) {
                // not inserted -> already have the same object pointer
                // as reference counter is one -> error
                THROW1_TRACE(runtime_error,
                             "double write of CObject with counter == 1");
            }
#endif
            // new object
            _ASSERT(m_Objects.empty() || m_Objects.back().IsWritten());
            m_Objects.push_back(CConstObjectInfo(object, typeInfo,
                                                 CConstObjectInfo::eNonCObject));
            return m_Objects.back();
        }
        else if ( cObject->Referenced() ) {
            // multiple reference -> normal processing
        }
        else {
            // not refernced -> error
            THROW1_TRACE(runtime_error,
                         "registering non referenced CObject");
        }
    }

    TObjectIndex newIndex = m_Objects.size();
    pair<TObjectsByPtr::iterator, bool> ins =
        m_ObjectsByPtr.insert(TObjectsByPtr::value_type(object, newIndex));

    if ( !ins.second ) {
        // not inserted -> already have the same object pointer
        TObjectIndex index = ins.first->second;
        _ASSERT(index != CWriteObjectInfo::eObjectIndexNotWritten);
        CWriteObjectInfo& objectInfo = m_Objects[index];
        _ASSERT(objectInfo.GetTypeInfo() == typeInfo);
        _ASSERT(objectInfo.IsWritten());
        return objectInfo;
    }

    // new object
    _ASSERT(m_Objects.empty() || m_Objects.back().IsWritten());
    m_Objects.push_back(CWriteObjectInfo(object, typeInfo));

#if _DEBUG
    // check for overlapping with previous object
    TObjectsByPtr::iterator check = ins.first;
    if ( check != m_ObjectsByPtr.begin() ) {
        --check;
        if ( m_Objects[check->second].GetTypeInfo()->EndOf(check->first) > object )
            THROW1_TRACE(runtime_error, "overlapping objects");
    }

    // check for overlapping with next object
    check = ins.first;
    if ( ++check != m_ObjectsByPtr.end() ) {
        if ( typeInfo->EndOf(object) > check->first )
            THROW1_TRACE(runtime_error, "overlapping objects");
    }
#endif

    _ASSERT(&m_Objects[ins.first->second] == &m_Objects.back());
    return m_Objects.back();
}

void COObjectList::MarkObjectWritten(CWriteObjectInfo& info)
{
    _ASSERT(!info.IsWritten());
    _ASSERT(&info == &m_Objects.back());
    info.m_Index = m_Objects.size() - 1;
}

void COObjectList::CheckAllWritten(void) const
{
    if ( !m_Objects.empty() && !m_Objects.back().IsWritten() )
        THROW1_TRACE(runtime_error, "not all objects written");
}

void COObjectList::ForgetObjects(size_t from, size_t to)
{
    _ASSERT(0 <= from);
    _ASSERT(from <= to);
    _ASSERT(to <= GetWrittenObjectCount());
    _ASSERT(m_Objects.empty() || m_Objects.back().IsWritten());
    for ( size_t i = from; i < to; ++i ) {
        CWriteObjectInfo& info = m_Objects[i];
        _ASSERT(info.IsWritten());
        CConstObjectInfo& object = info.m_Object;
        TConstObjectPtr objectPtr = object.GetObjectPtr();
        if ( objectPtr ) {
            m_ObjectsByPtr.erase(objectPtr);
            object.ResetObjectPtr();
        }
    }
}

END_NCBI_SCOPE
