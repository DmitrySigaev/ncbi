/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: hitlist.hpp

Author: Jason Papadopoulos

Contents: Interface for CHitList class

******************************************************************************/

/// @file hitlist.hpp
/// Interface for CHitList class

#ifndef _ALGO_COBALT_HITLIST_HPP_
#define _ALGO_COBALT_HITLIST_HPP_

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/hit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// An ordered collection of CHit objects
class CHitList {

public:
    /// add a reminder of whether a hit should be kept
    typedef pair<bool, CHit *> TListEntry;

    /// base type of hitlist
    typedef vector<TListEntry> TList;

    /// constructor
    ///
    CHitList() {}

    /// constructor (deletes all hits stored)
    ///
    ~CHitList() 
    {
        NON_CONST_ITERATE(TList, itr, m_List) {
            delete (*itr).second;
        }
    }

    /// Retrieve number of hits in list
    /// @return The number of hits
    ///
    int Size() { return m_List.size(); }

    /// Determine whether a list contains no hits
    /// @return true if list has no hits
    bool Empty() { return m_List.empty(); }

    /// Append a hit to the hitlist
    /// @param hit The hit to append
    ///
    void AddToHitList(CHit *hit) 
    { 
        m_List.push_back(TListEntry(true, hit)); 
    }

    /// Retrieve a hit from the hitlist
    /// @param index Which hit to retrieve [in]
    /// @return Pointer to the specified hit
    ///
    CHit *GetHit(int index) 
    { 
        _ASSERT(index < Size());
        return m_List[index].second; 
    }

    /// Determine whether a hit in the hitlist has
    /// been scheduled for deletion
    /// @param index Which hit to query [in]
    /// @return true if hit is to be kept, false otherwise
    ///
    bool GetKeepHit(int index)
    { 
        _ASSERT(index < Size());
        return m_List[index].first;
    }

    /// Set whether a hit in the hitlist will be 
    /// scheduled for deletion
    /// @param index Which hit to set [in]
    /// @param keep true if hit is to be kept, false otherwise [in]
    ///
    void SetKeepHit(int index, bool keep) 
    { 
        _ASSERT(index < Size());
        m_List[index].first = keep; 
    }

    /// Delete all hits scheduled to be deleted
    ///
    void PurgeUnwantedHits()
    {
        int i, j;
        for (i = j = 0; i < Size(); i++) {
            if (m_List[i].first == false)
                delete m_List[i].second;
            else
                m_List[j++] = m_List[i];
        }
        m_List.resize(j);
    }

    /// Delete all hits unconditionally
    ///
    void PurgeAllHits()
    {
        for (int i = 0; i < Size(); i++)
            delete m_List[i].second;
        m_List.clear();
    }

    /// Empty out the list without deleting the
    /// hits in it. Useful for cases where the 
    /// hitlist is a subset of another hitlist
    ///
    void ResetList() { m_List.clear(); }

    /// Sort the hits in the hitlist in order of
    /// decreasing score
    ///
    void SortByScore();

    /// Sort the hits in the hitlist by increasing
    /// sequence1 index, then by increasing sequence1
    /// start offset, then increasing sequence2 
    /// start offset
    ///
    void SortByStartOffset();

    /// Sort the list in a canonical form: first swap
    /// the indices and ranges on all hits and subhits so
    /// that the ordinal ID of sequence 1 is always less
    /// than that of sequence2. The sort by increasing
    /// sequence1 index, then increasing sequence1
    /// start offset, then increasing sequence2 start 
    /// offset. Finally, delete all duplicate hits
    ///
    void MakeCanonical();

    /// For each pair of hits with the same sequence2,
    /// produce a list of hits between sequence1 of the first
    /// hit and sequence1 of the second hit. This new hit
    /// contains one subhit for each of the subhits in the
    /// originals that overlap on sequence2.
    /// @param matched_list Collection of matched hits [out]
    ///
    void MatchOverlappingSubHits(CHitList& matched_list);

    /// Append one hitlist to another. The hits in the
    /// input list are duplicated
    /// @param hitlist The lisqt to append [in]
    /// @return the updated list
    ///
    CHitList& operator+=(CHitList& hitlist);

private:
    TList m_List;  ///< current list of hits
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_HITLIST_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.4  2005/11/15 23:24:15  papadopo
  add doxygen

  Revision 1.3  2005/11/08 18:46:22  papadopo
  assert -> _ASSERT

  Revision 1.2  2005/11/08 17:40:26  papadopo
  ASSERT->assert

  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
