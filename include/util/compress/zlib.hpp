#ifndef UTIL_COMPRESS__ZLIB__HPP
#define UTIL_COMPRESS__ZLIB__HPP

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
 * File Description:  ZLib Compression API
 *
 * NOTE: The zlib documentation can be found here: 
 *       http://zlib.org   or   http://www.gzip.org/zlib/manual.html
 */

#include <util/compress/stream.hpp>
#include <util/compress/zlib/zlib.h>
#include <util/compress/zlib/zutil.h>


/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Special compressor's parameters (description from zlib docs)
//        
// <window_bits>
//    This parameter is the base two logarithm of the window size
//    (the size of the history buffer). It should be in the range 8..15 for
//    this version of the library. Larger values of this parameter result
//    in better compression at the expense of memory usage. 
//
// <mem_level> 
//    The "mem_level" parameter specifies how much memory should be
//    allocated for the internal compression state. mem_level=1 uses minimum
//    memory but is slow and reduces compression ratio; mem_level=9 uses
//    maximum memory for optimal speed. The default value is 8. See zconf.h
//    for total memory usage as a function of windowBits and memLevel.
//
// <strategy> 
//    The strategy parameter is used to tune the compression algorithm.
//    Use the value Z_DEFAULT_STRATEGY for normal data, Z_FILTERED for data
//    produced by a filter (or predictor), or Z_HUFFMAN_ONLY to force
//    Huffman encoding only (no string match). Filtered data consists mostly
//    of small values with a somewhat random distribution. In this case,
//    the compression algorithm is tuned to compress them better. The effect
//    of Z_FILTERED is to force more Huffman coding and less string matching;
//    it is somewhat intermediate between Z_DEFAULT and Z_HUFFMAN_ONLY.
//    The strategy parameter only affects the compression ratio but not the
//    correctness of the compressed output even if it is not set appropriately.



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionZip class
//

class NCBI_XUTIL_EXPORT CZipCompression : public CCompression 
{
public:
    // 'ctors
    CZipCompression(
        ELevel level    = eLevel_Default,
        int window_bits = MAX_WBITS,             // [8..15]
        int mem_level   = DEF_MEM_LEVEL,         // [1..9] 
        int strategy    = Z_DEFAULT_STRATEGY     // [0,1]
    );
    virtual ~CZipCompression(void);

    // Returns default compression level for a compression algorithm
    virtual ELevel GetDefaultLevel(void) const
        { return ELevel(Z_DEFAULT_COMPRESSION); };

    //
    // Utility functions 
    //
    // Note:
    //    The CompressBuffer/DecompressBuffer do not use extended options
    //    passed in constructor like <window_bits>, <mem_level> and
    //    <strategy>; because the native ZLib compress2/uncompress
    //    functions use primitive initialization methods without a such
    //    options. 
  

    // (De)compress the source buffer into the destination buffer.
    // Return TRUE if operation was succesfully or FALSE otherwise.
    // Altogether, the total size of the destination buffer must be little
    // more then size of the source buffer (at least 0.1% larger + 12 bytes).
    virtual bool CompressBuffer(
        const void* src_buf, unsigned int  src_len,
        void*       dst_buf, unsigned int  dst_size,
        /* out */            unsigned int* dst_len
    );
    virtual bool DecompressBuffer(
        const void* src_buf, unsigned int  src_len,
        void*       dst_buf, unsigned int  dst_size,
        /* out */            unsigned int* dst_len
    );

    // (De)compress file "src_file" and put result to file with name "dst_file".
    // Return TRUE on success, FALSE on error.
    virtual bool CompressFile(
        const string& src_file,
        const string& dst_file,
        size_t        buf_size = kCompressionDefaultBufSize
    );
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        buf_size = kCompressionDefaultBufSize
    );
    
protected:
    // Format string with last error description
    string FormatErrorMessage(string where, bool use_stream_data = true) const;

protected:
    z_stream  m_Stream;         // Compressor stream
    int       m_WindowBits;     // The base two logarithm of the window size
                                // (the size of the history buffer). 
    int       m_MemLevel;       // The allocation memory level for the
                                // internal compression state
    int       m_Strategy;       // Parameter to tune the compression algorithm
};

 

//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressionFile class
//

// Note, Read() copies data from the compressed file in chunks of size
// BZ_MAX_UNUSED bytes before decompressing it. If the file contains more
// bytes than strictly needed to reach the logical end-of-stream, Read()
// will almost certainly read some of the trailing data before signalling of
// sequence end.
//
class NCBI_XUTIL_EXPORT CZipCompressionFile : public CZipCompression,
                                              public CCompressionFile
{
public:
    // 'ctors (for a special parameters description see CZipCompression)
    // Throw exception CCompressionException::eCompressionFile on error.
    CZipCompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level       = eLevel_Default,
        int           window_bits = MAX_WBITS,
        int           mem_level   = DEF_MEM_LEVEL,
        int           strategy    = Z_DEFAULT_STRATEGY
    );
    CZipCompressionFile(
        ELevel        level       = eLevel_Default,
        int           window_bits = MAX_WBITS,
        int           mem_level   = DEF_MEM_LEVEL,
        int           strategy    = Z_DEFAULT_STRATEGY
    );
    ~CZipCompressionFile(void);

    // Opens a gzip (.gz) file for reading or writing.
    // This function can be used to read a file which is not in gzip format;
    // in this case Read() will directly read from the file without
    // decompression. 
    // Return TRUE if file was opened succesfully or FALSE otherwise.
    virtual bool Open(const string& file_name, EMode mode);

    // Read up to "len" uncompressed bytes from the compressed file "file"
    // into the buffer "buf". If the input file was not in gzip format,
    // gzread copies the given number of bytes into the buffer. 
    // Return the number of bytes actually read
    // (0 for end of file, -1 for error).
    // The number of really readed bytes can be less than requested.
    virtual int Read(void* buf, int len);

    // Writes the given number of uncompressed bytes into the compressed file.
    // Return the number of bytes actually written or -1 for error.
    virtual int Write(const void* buf, int len);

    // Flushes all pending output if necessary, closes the compressed file.
    // Return TRUE on success, FALSE on error.
    virtual bool Close(void);
};



//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressor class
//

class NCBI_XUTIL_EXPORT CZipCompressor : public CZipCompression,
                                         public CCompressionProcessor
{
public:
    // 'ctors
    CZipCompressor(
        ELevel level       = eLevel_Default,
        int    window_bits = MAX_WBITS,
        int    mem_level   = DEF_MEM_LEVEL,
        int    strategy    = Z_DEFAULT_STRATEGY
    );
    virtual ~CZipCompressor(void);

protected:
    virtual EStatus Init   (void); 
    virtual EStatus Process(const char* in_buf,  unsigned long  in_len,
                            char*       out_buf, unsigned long  out_size,
                            /* out */            unsigned long* in_avail,
                            /* out */            unsigned long* out_avail);
    virtual EStatus Flush  (char*       out_buf, unsigned long  out_size,
                            /* out */            unsigned long* out_avail);
    virtual EStatus Finish (char*       out_buf, unsigned long  out_size,
                            /* out */            unsigned long* out_avail);
    virtual EStatus End    (void);
};



//////////////////////////////////////////////////////////////////////////////
//
// CZipDecompressor class
//

class NCBI_XUTIL_EXPORT CZipDecompressor : public CZipCompression,
                                           public CCompressionProcessor
{
public:
    // 'ctors
    CZipDecompressor(int window_bits = MAX_WBITS);
    virtual ~CZipDecompressor(void);

protected:
    virtual EStatus Init   (void); 
    virtual EStatus Process(const char* in_buf,  unsigned long  in_len,
                            char*       out_buf, unsigned long  out_size,
                            /* out */            unsigned long* in_avail,
                            /* out */            unsigned long* out_avail);
    virtual EStatus Flush  (char*       out_buf, unsigned long  out_size,
                            /* out */            unsigned long* out_avail);
    virtual EStatus Finish (char*       out_buf, unsigned long  out_size,
                            /* out */            unsigned long* out_avail);
    virtual EStatus End    (void);
};



//////////////////////////////////////////////////////////////////////////////
//
// Compression/decompression stream processors (for details see "stream.hpp")
//

class NCBI_XUTIL_EXPORT CZipStreamCompressor
    : public CCompressionStreamProcessor
{
public:
    CZipStreamCompressor(
        CCompression::ELevel level       = CCompression::eLevel_Default,
        streamsize           in_bufsize  = kCompressionDefaultBufSize,
        streamsize           out_bufsize = kCompressionDefaultBufSize,
        int                  window_bits = MAX_WBITS,
        int                  mem_level   = DEF_MEM_LEVEL,
        int                  strategy    = Z_DEFAULT_STRATEGY)

        : CCompressionStreamProcessor(
              new CZipCompressor(level, window_bits, mem_level, strategy),
              eDelete, in_bufsize, out_bufsize)
    {}
};


class NCBI_XUTIL_EXPORT CZipStreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    CZipStreamDecompressor(
        streamsize  in_bufsize   = kCompressionDefaultBufSize,
        streamsize  out_bufsize  = kCompressionDefaultBufSize,
        int         window_bits  = MAX_WBITS)

        : CCompressionStreamProcessor(new CZipDecompressor(window_bits),
                                      eDelete, in_bufsize, out_bufsize)
    {}
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/07/15 15:45:45  ivanov
 * Improved error diagnostics
 *
 * Revision 1.4  2003/07/10 16:22:27  ivanov
 * Added buffer size parameter into [De]CompressFile() functions.
 * Cosmetic changes.
 *
 * Revision 1.3  2003/06/17 15:48:59  ivanov
 * Removed all standalone compression/decompression I/O classes.
 * Added CZipStream[De]compressor classes. Now all zlib-based I/O stream
 * classes can be constructed using unified CCompression[I/O]Stream
 * (see stream.hpp) and CZipStream[De]compressor classes.
 *
 * Revision 1.2  2003/06/03 20:09:54  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__ZLIB__HPP */
