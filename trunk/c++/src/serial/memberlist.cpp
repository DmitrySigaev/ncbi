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
* Revision 1.26  2002/01/24 23:30:01  vakatov
* Note for ourselves that the bug workaround "BW_010" is not needed
* anymore, and we should get rid of it in about half a year
*
* Revision 1.25  2001/10/15 19:48:26  vakatov
* Use two #if's instead of "#if ... && ..." as KAI cannot handle #if x == y
*
* Revision 1.24  2001/09/04 17:04:02  vakatov
* Extended workaround for the SUN Forte 6 Update 1 compiler internal bug
* to all higher versions of the compiler, as Update 2 also has this the
* same bug!
*
* Revision 1.23  2001/08/20 20:23:19  vakatov
* Workaround for the SUN Forte 6 Update 1 compiler internal bug.
*
* Revision 1.22  2000/10/03 17:22:43  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.21  2000/09/28 16:29:41  vasilche
* Reverted back changes in processing parent classes.
* It causes exceptions with map container.
*
* Revision 1.20  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.19  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.18  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.17  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.16  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.15  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.14  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.13  2000/06/01 19:07:03  vasilche
* Added parsing of XML data.
*
* Revision 1.12  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.11  2000/04/10 21:01:49  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.10  2000/04/10 18:01:57  vasilche
* Added Erase() for STL types in type iterators.
*
* Revision 1.9  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.8  2000/02/01 21:47:22  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.7  2000/01/10 19:46:39  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  1999/10/04 16:22:16  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.5  1999/07/22 19:40:55  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.4  1999/07/14 18:58:08  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.3  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.2  1999/07/01 17:55:29  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.1  1999/06/30 16:04:52  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberlist.hpp>
#include <serial/memberid.hpp>
#include <serial/member.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CItemsInfo::CItemsInfo(void)
    : m_ZeroTagIndex(kInvalidMember)
{
}

CItemsInfo::~CItemsInfo(void)
{
// NOTE:  This compiler bug was fixed by Jan 24 2002, test passed with:
//           CC: Sun WorkShop 6 update 2 C++ 5.3 Patch 111685-03 2001/10/19
//        We leave the workaround here for maybe half a year (for other guys).
#if defined(NCBI_COMPILER_WORKSHOP)
// We have to use two #if's here because KAI C++ cannot handle #if foo == bar
#  if (NCBI_COMPILER_VERSION == 530)
    // BW_010::  to workaround (already reported to SUN, CASE ID 62563729)
    //           internal bug of the SUN Forte 6 Update 1 and Update 2 compiler
    (void) atoi("5");
#  endif
#endif
}

void CItemsInfo::AddItem(CItemInfo* item)
{
    // clear cached maps (byname and bytag)
    m_ItemsByName.reset(0);
    m_ZeroTagIndex = kInvalidMember;
    m_ItemsByTag.reset(0);
    m_ItemsByOffset.reset(0);

    // update item's tag
    if ( !item->GetId().HaveExplicitTag() ) {
        TTag tag = CMemberId::eFirstTag;
        if ( !Empty() ) {
            TMemberIndex lastIndex = LastIndex();
            const CItemInfo* lastItem = GetItemInfo(lastIndex);
            if ( lastIndex != FirstIndex() ||
                 !lastItem->GetId().HaveParentTag() )
                tag = lastItem->GetId().GetTag() + 1;
        }
        item->GetId().SetTag(tag, false);
    }

    // add item
    m_Items.push_back(AutoPtr<CItemInfo>(item));
    item->m_Index = LastIndex();
}

const CItemsInfo::TItemsByName& CItemsInfo::GetItemsByName(void) const
{
    TItemsByName* items = m_ItemsByName.get();
    if ( !items ) {
        m_ItemsByName.reset(items = new TItemsByName);
        for ( CIterator i(*this); i.Valid(); ++i ) {
            const CItemInfo* itemInfo = GetItemInfo(i);
            const string& name = itemInfo->GetId().GetName();
            if ( !items->insert(TItemsByName::value_type(name, *i)).second ) {
                if ( !name.empty() )
                    THROW1_TRACE(runtime_error, "duplicated member name");
            }
        }
    }
    return *items;
}

const CItemsInfo::TItemsByOffset&
CItemsInfo::GetItemsByOffset(void) const
{
    TItemsByOffset* items = m_ItemsByOffset.get();
    if ( !items ) {
        // create map
        m_ItemsByOffset.reset(items = new TItemsByOffset);
        // fill map
        for ( CIterator i(*this); i.Valid(); ++i ) {
            const CItemInfo* itemInfo = GetItemInfo(i);
            size_t offset = itemInfo->GetOffset();
            if ( !items->insert(TItemsByOffset::value_type(offset, *i)).second ) {
                THROW1_TRACE(runtime_error, "conflict member offset");
            }
        }
/*
        // check overlaps
        size_t nextOffset = 0;
        for ( TItemsByOffset::const_iterator m = members->begin();
              m != members->end(); ++m ) {
            size_t offset = m->first;
            if ( offset < nextOffset ) {
                THROW1_TRACE(runtime_error,
                             "overlapping members");
            }
            nextOffset = offset + m_Members[m->second]->GetSize();
        }
*/
    }
    return *items;
}

void CItemsInfo::UpdateTagMap(void) const
{
    TMemberIndex zeroTagIndex = kInvalidMember;

    for ( CIterator i(*this); i.Valid(); ++i ) {
        const CItemInfo* itemInfo = GetItemInfo(i);
        TTag tag = itemInfo->GetId().GetTag();
        if ( zeroTagIndex == kInvalidMember ) {
            if ( *i == FirstIndex() )
                zeroTagIndex = *i - tag;
        }
        else {
            if ( zeroTagIndex != *i - tag )
                zeroTagIndex = kInvalidMember;
        }
    }

    m_ZeroTagIndex = zeroTagIndex;
    if ( zeroTagIndex == kInvalidMember ) {
        TItemsByTag* items;
        m_ItemsByTag.reset(items = new TItemsByTag);
        for ( CIterator i(*this); i.Valid(); ++i ) {
            const CItemInfo* itemInfo = GetItemInfo(i);
            TTag tag = itemInfo->GetId().GetTag();
            if ( !items->insert(TItemsByTag::value_type(tag, *i)).second ) {
                THROW1_TRACE(runtime_error, "duplicated member tag");
            }
        }
    }
}

TMemberIndex CItemsInfo::Find(const CLightString& name) const
{
    const TItemsByName& items = GetItemsByName();
    TItemsByName::const_iterator i = items.find(name);
    if ( i == items.end() )
        return kInvalidMember;
    return i->second;
}

TMemberIndex CItemsInfo::Find(const CLightString& name, TMemberIndex pos) const
{
    for ( CIterator i(*this, pos); i.Valid(); ++i ) {
        if ( name == GetItemInfo(i)->GetId().GetName() )
            return *i;
    }
    return kInvalidMember;
}

TMemberIndex CItemsInfo::Find(TTag tag) const
{
    if ( m_ZeroTagIndex != kInvalidMember ) {
        TMemberIndex index = tag + m_ZeroTagIndex;
        if ( index < FirstIndex() || index > LastIndex() )
            return kInvalidMember;
        return index;
    }

    TItemsByTag* items = m_ItemsByTag.get();
    if ( !items ) {
        UpdateTagMap();
        return Find(tag);
    }

    TItemsByTag::const_iterator mi = items->find(tag);
    if ( mi == items->end() )
        return kInvalidMember;
    return mi->second;
}

TMemberIndex CItemsInfo::Find(TTag tag, TMemberIndex pos) const
{
    for ( CIterator i(*this, pos); i.Valid(); ++i ) {
        if ( GetItemInfo(i)->GetId().GetTag() == tag )
            return *i;
    }
    return kInvalidMember;
}

END_NCBI_SCOPE
