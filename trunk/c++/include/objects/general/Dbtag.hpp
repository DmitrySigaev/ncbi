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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'general.asn'.
 *
 * ---------------------------------------------------------------------------
 */

#ifndef OBJECTS_GENERAL_DBTAG_HPP
#define OBJECTS_GENERAL_DBTAG_HPP


// generated includes
#include <objects/general/Dbtag_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_GENERAL_EXPORT CDbtag : public CDbtag_Base
{
    typedef CDbtag_Base Tparent;
public:

    // this must be kept in sync with the (private) list in Dbtag.cpp!
    enum EDbtagType {
        eDbtagType_bad,
        
        eDbtagType_ASAP,
        eDbtagType_ATCC,
        eDbtagType_ATCC_dna,
        eDbtagType_ATCC_in_host,
        eDbtagType_AceView_WormGenes,
        eDbtagType_BDGP_EST,
        eDbtagType_BDGP_INS,
        eDbtagType_CDD,
        eDbtagType_CK,
        eDbtagType_COG,
        eDbtagType_ENSEMBL,
        eDbtagType_ESTLIB,
        eDbtagType_FANTOM_DB,
        eDbtagType_FLYBASE,
        eDbtagType_GABI,
        eDbtagType_GDB,
        eDbtagType_GI,
        eDbtagType_GO,
        eDbtagType_GOA,
        eDbtagType_GeneDB,
        eDbtagType_GeneID,
        eDbtagType_H_InvDB,
        eDbtagType_IFO,
        eDbtagType_IMGT_GENEDB,
        eDbtagType_IMGT_HLA,
        eDbtagType_IMGT_LIGM,
        eDbtagType_ISFinder,
        eDbtagType_InterimID,
        eDbtagType_Interpro,
        eDbtagType_JCM,
        eDbtagType_LocusID,
        eDbtagType_MGD,
        eDbtagType_MGI,
        eDbtagType_MIM,
        eDbtagType_MaizeGDB,
        eDbtagType_NextDB,
        eDbtagType_PGN,
        eDbtagType_PID,
        eDbtagType_PIDd,
        eDbtagType_PIDe,
        eDbtagType_PIDg,
        eDbtagType_PIR,
        eDbtagType_PSEUDO,
        eDbtagType_RATMAP,
        eDbtagType_RGD,
        eDbtagType_RZPD,
        eDbtagType_RiceGenes,
        eDbtagType_SGD,
        eDbtagType_SoyBase,
        eDbtagType_SubtiList,
        eDbtagType_UniGene,
        eDbtagType_UniProt_SwissProt,
        eDbtagType_UniProt_TrEMBL,
        eDbtagType_UniSTS,
        eDbtagType_VBASE2,
        eDbtagType_WorfDB,
        eDbtagType_WormBase,
        eDbtagType_ZFIN,
        eDbtagType_axeldb,
        eDbtagType_dbEST,
        eDbtagType_dbSNP,
        eDbtagType_dbSTS,
        eDbtagType_dictyBase,
        eDbtagType_niaEST,
        eDbtagType_taxon,

        // only approved for RefSeq
        eDbtagType_GenBank,
        eDbtagType_EMBL,
        eDbtagType_DDBJ,
        eDbtagType_REBASE,
        eDbtagType_CloneID
    };

    // constructor
    CDbtag(void);
    // destructor
    ~CDbtag(void);

    // Comparison functions
    bool Match(const CDbtag& dbt2) const;
    int Compare(const CDbtag& dbt2) const;
    
    // Appends a label to "label" based on content of CDbtag
    void GetLabel(string* label) const;

    // Test if DB is approved by the consortium.
    // 'GenBank', 'EMBL' and 'DDBJ' are approved only in the
    // context of a RefSeq record.
    bool IsApproved(bool refseq = false) const;
    // Test if DB is approved (case insensitive).
    // Returns the case sensetive DB name if approved, NULL otherwise.
    const char* IsApprovedNoCase(bool refseq = false) const;

    // Retrieve the enumerated type for the dbtag
    EDbtagType GetType(void) const;

    // Force a refresh of the internal type
    void InvalidateType(void);
    
    // Get a URL to the resource (if available)
    // @return
    //   the URL or an empty string if non is available
    string GetUrl(void) const;

private:

    // our enumerated (parsed) type
    mutable EDbtagType m_Type;

    // Prohibit copy constructor & assignment operator
    CDbtag(const CDbtag&);
    CDbtag& operator= (const CDbtag&);
};



/////////////////// CDbtag inline methods

// constructor
inline
CDbtag::CDbtag(void)
    : m_Type(eDbtagType_bad)
{
}


/////////////////// end of CDbtag inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/02/10 20:19:38  shomrat
 * IMGT/GENE-DB now legal not just for RefSeq
 *
 * Revision 1.12  2005/02/07 19:26:59  shomrat
 * Added GetUrl()
 *
 * Revision 1.11  2004/12/29 19:01:30  shomrat
 * Added VBASE2; Fixed MaizeGDB typo
 *
 * Revision 1.10  2004/10/22 15:18:26  shomrat
 * Updated DB list
 *
 * Revision 1.9  2004/08/30 13:23:53  shomrat
 * + eDbtagType_H_InvDB
 *
 * Revision 1.8  2004/05/28 20:09:44  johnson
 * Added Compare for seq-id type General (CDbtag)
 *
 * Revision 1.7  2004/04/23 16:55:29  shomrat
 * + IsApprovedNoCase
 *
 * Revision 1.6  2004/01/20 16:04:36  dicuccio
 * Implemented enumerated type interpretation of string-based database name
 *
 * Revision 1.5  2003/06/27 15:39:49  shomrat
 * Added IsApproved
 *
 * Revision 1.4  2002/12/26 12:40:33  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.3  2002/01/10 19:48:26  clausen
 * Added GetLabel
 *
 * Revision 1.2  2001/06/25 18:51:58  grichenk
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.1  2000/11/21 18:58:04  vasilche
 * Added Match() methods for CSeq_id, CObject_id and CDbtag.
 *
 * ===========================================================================
 */

#endif // OBJECTS_GENERAL_DBTAG_HPP
