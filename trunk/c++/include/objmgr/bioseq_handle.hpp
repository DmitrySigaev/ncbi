#ifndef BIOSEQ_HANDLE__HPP
#define BIOSEQ_HANDLE__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*
*/

#include <objects/objmgr/seq_id_handle.hpp>
#include <objects/objmgr/impl/bioseq_info.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeqMap;
class CSeqVector;
class CScope;


// Bioseq handle -- must be a copy-safe const type.
class NCBI_XOBJMGR_EXPORT CBioseq_Handle
{
public:
    // Destructor
    virtual ~CBioseq_Handle(void);

    // Bioseq core -- using partially populated CBioseq
    typedef CConstRef<CBioseq> TBioseqCore;


    CBioseq_Handle(void)
        : m_Scope(0),
          m_Bioseq_Info(0) {}
    CBioseq_Handle(const CBioseq_Handle& h);
    CBioseq_Handle& operator= (const CBioseq_Handle& h);

    // Comparison
    bool operator== (const CBioseq_Handle& h) const;
    bool operator!= (const CBioseq_Handle& h) const;
    bool operator<  (const CBioseq_Handle& h) const;

    // Check
    operator bool(void)  const { return ( bool(m_Bioseq_Info)  &&  m_Value); }
    bool operator!(void) const { return (!m_Bioseq_Info  || !m_Value); }

    // Get the complete bioseq (as loaded by now)
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual const CBioseq& GetBioseq(void) const;

    const CSeq_id* GetSeqId(void)  const;

    // Get top level seq-entry for a bioseq
    virtual const CSeq_entry& GetTopLevelSeqEntry(void) const;

    // Go up to a certain complexity level (or the nearest level of the same
    // priority if the required class is not found):
    // level   class
    // 0       not-set (0) ,
    // 3       nuc-prot (1) ,              -- nuc acid and coded proteins
    // 2       segset (2) ,                -- segmented sequence + parts
    // 2       conset (3) ,                -- constructed sequence + parts
    // 1       parts (4) ,                 -- parts for 2 or 3
    // 1       gibb (5) ,                  -- geninfo backbone
    // 1       gi (6) ,                    -- geninfo
    // 5       genbank (7) ,               -- converted genbank
    // 3       pir (8) ,                   -- converted pir
    // 4       pub-set (9) ,               -- all the seqs from a single publication
    // 4       equiv (10) ,                -- a set of equivalent maps or seqs
    // 3       swissprot (11) ,            -- converted SWISSPROT
    // 3       pdb-entry (12) ,            -- a complete PDB entry
    // 4       mut-set (13) ,              -- set of mutations
    // 4       pop-set (14) ,              -- population study
    // 4       phy-set (15) ,              -- phylogenetic study
    // 4       eco-set (16) ,              -- ecological sample study
    // 4       gen-prod-set (17) ,         -- genomic products, chrom+mRNa+protein
    // 4       wgs-set (18) ,              -- whole genome shotgun project
    // 0       other (255)
    const CSeq_entry& GetComplexityLevel(CBioseq_set::EClass cls) const;

    // Get bioseq core structure
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual TBioseqCore GetBioseqCore(void) const;

    // Get sequence map.
    virtual const CSeqMap& GetSeqMap(void) const;

    // CSeqVector constructor flags
    enum EVectorCoding {
        eCoding_NotSet, // Use original coding - DANGEROUS! - may change
        eCoding_Ncbi,   // Set coding to binary coding (Ncbi4na or Ncbistdaa)
        eCoding_Iupac   // Set coding to printable coding (Iupacna or Iupacaa)
    };
    enum EVectorStrand {
        eStrand_Plus,   // Plus strand
        eStrand_Minus   // Minus strand
    };

    // Get sequence: Iupacna or Iupacaa if use_iupac_coding is true
    CSeqVector GetSeqVector(EVectorCoding coding,
                            ENa_strand strand = eNa_strand_plus) const;
    CSeqVector GetSeqVector(ENa_strand strand = eNa_strand_plus) const;
    CSeqVector GetSeqVector(EVectorCoding coding, EVectorStrand strand) const;
    CSeqVector GetSeqVector(EVectorStrand strand) const;

    // Sequence filtering: get a seq-vector for a part of the sequence.
    // The part shown depends oon the mode selected. If the location
    // contains references to other sequences they are ignored (unlike
    // CBioseq constructor, which constructs a bioseq using all references
    // from a location). Strand information from "location" is ingored
    // when creating merged or excluded views. If "minus_strand" is true,
    // the result is reverse-complement.
    enum ESequenceViewMode {
        eViewConstructed,    // Do not merge or reorder intervals
        eViewMerged,         // Merge overlapping intervals, sort by location
        eViewExcluded        // Show intervals not included in the seq-loc
    };
    CSeqVector GetSequenceView(const CSeq_loc& location,
                               ESequenceViewMode mode,
                               EVectorCoding coding = eCoding_Ncbi,
                               ENa_strand strand = eNa_strand_plus) const;


    CConstRef<CSeqMap> GetSeqMapByLocation(const CSeq_loc& location,
                                           ESequenceViewMode mode) const;

    CScope& GetScope(void) const;

    // Modification functions
    // Add/remove/replace annotations:
    void AddAnnot(CSeq_annot& annot);
    void RemoveAnnot(const CSeq_annot& annot);
    void ReplaceAnnot(const CSeq_annot& old_annot, CSeq_annot& new_annot);

private:
    CBioseq_Handle(const CSeq_id_Handle& value,
                   CScope& scope,
                   CBioseq_Info& bioseq);

    // Get the internal seq-id handle
    const CSeq_id_Handle&  GetKey(void) const;

    // Get data source
    CDataSource& x_GetDataSource(void) const;

    bool x_IsSynonym(const CSeq_id& id) const;

    CSeq_id_Handle       m_Value;       // Seq-id equivalent
    CScope*              m_Scope;
    CRef<CBioseq_Info>   m_Bioseq_Info;
    TTSE_Lock            m_TSE_Lock;

    friend class CSeqVector;
    friend class CHandleRangeMap;
    friend class CDataSource;
    friend class CScope;
    friend class CAnnotTypes_CI;
};


inline
CBioseq_Handle::CBioseq_Handle(const CSeq_id_Handle& value,
                               CScope& scope,
                               CBioseq_Info& bioseq)
    : m_Value(value),
      m_Scope(&scope),
      m_Bioseq_Info(&bioseq)
{
}

inline
CBioseq_Handle::CBioseq_Handle(const CBioseq_Handle& h)
    : m_Value(h.m_Value),
      m_Scope(h.m_Scope),
      m_Bioseq_Info(h.m_Bioseq_Info)
{
    // m_TSE_Info should never be null
    if ( m_Bioseq_Info )
        m_TSE_Lock.Reset(m_Bioseq_Info->m_TSE_Info);
}

inline
CBioseq_Handle& CBioseq_Handle::operator= (const CBioseq_Handle& h)
{
    if (&h != this) {
        m_Value = h.m_Value;
        m_Scope = h.m_Scope;
        m_Bioseq_Info = h.m_Bioseq_Info;
        if ( m_Bioseq_Info )
            m_TSE_Lock.Reset(m_Bioseq_Info->m_TSE_Info);
    }
    return *this;
}

inline
const CSeq_id_Handle& CBioseq_Handle::GetKey(void) const
{
    return m_Value;
}

inline
bool CBioseq_Handle::operator== (const CBioseq_Handle& h) const
{
    if (m_Scope != h.m_Scope) {
        THROW1_TRACE(runtime_error,
            "CBioseq_Handle::operator==() -- "
            "Unable to compare handles from different scopes");
    }
    if (bool(m_Bioseq_Info)  &&  bool(h.m_Bioseq_Info))
        return m_Bioseq_Info == h.m_Bioseq_Info;
    // Compare by id key
    return m_Value == h.m_Value;
}

inline
bool CBioseq_Handle::operator!= (const CBioseq_Handle& h) const
{
    return !(*this == h);
}

inline
bool CBioseq_Handle::operator< (const CBioseq_Handle& h) const
{
    if (m_Scope != h.m_Scope) {
        THROW1_TRACE(runtime_error,
            "CBioseq_Handle::operator<() -- "
            "Unable to compare CBioseq_Handles from different scopes");
    }
    if (bool(m_Bioseq_Info)  &&  bool(h.m_Bioseq_Info))
        return m_Bioseq_Info < h.m_Bioseq_Info;
    return m_Value < h.m_Value;
}

inline
CDataSource& CBioseq_Handle::x_GetDataSource(void) const
{
    // m_TSE_Info and its m_DataSource should never be null
    if ( !m_Bioseq_Info ) {
        THROW1_TRACE(runtime_error,
            "CBioseq_Handle::x_GetDataSource() -- "
            "Can not resolve data source for bioseq handle.");
    }
    return *m_Bioseq_Info->m_TSE_Info->m_DataSource;
}

inline
CScope& CBioseq_Handle::GetScope(void) const 
{
    return *m_Scope;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.36  2003/03/27 19:39:34  grichenk
* +CBioseq_Handle::GetComplexityLevel()
*
* Revision 1.35  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.34  2003/03/18 21:48:27  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.33  2003/03/18 14:46:35  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.32  2003/03/14 19:10:33  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.31  2003/03/12 20:09:30  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.30  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.29  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.28  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.26  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/11/08 22:15:50  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.24  2002/09/03 21:26:58  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.23  2002/08/23 16:49:06  grichenk
* Added warning about using CreateResolvedSeqMap()
*
* Revision 1.22  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.21  2002/06/12 14:39:00  grichenk
* Renamed enumerators
*
* Revision 1.20  2002/06/06 19:36:02  clausen
* Added GetTitle()
*
* Revision 1.19  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.18  2002/05/21 18:39:27  grichenk
* CBioseq_Handle::GetResolvedSeqMap() -> CreateResolvedSeqMap()
*
* Revision 1.17  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.16  2002/05/03 18:35:36  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.15  2002/04/23 19:01:06  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.14  2002/04/22 20:06:58  grichenk
* +GetSequenceView(), +x_IsSynonym()
*
* Revision 1.13  2002/04/18 20:35:10  gouriano
* correction in comment
*
* Revision 1.12  2002/04/11 12:07:28  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.11  2002/03/19 19:17:33  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.10  2002/03/15 18:10:04  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.9  2002/03/04 15:08:43  grichenk
* Improved CTSE_Info locks
*
* Revision 1.8  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.7  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.6  2002/02/01 21:49:10  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.5  2002/01/29 17:05:53  grichenk
* GetHandle() -> GetKey()
*
* Revision 1.4  2002/01/28 19:45:33  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // BIOSEQ_HANDLE__HPP
