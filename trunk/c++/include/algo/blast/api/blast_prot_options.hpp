#ifndef ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP

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
 * Authors:  Christiam Camacho
 *
 */

/// @file blast_prot_options.hpp
/// Declares the CBlastProteinOptionsHandle class.


#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the protein-protein options to the BLAST algorithm.
///
/// Adapter class for protein-protein BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastProteinOptionsHandle : 
                                            public CBlastOptionsHandle
{
public:
    
    /// Creates object with default options set
    CBlastProteinOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CBlastProteinOptionsHandle() {}

    /******************* Lookup table options ***********************/
    /// Returns WordThreshold
    int GetWordThreshold() const { return m_Opts->GetWordThreshold(); }
    /// Sets WordThreshold
    /// @param wt WordThreshold [in]
    void SetWordThreshold(int wt) { m_Opts->SetWordThreshold(wt); }

    /// Returns WordSize
    int GetWordSize() const { return m_Opts->GetWordSize(); }
    /// Sets WordSize
    /// @param ws WordSize [in]
    void SetWordSize(int ws) { m_Opts->SetWordSize(ws); }

    /******************* Initial word options ***********************/

    /// Returns XDropoff
    double GetXDropoff() const { return m_Opts->GetXDropoff(); } 
    /// Sets XDropoff
    /// @param x XDropoff [in]
    void SetXDropoff(double x) { m_Opts->SetXDropoff(x); }

    /******************* Query setup options ************************/
    /// Is SEG filtering enabled?
    bool GetSegFiltering() const { return m_Opts->GetSegFiltering(); }
    /// Enable SEG filtering.
    /// @param val enable SEG filtering [in]
    void SetSegFiltering(bool val) { m_Opts->SetSegFiltering(val); }

    /// Get window parameter for seg
    int GetSegFilteringWindow() const { return m_Opts->GetSegFilteringWindow(); }
    /// Set window parameter for seg.  Acceptable value are > 0.  
    /// @param window seg filtering parameter window [in]
    void SetSegFilteringWindow(int window) { m_Opts->SetSegFilteringWindow(window); }

    /// Get locut parameter for seg
    double GetSegFilteringLocut() const { return m_Opts->GetSegFilteringLocut(); }
    /// Set locut parameter for seg.  Acceptable values are greater than 0.
    /// @param locut seg filtering parameter locut [in]
    void SetSegFilteringLocut(double locut) { m_Opts->SetSegFilteringLocut(locut); }

    /// Get hicut parameter for seg
    double GetSegFilteringHicut() const { return m_Opts->GetSegFilteringHicut(); }
    /// Set hicut parameter for seg.  Acceptable values are greater than Locut 
    ///  (see SetSegFilteringLocut).
    /// @param hicut seg filtering parameter hicut [in]
    void SetSegFilteringHicut(double hicut) { m_Opts->SetSegFilteringHicut(hicut); }

    /************************ Scoring options ************************/
    /// Returns MatrixName
    const char* GetMatrixName() const { return m_Opts->GetMatrixName(); }
    /// Sets MatrixName
    /// @param matrix MatrixName [in]
    void SetMatrixName(const char* matrix) { m_Opts->SetMatrixName(matrix); }

    /// Returns GapOpeningCost
    int GetGapOpeningCost() const { return m_Opts->GetGapOpeningCost(); }
    /// Sets GapOpeningCost
    /// @param g GapOpeningCost [in]
    void SetGapOpeningCost(int g) { m_Opts->SetGapOpeningCost(g); }

    /// Returns GapExtensionCost
    int GetGapExtensionCost() const { return m_Opts->GetGapExtensionCost(); }
    /// Sets GapExtensionCost
    /// @param e GapExtensionCost [in]
    void SetGapExtensionCost(int e) { m_Opts->SetGapExtensionCost(e); }
    
protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastp", "plain");
    }
    
    /// Overrides LookupTableDefaults for protein options
    virtual void SetLookupTableDefaults();
    /// Overrides QueryOptionDefaults for protein options
    virtual void SetQueryOptionDefaults();
    /// Overrides InitialWordOptionsDefaults for protein options
    virtual void SetInitialWordOptionsDefaults();
    /// Overrides GappedExtensionDefaults for protein options
    virtual void SetGappedExtensionDefaults();
    /// Overrides ScoringOptionsDefaults for protein options
    virtual void SetScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for protein options
    virtual void SetHitSavingOptionsDefaults();
    /// Overrides EffectiveLengthsOptionsDefaults for protein options
    virtual void SetEffectiveLengthsOptionsDefaults();
    /// Overrides SubjectSequenceOptionsDefaults for protein options
    virtual void SetSubjectSequenceOptionsDefaults(); 

private:
    /// Disallow copy constructor
    CBlastProteinOptionsHandle(const CBlastProteinOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastProteinOptionsHandle& operator=(const CBlastProteinOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2006/06/12 17:16:44  madden
 * Remove [GS]etMatrixPath
 *
 * Revision 1.14  2005/05/09 20:08:48  bealer
 * - Add program and service strings to CBlastOptions for remote blast.
 * - New CBlastOptionsHandle constructor for CRemoteBlast.
 * - Prohibit copy construction/assignment for CRemoteBlast.
 * - Code in each BlastOptionsHandle derived class to set program+service.
 *
 * Revision 1.13  2005/03/31 13:43:49  camacho
 * BLAST options API clean-up
 *
 * Revision 1.12  2005/03/10 13:20:22  madden
 * Moved [GS]etWindowSize and [GS]etEffectiveSearchSpace to CBlastOptionsHandle
 *
 * Revision 1.11  2005/03/02 16:45:24  camacho
 * Remove use_real_db_size
 *
 * Revision 1.10  2005/02/24 13:46:20  madden
 * Add setters and getters for filtering options
 *
 * Revision 1.9  2004/12/28 13:36:17  madden
 * [GS]etWordSize is now an int rather than a short
 *
 * Revision 1.8  2004/06/08 22:27:36  camacho
 * Add missing doxygen comments
 *
 * Revision 1.7  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.6  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.5  2004/03/10 14:55:02  madden
 * Added methods for get/set matrix, matrix-path, gap-opening, gap-extension
 *
 * Revision 1.4  2004/01/16 20:44:08  bealer
 * - Add locality flag for options handle classes.
 *
 * Revision 1.3  2003/12/15 23:41:36  dondosha
 * Added [gs]etters of database (subject) length and number of sequences to general options handle
 *
 * Revision 1.2  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.1  2003/11/26 18:22:16  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP */
