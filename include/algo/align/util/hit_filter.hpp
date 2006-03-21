#ifndef ALGO_ALIGN_UTIL_HITFILTER__HPP
#define ALGO_ALIGN_UTIL_HITFILTER__HPP

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
*
*/

#include <algo/align/util/hit_comparator.hpp>

#include <algorithm>
#include <numeric>
#include <vector>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////
// CHitCoverageAccumulator

template<class THit>
class CHitCoverageAccumulator
{
public:

    typedef CRef<THit>    THitRef;
    typedef typename THit::TCoord  TCoord;

    CHitCoverageAccumulator(Uint1 where): 
        m_Where(where),  
        m_Finish(0)
    {
        if(m_Where == 0) {
            m_i1 = 0;
            m_i2 = 1;
        }
        else {
            m_i1 = 2;
            m_i2 = 3;
        }
    }

    TCoord operator() (TCoord iVal, const THitRef& ph)
    {
        const TCoord box [] = { ph->GetQueryMin(), ph->GetQueryMax(),
                                ph->GetSubjMin(), ph->GetSubjMax()   };

        if(box[m_i1] > m_Finish || m_Finish == 0) {
            return iVal + (m_Finish = box[m_i2]) - box[m_i1] + 1;
        }
        else {
            TCoord Finish0 = m_Finish;
            return (box[m_i2] > Finish0)?
                (iVal + (m_Finish = box[m_i2]) - Finish0): iVal;
        }
    }

private:

    Uint1   m_Where;
    TCoord  m_Finish;
    Uint1   m_i1, m_i2;
};

/////////////////////////////////////////////
/////////////////////////////////////////////

template<class THit>
class CHitFilter: public CObject
{
public:

    typedef CRef<THit>              THitRef;
    typedef vector<THitRef>         THitRefs;
    typedef typename THit::TCoord   TCoord;

    static  TCoord s_GetCoverage(Uint1 where, 
                                 typename THitRefs::const_iterator from,
                                 typename THitRefs::const_iterator to)
    {
        // since we need to sort, create and init a local vector
        THitRefs hitrefs (to - from);
        typedef typename THitRefs::const_iterator TCI; 
        typedef typename THitRefs::iterator TI; 
        TCI ii = from;
        TI jj = hitrefs.begin();
        while(ii != to) {
            *jj++ = *ii++;
        }
        
        // prepare a sorter object and sort
        typedef CHitComparator<THit> THitComparator;
        typename THitComparator::ESortCriterion sort_type (
            where == 0? 
            THitComparator::eQueryMin:
            THitComparator::eSubjMin);

        THitComparator sorter (sort_type);
        stable_sort(hitrefs.begin(), hitrefs.end(), sorter);
        
        // compute coverage
        return accumulate(hitrefs.begin(), hitrefs.end(), 
                          TCoord(0), 
                          CHitCoverageAccumulator<THit>(where));
    }

    // 0 = query min, 1 = query max, 2 = subj min, 3 = subj max
    static void s_GetSpan(const THitRefs& hitrefs, TCoord span [4]) 
    {      
        span[0] = span[2] = kMax_UInt; 
        span[1] = span[3] = 0;
          
        for(typename THitRefs::const_iterator ii = hitrefs.begin(),
                ii_end = hitrefs.end(); ii != ii_end; ++ii) {
            
            TCoord x = (*ii)->GetQueryMin();
            if(span[0] > x) {
                span[0] = x;
            }
            x = (*ii)->GetSubjMin();
            if(span[2] > x) {
                span[2] = x;
            }
            x = (*ii)->GetQueryMax();
            if(span[1] < x) {
                span[1] = x;
            }
            x = (*ii)->GetSubjMax();
            if(span[3] < x) {
                span[3] = x;
            }
        }
    }

    // Multiple greedy reconciliation algorithm
    static void s_RunGreedy(typename THitRefs::iterator hri_beg, 
                            typename THitRefs::iterator hri_end,
                            TCoord min_hit_len = 100) {

        if(hri_beg > hri_end) {
            NCBI_THROW(CAlgoAlignUtilException, eInternal, 
                       "Invalid input iterator order");
        }

        const size_t dim = hri_end - hri_beg;

        // compile ordered set of hit ends
        typename THitRefs::iterator ii = hri_beg;
        CStopWatch sw (CStopWatch::eStart);
        THitEnds hit_ends;
        for(size_t i = 0; i < dim; ++i, ++ii) {

            THitRef& h = *ii;
            for(Uint1 point = 0; point < 4; ++point) {
                THitEnd he;
                he.m_Point = point;
                he.m_Ptr = &h;
                he.m_X = h->GetBox()[point];
                hit_ends.insert(he);
            }
        }

        vector<bool> skip (dim, false);
        vector<bool> del (dim, false);
        const THitRef* hitref_firstptr = &(*hri_beg);

        while(true) {

            // select the best hit
            double score_best = 0;
            typename THitRefs::iterator ii = hri_beg, ii_best = hri_end;
            for(size_t i = 0; i < dim; ++i, ++ii) {
                if(skip[i] == false) {
                    const double score = (*ii)->GetScore();
                    if(score > score_best) {
                        ii_best = ii;
                        score_best = score;
                    }
                }
            }

            if(ii_best == hri_end) {
                break;
            }

            // truncate overlaps with the current best hit
            const THitRef& hc = *ii_best;
            THitEnd he [4];
            for(Uint1 point = 0; point < 4; ++point) {
                he[point].m_Point = point;
                he[point].m_Ptr = const_cast<THitRef*>(&hc);
                he[point].m_X = hc->GetBox()[point];
            }

            for(Uint1 where = 0; where < 2; ++where) {

                const TCoord cmin = hc->GetMin(where);
                const TCoord cmax = hc->GetMax(where);

                const THitEnd *phe_lo, *phe_hi;
                const Uint1 i1 = 2*where, i2 = i1 + 1;
                if(hc->GetStrand(where)) {
                    phe_lo = &he[i1];
                    phe_hi = &he[i2];
                }
                else {
                    phe_lo = &he[i2];
                    phe_hi = &he[i1];
                }
                typedef typename THitEnds::iterator THitEndsIter;
                THitEndsIter ii0 = hit_ends.lower_bound(*phe_lo);
                THitEndsIter ii1 = hit_ends.lower_bound(*phe_hi);
                typedef list<THitEndsIter> TIters;

                for(typename THitEnds::iterator ii = ii0; ii != ii1; ++ii) {
                    
                    const THitEnd& he = *ii;
                    const size_t hitrefidx = he.m_Ptr - hitref_firstptr;
                    const bool alive = !skip[hitrefidx];
                    const bool self = he.m_Ptr == &hc;

                    if(alive && !self) {

                        int newpos = sx_Cleave(*he.m_Ptr, he.m_Point/2, cmin, cmax, 
                                               min_hit_len);

                        if(newpos <= -2) { // eliminate
                            del[hitrefidx] = skip[hitrefidx] = true;
                        }
                        if(newpos >= 0) { // truncated
                            const TCoord* box = (*he.m_Ptr)->GetBox();
                            {{
                                THitEnd he2;
                                he2.m_Point = he.m_Point;
                                he2.m_Ptr = he.m_Ptr;
                                he2.m_X = box[he2.m_Point];
                                hit_ends.insert(he2);
                            }}
                            {{
                                THitEnd he3;
                                he3.m_Point = (he.m_Point + 2) % 4;
                                he3.m_Ptr = he.m_Ptr;
                                he3.m_X = box[he3.m_Point];
                                hit_ends.insert(he3);
                            }}
                        }
                    }
                }
            }

            skip[&hc - hitref_firstptr] = true;
        }

        ii = hri_beg;
        for(size_t i = 0; i < dim; ++i, ++ii) {
            if(del[i]) {
                ii->Reset(NULL);
            }
        }
    }

protected:

    struct SHitEnd {

        Uint1    m_Point; // 0 = query start, 1 = query stop, 2 = subj start, 3 = stop
        THitRef* m_Ptr;
        TCoord   m_X;

        bool operator < (const SHitEnd& rhs) const {
            
            const Uint1 where1 = m_Point < 2? 0: 1;
            const Uint1 where2 = rhs.m_Point < 2? 0: 1;
            const THit& h1 = **m_Ptr;
            const THit& h2 = **rhs.m_Ptr;
            int c = h1.GetId(where1)->CompareOrdered(*h2.GetId(where2));
            return c < 0? true: (c > 0? false: m_X < rhs.m_X);
        }

        friend ostream& operator << (ostream& ostr, const SHitEnd& he) {
            return 
                ostr << "(Point,Coord) = (" 
                     << int(he.m_Point) << ", " 
                     << he.m_X << ")\n"
                     << **he.m_Ptr;
        }
    };

    typedef SHitEnd THitEnd;
    typedef multiset<THitEnd> THitEnds;

    // return adjusted coordinate; -1 if unaffected; -2 to delete; -3 on exception
    static int sx_Cleave(THitRef& h, Uint1 where, 
                         TCoord cmin, TCoord cmax, TCoord min_hit_len) 
    {
        int rv = -1;

        try {
            TCoord lmin = h->GetMin(where), lmax = h->GetMax(where);
            if(cmin <= lmin && lmax <= cmax) {
                return -2;
            }
            if(lmin <= cmin && cmin <= lmax) {
                if(cmin == 0) return -1;
                h->Modify(2*where + 1, lmax = cmin - 1);
                rv = lmax;
            }
            if(lmin <= cmax && cmax <= lmax) {
                h->Modify(2*where, lmin = cmax + 1);
                rv = lmin;
            }
            if(1 + lmax - lmin < min_hit_len) {
                return -2;
            }
        }
        catch(CAlgoAlignUtilException&) {
            rv = -3; // hit would become inconsistent through the adjustment
        }
        return rv;
    }
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/03/21 16:16:44  kapustin
 * Support edit transcript string
 *
 * Revision 1.5  2005/09/12 16:21:34  kapustin
 * Add compartmentization algorithm
 *
 * Revision 1.4  2005/07/28 16:43:35  kapustin
 * sort => stable_sort
 *
 * Revision 1.3  2005/07/28 02:16:46  ucko
 * s_GetCoverage: remove redundant class specifier that made GCC 2.95 choke.
 *
 * Revision 1.2  2005/07/27 20:45:03  kapustin
 * Move s_GetCoverage() definition under the class declaration 
 * to calm down MSVC
 *
 * Revision 1.1  2005/07/27 18:53:16  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_HITFILTER__HPP */
