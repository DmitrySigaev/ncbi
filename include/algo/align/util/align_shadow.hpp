#ifndef ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP
#define ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   CAlignShadow class
*
* CAlignShadow is a simplified yet and compact representation of
* a pairwise sequence alignment which keeps information about
* the alignment's borders
*
*/

#include <corelib/ncbiobj.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/align/util/algo_align_util_exceptions.hpp>

BEGIN_SCOPE(objects)
    class CSeq_id;
END_SCOPE(objects)


BEGIN_NCBI_SCOPE


class NCBI_XALGOALIGN_EXPORT CAlignShadow: public CObject
{
public:

    typedef CConstRef<objects::CSeq_id> TId;

    // c'tors
    CAlignShadow(void);
    CAlignShadow(const objects::CSeq_align& seq_align);

    virtual ~CAlignShadow() {}

    // getters / setters
    const TId& GetId(Uint1 where) const;
    const TId& GetQueryId(void) const;
    const TId& GetSubjId(void) const;

    void  SetId(Uint1 where, const TId& id);
    void  SetQueryId(const TId& id);
    void  SetSubjId(const TId& id);

    bool  GetStrand(Uint1 where) const;
    bool  GetQueryStrand(void) const;
    bool  GetSubjStrand(void) const;

    void  SetStrand(Uint1 where, bool strand);
    void  SetQueryStrand(bool strand);
    void  SetSubjStrand(bool strand);

    typedef TSeqPos TCoord;
    
    const TCoord* GetBox(void) const;
    void  SetBox(const TCoord box [4]);

    TCoord GetMin(Uint1 where) const;
    TCoord GetMax(Uint1 where) const;
    TCoord GetQueryMin(void) const;
    TCoord GetQueryMax(void) const;
    TCoord GetSubjMin(void) const;
    TCoord GetSubjMax(void) const;
    void   SetMax(Uint1 where, TCoord pos);
    void   SetMin(Uint1 where, TCoord pos);
    void   SetQueryMin(TCoord pos);
    void   SetQueryMax(TCoord pos);
    void   SetSubjMin(TCoord pos);
    void   SetSubjMax(TCoord pos);

    TCoord GetQuerySpan(void) const;
    TCoord GetSubjSpan(void) const;

    TCoord GetStart(Uint1 where) const;
    TCoord GetStop(Uint1 where) const;
    TCoord GetQueryStart(void) const;
    TCoord GetQueryStop(void) const;
    TCoord GetSubjStart(void) const;
    TCoord GetSubjStop(void) const;
    void   SetStop(Uint1 where, TCoord pos);
    void   SetStart(Uint1 where, TCoord pos);
    void   SetQueryStart(TCoord pos);
    void   SetQueryStop(TCoord pos);
    void   SetSubjStart(TCoord pos);
    void   SetSubjStop(TCoord pos);

    void         Shift(Int4 shift_query, Int4 shift_subj);
    virtual void Modify(Uint1 where, TCoord new_pos);

    // tabular serialization
    friend NCBI_XALGOALIGN_EXPORT CNcbiOstream& operator << (CNcbiOstream& os, 
                                      const CAlignShadow& align_shadow);

protected:
    
    std::pair<TId,TId>   m_Id;     // Query and subj IDs

    TCoord  m_Box [4];    // [0] = query_start, [1] = query_stop
                          // [2] = subj_start,[3] = subj_stop, all zero-based;
                          // Order in which query and subj coordinates go
                          // reflects strand.

    // tabular serialization without IDs
    virtual void   x_PartialSerialize(CNcbiOstream& os) const;
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/09/12 16:21:34  kapustin
 * Add compartmentization algorithm
 *
 * Revision 1.12  2005/07/28 15:17:02  kapustin
 * Add export specifiers
 *
 * Revision 1.11  2005/07/28 14:55:25  kapustin
 * Use std::pair instead of array to fix gcc304 complains
 *
 * Revision 1.10  2005/07/28 12:29:26  kapustin
 * Convert to non-templatized classes where causing compilation incompatibility
 *
 * Revision 1.9  2005/07/28 02:15:57  ucko
 * Comment out inheritance from CObject until we can find a way to do so
 * without crashing GCC 3.0.4's optimizer. :-/
 *
 * Revision 1.8  2005/07/27 18:52:59  kapustin
 * Derive from CObject
 *
 * Revision 1.7  2005/04/18 20:44:39  ucko
 * Add some forward declarations to unconfuse GCC 2.95.
 *
 * Revision 1.6  2005/04/18 15:24:08  kapustin
 * Split CAlignShadow into core and blast tabular representation
 *
 * Revision 1.5  2005/03/04 17:17:43  dicuccio
 * Added export specifiers
 *
 * Revision 1.4  2005/03/03 19:33:19  ucko
 * Drop empty "inline" promises from member declarations.
 *
 * Revision 1.3  2004/12/22 21:25:15  kapustin
 * Move friend template definition to the header.
 * Declare explicit specialization.
 *
 * Revision 1.2  2004/12/22 15:50:02  kapustin
 * Drop dllexport specifier
 *
 * Revision 1.1  2004/12/21 19:59:58  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP  */

