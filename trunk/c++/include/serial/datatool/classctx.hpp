#ifndef CLASSCTX_HPP
#define CLASSCTX_HPP

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
*   Class code generator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/02/01 21:46:15  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.15  1999/12/29 16:01:50  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.14  1999/11/19 15:48:09  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.13  1999/11/18 17:13:06  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.12  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <set>

USING_NCBI_SCOPE;

class CDataType;
class CChoiceDataType;
class CFileCode;

class CClassContext
{
public:
    virtual ~CClassContext(void);

    typedef set<string> TIncludes;

    virtual string GetMethodPrefix(void) const = 0;
    virtual TIncludes& HPPIncludes(void) = 0;
    virtual TIncludes& CPPIncludes(void) = 0;
    virtual void AddForwardDeclaration(const string& className,
                                       const string& namespaceName) = 0;
    virtual void AddHPPCode(const CNcbiOstrstream& code) = 0;
    virtual void AddCPPCode(const CNcbiOstrstream& code) = 0;
};

#endif
