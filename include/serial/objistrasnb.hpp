#ifndef OBJISTRASNB__HPP
#define OBJISTRASNB__HPP

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
* Revision 1.6  1999/07/22 17:33:42  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.5  1999/07/21 20:02:14  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
*
* Revision 1.4  1999/07/21 14:19:57  vasilche
* Added serialization of bool.
*
* Revision 1.3  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.2  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.1  1999/07/02 21:31:45  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/objstrasnb.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStreamAsnBinary : public CObjectIStream
{
public:
    typedef CObjectStreamAsnBinaryDefs::TByte TByte;
    typedef CObjectStreamAsnBinaryDefs::TTag TTag;
    typedef CObjectStreamAsnBinaryDefs::ETag ETag;
    typedef CObjectStreamAsnBinaryDefs::EClass EClass;

    CObjectIStreamAsnBinary(CNcbiIstream& in);

    virtual void ReadStd(bool& data);
    virtual void ReadStd(char& data);
    virtual void ReadStd(unsigned char& data);
    virtual void ReadStd(signed char& data);
    virtual void ReadStd(short& data);
    virtual void ReadStd(unsigned short& data);
    virtual void ReadStd(int& data);
    virtual void ReadStd(unsigned int& data);
    virtual void ReadStd(long& data);
    virtual void ReadStd(unsigned long& data);
    virtual void ReadStd(float& data);
    virtual void ReadStd(double& data);

    virtual void SkipValue(void);

    unsigned char ReadByte(void);
    signed char ReadSByte(void);
    void ReadBytes(char* bytes, size_t size);
    virtual string ReadString(void);
    virtual char* ReadCString(void);

    void ExpectByte(TByte byte);

    ETag ReadSysTag(void);
    void BackSysTag(void);
    void FlushSysTag(bool constructed = false);
    TTag ReadTag(void);

    ETag ReadSysTag(EClass c, bool constructed);
    TTag ReadTag(EClass c, bool constructed);

    void ExpectSysTag(ETag tag);
    void ExpectSysTag(EClass c, bool constructed, ETag tag);
    bool LastTagWas(EClass c, bool constructed);
    bool LastTagWas(EClass c, bool constructed, ETag tag);

    size_t ReadShortLength(void);
    size_t ReadLength(void);

    void ExpectShortLength(size_t length);
    void ExpectIndefiniteLength(void);
    void ExpectEndOfContent(void);

protected:
    virtual void VBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member);
    virtual void EndMember(const Member& member);
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length);

    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);

private:
    virtual EPointerType ReadPointerType(void);
    virtual string ReadMemberPointer(void);
    virtual void ReadMemberPointerEnd(void);
    virtual TIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual void ReadOtherPointerEnd(void);

    string ReadClassTag(void);

    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

    CNcbiIstream& m_Input;

    TByte m_LastTagByte;
    enum ELastTagState {
        eNoTagRead,
        eSysTagRead,
        eSysTagBack
    };
    ELastTagState m_LastTagState;
};

//#include <serial/objistrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRASNB__HPP */
