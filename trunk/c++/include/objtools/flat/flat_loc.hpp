#ifndef OBJECTS_FLAT___FLAT_LOC__HPP
#define OBJECTS_FLAT___FLAT_LOC__HPP

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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- location representation
*
*/

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declarations
class CInt_fuzz;
class CSeq_id;
class CSeq_interval;
class CSeq_loc;
class CFlatContext;


class CFlatLoc : public CObject // derived from CObject to allow for caching
{
public:
    struct SInterval
    {
        string          m_Accession;
        CRange<TSeqPos> m_Range; // 1-based; should be finite
    };
    typedef vector<SInterval> TIntervals;
    
    CFlatLoc(const CSeq_loc& loc, CFlatContext& ctx);

    const string&     GetString(void)    const { return m_String;    }
    const TIntervals& GetIntervals(void) const { return m_Intervals; }
    
private:
    string     m_String;    // whole location, as a GB-style string
    TIntervals m_Intervals; // individual intervals/points

    void x_Add   (const CSeq_loc& loc, CNcbiOstrstream& oss,
                  CFlatContext& ctx);
    void x_Add   (const CSeq_interval& si, CNcbiOstrstream& oss,
                  CFlatContext& ctx);
    // these convert from 0-based to 1-based coordinates in the process
    void x_AddPnt(TSeqPos pnt, const CInt_fuzz* fuzz, CNcbiOstrstream& oss,
                  CFlatContext& ctx);
    void x_AddInt(TSeqPos from, TSeqPos to, const string& accn);

    static void x_AddID(const CSeq_id& id, CNcbiOstrstream& oss,
                        CFlatContext& ctx, string *s = 0);
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_LOC__HPP */
