#if defined(HTML___HTMLHELPER__HPP)  &&  !defined(HTML___HTMLHELPER__INL)
#define HTML___HTMLHELPER__INL

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
 * File Description:   HTML helper classes functions inline implementation
 *
 */


inline CIDs::CIDs(void)
{
    return;
}


inline CIDs::~CIDs(void)
{
    return;
}


inline void CIDs::AddID(int id)
{
    push_back(id);
}


inline bool CIDs::ExtractID(int id)
{
    iterator i = find(begin(), end(), id);
    if ( i != end() ) {
        erase(i);
        return true;
    }
    return false;
}


inline size_t CIDs::CountIDs(void) const
{
    return size();
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.5  2000/10/13 19:54:52  vasilche
 * Fixed warnings on 64 bit compiler.
 *
 * Revision 1.4  2000/01/21 20:06:53  pubmed
 * Volodya: support of non-existing documents for Sequences
 *
 * Revision 1.3  1999/03/15 19:57:56  vasilche
 * CIDs now use set instead of map.
 *
 * Revision 1.2  1999/01/15 19:46:18  vasilche
 * Added CIDs class to hold sorted list of IDs.
 *
 * Revision 1.1  1999/01/07 16:41:54  vasilche
 * CHTMLHelper moved to separate file.
 * TagNames of CHTML classes ara available via s_GetTagName() static
 * method.
 * Input tag types ara available via s_GetInputType() static method.
 * Initial selected database added to CQueryBox.
 * Background colors added to CPagerBax & CSmallPagerBox.
 *
 * ===========================================================================
 */

#endif /* def HTML___HTMLHELPER__HPP  &&  ndef HTML___HTMLHELPER__INL */
