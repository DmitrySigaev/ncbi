#ifndef MEMBER__HPP
#define MEMBER__HPP

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
* Revision 1.15  2000/09/19 14:10:24  vasilche
* Added files to MSVC project
* Updated shell scripts to use new datattool path on MSVC
* Fixed internal compiler error on MSVC
*
* Revision 1.14  2000/09/18 20:00:02  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.13  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.12  2000/06/16 20:01:20  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.11  2000/06/16 19:24:22  vasilche
* Updated MSVC project.
* Fixed error on MSVC with static const class member.
*
* Revision 1.10  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.9  2000/06/15 19:26:33  vasilche
* Fixed compilation error on Mac.
*
* Revision 1.8  2000/04/28 16:58:01  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.7  2000/03/07 14:05:29  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.6  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.5  1999/09/01 17:38:00  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.4  1999/08/31 17:50:03  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/06/30 16:04:21  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:38  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialutil.hpp>
#include <serial/item.hpp>
#include <serial/hookdata.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CClassTypeInfo;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CReadClassMemberHook;
class CWriteClassMemberHook;
class CCopyClassMemberHook;

class CDelayBuffer;

class CMemberInfo : public CItemInfo
{
    typedef CItemInfo CParent;
public:
    typedef TConstObjectPtr (*TMemberGetConst)(const CMemberInfo* memberInfo,
                                               TConstObjectPtr classPtr);
    typedef TObjectPtr (*TMemberGet)(const CMemberInfo* memberInfo,
                                     TObjectPtr classPtr);
    typedef void (*TMemberRead)(CObjectIStream& in,
                                const CMemberInfo* memberInfo,
                                TObjectPtr classPtr);
    typedef void (*TMemberWrite)(CObjectOStream& out,
                                 const CMemberInfo* memberInfo,
                                 TConstObjectPtr classPtr);
    typedef void (*TMemberSkip)(CObjectIStream& in,
                                const CMemberInfo* memberInfo);
    typedef void (*TMemberCopy)(CObjectStreamCopier& copier,
                                const CMemberInfo* memberInfo);

    CMemberInfo(const CClassTypeInfo* classType,
                const CMemberId& id, TOffset offset, const CTypeRef& type);
    CMemberInfo(const CClassTypeInfo* classType,
                const CMemberId& id, TOffset offset, TTypeInfo type);
    CMemberInfo(const CClassTypeInfo* classType,
                const char* id, TOffset offset, const CTypeRef& type);
    CMemberInfo(const CClassTypeInfo* classType,
                const char* id, TOffset offset, TTypeInfo type);
    ~CMemberInfo(void);

    const CClassTypeInfo* GetClassType(void) const;

    bool Optional(void) const;
    CMemberInfo* SetOptional(void);

    TConstObjectPtr GetDefault(void) const;
    CMemberInfo* SetDefault(TConstObjectPtr def);

    bool HaveSetFlag(void) const;
    CMemberInfo* SetSetFlag(const bool* setFlag);
    CMemberInfo* SetOptional(const bool* setFlag);

    bool GetSetFlag(TConstObjectPtr object) const;
    bool& GetSetFlag(TObjectPtr object) const;
    void UpdateSetFlag(TObjectPtr object, bool value) const;

    bool CanBeDelayed(void) const;
    CMemberInfo* SetDelayBuffer(CDelayBuffer* buffer);
    CDelayBuffer& GetDelayBuffer(TObjectPtr object) const;
    const CDelayBuffer& GetDelayBuffer(TConstObjectPtr object) const;

    // I/O
    void ReadMember(CObjectIStream& in, TObjectPtr classPtr) const;
    void ReadMissingMember(CObjectIStream& in, TObjectPtr classPtr) const;
    void WriteMember(CObjectOStream& out, TConstObjectPtr classPtr) const;
    void CopyMember(CObjectStreamCopier& copier) const;
    void CopyMissingMember(CObjectStreamCopier& copier) const;
    void SkipMember(CObjectIStream& in) const;
    void SkipMissingMember(CObjectIStream& in) const;

    TObjectPtr GetMemberPtr(TObjectPtr classPtr) const;
    TConstObjectPtr GetMemberPtr(TConstObjectPtr classPtr) const;

    // hooks
    void SetGlobalReadHook(CReadClassMemberHook* hook);
    void SetLocalReadHook(CObjectIStream& in, CReadClassMemberHook* hook);
    void ResetGlobalReadHook(void);
    void ResetLocalReadHook(CObjectIStream& in);

    void SetGlobalWriteHook(CWriteClassMemberHook* hook);
    void SetLocalWriteHook(CObjectOStream& out, CWriteClassMemberHook* hook);
    void ResetGlobalWriteHook(void);
    void ResetLocalWriteHook(CObjectOStream& out);

    void SetGlobalCopyHook(CCopyClassMemberHook* hook);
    void SetLocalCopyHook(CObjectStreamCopier& copier,
                          CCopyClassMemberHook* hook);
    void ResetGlobalCopyHook(void);
    void ResetLocalCopyHook(CObjectStreamCopier& copier);

    // default I/O (without hooks)
    void DefaultReadMember(CObjectIStream& in,
                           TObjectPtr classPtr) const;
    void DefaultReadMissingMember(CObjectIStream& in,
                                  TObjectPtr classPtr) const;
    void DefaultWriteMember(CObjectOStream& out,
                            TConstObjectPtr classPtr) const;
    void DefaultCopyMember(CObjectStreamCopier& copier) const;
    void DefaultCopyMissingMember(CObjectStreamCopier& copier) const;

    virtual void UpdateDelayedBuffer(CObjectIStream& in,
                                     TObjectPtr classPtr) const;

private:
    // containing class type info
    const CClassTypeInfo* m_ClassType;
    // is optional
    bool m_Optional;
    // default value
    TConstObjectPtr m_Default;
    // offset of 'SET' flag inside object
    TOffset m_SetFlagOffset;
    // offset of delay buffer inside object
    TOffset m_DelayOffset;

    TMemberGetConst m_GetConstFunction;
    TMemberGet m_GetFunction;

	struct SMemberRead {
		SMemberRead(TMemberRead main, TMemberRead missing)
			: m_Main(main), m_Missing(missing)
		{
		}
		TMemberRead m_Main, m_Missing;
	};
	struct SMemberCopy {
		SMemberCopy(TMemberCopy main, TMemberCopy missing)
			: m_Main(main), m_Missing(missing)
		{
		}
		TMemberCopy m_Main, m_Missing;
	};

    CHookData<CObjectIStream, CReadClassMemberHook, SMemberRead> m_ReadHookData;
    CHookData<CObjectOStream, CWriteClassMemberHook, TMemberWrite> m_WriteHookData;
    CHookData<CObjectStreamCopier, CCopyClassMemberHook, SMemberCopy> m_CopyHookData;
    TMemberSkip m_SkipFunction;
    TMemberSkip m_SkipMissingFunction;

    void SetReadHook(CObjectIStream* stream, CReadClassMemberHook* hook);
    void SetWriteHook(CObjectOStream* stream, CWriteClassMemberHook* hook);
    void SetCopyHook(CObjectStreamCopier* stream, CCopyClassMemberHook* hook);
    void ResetReadHook(CObjectIStream* stream);
    void ResetWriteHook(CObjectOStream* stream);
    void ResetCopyHook(CObjectStreamCopier* stream);

    void SetReadFunction(TMemberRead func);
    void SetReadMissingFunction(TMemberRead func);
    void SetWriteFunction(TMemberWrite func);
    void SetCopyFunction(TMemberCopy func);
    void SetCopyMissingFunction(TMemberCopy func);
    void SetSkipFunction(TMemberSkip func);
    void SetSkipMissingFunction(TMemberSkip func);

    void UpdateGetFunction(void);
    void UpdateReadFunction(void);
    void UpdateReadMissingFunction(void);
    void UpdateWriteFunction(void);
    void UpdateCopyMissingFunction(void);
    void UpdateSkipMissingFunction(void);

    static TConstObjectPtr GetConstSimpleMember(const CMemberInfo* memberInfo,
                                                TConstObjectPtr classPtr);
    static TConstObjectPtr GetConstDelayedMember(const CMemberInfo* memberInfo,
                                                 TConstObjectPtr classPtr);
    static TObjectPtr GetSimpleMember(const CMemberInfo* memberInfo,
                                      TObjectPtr classPtr);
    static TObjectPtr GetWithSetFlagMember(const CMemberInfo* memberInfo,
                                           TObjectPtr classPtr);
    static TObjectPtr GetDelayedMember(const CMemberInfo* memberInfo,
                                       TObjectPtr classPtr);
    

    static void ReadSimpleMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo,
                                 TObjectPtr classPtr);
    static void ReadWithSetFlagMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr);
    static void ReadLongMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo,
                                 TObjectPtr classPtr);
    static void ReadHookedMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo,
                                 TObjectPtr classPtr);
    static void ReadMissingSimpleMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr);
    static void ReadMissingOptionalMember(CObjectIStream& in,
                                            const CMemberInfo* memberInfo,
                                            TObjectPtr classPtr);
    static void ReadMissingHookedMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr);
    static void WriteSimpleMember(CObjectOStream& out,
                                  const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr);
    static void WriteOptionalMember(CObjectOStream& out,
                                      const CMemberInfo* memberInfo,
                                      TConstObjectPtr classPtr);
    static void WriteWithDefaultMember(CObjectOStream& out,
                                         const CMemberInfo* memberInfo,
                                         TConstObjectPtr classPtr);
    static void WriteWithSetFlagMember(CObjectOStream& out,
                                         const CMemberInfo* memberInfo,
                                         TConstObjectPtr classPtr);
    static void WriteLongMember(CObjectOStream& out,
                                  const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr);
    static void WriteHookedMember(CObjectOStream& out,
                                  const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr);
    static void CopySimpleMember(CObjectStreamCopier& copier,
                                 const CMemberInfo* memberInfo);
    static void CopyHookedMember(CObjectStreamCopier& copier,
                                 const CMemberInfo* memberInfo);
    static void CopyMissingSimpleMember(CObjectStreamCopier& copier,
                                        const CMemberInfo* memberInfo);
    static void CopyMissingOptionalMember(CObjectStreamCopier& copier,
                                            const CMemberInfo* memberInfo);
    static void CopyMissingHookedMember(CObjectStreamCopier& copier,
                                        const CMemberInfo* memberInfo);
    static void SkipSimpleMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo);
    static void SkipMissingSimpleMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo);
    static void SkipMissingOptionalMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo);
};

#include <serial/member.inl>

END_NCBI_SCOPE

#endif  /* MEMBER__HPP */
