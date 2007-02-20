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
 * Author:  Christiam Camacho
 *
 */

/** @file sseqloc.hpp
 * Definition of SSeqLoc structure
 */

#ifndef ALGO_BLAST_API___SSEQLOC__HPP
#define ALGO_BLAST_API___SSEQLOC__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Structure combining a Seq-loc, scope and masking locations for one sequence
struct SSeqLoc {
    /// Seq-loc describing the sequence to use as query/subject to BLAST
    /// The types of Seq-loc currently supported are: whole, seq-interval...
    /// @todo FIXME Complete the list of supported seq-locs
    CConstRef<objects::CSeq_loc>     seqloc;
    
    /// Scope where the sequence referenced can be found by the toolkit's
    /// object manager
    mutable CRef<objects::CScope>    scope;
    
    /// Seq-loc describing regions to mask in the seqloc field
    /// Acceptable types of Seq-loc are Seq-interval and Packed-int
    /// @sa ignore_strand_in_mask
    CRef<objects::CSeq_loc>          mask;

    /// This member dictates how the strand in the mask member is interpreted.
    /// If true, it means that the Seq-loc in mask is assumed to be on the plus
    /// strand AND that the complement of this should also be applied (i.e.:
    /// the strand specification of the mask member will be ignored). If it's
    /// false, then the strand specification of the mask member will be obeyed
    /// and only those regions on specific strands will be masked.
    /// @note the default value of this field is true
    /// @sa mask
    bool                             ignore_strand_in_mask;
    
    /// Default constructor
    SSeqLoc()
        : seqloc(), scope(), mask(), ignore_strand_in_mask(true) {}

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s)
        : seqloc(sl), scope(s), mask(0), ignore_strand_in_mask(true) {}

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s)
        : seqloc(&sl), scope(&s), mask(0), ignore_strand_in_mask(true) {}

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    /// @param m Masking location(s) applicable to sl [in]
    /// @param ignore_mask_strand Ignore the mask specified in m? [in]
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s,
            objects::CSeq_loc* m, bool ignore_mask_strand = true)
        : seqloc(sl), scope(s), mask(m), 
          ignore_strand_in_mask(ignore_mask_strand) {
        if (m != NULL && ignore_strand_in_mask) {
              mask->ResetStrand();
        }
    }

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    /// @param m Masking location(s) applicable to sl [in]
    /// @param ignore_mask_strand Ignore the mask specified in m? [in]
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s,
            objects::CSeq_loc& m, bool ignore_mask_strand = true)
        : seqloc(&sl), scope(&s), mask(&m),
          ignore_strand_in_mask(ignore_mask_strand) {
        if (ignore_strand_in_mask) {
              mask->ResetStrand();
        }
    }
};

/// Vector of sequence locations
typedef vector<SSeqLoc>   TSeqLocVector;


/// Search Query
///
/// This class represents the data relevant to one query in a blast
/// search.  The types of Seq-loc currently supported are "whole" and
/// "int".  The scope is expected to contain this Seq-loc, and the
/// mask represents the regions of this query that are disabled for
/// this search, or for some frames of this search, via one of several
/// algorithms, or that are specified by the user as masked regions.
class CBlastSearchQuery : public CObject {
public:
    /// Constructor
    ///
    /// Build a CBlastSearchQuery object.
    ///
    /// @param sl The query itself.
    /// @param sc The scope containing the query.
    /// @param m Regions of the query that are masked.
    CBlastSearchQuery(const objects::CSeq_loc & sl,
                      objects::CScope         & sc,
                      TMaskedQueryRegions       m)
        : seqloc(& sl), scope(& sc), mask(m)
    {
    }
    
    /// Default constructor
    ///
    /// This is necessary in order to add this type to a std::vector.
    CBlastSearchQuery()
    {
    }
    
    /// Get the query Seq-loc.
    /// @return The Seq-loc representing the query
    CConstRef<objects::CSeq_loc> GetQuerySeqLoc() const
    {
        return seqloc;
    }
    
    /// Get the query CScope.
    /// @return The CScope containing the query
    CRef<objects::CScope> GetScope() const
    {
        return scope;
    }
    
    /// Get the masked query regions.
    ///
    /// The masked regions of the query, or of some frames or strands of the
    /// query, are returned.
    ///
    /// @return The masked regions of the query.
    TMaskedQueryRegions GetMaskedRegions() const
    {
        return mask;
    }
    
    /// Set the masked query regions.
    ///
    /// The indicated set of masked regions is applied to this query,
    /// replacing any existing masked regions.
    ///
    /// @param mqr The set of regions to mask.
    void SetMaskedRegions(TMaskedQueryRegions mqr)
    {
        mask = mqr;
    }
    
    /// Masked a region of this query.
    ///
    /// The CSeqLocInfo object is added to the list of masked regions
    /// of this query.
    ///
    /// @param sli A CSeqLocInfo indicating the region to mask.
    void AddMask(CRef<CSeqLocInfo> sli)
    {
        mask.push_back(sli);
    }
    
private:
    /// The Seq-loc representing the query.
    CConstRef<objects::CSeq_loc> seqloc;
    
    /// This scope contains the query.
    mutable CRef<objects::CScope> scope;
    
    /// These regions of the query are masked.
    TMaskedQueryRegions mask;
};


/// Query Vector
///
/// This class represents the data relevant to all queries in a blast
/// search.  The queries are represented as CBlastSearchQuery objects.
/// Each contains a Seq-loc, scope, and a list of filtered regions.

class CBlastQueryVector : public CObject {
public:
    /// size_type type definition
    typedef vector< CRef<CBlastSearchQuery> >::size_type size_type;

    /// Add a query to the set.
    ///
    /// The CBlastSearchQuery is added to the list of queries for this
    /// search.
    ///
    /// @param q A query to add to the set.
    void AddQuery(CRef<CBlastSearchQuery> q)
    {
        m_Queries.push_back(q);
    }
    
    /// Returns true if this query vector is empty.
    bool Empty() const
    {
        return m_Queries.empty();
    }
    
    /// Returns the number of queries found in this query vector.
    size_type Size() const
    {
        return m_Queries.size();
    }
    
    /// Get the query Seq-loc for a query by index.
    /// @param i The index of a query.
    /// @return The Seq-loc representing the query.
    CConstRef<objects::CSeq_loc> GetQuerySeqLoc(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i]->GetQuerySeqLoc();
    }
    
    /// Get the scope containing a query by index.
    /// @param i The index of a query.
    /// @return The CScope containing the query.
    CRef<objects::CScope> GetScope(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i]->GetScope();
    }
    
    /// Get the masked regions for a query by number.
    /// @param i The index of a query.
    /// @return The masked (filtered) regions of that query.
    TMaskedQueryRegions GetMaskedRegions(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i]->GetMaskedRegions();
    }
    
    /// Assign a list of masked regions to one query.
    /// @param i The index of the query.
    /// @param mqr The masked regions for this query.
    void SetMaskedRegions(size_type i, TMaskedQueryRegions mqr)
    {
        _ASSERT(i < m_Queries.size());
        m_Queries[i]->SetMaskedRegions(mqr);
    }
    
    /// Add a masked region to the set for a query.
    /// @param i The index of the query.
    /// @param sli The masked region to add.
    void AddMask(size_type i, CRef<CSeqLocInfo> sli)
    {
        m_Queries[i]->AddMask(sli);
    }
    
    /// Get the CBlastSearchQuery object at index i
    /// @param i The index of a query.
    CRef<CBlastSearchQuery>
    GetBlastSearchQuery(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i];
    }
    
private:
    /// The set of queries used for a search.
    vector< CRef<CBlastSearchQuery> > m_Queries;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_API___SSEQLOC__HPP */


