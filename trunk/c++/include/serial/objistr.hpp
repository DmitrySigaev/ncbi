#ifndef OBJISTR__HPP
#define OBJISTR__HPP

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
* Revision 1.2  1999/06/04 20:51:32  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:24  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CClassInfoTmpl;

class CIObjectInfo
{
public:
    typedef CTypeInfo::TObjectPtr TObjectPtr;
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    CIObjectInfo(void)
        : m_Object(0), m_TypeInfo(0)
        {
        }
    CIObjectInfo(TObjectPtr object, TTypeInfo typeInfo)
        : m_Object(object), m_TypeInfo(typeInfo)
        {
        }

private:
    TObjectPtr m_Object;
    TTypeInfo m_TypeInfo;
};

class CIClassInfo
{
public:
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    CIClassInfo(void)
        : m_TypeInfo(0)
        {
        }

private:
    TTypeInfo m_TypeInfo;
};

class CObjectIStream
{
public:
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;
    typedef const CTypeInfo* TTypeInfo;
    typedef unsigned TIndex;

    // root reader
    void Read(TObjectPtr object, TTypeInfo typeInfo);

    // std C types readers
    virtual void ReadStd(char& data) = 0;
    virtual void ReadStd(unsigned char& data) = 0;
    virtual void ReadStd(signed char& data) = 0;
    virtual void ReadStd(short& data) = 0;
    virtual void ReadStd(unsigned short& data) = 0;
    virtual void ReadStd(int& data) = 0;
    virtual void ReadStd(unsigned int& data) = 0;
    virtual void ReadStd(long& data) = 0;
    virtual void ReadStd(unsigned long& data) = 0;
    virtual void ReadStd(float& data) = 0;
    virtual void ReadStd(double& data) = 0;
    virtual void ReadStd(string& data) = 0;
    virtual void ReadStd(const char*& data) = 0;
    virtual void ReadStd(char*& data) = 0;

    // object level readers
    // reads type info
    virtual TObjectPtr ReadPointer(TTypeInfo declaredType) = 0;
    virtual TTypeInfo ReadTypeInfo(void) = 0;

    // read object content
    virtual void ReadObject(TObjectPtr object,
                            const CClassInfoTmpl* typeInfo) = 0;

    // block interface
    class Block;
    virtual unsigned Begin(Block& block, bool tmpl);
    virtual bool Next(Block& block);
    virtual void End(Block& block);
    class Block
    {
    public:
        Block(CObjectIStream& in, bool tmpl = false)
            : m_In(in), m_Count(in.Begin(*this, tmpl))
            {
            }
        bool Next(void)
            {
                switch ( m_Count ) {
                case unsigned(-1):
                    return m_In.Next(*this);
                case 0:
                    return false;
                default:
                    --m_Count;
                    return true;
                }
            }
        ~Block(void)
            {
                switch ( m_Count ) {
                case unsigned(-1):
                    m_In.End(*this);
                    return;
                case 0:
                    return;
                default:
                    ERR_POST("not all elements read");
                    throw runtime_error("not all elements read");
                }
            }
    private:
        CObjectIStream& m_In;
        unsigned m_Count;
    };

protected:
    // low level readers
    virtual const string& ReadString(void) = 0;
    virtual const string& ReadId(void) = 0;

    TTypeInfo GetRegiteredClass(TIndex index) const;
    TTypeInfo GetRegiteredClass(const string& name) const;
    TTypeInfo RegisterClass(TTypeInfo typeInfo);

    TTypeInfo GetClass(TIndex index) const;
    const CIObjectInfo& GetObjectInfo(TIndex index) const;
    TIndex RegisterObject(TObjectPtr object, TTypeInfo typeInfo);
    
private:
    vector<CIObjectInfo> m_Objects;
    vector<CIClassInfo> m_Classes;
};

//#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
