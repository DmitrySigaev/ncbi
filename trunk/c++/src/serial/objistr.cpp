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
* Revision 1.7  1999/06/16 20:35:30  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:19:48  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/11 19:14:57  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.4  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:52  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

// root reader
void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    TIndex index = RegisterObject(object, typeInfo);
    _TRACE("CObjectIStream::ReadData(" << unsigned(object) << ", "
           << typeInfo->GetName() << ") @" << index);
    ReadData(object, typeInfo);
}

void CObjectIStream::ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    TIndex index = RegisterObject(object, typeInfo);
    _TRACE("CObjectIStream::ReadData(" << unsigned(object) << ", "
           << typeInfo->GetName() << ") @" << index);
    ReadData(object, typeInfo);
}

void CObjectIStream::SkipValue(void)
{
    THROW1_TRACE(runtime_error, "cannot skip value");
}

void CObjectIStream::FBegin(Block& block)
{
    SetNonFixed(block);
    VBegin(block);
}

void CObjectIStream::VBegin(Block& )
{
}

void CObjectIStream::FNext(const Block& )
{
}

bool CObjectIStream::VNext(const Block& )
{
    return false;
}

void CObjectIStream::FEnd(const Block& )
{
}

void CObjectIStream::VEnd(const Block& )
{
}

CObjectIStream::Block::Block(CObjectIStream& in)
    : m_In(in), m_Fixed(false), m_Finished(false), m_NextIndex(0), m_Size(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, EFixed )
    : m_In(in), m_Fixed(true), m_Finished(false), m_NextIndex(0), m_Size(0)
{
    in.FBegin(*this);
}

bool CObjectIStream::Block::Next(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() >= GetSize() ) {
            return false;
        }
        m_In.FNext(*this);
    }
    else {
        if ( Finished() ) {
            return false;
        }
        if ( !m_In.VNext(*this) ) {
            m_Finished = true;
            return false;
        }
    }
    IncIndex();
    return true;
}

CObjectIStream::Block::~Block(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() != GetSize() ) {
            _TRACE("not all elements read");
            ERR_POST("not all elements read");
            return;
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        m_In.FEnd(*this);
    }
    else {
        if ( !Finished() ) {
            _TRACE("not all elements read");
            ERR_POST("not all elements read");
            return;
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        m_In.VEnd(*this);
    }
}

string CObjectIStream::ReadId(void)
{
    return ReadString();
}

string CObjectIStream::ReadMemberName(void)
{
    return ReadId();
}

CObjectIStream::TIndex CObjectIStream::RegisterObject(TObjectPtr object,
                                                      TTypeInfo typeInfo)
{
    TIndex index = m_Objects.size();
    m_Objects.push_back(CIObjectInfo(object, typeInfo));
    return index;
}

const CIObjectInfo& CObjectIStream::GetRegisteredObject(TIndex index) const
{
    if ( index >= m_Objects.size() ) {
        THROW1_TRACE(runtime_error, "invalid object index");
    }
    return m_Objects[index];
}

END_NCBI_SCOPE

