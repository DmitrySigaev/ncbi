#ifndef CLASSINFO__HPP
#define CLASSINFO__HPP

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
* Revision 1.8  1999/06/24 14:44:37  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 18:38:48  vasilche
* Fixed order of members in class.
* Added checks for overlapped members.
*
* Revision 1.6  1999/06/15 16:20:00  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:37  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.3  1999/06/07 19:59:37  vasilche
* offset_t -> size_t
*
* Revision 1.2  1999/06/04 20:51:31  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:23  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class COObjectList;
class CMemberInfo;

class CClassInfoTmpl : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef list<CMemberInfo*> TMembers;
    typedef map<string, const CMemberInfo*> TMembersByName;
    typedef map<size_t, const CMemberInfo*> TMembersByOffset;

    CClassInfoTmpl(const type_info& ti, size_t size, void* (*creator)(void));
    CClassInfoTmpl(const type_info& ti, size_t size, void* (*creator)(void),
                   const CTypeRef& parent, size_t offset);
    virtual ~CClassInfoTmpl(void);

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    virtual TConstObjectPtr GetDefault(void) const;
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;

    // AddMember will take ownership of member
    CClassInfoTmpl* AddMember(CMemberInfo* member);

    virtual const CMemberInfo* FindMember(const string& name) const;
    virtual const CMemberInfo* LocateMember(TConstObjectPtr object,
                                            TConstObjectPtr member,
                                            TTypeInfo memberTypeInfo) const;

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const;

private:
    size_t m_Size;
    TObjectPtr (*m_Creator)(void);

    TMembers m_Members;
    TMembersByName m_MembersByName;
    TMembersByOffset m_MembersByOffset;
};

template<class CLASS, class PCLASS = CLASS>
class CClassInfo : public CClassInfoTmpl
{
public:
    CClassInfo(void)
        : CClassInfoTmpl(typeid(CLASS), sizeof(CLASS), &sx_Create)
        {
        }
    CClassInfo(const CTypeRef& pTypeRef)
        : CClassInfoTmpl(typeid(CLASS), sizeof(CLASS), &sx_Create,
                         pTypeRef,
                         size_t(static_cast<const PCLASS*>
                                (static_cast<const CLASS*>(0))))
        {
        }

    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const
        {
            return GetTypeInfoById(typeid(*static_cast<const CLASS*>(object)));
        }

protected:

    static TObjectPtr sx_Create(void)
        {
            return new CLASS();
        }
};

//#include <serial/classinfo.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFO__HPP */
