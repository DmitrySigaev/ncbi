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
#include <serial/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <map>
#include <list>
#include <vector>

BEGIN_NCBI_SCOPE

class CTemplateResolver1;
class CTemplateResolver2;

class CStlClassInfoListImpl : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlClassInfoListImpl(const type_info& id, const CTypeRef& dataType);

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataTypeRef.Get();
        }

    virtual void AnnotateTemplate(CObjectOStream& out) const;

private:
    static CTemplateResolver1 sm_Resolver;

    CTypeRef m_DataTypeRef;
};

template<class Data>
class CStlClassInfoList : CStlClassInfoListImpl
{
    typedef CStlClassInfoListImpl CParent;

public:
    typedef list<Data> TObjectType;

    CStlClassInfoList(void);

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
            static TTypeInfo typeInfo = new CStlClassInfoList;
            return typeInfo;
        }

protected:
    virtual void CollectObjects(COObjectList& list,
                                TConstObjectPtr object) const
        {
            const TObjectType& l = *static_cast<const TObjectType*>(object);
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                AddObject(list, &*i, GetDataTypeInfo());
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& l = *static_cast<const TObjectType*>(object);
            CObjectOStream::Block block(out, l.size());
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                block.Next();
                out.Write(&*i, GetDataTypeInfo());
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& l = *static_cast<TObjectType*>(object);
            CObjectIStream::Block block(in);
            while ( block.Next() ) {
                l.push_back(Data());
                in.Read(&l.back(), GetDataTypeInfo());
            }
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */
