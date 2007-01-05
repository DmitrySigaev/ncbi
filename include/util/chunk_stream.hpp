#ifndef UTIL___CHUNK_STREAM__HPP
#define UTIL___CHUNK_STREAM__HPP

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
 * Authors:
 *   Dmitry Kazimirov - Initial revision.
 *
 * File Description:
 *   Declarations of CChunkStreamReader and CChunkStreamWriter. These are
 *   classes for serialization and deserialization of chunks of binary data to
 *   and from sequences of bytes.
 *
 */

/// @file simple_cmd_serializer.hpp
/// This file contains declarations of classes CChunkStreamReader and
/// CChunkStreamWriter. The former serializes chunks of binary data to a byte
/// stream and the latter deserializes these chunks back from the stream.

#include <corelib/ncbimisc.hpp>

BEGIN_NCBI_SCOPE

/// Input stream format error exception for the CChunkStreamReader class.
/// @sa CChunkStreamReader
class NCBI_XUTIL_EXPORT CChunkStreamFormatException
{
// Construction
public:
    /// Initialize this exception object.
    /// @param offset
    ///   The offset at which a format error has occured.
    CChunkStreamFormatException(off_t offset);

// Attributes
public:
    /// Return error message associated with the exception.
    /// @return
    ///   A pointer to the buffer containing the error message.
    virtual const char* what() const;

    /// Return the offset in the input stream at which this error has
    /// occured.
    /// @return
    ///   Offset of the character that caused this error.
    off_t GetOffset() const;

// Implementation
private:
    off_t m_Offset;
};

inline CChunkStreamFormatException::CChunkStreamFormatException(off_t offset) :
    m_Offset(offset)
{
}

inline off_t CChunkStreamFormatException::GetOffset() const
{
    return m_Offset;
}

/// Class for reading series of chunks sent by a CChunkStreamWriter instance as
/// a stream of bytes.  Objects of this class parse the input byte stream and
/// notify the caller of parsing events.
/// @sa CChunkStreamWriter
class NCBI_XUTIL_EXPORT CChunkStreamReader
{
// Types
public:
    /// Enumeration of the input stream parsing events.
    enum EStreamParsingEvent {
        /// Notify that a part of a chunk has been read and can be
        /// fetched by a call to the GetChunkPart() method.
        eChunkPart,

        /// Notify that the last part of the chunk has been read. This event
        /// means that the chunk value can be assembled and interpreted now.
        /// Use the GetChunkPart() method to retrieve this final part of the
        /// chunk.
        eChunk,

        /// Notify that a control symbol has been read. Use the GetChunkPart()
        /// method to retrieve its value. Control symbols are always one
        /// character width so a call to GetChunkPartSize() will return 1.
        eControlSymbol,

        /// Notify that the end of the input buffer has been reached. New
        /// buffer processing can be started now by calling SetNewBuffer().
        eEndOfBuffer
    };

// Construction
public:
    /// Initialize the state machine of this object.
    CChunkStreamReader();

    /// Reinitialize this object.
    void Reset();

// Attributes
public:
    /// Set offset of the current character in the input stream to the
    /// specified value.
    /// @param offset
    ///   The new value for the current character offset.
    void SetOffset(off_t offset);

    /// Return the offset of the current character in the input stream.
    /// @return
    ///   The current character offset.
    off_t GetOffset() const;

// Operations
public:
    /// Start processing of the next chunk of data.
    /// @param buffer
    ///   A pointer to the next chunk of data from the stream.
    /// @param buffer_size The size of the above chunk in bytes.
    void SetNewBuffer(const char* buffer, size_t buffer_size);

    /// Parse the input buffer until a parsing event occurs.
    /// @return
    ///   A constant from the EStreamParsingEvent enumeration.
    /// @sa EStreamParsingEvent
    EStreamParsingEvent NextParsingEvent();

    /// Return a pointer to the buffer that contains a part of the chunk
    /// currently being read. Note that this buffer is not zero-terminated. Use
    /// the GetChunkPartSize() method to retrieve the length of this buffer.
    /// The returned value is only valid after a successful call to the
    /// NextParsingEvent() method, that is, when this call returned either of
    /// the following values: eChunkPart, eChunk, or eControlSymbol.
    ///
    /// @return
    ///   A pointer to the inside part of the buffer passed as an argument to
    ///   the SetNewBuffer() method.
    const char* GetChunkPart() const;

    /// Return the size of the buffer returned by the GetChunkPart() method.
    /// The returned value is only value after a successful call to the
    /// NextParsingEvent() method.
    /// @return
    ///   Size of the buffer returned by a call to the GetChunkPart() method.
    size_t GetChunkPartSize() const;

// Implementation
private:
    enum EStreamParsingState {
        eReadControlChars,
        eReadChunkLength,
        eReadChunk
    };

    const char* m_Buffer, *m_ChunkPart;
    size_t m_BufferSize, m_ChunkPartSize;
    off_t m_Offset;
    // Register for keeping intermediate values of chunk lengths.
    size_t m_LengthAcc;
    EStreamParsingState m_State;
    bool m_ChunkPartContinued;
};

inline CChunkStreamReader::CChunkStreamReader() :
    m_Offset(0), m_State(eReadControlChars)
{
}

inline void CChunkStreamReader::Reset()
{
    m_Offset = 0;
    m_State = eReadControlChars;
}

inline void CChunkStreamReader::SetOffset(off_t offset)
{
    m_Offset = offset;
}

inline off_t CChunkStreamReader::GetOffset() const
{
    return m_Offset;
}

inline void CChunkStreamReader::SetNewBuffer(const char* buffer,
    size_t buffer_size)
{
    m_Buffer = buffer;
    m_BufferSize = buffer_size;
}

inline const char* CChunkStreamReader::GetChunkPart() const
{
    return m_ChunkPart;
}

inline size_t CChunkStreamReader::GetChunkPartSize() const
{
    return m_ChunkPartSize;
}

/// Class that serializes series of chunks of data for sending over binary
/// streams.
class NCBI_XUTIL_EXPORT CChunkStreamWriter
{
// Construction
public:
    /// Initialize this object.
    /// @param buffer
    ///   A pointer to the output buffer.
    /// @param buffer_size
    ///   The size of the above buffer. This size cannot be less than
    ///   2.5 * sizeof(size_t).
    CChunkStreamWriter(char* buffer, size_t buffer_size);

    /// Initialize this object.
    /// @param buffer
    ///   A pointer to the output buffer.
    /// @param buffer_size
    ///   The size of the above buffer. This size cannot be less than
    ///   2.5 * sizeof(size_t).
    /// @param max_buffer_size
    ///   The maximum size of the buffer that a call to GetOutputBuffer() will
    ///   ever return. If omitted, considered to be equal to buffer_size.
    CChunkStreamWriter(char* buffer, size_t buffer_size,
        size_t max_buffer_size);

    /// Reinitialize this object with new parameters. This method allows this
    /// object to be reused.
    /// @param buffer
    ///   A pointer to the new output buffer.
    /// @param buffer_size
    ///   The size of the above buffer. This size cannot be less than
    ///   2.5 * sizeof(size_t).
    void Reset(char* buffer, size_t buffer_size);

    /// Reinitialize this object with new parameters. This method allows this
    /// object to be reused.
    /// @param buffer
    ///   A pointer to the new output buffer.
    /// @param buffer_size
    ///   The size of the above buffer. This size cannot be less than
    ///   2.5 * sizeof(size_t).
    /// @param max_buffer_size
    ///   The maximum size of the buffer that a call to GetOutputBuffer() will
    ///   ever return. If omitted, considered to be equal to buffer_size.
    void Reset(char* buffer, size_t buffer_size, size_t max_buffer_size);

// Operations
public:
    /// Send a control symbol over the output buffer. Control symbol can be any
    /// byte character except digits. It is the caller's responsibility to
    /// verify that the symbol argument passed to this method satisfies this
    /// requirement. This method will assert if this requirement is violated.
    /// @param symbol
    ///   The byte to be sent. Can be any character except digits (0...9).
    /// @return
    ///   True if pushing "symbol" to the output buffer did not result in the
    ///   buffer overflow condition; false - when the output buffer is full and
    ///   must be flushed with a call to GetOutputBuffer() before
    ///   SendControlSymbol() or SendChunk() can be called again.
    bool SendControlSymbol(char symbol);

    /// Send a chunk of data to the output buffer.
    /// @param chunk
    ///   A pointer to the memory area containing the chunk of data to be sent.
    /// @param chunk_length
    ///   The length of the chunk of data to be sent, in bytes.
    /// @param to_be_continued
    ///   Indicate that a chunk continuation mark must be inserted in the
    ///   stream. The CChunkStreamReader class will fire a eChunkPart event
    ///   after reading the entire chunk of data pointed to by the chunk
    ///   parameter.
    /// @return
    ///   True if after the specified chunk has been fit in the output buffer
    ///   completely and there is still some space available in the buffer;
    ///   false - when the buffer has been filled and needs to be flushed by a
    ///   call to GetOutputBuffer() before the next chunk of data or control
    ///   symbol can be sent.
    bool SendChunk(const char* chunk, size_t chunk_length, bool to_be_continued);

    /// Return data to be sent over the output stream and extend internal
    /// pointers to the next buffer. This method can be called at any time,
    /// regardless of the return value of previously called SendChunk() or
    /// SendControlSymbol(). This method must also be explicitly called in
    /// the end of session for flushing the buffer when it's not filled up.
    /// @param output_buffer
    ///   A pointer to the variable for storing the buffer address into.
    /// @param output_buffer_size
    ///   A pointer to the variable of type size_t for storing the size of
    ///   the above buffer.
    void GetOutputBuffer(const char** output_buffer, size_t* output_buffer_size);

    /// @return
    ///   True if more data must be flushed before a call to SendChunk() or
    ///   SendControlSymbol() can be made.
    bool NextOutputBuffer();

// Implementation
private:
    char* m_Buffer;
    const char *m_OutputBuffer;
    const char *m_ChunkPart;
    size_t m_BufferSize;
    size_t m_OutputBufferSize;
    size_t m_ChunkPartSize;
    size_t m_MaxBufferSize;
    size_t m_InternalBufferSize;

    // The buffer for keeping control data that did not fit in the output
    // buffer. This buffer is primarily used for the number-to-string
    // conversion. Size factor 2.5 in the declaration below means
    // the following:
    //
    // An unsigned binary integer of size S bytes may require up to
    // L = ceil(S * log10(256)) digit positions when written in decimal
    // notation. A convenient approximation of log10(256) is 2.5:
    // L = ceil((S * 5) / 2). In case if S = sizeof(size_t), which is
    // always an even number: L = S / 2 * 5
    //
    // Additionally, one byte is reserved for either space or plus character
    // that follows the chunk size.
    char m_InternalBuffer[(sizeof(size_t) >> 1) * 5 + 1];
};

inline CChunkStreamWriter::CChunkStreamWriter(char* buffer, size_t buffer_size)
{
    Reset(buffer, buffer_size, buffer_size);
}

inline CChunkStreamWriter::CChunkStreamWriter(char* buffer, size_t buffer_size,
    size_t max_buffer_size)
{
    Reset(buffer, buffer_size, max_buffer_size);
}

inline void CChunkStreamWriter::Reset(char* buffer, size_t buffer_size)
{
    Reset(buffer, buffer_size, buffer_size);
}

inline void CChunkStreamWriter::GetOutputBuffer(const char** output_buffer,
    size_t* output_buffer_size)
{
    *output_buffer = m_OutputBuffer;
    *output_buffer_size = m_OutputBufferSize;
}

END_NCBI_SCOPE

#endif // !defined(UTIL___CHUNK_STREAM__HPP)
