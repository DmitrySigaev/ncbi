#ifndef DBAPI_DRIVER___POINTER_POT__HPP
#define DBAPI_DRIVER___POINTER_POT__HPP

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
 * Author:  Vladimir Soussov
 *
 * File Description:  Pot of pointers
 *
 */


#include <stdlib.h>
#include <corelib/ncbistl.hpp>


BEGIN_NCBI_SCOPE


typedef void* TPotItem;
typedef int (*FPotCompare)(TPotItem, TPotItem);


class CQuickSortStack
{
public:
    int Pop(int* i1, int* i2) {
        if ( m_NofItems ) {
            *i1 = m_Items[--m_NofItems];
            *i2 = m_Items[--m_NofItems];
            return 1;
        }
        return 0;
    };
    void Push(int i1, int i2);

    CQuickSortStack(int n = 64) {
        m_NofRooms = (n > 8) ? n : 64;
        m_Items    = new int[m_NofRooms];
        m_NofItems = 0;
    }
    ~CQuickSortStack(void) {
        if ( m_Items )
            delete[] m_Items;
    }

private:
    int* m_Items;
    int  m_NofItems;
    int  m_NofRooms;
};



class CPointerPot
{
public:
    int NofItems(void) const {
        return m_NofItems;
    };

    void* Get(int n) const {
        return (n >= 0  &&  n < m_NofItems) ? m_Items[n] : 0;
    };

    void Sort(FPotCompare cf);

    void Add(const TPotItem item, int check_4_unique = 0);

    void Remove(int n);
    void Remove(TPotItem item);

    CPointerPot& operator= (CPointerPot&);

    CPointerPot(int n = 16) {
        m_NofRooms = (n > 4) ? n : 16;
        m_Items    = new TPotItem[m_NofRooms];
        m_NofItems = 0;
    }

    ~CPointerPot(void) {
        if ( m_Items )
            delete[] m_Items;
    }

private:
    TPotItem* m_Items;
    int       m_NofItems;
    int       m_NofRooms;

    void x_SimpleSort(TPotItem* arr, int nof_items, FPotCompare cf);
};

class CMemPot 
{
public:
    void* Alloc(size_t nof_bytes) {
        char* c= new char[nof_bytes];
        m_Pot.Add((TPotItem) c);
        return c;
    }
	    
    void Free(void* ptr) {
        delete [] static_cast<char*>(ptr);
        m_Pot.Remove(ptr);
    }
    
    ~CMemPot() {
        for(int i= m_Pot.NofItems(); i--; delete[] static_cast<char*>(m_Pot.Get(i)));
    }
    
private:
    CPointerPot m_Pot;
};

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2002/08/28 17:07:24  vasilche
 * Fixed delete[] of void pointer
 *
 * Revision 1.3  2002/06/20 18:37:23  soussov
 * odbc related changes
 *
 * Revision 1.2  2001/11/06 17:58:07  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/09/21 23:39:55  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___POINTER_POT__HPP */

