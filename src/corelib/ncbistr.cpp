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
*   Eugene Vasilchenko
*
* File Description:
*   Some helper functions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/12/15 15:43:22  vasilche
* Added utilities to convert string <> int.
*
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <limits>
#include <errno.h>

BEGIN_NCBI_SCOPE

int StringToInt(const string& str)
{
    ::errno = 0;
    char* error = 0;
    long value = ::strtol(str.c_str(), &error, 0);
    if ( errno || error && *error ||
         value < numeric_limits<int>::min() ||
         value > numeric_limits<int>::max() )
        throw runtime_error("bad number");
    return value;
}

unsigned int StringToUInt(const string& str)
{
    ::errno = 0;
    char* error = 0;
    long long value = ::strtoll(str.c_str(), &error, 0);
    if ( errno || error && *error ||
         value < numeric_limits<unsigned int>::min() ||
         value > numeric_limits<unsigned int>::max() )
        throw runtime_error("bad number");
    return value;
}

double StringToDouble(const string& str)
{
    ::errno = 0;
    char* error = 0;
    double value = ::strtod(str.c_str(), &error);
    if ( errno || error && *error )
        throw runtime_error("bad number");
    return value;
}

string IntToString(int value)
{
    char buffer[64];
    ::snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}

string UIntToString(unsigned int value)
{
    char buffer[64];
    ::snprintf(buffer, sizeof(buffer), "%u", value);
    return buffer;
}

string DoubleToString(double value)
{
    char buffer[64];
    ::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

END_NCBI_SCOPE


