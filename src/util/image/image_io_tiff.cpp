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
 *    CImageIOTiff -- interface class for reading/writing TIFF files
 */

#include "image_io_tiff.hpp"
#include <corelib/ncbifile.hpp>
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

#ifdef HAVE_LIBTIFF

//
//
// LIBTIFF functions
//
//

#include <tiffio.h>

BEGIN_NCBI_SCOPE


//
// TIFFlib error / warning handlers
//
static void s_TiffReadErrorHandler(const char* module, const char* fmt,
                                   va_list args)
{
    string msg("Error reading TIFF image: ");
    msg += module;
    msg += ": ";
    msg += NStr::FormatVarargs(fmt, args);
    NCBI_THROW(CImageException, eReadError, msg);
}


static void s_TiffWriteErrorHandler(const char* module, const char* fmt,
                                    va_list args)
{
    string msg("Error writing TIFF image: ");
    msg += module;
    msg += ": ";
    msg += NStr::FormatVarargs(fmt, args);
    NCBI_THROW(CImageException, eWriteError, msg);
}


static void s_TiffWarningHandler(const char* module, const char* fmt,
                                 va_list args)
{
    string msg = module;
    msg += ": ";
    msg += NStr::FormatVarargs(fmt, args);
    LOG_POST(Warning << "Warning reading TIFF image: " << msg);
}


//
// TIFFLib i/o handlers
//
static tsize_t s_TIFFReadHandler(thandle_t handle, tdata_t data, tsize_t len)
{
    CNcbiIfstream* istr = reinterpret_cast<CNcbiIfstream*>(handle);
    if (istr) {
        istr->read(reinterpret_cast<char*>(data), len);
        return istr->gcount();
    }
    return 0;
}

static tsize_t s_TIFFWriteHandler(thandle_t handle, tdata_t data, tsize_t len)
{
    std::ofstream* ostr = reinterpret_cast<std::ofstream*>(handle);
    if (ostr) {
        ostr->write(reinterpret_cast<char*>(data), len);
        if (*ostr) {
            return len;
        }
    }
    return -1;
}

// dummy i/o handler.  TIFFLib tries to read from files opened for writing
static tsize_t s_TIFFDummyIOHandler(thandle_t, tdata_t, tsize_t)
{
    return -1;
}

static toff_t s_TIFFSeekHandler(thandle_t handle, toff_t offset,
                                int whence)
{
    CNcbiIfstream* istr = reinterpret_cast<CNcbiIfstream*>(handle);
    if (istr) {
        switch (whence) {
        case SEEK_SET:
            istr->seekg(offset, ios::beg);
            break;

        case SEEK_CUR:
            istr->seekg(offset, ios::cur);
            break;

        case SEEK_END:
            istr->seekg(offset, ios::end);
            break;
        }

        return istr->tellg();
    }
    return (toff_t)-1;
}

static int s_TIFFCloseHandler(thandle_t handle)
{
    std::ofstream* ostr = reinterpret_cast<std::ofstream*>(handle);
    if (ostr) {
        ostr->flush();
        if ( !*ostr ) {
            return -1;
        }
    }
    return 0;
}

static toff_t s_TIFFSizeHandler(thandle_t handle)
{
    toff_t offs = 0;
    CNcbiIfstream* istr = reinterpret_cast<CNcbiIfstream*>(handle);
    if (istr) {
        size_t curr_pos = istr->tellg();
        istr->seekg(0, ios::end);
        offs = istr->tellg();
        istr->seekg(curr_pos, ios::beg);
    }
    return offs;
}

static int s_TIFFMapFileHandler(thandle_t, tdata_t*, toff_t*)
{
    return -1;
}

static void s_TIFFUnmapFileHandler(thandle_t, tdata_t, toff_t)
{
}



//
// ReadImage()
// read an image in TIFF format.  This uses a fairly inefficient read, in that
// the data, once read, must be copied into the actual image.  This is largely
// the result of a reliance on a particular function (TIFFReadRGBAImage()); the
// implementation details may change at a future date to use a more efficient
// read mechanism
//
CImage* CImageIOTiff::ReadImage(CNcbiIstream& istr)
{
    TIFF*            tiff             = NULL;
    uint32*          raster           = NULL;
    TIFFErrorHandler old_err_handler  = NULL;
    TIFFErrorHandler old_warn_handler = NULL;
    CRef<CImage> image;
    try {

        old_err_handler  = TIFFSetErrorHandler(&s_TiffReadErrorHandler);
        old_warn_handler = TIFFSetWarningHandler(&s_TiffWarningHandler);

        // open our file
        tiff = TIFFClientOpen("", "rm",
                              reinterpret_cast<thandle_t>(&istr),
                              s_TIFFReadHandler,
                              s_TIFFDummyIOHandler,
                              s_TIFFSeekHandler,
                              s_TIFFCloseHandler,
                              s_TIFFSizeHandler,
                              s_TIFFMapFileHandler,
                              s_TIFFUnmapFileHandler);
        if ( !tiff ) {
            NCBI_THROW(CImageException, eReadError,
                       "CImageIOTiff::ReadImage(): error opening file ");
        }

        // extract the size parameters
        int width = 0;
        int height = 0;
        TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH,  &width);
        TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);

        // allocate a temporary buffer for the image
        raster = (uint32*)_TIFFmalloc(width * height * sizeof(uint32));
        if ( !TIFFReadRGBAImage(tiff, width, height, raster, 1) ) {
            _TIFFfree(raster);

            NCBI_THROW(CImageException, eReadError,
                       "CImageIOTiff::ReadImage(): error reading file");
        }

        // now we need to copy this data and pack it appropriately
        image = new CImage(width, height, 4);
        unsigned char* data = image->SetData();
        for (int j = 0;  j < height;  ++j) {
            for (int i = 0;  i < width;  ++i) {
                size_t idx = j * width + i;

                // TIFFReadRGBAImage(0 returns data in ABGR image, packed as a
                // 32-bit value, so we need to pick this apart here
                uint32 pixel = raster[idx];
                data[4 * idx + 0] = (unsigned char)((pixel & 0x000000ff));
                data[4 * idx + 1] = (unsigned char)((pixel & 0x0000ff00) >> 8);
                data[4 * idx + 2] = (unsigned char)((pixel & 0x00ff0000) >> 16);
                data[4 * idx + 3] = (unsigned char)((pixel & 0xff000000) >> 24);
            }
        }

        // clean-up
        _TIFFfree(raster);
        TIFFClose(tiff);
    }
    catch (...) {
        if (raster) {
            _TIFFfree(raster);
            raster = NULL;
        }

        if (tiff) {
            TIFFClose(tiff);
            tiff = NULL;
        }

        TIFFSetErrorHandler(old_err_handler);
        TIFFSetWarningHandler(old_warn_handler);

        // throw to a higher level
        throw;
    }

    TIFFSetErrorHandler(old_err_handler);
    TIFFSetWarningHandler(old_warn_handler);
    return image.Release();
}


//
// ReadImage()
// read a sub-image from a file.  We use a brain-dead implementation that reads
// the whole image in, then subsets it.  This may change in the future.
//
CImage* CImageIOTiff::ReadImage(CNcbiIstream& istr,
                                size_t x, size_t y, size_t w, size_t h)
{
    CRef<CImage> image(ReadImage(istr));
    return image->GetSubImage(x, y, w, h);
}


//
// WriteImage()
// write an image to a file in TIFF format
//
void CImageIOTiff::WriteImage(const CImage& image, CNcbiOstream& ostr,
                              CImageIO::ECompress)
{
    TIFF* tiff = NULL;
    TIFFErrorHandler old_err_handler = NULL;
    TIFFErrorHandler old_warn_handler = NULL;

    try {
        if ( !image.GetData() ) {
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOTiff::WriteImage(): cannot write empty image");
        }

        old_err_handler  = TIFFSetErrorHandler(&s_TiffWriteErrorHandler);
        old_warn_handler = TIFFSetWarningHandler(&s_TiffWarningHandler);

        // open our file
        //ostr.open(file.c_str(), ios::out|ios::binary);
        tiff = TIFFClientOpen("", "wm",
                              reinterpret_cast<thandle_t>(&ostr),
                              s_TIFFDummyIOHandler,
                              s_TIFFWriteHandler,
                              s_TIFFSeekHandler,
                              s_TIFFCloseHandler,
                              s_TIFFSizeHandler,
                              s_TIFFMapFileHandler,
                              s_TIFFUnmapFileHandler);
        if ( !tiff ) {
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOTiff::WriteImage(): cannot write file");
        }

        // set a bunch of standard fields, defining our image dimensions
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH,        image.GetWidth());
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH,       image.GetHeight());
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE,     8);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL,   image.GetDepth());
        TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP,      image.GetHeight());

        // TIFF options
        TIFFSetField(tiff, TIFFTAG_COMPRESSION,   COMPRESSION_DEFLATE);
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC,   PHOTOMETRIC_RGB);
        TIFFSetField(tiff, TIFFTAG_FILLORDER,     FILLORDER_MSB2LSB);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG,  PLANARCONFIG_CONTIG);

        // write our information
        TIFFWriteEncodedStrip(tiff, 0,
                              const_cast<void*>
                              (reinterpret_cast<const void*> (image.GetData())),
                              image.GetWidth() * image.GetHeight() * image.GetDepth());
        TIFFClose(tiff);

        TIFFSetErrorHandler(old_err_handler);
        TIFFSetWarningHandler(old_warn_handler);
    }
    catch (...) {

        // close our file and wipe it from the system
        if (tiff) {
            TIFFClose(tiff);
            tiff = NULL;
        }

        // restore the standard error handlers
        TIFFSetErrorHandler  (old_err_handler);
        TIFFSetWarningHandler(old_warn_handler);

        throw;
    }
}


//
// WriteImage()
// write a sub-image to a file in TIFF format.  This uses a brain-dead
// implementation in that we subset the image first, then write the sub-image
// out.
//
void CImageIOTiff::WriteImage(const CImage& image, CNcbiOstream& ostr,
                              size_t x, size_t y, size_t w, size_t h,
                              CImageIO::ECompress compress)
{
    CRef<CImage> subimage(image.GetSubImage(x, y, w, h));
    WriteImage(*subimage, ostr, compress);
}


END_NCBI_SCOPE

#else // !HAVE_LIBTIFF

//
// LIBTIFF functionality not included - we stub out the various needed
// functions
//


BEGIN_NCBI_SCOPE


CImage* CImageIOTiff::ReadImage(CNcbiIstream&)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOTiff::ReadImage(): TIFF format not supported");
}


CImage* CImageIOTiff::ReadImage(CNcbiIstream&,
                                size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOTiff::ReadImage(): TIFF format not supported");
}


void CImageIOTiff::WriteImage(const CImage&, CNcbiOstream&,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOTiff::WriteImage(): TIFF format not supported");
}


void CImageIOTiff::WriteImage(const CImage&, CNcbiOstream&,
                              size_t, size_t, size_t, size_t,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOTiff::WriteImage(): TIFF format not supported");
}


END_NCBI_SCOPE


#endif  // HAVE_LIBTIFF

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/12/16 16:16:55  dicuccio
 * Fixed compiler warnings
 *
 * Revision 1.4  2003/12/16 15:49:37  dicuccio
 * Large re-write of image handling.  Added improved error-handling and support
 * for streams-based i/o (via hooks into each client library).
 *
 * Revision 1.3  2003/12/12 17:49:04  dicuccio
 * Intercept libtiff error messages and translate them into LOG_POST()/exception
 * where appropriate
 *
 * Revision 1.2  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

