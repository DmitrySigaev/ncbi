#ifndef CONNECT___NCBI_CONN_STREAM__HPP
#define CONNECT___NCBI_CONN_STREAM__HPP

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   CONN-based C++ streams
 *
 * Classes:
 *   CConn_IOStream - base class derived from "std::iostream" to perform I/O
 *                    by means of underlying CConn_Streambuf (implemented
 *                    privately in ncbi_conn_streambuf.[ch]pp).
 *
 *   CConn_SocketStream  - I/O stream based on socket connector.
 *
 *   CConn_HttpStream    - I/O stream based on HTTP connector (that is,
 *                         the stream, which connects to HTTP server and
 *                         exchanges information using HTTP protocol).
 *
 *   CConn_ServiceStream - I/O stream based on service connector, which is
 *                         able to exchange data to/from a named service
 *                         that can be found via dispatcher/load-balancing
 *                         daemon and implemented as either HTTP GCI,
 *                         standalone server, or NCBID service.
 *
 *   CConn_MemoryStream  - In-memory stream of data (analogous to strstream).
 *
 */

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>


/** @addtogroup ConnStreams
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CConn_Streambuf; // Forward declaration


const streamsize kConn_DefBufSize = 4096;


/*
 * Base class, derived from "std::iostream", does both input
 * and output, using the specified CONNECTOR. Input operations
 * can be tied to the output ones by setting 'do_tie' to 'true'
 * (default), which means that any input attempt first flushes
 * the output queue from the internal buffers. 'buf_size'
 * designates the size of the I/O buffers, which reside in between
 * the stream and underlying connector (which in turn may do
 * further buffering, if needed).
 */

class NCBI_XCONNECT_EXPORT CConn_IOStream : public iostream
{
public:
    CConn_IOStream
    (CONNECTOR       connector,
     const STimeout* timeout  = CONN_DEFAULT_TIMEOUT,
     streamsize      buf_size = kConn_DefBufSize,
     bool            do_tie   = true);
    virtual ~CConn_IOStream(void);

    CONN GetCONN() const;

private:
    CConn_Streambuf* m_CSb;
};



/*
 * This stream exchanges data in a TCP channel, using socket interface.
 * The endpoint is specified as host/port pair. The maximal
 * number of connection attempts is given as 'max_try'.
 * More details on that: <connect/ncbi_socket_connector.h>.
 */

class NCBI_XCONNECT_EXPORT CConn_SocketStream : public CConn_IOStream
{
public:
    CConn_SocketStream
    (const string&   host,         /* host to connect to  */
     unsigned short  port,         /* ... and port number */
     unsigned int    max_try  = 3, /* number of attempts  */
     const STimeout* timeout  = CONN_DEFAULT_TIMEOUT,
     streamsize      buf_size = kConn_DefBufSize);

    // This variant uses existing socket "sock" to build the stream upon it.
    // NOTE:  it revokes all ownership of the socket, and further assumes the
    // socket being in exclusive use of this stream's underlying CONN.
    // More details:  <ncbi_socket_connector.h>::SOCK_CreateConnectorOnTop().
    CConn_SocketStream
    (SOCK            sock,         /* socket              */
     unsigned int    max_try  = 3, /* number of attempts  */
     const STimeout* timeout  = CONN_DEFAULT_TIMEOUT,
     streamsize      buf_size = kConn_DefBufSize);
};



/*
 * This stream exchanges data with an HTTP server found by URL:
 * http://host[:port]/path[?args]
 *
 * Note that 'path' must include a leading slash,
 * 'args' can be empty, in which case the '?' is not appended to the path.
 *
 * 'User_header' (if not empty) should be a sequence of lines
 * in the form 'HTTP-tag: Tag value', separated by '\r\n', and
 * '\r\n'-terminated. It is included in the HTTP-header of each transaction.
 *
 * More elaborate specification of the server can be done via
 * SConnNetInfo structure, which otherwise will be created with the
 * use of a standard registry section to obtain default values
 * (details: <connect/ncbi_connutil.h>).
 *
 * THCC_Flags and other details: <connect/ncbi_http_connector.h>.
 *
 * Provided 'timeout' is set at connection level, and if different from
 * CONN_DEFAULT_TIMEOUT, it overrides value supplied by HTTP connector
 * (the latter value is kept in SConnNetInfo::timeout).
 */

class NCBI_XCONNECT_EXPORT CConn_HttpStream : public CConn_IOStream
{
public:
    CConn_HttpStream
    (const string&   host,
     const string&   path,
     const string&   args        = kEmptyStr,
     const string&   user_header = kEmptyStr,
     unsigned short  port        = 80,
     THCC_Flags      flags       = fHCC_AutoReconnect,
     const STimeout* timeout     = CONN_DEFAULT_TIMEOUT,
     streamsize      buf_size    = kConn_DefBufSize
     );

    CConn_HttpStream
    (const string&   url,
     THCC_Flags      flags       = fHCC_AutoReconnect,
     const STimeout* timeout     = CONN_DEFAULT_TIMEOUT,
     streamsize      buf_size    = kConn_DefBufSize
     );

    CConn_HttpStream
    (const SConnNetInfo* net_info    = 0,
     const string&       user_header = kEmptyStr,
     THCC_Flags          flags       = fHCC_AutoReconnect,
     const STimeout*     timeout     = CONN_DEFAULT_TIMEOUT,
     streamsize          buf_size    = kConn_DefBufSize
     );
};



/*
 * This stream exchanges the data with a named service, in a
 * constraint that the service is implemented as one of the specified
 * server 'types' (details: <connect/ncbi_server_info.h>).
 *
 * Additional specifications can be passed in the SConnNetInfo structure,
 * otherwise created by using service name as a registry section
 * to obtain the information from (details: <connect/ncbi_connutil.h>).
 *
 * Provided 'timeout' is set at connection level, and if different from
 * CONN_DEFAULT_TIMEOUT, it overrides value supplied by underlying connector
 * (the latter value is kept in SConnNetInfo::timeout).
 */

class NCBI_XCONNECT_EXPORT CConn_ServiceStream : public CConn_IOStream
{
public:
    CConn_ServiceStream
    (const string&         service,
     TSERV_Type            types    = fSERV_Any,
     const SConnNetInfo*   net_info = 0,
     const SSERVICE_Extra* params   = 0,
     const STimeout*       timeout  = CONN_DEFAULT_TIMEOUT,
     streamsize            buf_size = kConn_DefBufSize);
};



class CRWLock; // Forward declaration

/*
 * In-memory stream.
 */

class NCBI_XCONNECT_EXPORT CConn_MemoryStream : public CConn_IOStream
{
public:
    CConn_MemoryStream(CRWLock*   lk = 0,
                       bool       pass_lk_ownership = true,
                       streamsize buf_size = kConn_DefBufSize);
};


END_NCBI_SCOPE


/* @} */


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.20  2003/04/29 19:58:04  lavr
 * Constructor taking a URL added in CConn_HttpStream
 *
 * Revision 6.19  2003/04/11 17:55:30  lavr
 * Proper indentation of some fragments
 *
 * Revision 6.18  2003/04/09 17:58:42  siyan
 * Added doxygen support
 *
 * Revision 6.17  2003/01/17 19:44:20  lavr
 * Reduce dependencies
 *
 * Revision 6.16  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.15  2002/08/12 15:05:15  lavr
 * Included header files reordered
 *
 * Revision 6.14  2002/06/12 19:19:25  lavr
 * Guard macro name standardized
 *
 * Revision 6.13  2002/06/06 19:01:31  lavr
 * Take advantage of CConn_Exception class
 * Some housekeeping: guard macro name changed, log moved to the end
 *
 * Revision 6.12  2002/02/21 18:04:24  lavr
 * +class CConn_MemoryStream
 *
 * Revision 6.11  2002/01/28 20:17:43  lavr
 * +Forward declaration of CConn_Streambuf and a private member pointer
 * of this type (for clean destruction of a streambuf sub-object)
 *
 * Revision 6.10  2001/12/10 19:41:16  vakatov
 * + CConn_SocketStream::CConn_SocketStream(SOCK sock, ....)
 *
 * Revision 6.9  2001/12/07 22:55:41  lavr
 * More comments added
 *
 * Revision 6.8  2001/09/24 20:25:57  lavr
 * +SSERVICE_Extra* parameter for CConn_ServiceStream::CConn_ServiceStream()
 *
 * Revision 6.7  2001/04/24 21:18:41  lavr
 * Default timeout is set as a special value CONN_DEFAULT_TIMEOUT.
 * Removed wrong log for R6.6.
 *
 * Revision 6.5  2001/02/09 17:38:16  lavr
 * Typo fixed in comments
 *
 * Revision 6.4  2001/01/12 23:48:51  lavr
 * GetCONN method added
 *
 * Revision 6.3  2001/01/11 23:04:04  lavr
 * Bugfixes; tie is now done at streambuf level, not in iostream
 *
 * Revision 6.2  2001/01/10 21:41:08  lavr
 * Added classes: CConn_SocketStream, CConn_HttpStream, CConn_ServiceStream.
 * Everything is now wordly documented.
 *
 * Revision 6.1  2001/01/09 23:35:24  vakatov
 * Initial revision (draft, not tested in run-time)
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CONN_STREAM__HPP */
