#ifndef ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP

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

/// @file blast_nucl_options.hpp
/// Declares the CBlastNucleotideOptionsHandle class.

#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the nucleotide-nucleotide options to the BLAST algorithm.
///
/// Adapter class for nucleotide-nucleotide BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastNucleotideOptionsHandle : 
                                            public CBlastOptionsHandle
{
public:

    /// Creates object with default options set
    CBlastNucleotideOptionsHandle();
    ~CBlastNucleotideOptionsHandle() {}
    CBlastNucleotideOptionsHandle(const CBlastNucleotideOptionsHandle& rhs);
    CBlastNucleotideOptionsHandle& operator=(const CBlastNucleotideOptionsHandle& rhs);

    virtual void SetDefaults();

    /******************* Lookup table options ***********************/
    int GetLookupTableType() const { return m_Opts->GetLookupTableType(); }
    void SetLookupTableType(int type) { m_Opts->SetLookupTableType(type); }

    unsigned char GetScanStep() const { return m_Opts->GetScanStep(); }
    void SetScanStep(unsigned char step) { m_Opts->SetScanStep(step); }

    short GetWordSize() const { return m_Opts->GetWordSize(); }
    void SetWordSize(short ws) { m_Opts->SetWordSize(ws); }

    /******************* Query setup options ************************/
    objects::ENa_strand GetStrandOption() const { 
        return m_Opts->GetStrandOption();
    }
    void SetStrandOption(objects::ENa_strand strand) {
        m_Opts->SetStrandOption(strand);
    }

    /******************* Initial word options ***********************/
    int GetWindowSize() const { return m_Opts->GetWindowSize(); }
    void SetWindowSize(int ws) { m_Opts->SetWindowSize(ws); }

    SeedContainerType GetSeedContainerType() const {
        return m_Opts->GetSeedContainerType();
    }
    void SetSeedContainerType(SeedContainerType sct) {
        m_Opts->SetSeedContainerType(sct);
    }

    SeedExtensionMethod GetSeedExtensionMethod() const {
        return m_Opts->GetSeedExtensionMethod();
    }
    // Note that the scan step (or stride) is changed as a side effect of
    // calling this method because. This is because the scan step is best
    // calculated for the eRightAndLeft seed extension method and a fixed value
    // is used for the eRight seed extension method.
    void SetSeedExtensionMethod(SeedExtensionMethod sem) {
        switch (sem) {
        case eRight: 
            SetScanStep(COMPRESSION_RATIO);
            break;
        case eRightAndLeft:
        {
            unsigned int s = CalculateBestStride(GetWordSize(), 
                                                 GetVariableWordSize(), 
                                                 GetLookupTableType());
            SetScanStep(s);
            break;
        }
        default:
            abort();
        }
        m_Opts->SetSeedExtensionMethod(sem);
    }

    bool GetVariableWordSize() const { return m_Opts->GetVariableWordSize(); }
    void SetVariableWordSize(bool val = true) { 
        m_Opts->SetVariableWordSize(val); 
    }

    bool GetUngappedExtension() const { return m_Opts->GetUngappedExtension();}
    void SetUngappedExtension(bool val = true) { 
        m_Opts->SetUngappedExtension(val); 
    }

    double GetXDropoff() const { return m_Opts->GetXDropoff(); } 
    void SetXDropoff(double x) { m_Opts->SetXDropoff(x); }

    /******************* Gapped extension options *******************/
    double GetGapXDropoffFinal() const { 
        return m_Opts->GetGapXDropoffFinal(); 
    }
    void SetGapXDropoffFinal(double x) { m_Opts->SetGapXDropoffFinal(x); }

    int GetGapExtnAlgorithm() const { return m_Opts->GetGapExtnAlgorithm(); }
    void SetGapExtnAlgorithm(int algo) { m_Opts->SetGapExtnAlgorithm(algo); }

    /************************ Scoring options ************************/
    int GetMatchReward() const { return m_Opts->GetMatchReward(); }
    void SetMatchReward(int r) { m_Opts->SetMatchReward(r); }

    int GetMismatchPenalty() const { return m_Opts->GetMismatchPenalty(); }
    void SetMismatchPenalty(int p) { m_Opts->SetMismatchPenalty(p); }

    /******************** Effective Length options *******************/
    Int8 GetDbLength() const { return m_Opts->GetDbLength(); }
    void SetDbLength(Int8 len) { m_Opts->SetDbLength(len); }

    unsigned int GetDbSeqNum() const { return m_Opts->GetDbSeqNum(); }
    void SetDbSeqNum(unsigned int num) { m_Opts->SetDbSeqNum(num); }

    Int8 GetEffectiveSearchSpace() const { 
        return m_Opts->GetEffectiveSearchSpace(); 
    }
    void SetEffectiveSearchSpace(Int8 eff) {
        m_Opts->SetEffectiveSearchSpace(eff);
    }

    bool GetUseRealDbSize() const { return m_Opts->GetUseRealDbSize(); }
    void SetUseRealDbSize(bool u = true) { m_Opts->SetUseRealDbSize(u); }


    void SetTraditionalBlastnDefaults();
    void SetTraditionalMegablastDefaults();

protected:
    virtual void SetLookupTableDefaults();
    virtual void SetMBLookupTableDefaults();
    virtual void SetQueryOptionDefaults();
    virtual void SetInitialWordOptionsDefaults();
    virtual void SetMBInitialWordOptionsDefaults();
    virtual void SetGappedExtensionDefaults();
    virtual void SetScoringOptionsDefaults();
    virtual void SetMBScoringOptionsDefaults();
    virtual void SetHitSavingOptionsDefaults();
    virtual void SetEffectiveLengthsOptionsDefaults();
    virtual void SetSubjectSequenceOptionsDefaults();
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.1  2003/11/26 18:22:14  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP */
