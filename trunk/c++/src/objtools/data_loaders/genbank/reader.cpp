/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 */

#include <objects/objmgr/reader.hpp>
#include <util/stream_utils.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

ostream & operator << (ostream &os, const CStreamable &obj)
{
    obj.Save(os);
    return os;
}

istream & operator >> (istream &is, CStreamable &obj)
{
    obj.Restore(is);
    return is;
}

void CIntStreamable::Save(ostream &os) const
{
    for(unsigned i = 0; i < sizeof(m_Value); ++i)
        os.put(static_cast<char>((m_Value>>(8 * i)) & 0xff));
}

void CIntStreamable::Restore(istream &is)
{
    m_Value=0;
    for(unsigned i = 0; i < sizeof(m_Value); ++i)
        m_Value += static_cast<TInt>(is.get())<<(8 * i);
}

void CStringStreamable::Save(ostream &os) const
{
    CIntStreamable length = m_Value.length();
    os << length;
    os.write(m_Value.data(), m_Value.length());
}

void CStringStreamable::Restore(istream &is)
{
    CIntStreamable length;
    is >> length;
    if ( is ) {
        m_Value.resize(length.Value());
        is.read(&m_Value[0], length.Value());
    }
}

void CBlob::Save(ostream &os) const
{
    os << m_Class << m_Descr;
}

void CBlob::Restore(istream &is)
{
    is >> m_Class >> m_Descr;
}

size_t CIStream::Read(istream &is, char* buffer, size_t bufferLength)
{
    return CStreamUtils::Readsome(is, buffer, bufferLength);
}

bool CIStream::Eof()
{
#if 0
    CT_INT_TYPE c = get();

    if (eof())
        return true;

    if ( good() )
        putback(c);

    return false;
#else
    return eof();
#endif
}

CIntStreamable::TInt
CReader::GetConst(string &) const
{
    return 0;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.10  2003/03/26 20:42:50  lavr
 * CIStream::Eof() made (temporarily) non-destructive w/o get()
 *
 * Revision 1.9  2003/02/26 18:02:39  vasilche
 * Added istream error check.
 * Avoid use of string::c_str() method.
 *
 * Revision 1.8  2003/02/25 22:03:44  vasilche
 * Fixed identation.
 *
 * Revision 1.7  2002/11/27 21:09:43  lavr
 * Take advantage of CStreamUtils::Readsome() in CIStream::Read()
 * CIStream::Eof() modified to use get() instead of operator>>()
 *
 * Revision 1.6  2002/05/06 03:28:47  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.5  2002/03/27 20:23:50  butanaev
 * Added connection pool.
 *
 * Revision 1.4  2002/03/27 18:06:08  kimelman
 * stream.read/write instead of << >>
 *
 * Revision 1.3  2002/03/21 19:14:54  kimelman
 * GB related bugfixes
 *
 * Revision 1.2  2002/03/20 04:50:13  kimelman
 * GB loader added
 *
 * Revision 1.1  2002/01/11 19:06:21  gouriano
 * restructured objmgr
 *
 * Revision 1.6  2001/12/13 00:19:25  kimelman
 * bugfixes:
 *
 * Revision 1.5  2001/12/12 21:46:40  kimelman
 * Compare interface fix
 *
 * Revision 1.4  2001/12/10 20:08:01  butanaev
 * Code cleanup.
 *
 * Revision 1.3  2001/12/07 21:24:59  butanaev
 * Interface development, code beautyfication.
 *
 * Revision 1.2  2001/12/07 16:43:58  butanaev
 * Fixed includes.
 *
 * Revision 1.1  2001/12/07 16:10:22  butanaev
 * Switching to new reader interfaces.
 *
 * Revision 1.2  2001/12/06 18:06:22  butanaev
 * Ported to linux.
 *
 * Revision 1.1  2001/12/06 14:35:22  butanaev
 * New streamable interfaces designed, ID1 reimplemented.
 *
 */
