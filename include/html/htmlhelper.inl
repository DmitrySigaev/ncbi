#if defined(HTMLHELPER__HPP)  &&  !defined(HTMLHELPER__INL)
#define HTMLHELPER__INL

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

inline CIDs::CIDs(void)
{
}

inline CIDs::~CIDs(void)
{
}

inline bool CIDs::HaveID(int id) const
{
    return find(id) != end();
}

inline bool CIDs::ExtractID(int id)
{
    iterator i = find(id);
    if ( i == end() )
        return false;
    erase(i);
    return true;
}

inline void CIDs::AddID(int id)
{
    insert(value_type(id, id));
}

inline void CIDs::RemoveID(int id)
{
    erase(id);
}

inline int CIDs::CountIDs(void) const
{
    return size();
}

#endif /* def HTMLHELPER__HPP  &&  ndef HTMLHELPER__INL */
