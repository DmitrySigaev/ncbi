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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    CImageIOJpeg -- interface class for reading/writing JPEG files
 */

#include "image_io_jpeg.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

#ifdef HAVE_LIBJPEG
//
//
// JPEG SUPPORT
//
//

#include <stdio.h>

// hack to get around duplicate definition of INT32
#ifdef NCBI_OS_MSWIN
#define XMD_H
#endif

// jpeglib include (not extern'ed already (!))
extern "C" {
#include <jpeglib.h>
}


BEGIN_NCBI_SCOPE

static const int sc_JpegBufLen = 4096;



static void s_JpegErrorHandler(j_common_ptr ptr)
{
    string msg("Error processing JPEG image: ");
    msg += ptr->err->jpeg_message_table[ptr->err->msg_code];
    if (ptr->is_decompressor) {
        NCBI_THROW(CImageException, eReadError, msg);
    } else {
        NCBI_THROW(CImageException, eWriteError, msg);
    }
}


static void s_JpegOutputHandler(j_common_ptr ptr)
{
    string msg("JPEG message: ");
    msg += ptr->err->jpeg_message_table[ptr->err->msg_code];
    LOG_POST(Warning << msg);
}


//
// specialized internal structs used for reading/writing from streams
//

struct SJpegInput {

    // the input data elements
    struct jpeg_source_mgr pub;

    // our buffer and stream
    CNcbiIstream* stream;
    JOCTET* buffer;
};


struct SJpegOutput {

    // the input data elements
    struct jpeg_destination_mgr pub;

    // our buffer and stream
    CNcbiOstream* stream;
    JOCTET* buffer;
};


//
// JPEG stream i/o handlers handlers
//

// initialize reading on the stream
static void s_JpegReadInit(j_decompress_ptr cinfo)
{
    struct SJpegInput* sptr = (SJpegInput*)cinfo->src;

    // set up our buffer for the first read
    sptr->pub.bytes_in_buffer = 0;
    sptr->pub.next_input_byte = sptr->buffer;
}


// grab data from the stream
static unsigned char s_JpegReadBuffer(j_decompress_ptr cinfo)
{
    struct SJpegInput* sptr = (SJpegInput*)cinfo->src;

    // read data from the stream
    sptr->stream->read(reinterpret_cast<char*>(sptr->buffer), sc_JpegBufLen);

    // reset our counts
    sptr->pub.bytes_in_buffer = sptr->stream->gcount();
    sptr->pub.next_input_byte = sptr->buffer;

    return TRUE;
}


// skip over some data in an input stream (rare operation)
static void s_JpegReadSkipData(j_decompress_ptr cinfo, long bytes)
{
    struct SJpegInput* sptr = (SJpegInput*)cinfo->src;

    if (bytes > 0) {
        while (bytes > sptr->pub.bytes_in_buffer) {
            bytes -= sptr->pub.bytes_in_buffer;
            s_JpegReadBuffer(cinfo);
        }

        sptr->pub.next_input_byte += (size_t) bytes;
        sptr->pub.bytes_in_buffer -= (size_t) bytes;
    }
}


// finalize reading in a stream (no-op)
static void s_JpegReadTerminate(j_decompress_ptr)
{
    // don't need to do anything here
}


// initialize writing to a stream
static void s_JpegWriteInit(j_compress_ptr cinfo)
{
    struct SJpegOutput* sptr = (SJpegOutput*)cinfo->dest;

    // set up our buffer for the first read
    sptr->pub.next_output_byte = sptr->buffer;
    sptr->pub.free_in_buffer   = sc_JpegBufLen;
}


// fill the input buffer
static unsigned char s_JpegWriteBuffer(j_compress_ptr cinfo)
{
    struct SJpegOutput* sptr = (SJpegOutput*)cinfo->dest;

    // read data from the stream
    sptr->stream->write(reinterpret_cast<const char*>(sptr->buffer),
                        sc_JpegBufLen);

    // reset our counts
    sptr->pub.next_output_byte = sptr->buffer;
    sptr->pub.free_in_buffer  = sc_JpegBufLen;

    return TRUE;
}


// finalize our JPEG output
static void s_JpegWriteTerminate(j_compress_ptr cinfo)
{
    // don't need to do anything here
    struct SJpegOutput* sptr = (SJpegOutput*)cinfo->dest;
    size_t datacount = sc_JpegBufLen - sptr->pub.free_in_buffer;

    // read data from the stream
    if (datacount > 0) {
        sptr->stream->write(reinterpret_cast<const char*>(sptr->buffer),
                            datacount);
    }
    sptr->stream->flush();
    if ( !*(sptr->stream) ) {
        NCBI_THROW(CImageException, eWriteError,
                   "Error writing to JPEG stream");
    }
}


//
// set up our read structure to handle stream sources
// this is provided in place of jpeg_stdio_src()
//
static void s_JpegReadSetup(j_decompress_ptr cinfo,
                            CNcbiIstream& istr, JOCTET* buffer)
{
    struct SJpegInput* sptr = NULL;
    if (cinfo->src == NULL) {
        cinfo->src =
            (struct jpeg_source_mgr*)malloc(sizeof(SJpegInput));
        sptr = (SJpegInput*)cinfo->src;
    }

    // allocate buffer and save our stream
    sptr->stream = &istr;
    sptr->buffer = buffer;

    // set our callbacks
    sptr->pub.init_source       = s_JpegReadInit;
    sptr->pub.fill_input_buffer = s_JpegReadBuffer;
    sptr->pub.skip_input_data   = s_JpegReadSkipData;
    sptr->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    sptr->pub.term_source       = s_JpegReadTerminate;

    // make sure the structure knows we're just starting now
    sptr->pub.bytes_in_buffer = 0;
    sptr->pub.next_input_byte = buffer;
}



//
// set up our read structure to handle stream sources
// this is provided in place of jpeg_stdio_src()
//
static void s_JpegWriteSetup(j_compress_ptr cinfo,
                             CNcbiOstream& ostr, JOCTET* buffer)
{
    struct SJpegOutput* sptr = NULL;
    if (cinfo->dest == NULL) {
        cinfo->dest =
            (struct jpeg_destination_mgr*)malloc(sizeof(SJpegOutput));
        sptr = (SJpegOutput*)cinfo->dest;
    }

    // allocate buffer and save our stream
    sptr->stream = &ostr;
    sptr->buffer = buffer;

    // set our callbacks
    sptr->pub.init_destination    = s_JpegWriteInit;
    sptr->pub.empty_output_buffer = s_JpegWriteBuffer;
    sptr->pub.term_destination    = s_JpegWriteTerminate;

    // make sure the structure knows we're just starting now
    sptr->pub.free_in_buffer = sc_JpegBufLen;
    sptr->pub.next_output_byte = buffer;
}



//
// ReadImage()
// read a JPEG format image into memory, returning the image itself
// This version reads the entire image
//
CImage* CImageIOJpeg::ReadImage(CNcbiIstream& istr)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    CRef<CImage> image;

    // standard libjpeg stuff
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    try {
        // open our file for reading
        // set up the standard error handler
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_decompress(&cinfo);

        // set up our standard stream processor
        s_JpegReadSetup(&cinfo, istr, buf_ptr);

        //jpeg_stdio_src(&cinfo, fp);
        jpeg_read_header(&cinfo, TRUE);

        // decompression parameters
        cinfo.dct_method = JDCT_FLOAT;
        jpeg_start_decompress(&cinfo);

        // allocate an image to hold our data
        image.Reset(new CImage(cinfo.output_width, cinfo.output_height,
                               cinfo.out_color_components));

        // we process the image 1 scanline at a time
        unsigned char *data[1];
        data[0] = image->SetData();
        size_t stride = image->GetWidth() * image->GetDepth();
        for (size_t i = 0;  i < image->GetHeight();  ++i) {
            jpeg_read_scanlines(&cinfo, data, 1);
            data[0] += stride;
        }

        // standard clean-up
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
    }
    catch (...) {

        // clean up our mess
        jpeg_destroy_decompress(&cinfo);

        throw;
    }
    return image.Release();
}


//
// ReadImage()
// read a JPEG format image into memory, returning the image itself
// This version reads only a subset of the image
//
CImage* CImageIOJpeg::ReadImage(CNcbiIstream& istr,
                                size_t x, size_t y, size_t w, size_t h)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    CRef<CImage> image;

    // standard libjpeg stuff
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    try {
        // open our file for reading
        // set up the standard error handler
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_decompress(&cinfo);

        // set up our standard stream processor
        s_JpegReadSetup(&cinfo, istr, buf_ptr);

        //jpeg_stdio_src(&cinfo, fp);
        jpeg_read_header(&cinfo, TRUE);

        // further validation: make sure we're actually on the image
        if (x >= cinfo.output_width  ||  y >= cinfo.output_height) {
            string msg("CImageIOJpeg::ReadImage(): invalid starting position: ");
            msg += NStr::IntToString(x);
            msg += ", ";
            msg += NStr::IntToString(y);
            NCBI_THROW(CImageException, eReadError, msg);
        }

        // clamp our width and height to the image size
        if (x + w >= cinfo.output_width) {
            w = cinfo.output_width - x;
            LOG_POST(Warning
                     << "CImageIOJpeg::ReadImage(): clamped width to " << w);
        }

        if (y + h >= cinfo.output_height) {
            h = cinfo.output_height - y;
            LOG_POST(Warning
                     << "CImageIOJpeg::ReadImage(): clamped height to " << h);
        }

        // decompression parameters
        cinfo.dct_method = JDCT_FLOAT;
        jpeg_start_decompress(&cinfo);


        // allocate an image to store the subimage's data
        CRef<CImage> image(new CImage(w, h, cinfo.out_color_components));

        // also allocate a single scanline
        // we plan to process the input file on a line-by-line basis
        vector<unsigned char> row(cinfo.output_width * image->GetDepth());
        unsigned char *data[1];
        data[0] = &row[0];

        size_t i;

        // start by skipping a number of scan lines
        for (i = 0;  i < y;  ++i) {
            jpeg_read_scanlines(&cinfo, data, 1);
        }

        // read the chunk of interest
        unsigned char* to_data = image->SetData();
        size_t to_stride  = image->GetWidth() * image->GetDepth();
        size_t offs       = x * image->GetDepth();
        size_t scan_width = w * image->GetDepth();
        for (;  i < y + h;  ++i) {
            jpeg_read_scanlines(&cinfo, data, 1);
            memcpy(to_data, data[0] + offs, scan_width);
            to_data += to_stride;
        }

        // standard clean-up
        jpeg_destroy_decompress(&cinfo);
    }
    catch (...) {

        // clean up our mess
        jpeg_destroy_decompress(&cinfo);

        throw;
    }
    return image.Release();
}


//
// WriteImage()
// write an image to a file in JPEG format
// This version writes the entire image
//
void CImageIOJpeg::WriteImage(const CImage& image, CNcbiOstream& ostr,
                              CImageIO::ECompress compress)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    try {
        // standard libjpeg setup
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_compress(&cinfo);
        //jpeg_stdio_dest(&cinfo, fp);
        s_JpegWriteSetup(&cinfo, ostr, buf_ptr);

        cinfo.image_width      = image.GetWidth();
        cinfo.image_height     = image.GetHeight();
        cinfo.input_components = image.GetDepth();
        cinfo.in_color_space   = JCS_RGB;
        jpeg_set_defaults(&cinfo);

        // set our compression quality
        int quality = 70;
        switch (compress) {
        case CImageIO::eCompress_None:
            quality = 100;
            break;
        case CImageIO::eCompress_Low:
            quality = 90;
            break;
        case CImageIO::eCompress_Medium:
            quality = 70;
            break;
        case CImageIO::eCompress_High:
            quality = 40;
            break;
        default:
            LOG_POST(Error << "unknown compression type: " << (int)compress);
            break;
        }
        jpeg_set_quality(&cinfo, quality, TRUE);

        // begin compression
        jpeg_start_compress(&cinfo, true);

        // process our data on a line-by-line basis
        JSAMPROW data[1];
        data[0] =
            const_cast<JSAMPLE*> (static_cast<const JSAMPLE*> (image.GetData()));
        for (size_t i = 0;  i < image.GetHeight();  ++i) {
            jpeg_write_scanlines(&cinfo, data, 1);
            data[0] += image.GetWidth() * image.GetDepth();
        }

        // standard clean-up
        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
    }
    catch (...) {
        jpeg_destroy_compress(&cinfo);
        throw;
    }
}


//
// WriteImage()
// write an image to a file in JPEG format
// This version writes only a subpart of the image
//
void CImageIOJpeg::WriteImage(const CImage& image, CNcbiOstream& ostr,
                              size_t x, size_t y, size_t w, size_t h,
                              CImageIO::ECompress compress)
{
    // validate our image position as inside the image we've been passed
    if (x >= image.GetWidth()  ||  y >= image.GetHeight()) {
        string msg("CImageIOJpeg::WriteImage(): invalid image position: ");
        msg += NStr::IntToString(x);
        msg += ", ";
        msg += NStr::IntToString(y);
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // clamp our width and height
    if (x + w >= image.GetWidth()) {
        w = image.GetWidth() - x;
        LOG_POST(Warning
                 << "CImageIOJpeg::WriteImage(): clamped width to " << w);
    }

    if (y + h >= image.GetHeight()) {
        h = image.GetHeight() - y;
        LOG_POST(Warning
                 << "CImageIOJpeg::WriteImage(): clamped height to " << h);
    }

    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    try {
        // standard libjpeg setup
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_compress(&cinfo);
        //jpeg_stdio_dest(&cinfo, fp);
        s_JpegWriteSetup(&cinfo, ostr, buf_ptr);

        cinfo.image_width      = w;
        cinfo.image_height     = h;
        cinfo.input_components = image.GetDepth();
        cinfo.in_color_space   = JCS_RGB;
        jpeg_set_defaults(&cinfo);

        // set our compression quality
        int quality = 70;
        switch (compress) {
        case CImageIO::eCompress_None:
            quality = 100;
            break;
        case CImageIO::eCompress_Low:
            quality = 90;
            break;
        case CImageIO::eCompress_Medium:
            quality = 70;
            break;
        case CImageIO::eCompress_High:
            quality = 40;
            break;
        default:
            LOG_POST(Error << "unknown compression type: " << (int)compress);
            break;
        }
        jpeg_set_quality(&cinfo, quality, TRUE);

        // begin compression
        jpeg_start_compress(&cinfo, true);

        // process our data on a line-by-line basis
        const unsigned char* data_start =
            image.GetData() + (y * image.GetWidth() + x) * image.GetDepth();
        size_t data_stride = image.GetWidth() * image.GetDepth();

        JSAMPROW data[1];
        data[0] = const_cast<JSAMPLE*> (static_cast<const JSAMPLE*> (data_start));
        for (size_t i = 0;  i < h;  ++i) {
            jpeg_write_scanlines(&cinfo, data, 1);
            data[0] += data_stride;
        }

        // standard clean-up
        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
    }
    catch (...) {
        jpeg_destroy_compress(&cinfo);
        throw;
    }
}


END_NCBI_SCOPE


#else // !HAVE_LIBJPEG

//
// JPEGLIB functionality not included - we stub out the various needed
// functions
//


BEGIN_NCBI_SCOPE


CImage* CImageIOJpeg::ReadImage(CNcbiIstream& file)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImage(): JPEG format not supported");
}


CImage* CImageIOJpeg::ReadImage(CNcbiIstream& file,
                                size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImage(): JPEG format not supported");
}


void CImageIOJpeg::WriteImage(const CImage& image, CNcbiOstream& file,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::WriteImage(): JPEG format not supported");
}


void CImageIOJpeg::WriteImage(const CImage& image, CNcbiOstream& file,
                              size_t, size_t, size_t, size_t,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::WriteImage(): JPEG format not supported");
}


END_NCBI_SCOPE


#endif  // HAVE_LIBJPEG


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/12/16 15:49:36  dicuccio
 * Large re-write of image handling.  Added improved error-handling and support
 * for streams-based i/o (via hooks into each client library).
 *
 * Revision 1.4  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.3  2003/10/02 15:37:33  ivanov
 * Get rid of compilation warnings
 *
 * Revision 1.2  2003/06/16 19:20:02  dicuccio
 * Added guard for libjpeg on Win32
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

