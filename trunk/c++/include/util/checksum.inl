#if defined(CHECKSUM__HPP)  &&  !defined(CHECKSUM__INL)
#define CHECKSUM__INL

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
*   CRC32 calculation inline methods
*/

inline
CChecksum::EMethod CChecksum::GetMethod(void) const
{
    return m_Method;
}

inline
bool CChecksum::Valid(void) const
{
    return GetMethod() != eNone;
}

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum)
{
    return checksum.WriteChecksum(out);
}

inline
void CChecksum::AddLine(const char* line, size_t length)
{
    AddChars(line, length);
    NextLine();
}

inline
void CChecksum::AddLine(const string& line)
{
    AddLine(line.data(), line.size());
}

inline
bool CChecksum::ValidChecksumLine(const char* line, size_t length) const
{
    return length > kMinimumChecksumLength &&
        line[0] == '/' && line[1] == '*' && // first four letter of checksum
        line[2] == ' ' && line[3] == 'O' && // see sx_Start in checksum.cpp
        ValidChecksumLineLong(line, length); // complete check
}

inline
bool CChecksum::ValidChecksumLine(const string& line) const
{
    return ValidChecksumLine(line.data(), line.size());
}


inline Uint4 CChecksum::GetChecksum() const
{
    _ASSERT(GetMethod() == eCRC32);
    return m_Checksum.m_CRC32;
}

inline void CChecksum::GetMD5Digest(unsigned char digest[16]) const
{
    _ASSERT(GetMethod() == eMD5);
    m_Checksum.m_MD5->Finalize(digest);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2003/07/29 22:11:42  vakatov
* Typo fixed (extra return op)
*
* Revision 1.4  2003/07/29 21:29:26  ucko
* Add MD5 support (cribbed from the C Toolkit)
*
* Revision 1.3  2003/04/15 16:12:09  kuznets
* GetChecksum() method implemented
*
* Revision 1.2  2001/01/05 20:08:52  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.1  2000/11/22 16:26:22  vasilche
* Added generation/checking of checksum to user files.
*
* ===========================================================================
*/

#endif /* def CHECKSUM__HPP  &&  ndef CHECKSUM__INL */
