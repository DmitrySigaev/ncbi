#ifndef NCBIERROR__HPP
#define NCBIERROR__HPP

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
 * Author:
 *
 *
 */

/// @file ncbierror.hpp
/// Defines NCBI C++ error code.

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbimisc.hpp>
#include <errno.h>
#include <string>

/** @addtogroup Exception
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
/*  errno codes:
E2BIG
EACCES
EADDRINUSE
EADDRNOTAVAIL
EAFNOSUPPORT
EAGAIN
EALREADY
EBADF
EBADMSG
EBUSY
ECANCELED
ECHILD
ECONNABORTED
ECONNREFUSED
ECONNRESET
EDEADLK
EDESTADDRREQ
EDOM
EEXIST
EFAULT
EFBIG
EHOSTUNREACH
EIDRM
EILSEQ
EINPROGRESS
EINTR
EINVAL
EIO
EISCONN
EISDIR
ELOOP
EMFILE
EMLINK
EMSGSIZE
ENAMETOOLONG
ENETDOWN
ENETRESET
ENETUNREACH
ENFILE
ENOBUFS
ENODATA
ENODEV
ENOENT
ENOEXEC
ENOLCK
ENOLINK
ENOMEM
ENOMSG
ENOPROTOOPT
ENOSPC
ENOSR
ENOSTR
ENOSYS
ENOTCONN
ENOTDIR
ENOTEMPTY
ENOTRECOVERABLE
ENOTSOCK
ENOTSUP
ENOTTY
ENXIO
EOPNOTSUPP
EOVERFLOW
EOWNERDEAD
EPERM
EPIPE
EPROTO
EPROTONOSUPPORT
EPROTOTYPE
ERANGE
EROFS
ESPIPE
ESRCH
ETIME
ETIMEDOUT
ETXTBSY
EWOULDBLOCK
EXDEV
*/
// define missing errno codes
#if NCBI_COMPILER_MSVC
#  if _MSC_VER < 1600

#    define EADDRINUSE      100
#    define EADDRNOTAVAIL   101
#    define EAFNOSUPPORT    102
#    define EALREADY        103
#    define EBADMSG         104
#    define ECANCELED       105
#    define ECONNABORTED    106
#    define ECONNREFUSED    107
#    define ECONNRESET      108
#    define EDESTADDRREQ    109
#    define EHOSTUNREACH    110
#    define EIDRM           111
#    define EINPROGRESS     112
#    define EISCONN         113
#    define ELOOP           114
#    define EMSGSIZE        115
#    define ENETDOWN        116
#    define ENETRESET       117
#    define ENETUNREACH     118
#    define ENOBUFS         119
#    define ENODATA         120
#    define ENOLINK         121
#    define ENOMSG          122
#    define ENOPROTOOPT     123
#    define ENOSR           124
#    define ENOSTR          125
#    define ENOTCONN        126
#    define ENOTRECOVERABLE 127
#    define ENOTSOCK        128
#    define ENOTSUP         129
#    define EOPNOTSUPP      130
#    define EOTHER          131
#    define EOVERFLOW       132
#    define EOWNERDEAD      133
#    define EPROTO          134
#    define EPROTONOSUPPORT 135
#    define EPROTOTYPE      136
#    define ETIME           137
#    define ETIMEDOUT       138
#    define ETXTBSY         139
#    define EWOULDBLOCK     140

#  endif
#endif // NCBI_COMPILER_MSVC

class NCBI_XNCBI_EXPORT CNcbiError
{
public:
    enum ECode {

        eSuccess                       = 0,

// names of generic error codes
        address_family_not_supported   = EAFNOSUPPORT,
        address_in_use                 = EADDRINUSE,
        address_not_available          = EADDRNOTAVAIL,
        already_connected              = EISCONN,
        argument_list_too_long         = E2BIG,
        argument_out_of_domain         = EDOM,
        bad_address                    = EFAULT,
        bad_file_descriptor            = EBADF,
        bad_message                    = EBADMSG,
        broken_pipe                    = EPIPE,
        connection_aborted             = ECONNABORTED,
        connection_already_in_progress = EALREADY,
        connection_refused             = ECONNREFUSED,
        connection_reset               = ECONNRESET,
        cross_device_link              = EXDEV,
        destination_address_required   = EDESTADDRREQ,
        device_or_resource_busy        = EBUSY,
        directory_not_empty            = ENOTEMPTY,
        executable_format_error        = ENOEXEC,
        file_exists                    = EEXIST,
        file_too_large                 = EFBIG,
        filename_too_long              = ENAMETOOLONG,
        function_not_supported         = ENOSYS,
        host_unreachable               = EHOSTUNREACH,
        identifier_removed             = EIDRM,
        illegal_byte_sequence          = EILSEQ,
        inappropriate_io_control_operation = ENOTTY,
        interrupted                    = EINTR,
        invalid_argument               = EINVAL,
        invalid_seek                   = ESPIPE,
        io_error                       = EIO,
        is_a_directory                 = EISDIR,
        message_size                   = EMSGSIZE,
        network_down                   = ENETDOWN,
        network_reset                  = ENETRESET,
        network_unreachable            = ENETUNREACH,
        no_buffer_space                = ENOBUFS,
        no_child_process               = ECHILD,
        no_link                        = ENOLINK,
        no_lock_available              = ENOLCK,
        no_message_available           = ENODATA,
        no_message                     = ENOMSG,
        no_protocol_option             = ENOPROTOOPT,
        no_space_on_device             = ENOSPC,
        no_stream_resources            = ENOSR,
        no_such_device_or_address      = ENXIO,
        no_such_device                 = ENODEV,
        no_such_file_or_directory      = ENOENT,
        no_such_process                = ESRCH,
        not_a_directory                = ENOTDIR,
        not_a_socket                   = ENOTSOCK,
        not_a_stream                   = ENOSTR,
        not_connected                  = ENOTCONN,
        not_enough_memory              = ENOMEM,
        not_supported                  = ENOTSUP,
        operation_canceled             = ECANCELED,
        operation_in_progress          = EINPROGRESS,
        operation_not_permitted        = EPERM,
        operation_not_supported        = EOPNOTSUPP,
        operation_would_block          = EWOULDBLOCK,
        owner_dead                     = EOWNERDEAD,
        permission_denied              = EACCES,
        protocol_error                 = EPROTO,
        protocol_not_supported         = EPROTONOSUPPORT,
        read_only_file_system          = EROFS,
        resource_deadlock_would_occur  = EDEADLK,
        resource_unavailable_try_again = EAGAIN,
        result_out_of_range            = ERANGE,
        state_not_recoverable          = ENOTRECOVERABLE,
        stream_timeout                 = ETIME,
        text_file_busy                 = ETXTBSY,
        timed_out                      = ETIMEDOUT,
        too_many_files_open_in_system  = ENFILE,
        too_many_files_open            = EMFILE,
        too_many_links                 = EMLINK,
        too_many_symbolic_link_levels  = ELOOP,
        value_too_large                = EOVERFLOW,
        wrong_protocol_type            = EPROTOTYPE

// Unknown error
        , eUnknown             = 0x1000

// NCBI-specific error codes
    };

    enum ECategory {
          eGeneric   = 0
        , eNcbi      = 1
        , eMsWindows = 2
    };

    ECode Code(void) const {
        return m_Code;
    }    
    ECategory Category(void) const {
        return m_Category;
    }    
    int Native(void) const {
        return m_Native;
    }    
    const string& Extra(void) const {
        return m_Extra;
    }    

    CNcbiError(const CNcbiError& err)
        : m_Code(err.m_Code), m_Category(err.m_Category)
        , m_Native(err.m_Native), m_Extra(err.m_Extra) {
    }
    ~CNcbiError(void)
    {
    }

    CNcbiError& operator=(const CNcbiError& err) {
        m_Code    = err.m_Code;
        m_Category= err.m_Category;
        m_Native  = err.m_Native;
        m_Extra   = err.m_Extra;
        return *this;
    }

    bool operator==(const CNcbiError& err) const {
        return 
            m_Native  == err.m_Native &&
            m_Category== err.m_Category;
    }

    DECLARE_OPERATOR_BOOL(m_Code != eSuccess);

    static const CNcbiError& GetLast(void);

    static void  Set(                     ECode code,  const string& extra = kEmptyStr);
    static void  SetErrno(        int native_err_code, const string& extra = kEmptyStr);
    static void  SetFromErrno(                         const string& extra = kEmptyStr);
    static void  SetWindowsError( int native_err_code, const string& extra = kEmptyStr);
    static void  SetFromWindowsError(                  const string& extra = kEmptyStr);

protected:
    CNcbiError(void);

private:
    ECode     m_Code;
    ECategory m_Category;
    int       m_Native;
    string    m_Extra;
};

NCBI_XNCBI_EXPORT CNcbiOstream& operator<< (CNcbiOstream& str, const CNcbiError& err);


/////////////////////////////////////////////////////////////////////////////
// template CheckType

template<typename _Type>
class CheckType
{
public:
    CheckType(_Type val)
        : m_Value(val)
        , m_Checked(false)
    {
    }
    CheckType(const CheckType& t)
        : m_Value(t.m_Value)
        , m_Checked(t.m_Checked)
    {
        t.m_Checked = true;
    }

    ~CheckType(void)
    {
        if (!m_Checked) {
            ERR_POST("CheckType value not checked");
        }
    }
    CheckType& operator=(const CheckType& t)
    {
        m_Value = t.m_Value;
        m_Checked = t.m_Checked;
        t.m_Checked = true;
        return *this;
    }

    bool operator==(const CheckType& t) const
    {
        m_Checked = true;
        t.m_Checked = true;
        return m_Value == t.m_Value;
    }
    bool operator!=(const CheckType& t) const
    {
        m_Checked = true;
        t.m_Checked = true;
        return m_Value != t.m_Value;
    }
    bool operator==(const _Type& t) const
    {
        m_Checked = true;
        return m_Value == t;
    }
    bool operator!=(const _Type& t) const
    {
        m_Checked = true;
        return m_Value != t;
    }
    operator _Type(void) const
    {
        m_Checked = true;
        return m_Value;
    }
private:
    _Type m_Value;
    mutable bool m_Checked;
};

/////////////////////////////////////////////////////////////////////////////

typedef CheckType<bool> CheckBool;

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/* @} */

#endif  /* NCBIERROR__HPP */
