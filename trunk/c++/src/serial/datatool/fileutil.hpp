#ifndef FILEUTIL_HPP
#define FILEUTIL_HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Several file utility functions/classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1999/12/30 21:33:39  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.4  1999/12/28 18:55:57  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/21 17:44:19  vasilche
* Fixed compilation on SunPro C++
*
* Revision 1.2  1999/12/21 17:18:34  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.1  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <memory>

USING_NCBI_SCOPE;

class SourceFile
{
public:
    SourceFile(const string& name, bool binary = false);
    SourceFile(const string& name, const string& dir, bool binary = false);
    ~SourceFile(void);

    operator CNcbiIstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiIstream* m_StreamPtr;
    bool m_Open;

    bool x_Open(const string& name, bool binary);
};

class DestinationFile
{
public:
    DestinationFile(const string& name, bool binary = false);
    ~DestinationFile(void);

    operator CNcbiOstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiOstream* m_StreamPtr;
    bool m_Open;
};

enum EFileType {
    eFileTypeNone,
    eASNText,
    eASNBinary,
    eXMLText
};

struct FileInfo {
    FileInfo(void)
        : type(eFileTypeNone)
        { }
    FileInfo(const string& n, EFileType t)
        : name(n), type(t)
        { }

    operator bool(void) const
        { return !name.empty(); }
    operator const string&(void) const
        { return name; }
    string name;
    EFileType type;
};

class CDelayedOfstream : public CNcbiOstrstream
{
public:
    CDelayedOfstream(const char* fileName);
    virtual ~CDelayedOfstream(void);

    bool is_open(void) const
        {
            return !m_FileName.empty();
        }
    void open(const char* fileName);
    void close(void);

protected:
    bool equals(void);
    bool rewrite(void);

private:
    string m_FileName;
    auto_ptr<CNcbiIfstream> m_Istream;
    auto_ptr<CNcbiOfstream> m_Ostream;
};

// return combined dir and name, inserting if needed '/'
string Path(const string& dir, const string& name);

// return base name of file i.e. without dir and extension
string BaseName(const string& path);

// return dir name of file
string DirName(const string& path);

// return valid C name
string Identifier(const string& typeName, bool capitalize = true);

#endif
