#ifndef ALGO_BLAST_API___TBLASTX_OPTIONS__HPP
#define ALGO_BLAST_API___TBLASTX_OPTIONS__HPP

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

/// @file tblastx_options.hpp
/// Declares the CTBlastxOptionsHandle class.


#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the translated nucleotide-translated nucleotide options to 
/// the BLAST algorithm.
///
/// Adapter class for translated nucleotide-translated nucleotide BLAST 
/// comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CTBlastxOptionsHandle : 
                                            public CBlastProteinOptionsHandle
{
public:

    /// Creates object with default options set
    CTBlastxOptionsHandle(EAPILocality locality);
    ~CTBlastxOptionsHandle() {}

    /******************* Query setup options ************************/
    objects::ENa_strand GetStrandOption() const { 
        return m_Opts->GetStrandOption();
    }
    void SetStrandOption(objects::ENa_strand strand) {
        m_Opts->SetStrandOption(strand);
    }

    /******************* Subject sequence options *******************/
    int GetDbGeneticCode() const {
        return m_Opts->GetDbGeneticCode();
    }
    void SetDbGeneticCode(int gc) {
        m_Opts->SetDbGeneticCode(gc);
    }

    int GetQueryGeneticCode() const {
        return m_Opts->GetQueryGeneticCode();
    }
    void SetQueryGeneticCode(int gc) {
        m_Opts->SetQueryGeneticCode(gc);
    }

protected:
    void SetLookupTableDefaults();
    void SetQueryOptionDefaults();
    void SetGappedExtensionDefaults();
    void SetScoringOptionsDefaults();
    void SetHitSavingOptionsDefaults();
    void SetSubjectSequenceOptionsDefaults();

private:
    CTBlastxOptionsHandle(const CTBlastxOptionsHandle& rhs);
    CTBlastxOptionsHandle& operator=(const CTBlastxOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.6  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.5  2004/02/17 18:43:05  bealer
 * - Change *GeneticCodeStr(*) to *GeneticCode(*)
 *
 * Revision 1.4  2004/01/16 20:42:57  bealer
 * - Add locality flag for blast options handle classes.
 *
 * Revision 1.3  2003/12/09 12:40:23  camacho
 * Added windows export specifiers
 *
 * Revision 1.2  2003/12/03 16:34:59  dondosha
 * SetDbGeneticCode now fills both integer and string option
 *
 * Revision 1.1  2003/11/26 18:22:18  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___TBLASTX_OPTIONS__HPP */
