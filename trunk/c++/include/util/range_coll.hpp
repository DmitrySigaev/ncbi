#ifndef UTIL___RANGE_COLL__HPP
#define UTIL___RANGE_COLL__HPP

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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *  class CRangeCollection - sorted collection of non-overlapping CRange-s
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <util/range.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

template<class Range, class Position>
struct PRangeLessPos
{
    bool    operator()(const Range &R, Position Pos)  { return R.GetToOpen() <= Pos;  }    
};
    
///////////////////////////////////////////////////////////////////////////////
// class CRangeCollection<Position> represent a sorted collection of 
// CRange<Position>. It is guaranteed that ranges in collection do not overlap
// and aren't adjacent so that there is no gap beween them.
// Adding an interval to the collection leads to possible merging it with 
// existing intervals.

template<class Position>
class CRangeCollection 
{
public:
    typedef Position    position_type;
    typedef CRangeCollection<position_type>  TThisType;

    typedef CRange<position_type>    TRange;
    typedef vector<TRange>      TRangeVector;    
    typedef typename TRangeVector::const_iterator    const_iterator;
    typedef typename TRangeVector::const_reverse_iterator    const_reverse_iterator;
    typedef typename TRangeVector::size_type size_type;
    
    CRangeCollection()   { }
    explicit CRangeCollection(const TRange& r)
    {
        m_vRanges.push_back(r);
    }
    CRangeCollection(const CRangeCollection &c)
    {
        m_vRanges = c.m_vRanges;
    }
    
    // immitating vector, but providing only "const" access to elements
    const_iterator  begin() const   
    {   
        return m_vRanges.begin();   
    }
    const_iterator  end() const     
    {   
        return m_vRanges.end(); 
    }
    const_reverse_iterator  rbegin() const  
    {   
        return m_vRanges.rbegin();  
    }
    const_reverse_iterator  rend() const    
    {   
        return m_vRanges.rend();    
    }
    size_type size() const  
    {   
        return m_vRanges.size();    
    }
    bool    empty() const   
    {   
        return m_vRanges.empty();   
    }
    const   TRange& operator[](size_type pos)   const   
    {  
        return m_vRanges[pos];  
    }
    void    clear()
    {
        m_vRanges.clear();
    }
    // returns iterator pointing to the TRange that has ToOpen > pos
    const_iterator  find(position_type pos)   const
    {
        return x_Find(pos);
    }
    position_type   GetFrom() const
    {
        if(! m_vRanges.empty())
            return begin()->GetFrom();
        else return -1;
    }
    position_type   GetToOpen() const
    {
        if(! m_vRanges.empty())
            return rbegin()->GetToOpen();
        else return -1;
    }
    position_type   GetTo() const
    {
        if(! m_vRanges.empty())
            return rbegin()->GetToOpen();
        else return -1;
    }
    bool            Empty() const
    {
        return empty();
    } 
    bool            NotEmpty() const
    {
        return ! empty();
    }
    position_type   GetLength (void) const
    {
       if(! m_vRanges.empty())  {
            position_type From = begin()->GetFrom();
            return rbegin()->GetToOpen() - From;
       } else return 0;
    }
    TRange          GetLimits() const
    {
        if(! m_vRanges.empty())  {
            position_type From = begin()->GetFrom();
            position_type To = rbegin()->GetTo();
            return TRange(From, To);
       } else return TRange(0, -1);
    }
    TThisType&  IntersectWith (const TRange& r)
    {
         x_IntersectWith(r);
         return *this;
    }
    TThisType &  operator &= (const TRange& r)
    {
         x_IntersectWith(r);
         return *this;
    }
    bool    operator == (const TThisType& c) const
    {
         return x_Equals(c);         
    }
    bool  IntersectingWith (const TRange& r) const
    {
        return x_Intersects(r).second;
    }
    TThisType&  CombineWith (const TRange& r)
    {
        x_CombineWith(r);
        return *this;
    }
    TThisType &  operator+= (const TRange& r)
    {
        x_CombineWith(r);
        return *this;
    }
    TThisType&  Subtract(const TRange& r)
    {
        x_Subtract(r);
        return *this;
    }
    TThisType &  operator-= (const TRange& r)
    {
       x_Subtract(r);
       return *this;
    }    
    TThisType&  IntersectWith (const TThisType &c)
    {
        x_IntersectWith(c);
        return *this;
    }
    TThisType &  operator&= (const TThisType &c)
    {
        x_IntersectWith(c);
        return *this;
    }
    TThisType&  CombineWith (const TThisType &c)
    {
        x_CombineWith(c);
        return *this;
    }
    TThisType &  operator+= (const TThisType &c)
    {
        x_CombineWith(c);
        return *this;
    }
    TThisType&  Subtract(const TThisType &c)
    {
        x_Subtract(c);
        return *this;
    }
    TThisType &  operator-= (const TThisType &c)
    {
       x_Subtract(c);
       return *this;
    }    

protected:
    typedef typename TRangeVector::iterator    iterator;
    typedef typename TRangeVector::reverse_iterator    reverse_iterator;

    iterator  begin()   
    {   
        return m_vRanges.begin();   
    }
    iterator  end()
    {   
        return m_vRanges.end(); 
    }    
    pair<iterator, bool>    x_Find(position_type pos)
    {
        PRangeLessPos<TRange, position_type>    p;
        iterator it = lower_bound(begin(), end(), pos, p);
        bool b_contains = it != end()  &&  it->GetFrom() >= pos;
        return make_pair(it, b_contains);
    }
    pair<iterator, bool>    x_Intersects(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    p;
        iterator it = lower_bound(begin(), end(), r.GetFrom(), p);
        bool b_intersects = it != end()  &&  it->GetFrom() < r.GetToOpen();
        return make_pair(it, b_intersects);
    }
   
    void    x_IntersectWith(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    p;

        position_type pos_to = r.GetTo();
        iterator it_right = lower_bound(begin(), end(), pos_to, p);
        if(it_right != end()) {
            if(it_right->GetFrom() <= pos_to) {   //intersects
                it_right->SetTo(pos_to);
                ++it_right; // exclude from removal
            }
            m_vRanges.erase(it_right, end()); // erase ranges to the right
        }

        position_type pos_from = this->R.GetFrom();
        iterator it_left = lower_bound(begin(), end(), pos_from, this->P);
        if(it_left != end()) {        
            if(it_left->GetFrom() < pos_from)    
                it_left->SetFrom(pos_from);
            m_vRanges.erase(begin(), it_left); //erase ranges to the left
        }
    }

    // returns iterator to the range representing result of combination
    iterator    x_CombineWith(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    p;

        position_type pos_from = r.GetFrom();
        position_type pos_to_open = r.GetToOpen();                    

        iterator it_begin_m = lower_bound(begin(), end(), pos_from - 1, p);        
        if(it_begin_m != end() && it_begin_m->GetFrom() <= pos_to_open)  { // intersection
            it_begin_m->CombineWith(r);
        
            iterator it_end_m = lower_bound(it_begin_m, end(), pos_to_open, p);
            if(it_end_m != end()  &&  it_end_m->GetFrom() <= pos_to_open) {// subject to merge
                it_begin_m->SetToOpen(it_end_m->GetToOpen()); 
                ++it_end_m; // including it into erased set
            }
            m_vRanges.erase(it_begin_m + 1, it_end_m); 
        } else {
            m_vRanges.insert(it_begin_m, r);
        }
        return it_begin_m;
    }

    void    x_Subtract(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    P;

        position_type pos_from = r.GetFrom();
        position_type pos_to_open = r.GetToOpen();

        iterator it_begin_e = lower_bound(begin(), end(), pos_from, P);
        if(it_begin_e != end()) {   // possible intersection
            
            if(it_begin_e->GetFrom() < pos_from  &&  it_begin_e->GetToOpen() > pos_to_open)    { 
                //it_begin_e contains R, split it in two
                it_begin_e = m_vRanges.insert(it_begin_e, *it_begin_e);
                it_begin_e->SetToOpen(pos_from);
                (++it_begin_e)->SetFrom(pos_to_open);
            } else  {
                if(it_begin_e->GetFrom() < pos_from) { // cut this segement , but don't erase
                    it_begin_e->SetToOpen(pos_from);
                    ++it_begin_e;
                }
                iterator it_end_e = lower_bound(it_begin_e, end(), pos_to_open, P);
                if(it_end_e != end()  &&  it_end_e->GetFrom() < pos_to_open)    { 
                    it_end_e->SetFrom(pos_to_open); // truncate
                }
                // erase ranges fully covered by R
                m_vRanges.erase(it_begin_e, it_end_e); 
            }
        } 
    }
    void    x_IntersectWith(const TThisType &c)
    {
        ITERATE(typename TThisType, it, c)   {
            x_IntersectWith(*it);
        }
    }
    void    x_CombineWith(const TThisType &c)
    {
        ITERATE(typename TThisType, it, c)   {
            x_CombineWith(*it);
        }
    }
    void    x_Subtract(const TThisType &c)
    {
        ITERATE(typename TThisType, it, c)   {
            x_Subtract(*it);
        }
    }
    bool    x_Equals(const TThisType &c) const
    {
        if(&c == this)  {
            return true;
        } else {
            return m_vRanges == c.m_vRanges;
        }
    }

protected:
    TRangeVector    m_vRanges;  
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/10/04 16:00:16  yazhuk
 * Added operator == and x_Equals()
 *
 * Revision 1.4  2004/04/26 14:51:43  ucko
 * Add "typename" and "this->" to accommodate GCC 3.4's stricter
 * treatment of templates.
 *
 * Revision 1.3  2004/02/12 20:51:52  yazhuk
 * Fixed GetFrom()
 *
 * Revision 1.2  2003/12/22 19:17:49  dicuccio
 * Use correct #include guard
 *
 * Revision 1.1  2003/12/01 16:29:45  yazhuk
 * Moved from gui/widgets/aln_multiple
 *
 * Revision 1.7  2003/10/10 18:56:12  yazhuk
 * Added GetFrom(), fixed find()
 *
 * Revision 1.6  2003/10/08 14:12:58  dicuccio
 * Moved CGlPane into opengl
 *
 * Revision 1.5  2003/09/29 13:38:48  yazhuk
 * Enforced coding policy
 *
 * Revision 1.4  2003/09/23 20:45:47  yazhuk
 * Added comments
 *
 * Revision 1.3  2003/09/08 20:47:50  yazhuk
 * Bugfixes
 *
 * Revision 1.2  2003/09/08 17:04:12  yazhuk
 * GCC compilation fixes
 *
 * Revision 1.1  2003/09/08 16:34:38  yazhuk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // UTIL___RANGE_COLL__HPP
