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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1999/04/15 21:59:58  vakatov
* [MSVC++]  Added "LIBCALLBACK" to the WriteAsn() proto
*
* Revision 1.3  1999/04/14 17:26:52  vasilche
* Fixed warning about mixing pointers to "C" and "C++" functions.
*
* Revision 1.2  1999/02/17 22:03:16  vasilche
* Assed AsnMemoryRead & AsnMemoryWrite.
* Pager now may return NULL for some components if it contains only one
* page.
*
* Revision 1.1  1999/01/28 15:11:09  vasilche
* Added new class CAsnWriteNode for displaying ASN.1 structure in HTML page.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/asnwrite.hpp>

BEGIN_NCBI_SCOPE

extern "C" {
    static Int2 LIBCALLBACK WriteAsn(Pointer data, CharPtr buffer, Uint2 size);
}

CAsnWriteNode::CAsnWriteNode(void)
    : m_Out(0)
{
}

CAsnWriteNode::~CAsnWriteNode(void)
{
    AsnIoClose(m_Out);
}

AsnIoPtr CAsnWriteNode::GetOut(void)
{
    AsnIoPtr out = m_Out;
    if ( !out ) {
        out = m_Out = AsnIoNew(ASNIO_TEXT | ASNIO_OUT, 0, this, 0, WriteAsn);
    }
    return out;
}

Int2 LIBCALLBACK WriteAsn(Pointer data, CharPtr buffer, Uint2 size)
{
    if ( !data || !buffer )
        return -1;

    static_cast<CAsnWriteNode*>(data)->AppendPlainText(string(buffer, size));
    return size;
}

void CAsnWriteNode::CreateSubNodes(void)
{
    AsnIoFlush(m_Out);
}

END_NCBI_SCOPE
