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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   CONN-based C++ stream buffer
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_conn_streambuf.hpp"
#include <corelib/ncbidbg.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <memory>

#define LOG_IF_ERROR(status, msg) x_LogIfError(DIAG_COMPILE_INFO, status, msg)


BEGIN_NCBI_SCOPE


CConn_Streambuf::CConn_Streambuf(CONNECTOR connector, const STimeout* timeout,
                                 streamsize buf_size, bool tie)
    : m_Conn(0), m_ReadBuf(0), m_BufSize(buf_size ? buf_size : 1),
      m_Tie(tie), x_GPos((CT_OFF_TYPE)(0)), x_PPos((CT_OFF_TYPE)(0))
{
    if ( !connector ) {
        ERR_POST("CConn_Streambuf::CConn_Streambuf(): NULL connector");
        return;
    }
    if (LOG_IF_ERROR(CONN_Create(connector, &m_Conn),
                     "CConn_Streambuf(): CONN_Create() failed") !=eIO_Success){
        return;
    }
    _ASSERT(m_Conn != 0);
    CONN_SetTimeout(m_Conn, eIO_Open,  timeout);
    CONN_SetTimeout(m_Conn, eIO_Read,  timeout);
    CONN_SetTimeout(m_Conn, eIO_Write, timeout);
    CONN_SetTimeout(m_Conn, eIO_Close, timeout);

    m_ReadBuf  = buf_size ? new CT_CHAR_TYPE[m_BufSize << 1] : &x_Buf;
    m_WriteBuf = buf_size ? m_ReadBuf + m_BufSize            : 0;

    setg(m_ReadBuf,  m_ReadBuf, m_ReadBuf);  // Empty get area
    setp(m_WriteBuf, m_WriteBuf + buf_size); // Put area (if any)

    SCONN_Callback cb;
    cb.func = x_OnClose;
    cb.data = this;
    CONN_SetCallback(m_Conn, eCONN_OnClose, &cb, 0);
}


CConn_Streambuf::~CConn_Streambuf()
{
    x_Cleanup();
    if (m_ReadBuf != &x_Buf) {
        delete[] m_ReadBuf;
    }
}


void CConn_Streambuf::x_Cleanup(bool if_close)
{
    if (!m_Conn)
        return;

    // Flush only if data pending
    if (pbase()  &&  pptr() > pbase()) {
        sync();
    }
    setg(0, 0, 0);
    setp(0, 0);

    CONN c = m_Conn;
    m_Conn = 0;
    if (if_close) {
        // Close only if not called from close callback
        EIO_Status status = CONN_Close(c);
        if (status != eIO_Success) {
            _TRACE("CConn_Streambuf::x_Cleanup(): "
                   "CONN_Close() failed (" << IO_StatusStr(status) << ")");
        }
    }
}


void CConn_Streambuf::x_OnClose(CONN conn, ECONN_Callback type, void* data)
{
    CConn_Streambuf* sb = static_cast<CConn_Streambuf*>(data);

    _ASSERT(type == eCONN_OnClose  &&  sb  &&  conn);
    _ASSERT(!sb->m_Conn  ||  sb->m_Conn == conn);
    sb->x_Cleanup(false);
}


EIO_Status CConn_Streambuf::x_LogIfError(const CDiagCompileInfo& diag_info,
                                         EIO_Status status, const string& msg)
{
    if (status != eIO_Success) {
        CNcbiDiag(diag_info) << "CConn_Streambuf::" << msg <<
            " (" << IO_StatusStr(status) << ")" << Endm;
    }
    return status;
}


CT_INT_TYPE CConn_Streambuf::overflow(CT_INT_TYPE c)
{
    if ( !m_Conn )
        return CT_EOF;

    if ( pbase() ) {
        // send buffer
        size_t n_write = pptr() - pbase();
        if ( n_write ) {
            size_t n_written;
            LOG_IF_ERROR(CONN_Write(m_Conn, pbase(),
                                    n_write, &n_written, eIO_WritePlain),
                         "overflow(): CONN_Write() failed");
            if ( !n_written )
                return CT_EOF;
            // update buffer content (get rid of the data just sent)
            memmove(pbase(), pbase() + n_written, n_write - n_written);
            x_PPos += (CT_OFF_TYPE) n_written;
            pbump(-int(n_written));
        }

        // store char
        if ( !CT_EQ_INT_TYPE(c, CT_EOF) )
            return sputc(CT_TO_CHAR_TYPE(c));
    } else if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // send char
        size_t n_written;
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        LOG_IF_ERROR(CONN_Write(m_Conn, &b, 1, &n_written, eIO_WritePlain),
                     "overflow(): CONN_Write(1) failed");
        x_PPos += (CT_OFF_TYPE) n_written;
        return n_written == 1 ? c : CT_EOF;
    }

    _ASSERT(CT_EQ_INT_TYPE(c, CT_EOF));
    return CONN_Flush(m_Conn) == eIO_Success ? CT_NOT_EOF(CT_EOF) : CT_EOF;
}


streamsize CConn_Streambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize m)
{
    if ( !m_Conn )
        return 0;

    if (m <= 0)
        return 0;
    size_t n = (size_t) m;

    EIO_Status status = eIO_Success;
    size_t n_written = 0;

    for (;;) {
        size_t x_written;

        if ( pbase() ) {
            if (pptr() + n < epptr()) {
                // Entirely fits into the buffer not causing an overflow
                memcpy(pptr(), buf, n);
                pbump(int(n));
                return (streamsize)(n_written + n);
            }
            if (status != eIO_Success)
                break;

            size_t x_write = (size_t)(pptr() - pbase());
            if (x_write) {
                status = CONN_Write(m_Conn, pbase(),
                                    x_write, &x_written, eIO_WritePlain);
                LOG_IF_ERROR(status, "xsputn(): CONN_Write(Plain) failed");
                if (!x_written)
                    break;
                memmove(pbase(), pbase() + x_written, x_write - x_written);
                x_PPos += (CT_OFF_TYPE) x_written;
                pbump(-int(x_written));
                continue;
            }
        }

        _ASSERT(n  &&  status == eIO_Success);
        status = CONN_Write(m_Conn, buf, n, &x_written, eIO_WritePersist);
        LOG_IF_ERROR(status, "xsputn(): CONN_Write(Persist) failed");
        if (!x_written) {
            if (!pbase())
                return (streamsize) n_written;
            break;
        }
        x_PPos    += (CT_OFF_TYPE) x_written;
        n_written += x_written;
        n         -= x_written;
        if (!n  ||  !pbase())
            return (streamsize) n_written;
        buf       += x_written;
        if (status != eIO_Success)
            break;
    }

    _ASSERT(n  &&  pbase());
    if (pptr() < epptr()) {
        size_t x_written = (size_t)(epptr() - pptr());
        if (x_written > n)
            x_written = n;
        memcpy(pptr(), buf, x_written);
        n_written += x_written;
        pbump(int(x_written));
    }

    return (streamsize) n_written;
}


CT_INT_TYPE CConn_Streambuf::underflow(void)
{
    if ( !m_Conn )
        return CT_EOF;

    // flush output buffer, if tied up to it
    if ( m_Tie ) {
        _VERIFY(sync() == 0);
    }
    _ASSERT(!gptr()  ||  gptr() >= egptr());

#ifdef NCBI_COMPILER_MIPSPRO
    if (m_MIPSPRO_ReadsomeGptrSetLevel  &&  m_MIPSPRO_ReadsomeGptr != gptr())
        return CT_EOF;
    m_MIPSPRO_ReadsomeGptr = (CT_CHAR_TYPE*)(-1L);
#endif /*NCBI_COMPILER_MIPSPRO*/

    // read from connection
    size_t     n_read;
    EIO_Status status = CONN_Read(m_Conn, m_ReadBuf,
                                  m_BufSize, &n_read, eIO_ReadPlain);
    if (!n_read) {
        if (status != eIO_Closed)
            LOG_IF_ERROR(status, "underflow(): CONN_Read() failed");
        return CT_EOF;
    }

    // update input buffer with the data just read
    x_GPos += (CT_OFF_TYPE) n_read;
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read);

    return CT_TO_INT_TYPE(*m_ReadBuf);
}


streamsize CConn_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    if ( !m_Conn )
        return 0;

    // flush output buffer, if tied up to it
    if ( m_Tie ) {
        _VERIFY(sync() == 0);
    }

    if (m <= 0)
        return 0;
    size_t n = (size_t) m;

    // first, read from the memory buffer
    size_t n_read = gptr() ? (size_t)(egptr() - gptr()) : 0;
    if (n_read > n)
        n_read = n;
    memcpy(buf, gptr(), n_read);
    gbump(int(n_read));
    buf += n_read;
    n   -= n_read;

    while ( n ) {
        // next, read from the connection
        size_t       x_read = n < (size_t) m_BufSize ? m_BufSize : n;
        CT_CHAR_TYPE* x_buf = n < (size_t) m_BufSize ? m_ReadBuf : buf;
        EIO_Status   status = CONN_Read(m_Conn, x_buf,
                                        x_read, &x_read, eIO_ReadPlain);
        if (!x_read)
            break;
        x_GPos += (CT_OFF_TYPE) x_read;
        // satisfy "usual backup condition", see standard: 27.5.2.4.3.13
        if (x_buf == m_ReadBuf) {
            size_t xx_read = x_read;
            if (x_read > n)
                x_read = n;
            memcpy(buf, m_ReadBuf, x_read);
            setg(m_ReadBuf, m_ReadBuf + x_read, m_ReadBuf + xx_read);
        } else {
            _ASSERT(x_read <= n);
            size_t xx_read = x_read > (size_t) m_BufSize ? m_BufSize : x_read;
            memcpy(m_ReadBuf, buf + x_read - xx_read, xx_read);
            setg(m_ReadBuf, m_ReadBuf + xx_read, m_ReadBuf + xx_read);
        }
        n_read += x_read;
        if (status != eIO_Success)
            break;
        buf    += x_read;
        n      -= x_read;
    }

    return (streamsize) n_read;
}


streamsize CConn_Streambuf::showmanyc(void)
{
    if ( !m_Conn )
        return -1;

    // flush output buffer, if tied up to it
    if ( m_Tie ) {
        _VERIFY(sync() == 0);
    }
    _ASSERT(!gptr()  ||  gptr() >= egptr());

    switch (CONN_Wait(m_Conn, eIO_Read, CONN_GetTimeout(m_Conn, eIO_Read))) {
    case eIO_Success:
        return  1;      // can read at least 1 byte
    case eIO_Closed:
        return -1;      // EOF
    default:
        return  0;      // no data available immediately
    }
}


int CConn_Streambuf::sync(void)
{
    if ( !m_Conn )
        return -1;

    do {
        if (CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF))
            return -1;
    } while (pbase()  &&  pptr() > pbase());
    return 0;
}


CNcbiStreambuf* CConn_Streambuf::setbuf(CT_CHAR_TYPE* /*buf*/,
                                        streamsize    /*buf_size*/)
{
    NCBI_THROW(CConnException, eConn, "CConn_Streambuf::setbuf() not allowed");
    /*NOTREACHED*/
    return this; /*dummy for compiler*/
}


CT_POS_TYPE CConn_Streambuf::seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                     IOS_BASE::openmode which)
{
    if (m_Conn  &&  off == 0  &&  whence == IOS_BASE::cur) {
        switch (which) {
        case IOS_BASE::out:
            return x_PPos + (CT_OFF_TYPE)(pptr() ? pptr() - pbase() : 0);
        case IOS_BASE::in:
            return x_GPos - (CT_OFF_TYPE)(gptr() ? egptr() - gptr() : 0);
        default:
            break;
        }
    }
    return (CT_POS_TYPE)((CT_OFF_TYPE)(-1));
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.66  2006/08/24 15:00:01  lavr
 * xsputn() to use I/O status when looping over actual writes to device
 *
 * Revision 6.65  2006/08/24 14:36:34  lavr
 * BUGFIX:  xsputn() not to update write count when flushing
 *
 * Revision 6.64  2006/08/23 19:32:04  lavr
 * +xsputn
 *
 * Revision 6.63  2006/08/22 18:24:52  lavr
 * NCBI_COMPILER_MIPSPRO closing added
 *
 * Revision 6.62  2006/07/24 21:11:11  lavr
 * Simplify CConn_Streambuf::xsgetn()
 *
 * Revision 6.61  2006/07/24 20:54:26  lavr
 * BUGFIX: CConn_Streambuf::overflow() to correctly advance in written data
 *
 * Revision 6.60  2006/05/01 19:26:04  lavr
 * Consistently use pbase() instead of m_WriteBuf where appropriate
 *
 * Revision 6.59  2005/12/15 15:14:10  lavr
 * Rollback to R6.55
 *
 * Revision 6.58  2005/12/14 22:29:01  lavr
 * Fix another extra flush bug in CConn_Streambuf::overflow()
 *
 * Revision 6.57  2005/12/14 22:24:55  lavr
 * Fix a bug in CConn_Streambuf::overflow()
 *
 * Revision 6.56  2005/12/14 21:34:57  lavr
 * Create streambuf initially unbuffered for write so that first
 * output would cause an overflow and open an underlying connection
 *
 * Revision 6.55  2005/05/17 00:19:18  lavr
 * OnClose safety hook added; GPos bug fixed
 *
 * Revision 6.54  2005/03/24 19:52:09  lavr
 * CConn_Streambuf()'s dtor: flush only if data pending
 *
 * Revision 6.53  2005/03/15 21:27:52  lavr
 * +CConn_Streambuf::Close()
 *
 * Revision 6.52  2005/02/03 21:43:00  lavr
 * assert() -> _ASSERT()
 *
 * Revision 6.51  2005/02/03 19:37:56  lavr
 * Fix flush() problem in overflow()
 *
 * Revision 6.50  2004/10/27 15:51:21  lavr
 * Use safer C style casting for CT_OFF_TYPE
 *
 * Revision 6.49  2004/10/27 14:17:46  ucko
 * underflow, xsgetn: Cast size_t to CT_OFF_TYPE before adding to CT_POS_TYPE.
 *
 * Revision 6.48  2004/10/26 20:31:33  lavr
 * Track both Put(formerly only) and Get(newly added) stream positions
 *
 * Revision 6.47  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 6.46  2004/05/17 20:58:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.45  2004/02/23 15:23:39  lavr
 * New (last) parameter "how" added in CONN_Write() API call
 *
 * Revision 6.44  2004/01/14 22:21:39  vasilche
 * Removed duplicate default arguments.
 *
 * Revision 6.43  2004/01/14 20:24:29  lavr
 * CConnStreambuf::seekoff(0, cur, out) added and implemented
 *
 * Revision 6.42  2004/01/09 17:39:15  lavr
 * Define and use internal 1-byte buffer for unbuffered streams' get ops
 *
 * Revision 6.41  2003/11/12 17:45:20  lavr
 * Minor cosmetic fix
 *
 * Revision 6.40  2003/11/12 16:40:13  ivanov
 * Fixed initial setup of get pointers (by Anton Lavrentiev)
 *
 * Revision 6.39  2003/11/04 03:09:28  lavr
 * xsgetn() fixed to advance buffer pointer when reading
 *
 * Revision 6.38  2003/11/03 20:06:49  lavr
 * Fix log message of the previous commit:
 * CConn_Streambuf::xsgetn() made standard-conforming
 *
 * Revision 6.36  2003/10/22 18:16:09  lavr
 * More consistent use of buffer pointers in the implementation
 *
 * Revision 6.35  2003/10/07 19:59:40  lavr
 * Replace '==' with (better) 'CT_EQ_INT_TYPE'
 *
 * Revision 6.34  2003/09/23 21:05:50  lavr
 * Rearranged included headers
 *
 * Revision 6.33  2003/05/31 05:24:43  lavr
 * NOTREACHED added after exception throw
 *
 * Revision 6.32  2003/05/20 19:07:57  lavr
 * Constructor changed (assert -> _ASSERT);  better destructor
 *
 * Revision 6.31  2003/05/20 18:24:06  lavr
 * Set non-zero ptrs (but still empty buf) in constructor to explicitly show
 * (to the stream level) that this streambuf has a memory-resident buffer
 *
 * Revision 6.30  2003/05/20 18:05:53  lavr
 * x_LogIfError() to accept and print approproate file location
 *
 * Revision 6.29  2003/05/20 16:46:29  lavr
 * Check for NULL connection in every streambuf method before proceding
 *
 * Revision 6.28  2003/05/12 18:32:27  lavr
 * Modified not to throw exceptions from stream buffer; few more improvements
 *
 * Revision 6.27  2003/04/25 15:20:37  lavr
 * Avoid GCC signed/unsigned warning by explicit cast
 *
 * Revision 6.26  2003/04/11 17:57:11  lavr
 * Define xsgetn() unconditionally
 *
 * Revision 6.25  2003/03/30 07:00:09  lavr
 * MIPS-specific workaround for lamely-designed stream read ops
 *
 * Revision 6.24  2003/03/28 03:58:08  lavr
 * CConn_Streambuf::xsgetn(): tiny formal fix in backup condition
 *
 * Revision 6.23  2003/03/28 03:30:36  lavr
 * Define CConn_Streambuf::xsgetn() unconditionally of compiler
 *
 * Revision 6.22  2002/12/19 17:24:08  lavr
 * Take advantage of new CConnException
 *
 * Revision 6.21  2002/10/28 15:46:20  lavr
 * Use "ncbi_ansi_ext.h" privately
 *
 * Revision 6.20  2002/08/28 03:40:54  lavr
 * Better buffer filling in xsgetn()
 *
 * Revision 6.19  2002/08/27 20:27:36  lavr
 * xsgetn(): if reading from external source try to pull as much as possible
 *
 * Revision 6.18  2002/08/07 16:32:12  lavr
 * Changed EIO_ReadMethod enums accordingly
 *
 * Revision 6.17  2002/06/06 19:03:25  lavr
 * Take advantage of CConn_Exception class and do not throw from destructor
 * Some housekeeping: log moved to the end
 *
 * Revision 6.16  2002/02/05 22:04:12  lavr
 * Included header files rearranged
 *
 * Revision 6.15  2002/02/05 16:05:26  lavr
 * List of included header files revised
 *
 * Revision 6.14  2002/02/04 20:19:55  lavr
 * +xsgetn() for MIPSPro compiler (buggy version supplied with std.library)
 * More assert()'s inserted into the code to check standard compliance
 *
 * Revision 6.13  2002/01/30 20:09:00  lavr
 * Define xsgetn() for WorkShop compiler also; few patches in underflow();
 * sync() properly redesigned (now standard-conformant)
 *
 * Revision 6.12  2002/01/28 20:20:18  lavr
 * Use auto_ptr only in constructor; satisfy "usual backup cond" in xsgetn()
 *
 * Revision 6.11  2001/12/07 22:58:44  lavr
 * More comments added
 *
 * Revision 6.10  2001/05/29 19:35:21  grichenk
 * Fixed non-blocking stream reading for GCC
 *
 * Revision 6.9  2001/05/17 15:02:50  lavr
 * Typos corrected
 *
 * Revision 6.8  2001/05/14 16:47:45  lavr
 * streambuf::xsgetn commented out as it badly interferes
 * with truly-blocking stream reading via istream::read.
 *
 * Revision 6.7  2001/05/11 20:40:48  lavr
 * Workaround of compiler warning about comparison of streamsize and size_t
 *
 * Revision 6.6  2001/05/11 14:04:07  grichenk
 * +CConn_Streambuf::xsgetn(), +CConn_Streambuf::showmanyc()
 *
 * Revision 6.5  2001/03/26 18:36:39  lavr
 * CT_EQ_INT_TYPE used throughout to compare CT_INT_TYPE values
 *
 * Revision 6.4  2001/03/24 00:34:40  lavr
 * Accurate conversions between CT_CHAR_TYPE and CT_INT_TYPE
 * (BUGFIX: promotion of (signed char)255 to int caused EOF (-1) gets returned)
 *
 * Revision 6.3  2001/01/12 23:49:20  lavr
 * Timeout and GetCONN method added
 *
 * Revision 6.2  2001/01/11 23:04:06  lavr
 * Bugfixes; tie is now done at streambuf level, not in iostream
 *
 * Revision 6.1  2001/01/09 23:34:51  vakatov
 * Initial revision (draft, not tested in run-time)
 *
 * ===========================================================================
 */
