#ifndef OBJISTRXML__HPP
#define OBJISTRXML__HPP

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
* Revision 1.9  2000/10/03 17:22:35  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.8  2000/09/29 16:18:14  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.7  2000/09/18 20:00:06  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.6  2000/09/01 13:16:01  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.5  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.4  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.3  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.2  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.1  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/lightstr.hpp>

BEGIN_NCBI_SCOPE

class CItemsInfo;

class CObjectIStreamXml : public CObjectIStream
{
public:
    typedef unsigned char TByte;

    CObjectIStreamXml(void);
    ~CObjectIStreamXml(void);

    ESerialDataFormat GetDataFormat(void) const;

    virtual string GetPosition(void) const;

    virtual string ReadFileHeader(void);

protected:
    EPointerType ReadPointerType(void);
    TObjectIndex ReadObjectPointer(void);
    string ReadOtherPointer(void);

    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void);
    virtual unsigned long ReadULong(void);
    virtual double ReadDouble(void);
    virtual void ReadNull(void);
    virtual void ReadString(string& s);
    long ReadEnum(const CEnumeratedTypeValues& values);

    virtual void SkipBool(void);
    virtual void SkipChar(void);
    virtual void SkipSNumber(void);
    virtual void SkipUNumber(void);
    virtual void SkipFNumber(void);
    virtual void SkipString(void);
    virtual void SkipNull(void);
    virtual void SkipByteBlock(void);

    CLightString SkipTagName(CLightString tag, const char* s, size_t length);
    CLightString SkipTagName(CLightString tag, const char* s);
    CLightString SkipTagName(CLightString tag, const string& s);
    CLightString SkipStackTagName(CLightString tag, size_t level);
    CLightString SkipStackTagName(CLightString tag, size_t level, char c);

    bool NextTagIsClosing(void);
    void OpenTag(const string& e);
    void CloseTag(const string& e);
    void OpenStackTag(size_t level);
    void CloseStackTag(size_t level);
    void OpenTag(TTypeInfo type);
    void CloseTag(TTypeInfo type);
    void OpenTagIfNamed(TTypeInfo type);
    void CloseTagIfNamed(TTypeInfo type);
    bool WillHaveName(TTypeInfo elementType);

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void ReadNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo,
                               TObjectPtr object);

    virtual void ReadContainer(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    virtual void SkipContainer(const CContainerTypeInfo* containerType);

    void ReadContainerContents(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    void SkipContainerContents(const CContainerTypeInfo* containerType);

    virtual void ReadChoice(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    virtual void SkipChoice(const CChoiceTypeInfo* choiceType);
    void ReadChoiceContents(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    void SkipChoiceContents(const CChoiceTypeInfo* choiceType);
#endif

    // low level I/O
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);
    virtual bool BeginContainerElement(TTypeInfo elementType);
    virtual void EndContainerElement(void);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType);
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos);
    void EndClassMember(void);

    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType);
    virtual void EndChoiceVariant(void);

    void BeginBytes(ByteBlock& );
    int GetHexChar(void);
    size_t ReadBytes(ByteBlock& block, char* dst, size_t length);
    void EndBytes(const ByteBlock& );

private:
    bool OutsideTag(void) const;
    bool InsideTag(void) const;
    bool InsideOpeningTag(void) const;
    bool InsideClosingTag(void) const;
    bool SelfClosedTag(void) const;

    void Found_lt(void);
    void Back_lt(void);
    void Found_lt_slash(void);
    void Found_gt(void);
    void Found_slash_gt(void);

    void EndSelfClosedTag(void);

    void EndTag(void);
    void EndOpeningTag(void);
    bool EndOpeningTagSelfClosed(void); // true if '/>' found, false if '>'
    void EndClosingTag(void);
    char BeginOpeningTag(void);
    char BeginClosingTag(void);
    void BeginData(void);

    int ReadEscapedChar(char endingChar);
    void ReadTagData(string& s);

    CLightString ReadName(char c);
    CLightString ReadAttributeName(void);
    void ReadAttributeValue(string& value);

    void SkipAttributeValue(char c);
    void SkipQDecl(void);

    char SkipWS(void);
    char SkipWSAndComments(void);

    void UnexpectedMember(const CLightString& id, const CItemsInfo& items);

    enum ETagState {
        eTagOutside,
        eTagInsideOpening,
        eTagInsideClosing,
        eTagSelfClosed
    };
    ETagState m_TagState;
};

#include <serial/objistrxml.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRXML__HPP */
