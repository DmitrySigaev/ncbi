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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer implementations for connect library objects
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>


BEGIN_NCBI_SCOPE


ERW_Result CSocketReaderWriter::PendingCount(size_t* count)
{
    if (!m_Sock) {
        return eRW_Error;
    }
    STimeout zero = {0, 0}, tmo;
    const STimeout* tmp = m_Sock->GetTimeout(eIO_Read);
    if (tmp) {
        tmo = *tmp;
        tmp = &tmo;
    }
    if (m_Sock->SetTimeout(eIO_Read, &zero) != eIO_Success) {
        return eRW_Error;
    }
    EIO_Status status = m_Sock->Read(0, 1, count, eIO_ReadPeek);
    if (m_Sock->SetTimeout(eIO_Read, tmp) != eIO_Success) {
        return eRW_Error;
    }
    return status == eIO_Success  ||  status == eIO_Timeout
        ? eRW_Success : eRW_Error;
}


ERW_Result CSocketReaderWriter::Read(void*   buf,
                                     size_t  count,
                                     size_t* n_read)
{
    if (!m_Sock) {
        return eRW_Error;
    }
    switch (m_Sock->Read(buf, count, n_read, eIO_ReadPlain)) {
    case eIO_Success:
        return eRW_Success;
    case eIO_Timeout:
        return eRW_Timeout;
    case eIO_Closed:
        return eRW_Eof;
    default:
        break;
    }
    return eRW_Error;
}


ERW_Result CSocketReaderWriter::Write(const void* buf,
                                      size_t      count,
                                      size_t*     n_written)
{
    if (!m_Sock) {
        return eRW_Error;
    }
    switch (m_Sock->Write(buf, count, n_written)) {
    case eIO_Success:
        return eRW_Success;
    case eIO_Timeout:
        return eRW_Timeout;
    case eIO_Closed:
        return eRW_Eof;
    default:
        break;
    }
    return eRW_Error;
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.7  2006/12/14 04:45:21  lavr
 * Derive from CConnIniter for auto-magical init (former CONNECT_InitInternal)
 *
 * Revision 1.6  2006/02/01 16:22:50  lavr
 * Introduce CConnReaderWriterBase to be able to init connect lib if needed
 *
 * Revision 1.5  2006/01/25 20:28:41  lavr
 * CSocketReaderWriter::PendingCount():  Return success on timeout (poll)
 *
 * Revision 1.4  2006/01/18 03:00:13  lavr
 * CSocketReaderWriter::PendingCount() fixed not to use buffer when peeking
 *
 * Revision 1.3  2005/02/01 19:06:47  lavr
 * Fix save/restore in CSocketReaderWriter::PendingCount()
 *
 * Revision 1.2  2005/02/01 19:04:47  lavr
 * Proper timeout save/restore in CSocketReaderWriter::PendingCount()
 *
 * Revision 1.1  2004/10/01 18:56:09  lavr
 * Initial revision
 *
 * ===========================================================================
 */
