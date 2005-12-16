/* $Id$
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
 * Author: Kevin Bealer
 *
 */

/** @file uniform_search.hpp
 * Uniform BLAST Search Interface.
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___UNIFORM_SEARCH_HPP
#define ALGO_BLAST_API___UNIFORM_SEARCH_HPP

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)


// Errors & Warnings:
// 
// An error is defined as a condition that halts or severely affects
// processing of one or more queries, and is represented as a string.
// A warning is defined as a detected condition or event that is
// reported by the search code, and may influence interpretation of
// the output, but does not stop the search or invalidate the
// results.

/// Exception class
///
/// Searches throw this when an error condition is detected in the
/// usage or execution of a search.  An example of a case where an
/// exception is appropriate is when a database cannot be found for a
/// local search, or if a memory allocation fails.  An example of a
/// non-exception error is if a search is completely masked.

class CSearchException : public CException {
public:
    /// Errors are classified into one of two types.
    enum EErrCode {
        /// Argument validation failed.
        eConfigErr,
        
        /// Memory allocation failed.
        eMemErr,
        
        /// Internal error (e.g. unimplemented methods).
        eInternal
    };
    
    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eConfigErr: return "eConfigErr";
        case eMemErr:    return "eMemErr";
        case eInternal:  return "eInternal";
        default:         return CException::GetErrCodeString();
        }
    }
    
    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CSearchException, CException);
};


/// Blast Search Subject
class NCBI_XBLAST_EXPORT CSearchDatabase : public CObject {
public:
    typedef vector<int> TGiList;
    enum EMoleculeType {
        eBlastDbIsProtein,
        eBlastDbIsNucleotide
    };

    CSearchDatabase(const string& dbname, EMoleculeType mol_type);

    CSearchDatabase(const string& dbname, EMoleculeType mol_type,
                    const string& entrez_query);

    CSearchDatabase(const string& dbname, EMoleculeType mol_type,
                    const TGiList& gilist);

    CSearchDatabase(const string& dbname, EMoleculeType mol_type,
                    const string& entrez_query, const TGiList& gilist);

    void SetDatabaseName(const string& dbname);
    string GetDatabaseName() const;

    void SetMoleculeType(EMoleculeType mol_type);
    EMoleculeType GetMoleculeType() const;

    void SetEntrezQueryLimitation(const string& entrez_query);
    string GetEntrezQueryLimitation() const;

    void SetGiListLimitation(const TGiList& gilist);
    const TGiList& GetGiListLimitation() const;

private:
    string          m_DbName;
    EMoleculeType   m_MolType;
    string          m_EntrezQueryLimitation;
    TGiList         m_GiListLimitation;
};


/// Search Results for One Query.
/// 
/// This class encapsulates all the search results and related data
/// corresponding to one of the input queries.

class NCBI_XBLAST_EXPORT CSearchResults : public CObject {
public:
    
    CSearchResults(CRef<objects::CSeq_align_set> align, 
                   const TQueryMessages & errs)
        : m_Alignment(align), m_Errors(errs)
    {
    }
    
    CConstRef<objects::CSeq_align_set> GetSeqAlign() const
    {
        return m_Alignment;
    }
    
    CConstRef<objects::CSeq_id> GetSeqId() const;
    
    TQueryMessages GetErrors(int min_severity = eBlastSevError) const;

#if 0
    /// Retrieve the query regions which were masked by BLAST
    /// @param flt_query_regions the return value [in|out]
    void GetFilteredQueryRegions(TSeqLocInfoVector& flt_query_regions);
#endif
    
private:
    CRef<objects::CSeq_align_set> m_Alignment;
    
    TQueryMessages m_Errors;
};


/// Search Results for All Queries.
/// 
/// This class encapsulates all of the search results and related data
/// from a search.

class NCBI_XBLAST_EXPORT CSearchResultSet {
public:
    CSearchResultSet()
    {
    }
    
    CSearchResultSet(TSeqAlignVector aligns, TSearchMessages msg_vec);
    
    CSearchResults & operator[](int i)
    {
        return *m_Results[i];
    }
    
    const CSearchResults & operator[](int i) const
    {
        return *m_Results[i];
    }
    
    CRef<CSearchResults> operator[](const objects::CSeq_id & ident);
    
    CConstRef<CSearchResults> operator[](const objects::CSeq_id & ident) const;
    
    int GetNumResults() const
    {
        return (int) m_Results.size();
    }
    
    void AddResult(CRef<CSearchResults> result)
    {
        m_Results.push_back(result);
    }
    
private:
    typedef vector< CRef<CSearchResults> > TResultsVector;
    
    /// Vector of results.
    TResultsVector m_Results;
};


/// Single Iteration Blast Database Search
/// 
/// FIXME: update comment
///
/// This class is the top-level Uniform Search Interface for blast
/// searches.  Concrete subclasses of this class will create and
/// return concrete subclasses of the IBlastDbSearch class.  Use this
/// class when you need to write code that decribes an algorithm over
/// the abstract IBlastDbSearch API, and is ignorant of the concrete
/// type of search it is performing.

class ISearch : public CObject {
public:
    // Configuration
    
    /// Configure the search
    virtual void SetOptions(CRef<CBlastOptionsHandle> options) = 0;
    
    /// Set the subject database(s) to search
    virtual void SetSubject(CConstRef<CSearchDatabase> subject) = 0;
    
    /// Run the search to completion.
    virtual CSearchResultSet Run() = 0;
};

class ISeqSearch : public ISearch {
public:

    virtual ~ISeqSearch() {}
    
    // Inputs
    
    /// Set the queries to search
    virtual void SetQueryFactory(CRef<IQueryFactory> query_factory) = 0;
};


/// Experimental interface (since this does not provide a full interface to
/// PSI-BLAST)
/// @note the CSearchResultSet that is returned from the Run method will
/// always contain 0 or 1 CSearchResults objects, as PSI-BLAST cannot do
/// multiple-PSSM searches
class IPssmSearch : public ISearch {
public:

    /// Set the queries to search
    /// @param pssm PSSM [in]
    virtual void SetQuery(CRef<objects::CPssmWithParameters> pssm) = 0;
};


/// Factory for ISearch.
/// 
/// This class is an abstract factory class for the ISearch class.
/// Concrete subclasses of this class will create and return concrete
/// subclasses of the ISearch class.  Use this class when you need to
/// write code that decribes an algorithm over the abstract ISearch
/// API, and is ignorant of the concrete type of search it is
/// performing (i.e.: local vs. remote search).

class ISearchFactory : public CObject {
public:
    /// Create a new search object with a sequence-based query.
    ///
    /// A search object will be constructed and configured for a
    /// search using a query that consists of one or more sequences.
    ///
    /// @return
    ///   A search object for a sequence search.
    virtual CRef<ISeqSearch>          GetSeqSearch()  = 0;
    
    /// Create a new search object with a pssm-based query.
    ///
    /// A search object will be constructed and configured for a
    /// search using a PSSM query.
    ///
    /// @return
    ///   A search object for a PSSM search.
    virtual CRef<IPssmSearch>         GetPssmSearch()  = 0;
    
    /// Create a CBlastOptionsHandle
    ///
    /// This creates a CBlastOptionsHandle for the specified program
    /// value.  The options can be used to configure a search created
    /// by the GetSeqSearch() or GetPssmSearch() methods.  The search
    /// object and the CBlastOptionsHandle object should be created by
    /// the same ISearchFactory subclass.
    ///
    /// @param program
    ///   The program type for this search.
    /// @return
    ///   An options handle object for this program and factory type.
    virtual CRef<CBlastOptionsHandle> GetOptions(EProgram program) = 0;
};


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___UNIFORM_SEARCH__HPP */

