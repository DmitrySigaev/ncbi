#ifndef UTIL_COMPRESS__STREAM__HPP
#define UTIL_COMPRESS__STREAM__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:  CCompression based C++ I/O streams
 *
 */

#include <util/compress/compress.hpp>


/** @addtogroup CompressionStreams
 *
 * @{
 */

BEGIN_NCBI_SCOPE

// Default compression I/O stream buffer size
const streamsize kCompressionDefaultBufSize = 16*1024;


//////////////////////////////////////////////////////////////////////////////
//
// CCompression based stream classes
//
//////////////////////////////////////////////////////////////////////////////
//
// All CCompression based streams uses a "stream processors" that works as
// adapters between current stream's I/O interface and a some other stream,
// specified in the constructor. Any stream processor can implement either
// compression or decompression.

// On reading from stream, data will be readed from underlying stream,
// processed by "read stream processor", and next processed data will be
// carried to called process.
// Also, all data written into CCompression[IO]Stream stream will be processed
// using "write stream processor" and further written into underlying stream. 
// 
// Note:
//   Notice that generally the input/output stream you pass to
//   CCompression[IO]Stream class constructor must be in binary mode, because
//   the compressed data always is binary (decompressed data also can be
//   binary) and the conversions which happen in text mode will corrupt it.
//
// Note:
//   CCompression[IO]Stream class objects must be finalized after use.
//   Only after finalization all data putted into stream will be processed
//   for sure. By default finalization called in the class destructor, however
//   it can be done in any time by call Finalize(). After finalization you
//   can only read from the stream (if it is derived from istream).
//
// Note:
//   There is one special aspect of CCompression[I]OStream class. Basically
//   the compression algorithms works on blocks of data. They waits until
//   a block is full and then compresses it. As long as you only feed data
//   to the stream without flushing it this works normally. If you flush
//   the stream, you force a premature end of the data block. This will cause
//   a worse compression factor. You should avoid flushing a output buffer.
//
//   Accordingly, the using input stream with compression usualy have worse
//   compression than output stream with compression. Because a stream needs
//   to flush data from compressor very often. Increasing compressor buffer
//   size can to amend this situation.
//


// Forward declaration
class CCompressionStreamProcessor;


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStream - base stream class
//

class NCBI_XUTIL_EXPORT CCompressionStream : virtual public CNcbiIos
{
public:
    // Stream processing direction
    enum EDirection {
        eRead,              // reading from stream
        eWrite,             // writing into stream
        eReadWrite          // eRead + eWrite
    };

    // 'ctors
    // If read/write stream processor is 0, that read/write operation
    // on this stream will be unsuccessful.
    CCompressionStream(CNcbiIos&                    stream,
                       CCompressionStreamProcessor* read_sp,
                       CCompressionStreamProcessor* write_sp);
    virtual ~CCompressionStream(void);

    // Finalize stream's compression/decompression process for read/write.
    // This function just calls a streambuf Finalize().
    virtual void Finalize(CCompressionStream::EDirection dir =
                          CCompressionStream::eReadWrite);
protected:
    CNcbiIos*                    m_Stream   ; // Underlying stream
    CCompressionStreambuf*       m_StreamBuf; // Stream buffer
    CCompressionStreamProcessor* m_Reader;    // Read processor
    CCompressionStreamProcessor* m_Writer;    // Write processor
};



//////////////////////////////////////////////////////////////////////////////
//
// I/O stream classes
//

class NCBI_XUTIL_EXPORT CCompressionIStream : public istream,
                                              public CCompressionStream
{
public:
    CCompressionIStream(CNcbiIos&                    stream,
                        CCompressionStreamProcessor* stream_processor)
        : istream(0), CCompressionStream(stream, stream_processor, 0)
    {}
};


class NCBI_XUTIL_EXPORT CCompressionOStream : public ostream,
                                              public CCompressionStream
{
public:
    CCompressionOStream(CNcbiIos&                    stream,
                        CCompressionStreamProcessor* stream_processor)
        : ostream(0), CCompressionStream(stream, 0, stream_processor)
    {}
};


class NCBI_XUTIL_EXPORT CCompressionIOStream : public iostream,
                                               public CCompressionStream
{
public:
    CCompressionIOStream(CNcbiIos&                    stream,
                         CCompressionStreamProcessor* read_sp,
                         CCompressionStreamProcessor* write_sp)
        : iostream(0), CCompressionStream(stream, read_sp, write_sp)
    {}
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStreamProcessor class
//
// Container class for storing a stream's processor read/write info.
//

class NCBI_XUTIL_EXPORT CCompressionStreamProcessor
{
public:
    // If to delete the used compression processor in the destructor
    enum EDeleteProcessor {
        eDelete,            // do      delete processor object 
        eNoDelete           // do  not delete processor object
    };

    // 'ctors
    CCompressionStreamProcessor(
        CCompressionProcessor* processor,
        EDeleteProcessor       need_delete  = eNoDelete,
        streamsize             in_bufsize   = kCompressionDefaultBufSize,
        streamsize             out_bufsize  = kCompressionDefaultBufSize
    );
    virtual ~CCompressionStreamProcessor(void);

private:
    CCompressionProcessor* m_Processor;	  // Compression/decompr processor
    CT_CHAR_TYPE*          m_InBuf;       // Buffer of unprocessed data
    streamsize             m_InBufSize;   // Unprocessed data buffer size
    CT_CHAR_TYPE*          m_OutBuf;      // Buffer of processed data
    streamsize             m_OutBufSize;  // Processed data buffer size 
    streamsize             m_LastOutAvail;// The size of last data received
                                          // from compression processor
    CT_CHAR_TYPE*          m_Begin;       // Begin and end of the pre/post
    CT_CHAR_TYPE*          m_End;         // processed data in the read/write
                                          // buffer
    EDeleteProcessor       m_NeedDelete;  // m_Processor auto-deleting flag
    CCompressionProcessor::EStatus
                           m_LastStatus;  // Last compressor status
    bool                   m_Finalized;   // True if a Finalize() already done
                                          // for compression processor  
    // Friend classes
    friend class CCompressionStreambuf;
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/06/17 15:49:32  ivanov
 * The second Compression API redesign. Replaced all standalone compression-
 * decompression I/O classes with unified CCompression[I/O]Stream classes.
 * Added CCompressionStreamProcessor base class. Some comments rearrangement.
 *
 * Revision 1.4  2003/06/04 12:42:01  ucko
 * Use our NcbiIos typedef for portability to old iostream implementations.
 *
 * Revision 1.3  2003/06/03 20:09:54  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.2  2003/04/11 19:57:25  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__STREAM__HPP */
