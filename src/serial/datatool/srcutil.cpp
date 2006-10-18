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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/datatool/srcutil.hpp>

BEGIN_NCBI_SCOPE

string Identifier(const string& typeName, bool capitalize)
{
    string s;
    s.reserve(typeName.size());
    string::const_iterator i = typeName.begin();
    if ( i != typeName.end() ) {
        s += capitalize? toupper((unsigned char)(*i)): *i;
        while ( ++i != typeName.end() ) {
            char c = *i;
            if ( c == '-' || c == '.' )
                c = '_';
            s += c;
        }
    }
    return s;
}

string Tabbed(const string& code, const char* tab)
{
    string out;
    SIZE_TYPE size = code.size();
    if ( size != 0 ) {
        if ( !tab )
            tab = "    ";
        const char* ptr = code.data();
        while ( size > 0 ) {
            out += tab;
            const char* endl =
                reinterpret_cast<const char*>(memchr(ptr, '\n', size));
            if ( !endl ) { // no more '\n'
                out.append(ptr, ptr + size);
                out += '\n';
                break;
            }
            ++endl; // skip '\n'
            size_t lineSize = endl - ptr;
            out.append(ptr, ptr + lineSize);
            ptr = endl;
            size -= lineSize;
        }
    }
    return out;
}

CNcbiOstream& WriteTabbed(CNcbiOstream& out, const string& code,
                          const char* tab)
{
    size_t size = code.size();
    if ( size != 0 ) {
        if ( !tab )
            tab = "    ";
        const char* ptr = code.data();
        while ( size > 0 ) {
            out << tab;
            const char* endl =
                reinterpret_cast<const char*>(memchr(ptr, '\n', size));
            if ( !endl ) { // no more '\n'
                out.write(ptr, size) << '\n';
                break;
            }
            ++endl; // skip '\n'
            size_t lineSize = endl - ptr;
            out.write(ptr, lineSize);
            ptr = endl;
            size -= lineSize;
        }
    }
    return out;
}

CNcbiOstream& PrintASNNewLine(CNcbiOstream& out, int indent)
{
    out <<
        '\n';
    for ( int i = 0; i < indent; ++i )
        out << "  ";
    return out;
}

END_NCBI_SCOPE
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2006/10/18 13:11:06  gouriano
* Moved Log to bottom
*
* Revision 1.4  2005/06/03 17:05:33  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.3  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2001/05/17 15:07:12  lavr
* Typos corrected
*
* Revision 1.1  2000/11/29 17:42:45  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* ===========================================================================
*/
