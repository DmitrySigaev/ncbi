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
 *   'seqloc.asn'.
 */

#ifndef OBJECTS_SEQLOC_SEQ_ID_HPP
#define OBJECTS_SEQLOC_SEQ_ID_HPP


// generated includes
#include <objects/seqloc/Seq_id_.hpp>
#include <corelib/ncbi_limits.hpp>
#include <serial/serializable.hpp>

#include <objects/seqloc/Textseq_id.hpp>

// generated classes

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CBioseq;


class NCBI_SEQLOC_EXPORT CSeq_id : public CSeq_id_Base,
                                   public CSerializable
{
    typedef CSeq_id_Base Tparent;

public:

    //
    // See also CSeq_id related functions in "util/sequence.hpp":
    //
    //   TSeqPos GetLength(const CSeq_id&, CScope*);
    //   bool IsSameBioseq(const CSeq_id&, const CSeq_id&, CScope*);
    //

    // Default constructor
    CSeq_id( void );

    // Takes either a FastA-style string delimited by vertical bars or
    // a raw accession (with optional version).
    CSeq_id( const string& the_id );

    // Construct a seq-id from a dbtag
    CSeq_id(const CDbtag& tag, bool set_as_general = true);

    // With proper choice
    CSeq_id(CSeq_id_Base::E_Choice the_type,
            int           int_seq_id);     // see explanation in x_Init below
    // With proper choice
    CSeq_id(CSeq_id_Base::E_Choice the_type,
            const string&          acc_in,  // see explanation in x_Init below
            const string&          name_in,
            // force not optional; if not given, use the constructor below
            const string&          version_in,
            const string&          release_in = kEmptyStr);

    CSeq_id(CSeq_id_Base::E_Choice the_type,
            const string&          acc_in,  // see explanation in x_Init below
            const string&          name_in,
            int                    version    = 0,
            const string&          release_in = kEmptyStr);

    // Need to lookup choice
    CSeq_id(const string& the_type,
            const string& acc_in,     // see explanation in x_Init below
            const string& name_in,
            // force not optional; if not given, use the constructor below
            const string& version_in,
            const string& release_in = kEmptyStr);

    CSeq_id(const string& the_type,
            const string& acc_in,   // see explanation in x_Init below
            const string& name_in,
            int           version    = 0 ,
            const string& release_in = kEmptyStr);

    // Destructor
    virtual ~CSeq_id(void);

    // Converts a string to a choice, no need to require a member.
    static CSeq_id::E_Choice WhichInverseSeqId(const char* SeqIdCode);

    // For s_IdentifyAccession (below)
    enum EAccessionInfo {
        // Mask for Seq_id type; allow 8 bits to be safe
        eAcc_type_mask = 0xff,

        // Useful flags
        fAcc_nuc       = 0x80000000,
        fAcc_prot      = 0x40000000,
        fAcc_predicted = 0x20000000, // only for refseq
        eAcc_flag_mask = 0xe0000000,

        // Common divisions and categories (0 << 8 .. 127 << 8)
        eAcc_other         = 0 << 8, // no further classification
        eAcc_est           = 1 << 8,
        eAcc_dirsub        = 2 << 8, // direct submission; trumps other values
        eAcc_genome        = 3 << 8,
        eAcc_div_patent    = 4 << 8,
        eAcc_htgs          = 5 << 8,
        eAcc_con           = 6 << 8, // just a contig/segset
        eAcc_segset        = eAcc_con, // was once wrongly split out
        eAcc_wgs           = 7 << 8,
        eAcc_division_mask = 0xff00,

        // Actual return values with EXAMPLE prefixes (to be followed
        // by digits), grouped by Seq-id type.  In most cases, there
        // are other prefixes with the same classification, and if not
        // there could be.
        eAcc_unknown         = e_not_set | eAcc_other,
        eAcc_unreserved_nuc  = e_not_set | 128 << 8 | fAcc_nuc,  // XY
        eAcc_unreserved_prot = e_not_set | 128 << 8 | fAcc_prot, // XYZ
        eAcc_ambiguous_nuc   = e_not_set | 192 << 8 | fAcc_nuc,  // N0-N1
        // Most N accessions are GenBank ESTs, but some low-numbered ones
        // (now only used as primary accessions) were assigned haphazardly,
        // and some are therefore ambiguous.
        eAcc_maybe_gb        = eAcc_ambiguous_nuc | 1,
        eAcc_maybe_embl      = eAcc_ambiguous_nuc | 2,
        eAcc_maybe_ddbj      = eAcc_ambiguous_nuc | 4,
        eAcc_gb_embl         = eAcc_maybe_gb | eAcc_maybe_embl,
        eAcc_gb_ddbj         = eAcc_maybe_gb | eAcc_maybe_ddbj,
        eAcc_embl_ddbj       = eAcc_maybe_embl | eAcc_maybe_ddbj,
        eAcc_gb_embl_ddbj    = (eAcc_maybe_gb | eAcc_maybe_embl
                                | eAcc_maybe_ddbj),

        eAcc_local  = e_Local  | eAcc_other,
        eAcc_gibbsq = e_Gibbsq | eAcc_other,
        eAcc_gibbmt = e_Gibbmt | eAcc_other,
        eAcc_giim   = e_Giim   | eAcc_other,

        eAcc_gb_prot        = e_Genbank | eAcc_other      | fAcc_prot, // AAA
        eAcc_gb_other_nuc   = e_Genbank | eAcc_other      | fAcc_nuc,  // AS
        eAcc_gb_est         = e_Genbank | eAcc_est        | fAcc_nuc,  // H
        eAcc_gb_dirsub      = e_Genbank | eAcc_dirsub     | fAcc_nuc,  // U
        eAcc_gb_genome      = e_Genbank | eAcc_genome     | fAcc_nuc,  // AE
        eAcc_gb_patent      = e_Genbank | eAcc_div_patent /* | fAcc_nuc */, //I
        eAcc_gb_patent_prot = e_Genbank | eAcc_div_patent | fAcc_prot, // AAE
        eAcc_gb_htgs        = e_Genbank | eAcc_htgs       | fAcc_nuc,  // AC
        eAcc_gb_con         = e_Genbank | eAcc_con,                    // CH
        eAcc_gb_segset      = eAcc_gb_con, // for compatibility
        eAcc_gb_wgs_nuc     = e_Genbank | eAcc_wgs        | fAcc_nuc,  // AAAA
        eAcc_gb_wgs_prot    = e_Genbank | eAcc_wgs        | fAcc_prot, // EAA
        eAcc_gsdb_dirsub    = e_Genbank | 128 << 8        | fAcc_nuc,  // J
        eAcc_gb_gsdb        = e_Genbank | 129 << 8        | fAcc_nuc,  // AD
        eAcc_gb_gss         = e_Genbank | 130 << 8        | fAcc_nuc,  // B
        eAcc_gb_sts         = e_Genbank | 131 << 8        | fAcc_nuc,  // G
        eAcc_gb_backbone    = e_Genbank | 132 << 8        | fAcc_nuc,  // S
        eAcc_gb_cdna        = e_Genbank | 133 << 8        | fAcc_nuc,  // BC

        eAcc_embl_prot      = e_Embl | eAcc_other      | fAcc_prot, // CAA
        eAcc_embl_other_nuc = e_Embl | eAcc_other      | fAcc_nuc,  // N00060
        eAcc_embl_est       = e_Embl | eAcc_est        | fAcc_nuc,  // F
        eAcc_embl_dirsub    = e_Embl | eAcc_dirsub     | fAcc_nuc,  // V
        eAcc_embl_genome    = e_Embl | eAcc_genome     | fAcc_nuc,  // AL
        eAcc_embl_patent    = e_Embl | eAcc_div_patent | fAcc_nuc,  // A
        eAcc_embl_htgs      = e_Embl | eAcc_htgs       | fAcc_nuc,  // unused
        eAcc_embl_con       = e_Embl | eAcc_con        | fAcc_nuc,  // AN
        eAcc_embl_wgs_nuc   = e_Embl | eAcc_wgs        | fAcc_nuc,  // CAAA
        eAcc_embl_wgs_prot  = e_Embl | eAcc_wgs        | fAcc_prot, // unused

        eAcc_pir       = e_Pir       | eAcc_other | fAcc_prot,
        eAcc_swissprot = e_Swissprot | eAcc_other | fAcc_prot,
        eAcc_patent    = e_Patent    | eAcc_other,

        eAcc_refseq_prot            = e_Other | eAcc_other  | fAcc_prot,  //NP_
        eAcc_refseq_genome          = e_Other | eAcc_genome | fAcc_nuc,   //NS_
        eAcc_refseq_wgs_nuc         = e_Other | eAcc_wgs    | fAcc_nuc,   //NZ_
        eAcc_refseq_wgs_prot        = e_Other | eAcc_wgs    | fAcc_prot,  //ZP_
        eAcc_refseq_contig          = e_Other | eAcc_segset,              //NT_
        eAcc_refseq_unreserved      = e_Other | 128 << 8,                 //AA_
        eAcc_refseq_mrna            = e_Other | 129 << 8    | fAcc_nuc,   //NM_
        eAcc_refseq_chromosome      = e_Other | 130 << 8    | fAcc_nuc,   //NC_
        eAcc_refseq_genomic         = e_Other | 131 << 8    | fAcc_nuc,   //NG_
        // non-coding RNA
        eAcc_refseq_ncrna           = e_Other | 132 << 8    | fAcc_nuc,   //NR_
        eAcc_refseq_wgs_intermed    = e_Other | 133 << 8    | fAcc_nuc,   //NW_
        eAcc_refseq_prot_predicted  = eAcc_refseq_prot  | fAcc_predicted, //XP_
        eAcc_refseq_mrna_predicted  = eAcc_refseq_mrna  | fAcc_predicted, //XM_
        eAcc_refseq_ncrna_predicted = eAcc_refseq_ncrna | fAcc_predicted, //XR_

        eAcc_general = e_General | eAcc_other,
        eAcc_gi      = e_Gi      | eAcc_other,

        eAcc_ddbj_prot      = e_Ddbj | eAcc_other      | fAcc_prot, // BAA
        eAcc_ddbj_other_nuc = e_Ddbj | eAcc_other      | fAcc_nuc,  // N00028
        eAcc_ddbj_est       = e_Ddbj | eAcc_est        | fAcc_nuc,  // C
        eAcc_ddbj_dirsub    = e_Ddbj | eAcc_dirsub     | fAcc_nuc,  // D
        eAcc_ddbj_genome    = e_Ddbj | eAcc_genome     | fAcc_nuc,  // AP
        eAcc_ddbj_patent    = e_Ddbj | eAcc_div_patent | fAcc_nuc,  // E
        eAcc_ddbj_htgs      = e_Ddbj | eAcc_htgs       | fAcc_nuc,  // AK
        eAcc_ddbj_con       = e_Ddbj | eAcc_con        | fAcc_nuc,  // BA
        eAcc_ddbj_wgs_nuc   = e_Ddbj | eAcc_wgs        | fAcc_nuc,  // BAAA
        eAcc_ddbj_wgs_prot  = e_Ddbj | eAcc_wgs        | fAcc_prot, // GAA

        eAcc_prf = e_Prf | eAcc_other | fAcc_prot,
        eAcc_pdb = e_Pdb | eAcc_other | fAcc_prot,

        eAcc_gb_tpa_nuc  = e_Tpg | eAcc_other | fAcc_nuc,    // BK
        eAcc_gb_tpa_prot = e_Tpg | eAcc_other | fAcc_prot,   // DAA

        eAcc_embl_tpa_nuc  = e_Tpe | eAcc_other | fAcc_nuc,  // BN
        eAcc_embl_tpa_prot = e_Tpe | eAcc_other | fAcc_prot, // unused

        eAcc_ddbj_tpa_nuc  = e_Tpd | eAcc_other | fAcc_nuc,  // BR
        eAcc_ddbj_tpa_prot = e_Tpd | eAcc_other | fAcc_prot  // FAA
    };

    static E_Choice GetAccType(EAccessionInfo info)
        { return static_cast<E_Choice>(info & eAcc_type_mask); }

    // Deduces information from a bare accession a la WHICH_db_accession;
    // may report false negatives on properties.
    static EAccessionInfo IdentifyAccession(const string& accession);
    EAccessionInfo IdentifyAccession(void) const;

    // Match() - TRUE if SeqIds are equivalent
    bool Match(const CSeq_id& sid2) const;

    // Compare return values
    enum E_SIC {
        e_error = 0,  // some problem
        e_DIFF,       // different SeqId types-can't compare
        e_NO,         // SeqIds compared, but are different
        e_YES         // SeqIds compared, are equivalent
    };

    // Compare() - more general
    E_SIC Compare(const CSeq_id& sid2) const;
    int CompareOrdered(const CSeq_id& sid2) const;
    bool operator<(const CSeq_id& sid2) const
        {
            return CompareOrdered(sid2) < 0;
        }

    // Return compatible CTextseq_id
    const CTextseq_id* GetTextseq_Id(void) const;

    // Implement serializable interface
    virtual void WriteAsFasta(ostream& out) const;
    CProxy DumpAsFasta(void) const { return Dump(eAsFasta); }
    const string AsFastaString(void) const;

    // return the label for a given string
    enum ELabelType {
        eType,
        eContent,
        eBoth,
        eFasta,

        // default is to show type + content
        eDefault = eBoth
    };

    enum ELabelFlags {
        fLabel_Version            = 0x10,
        fLabel_GeneralDbIsContent = 0x20,

        // default options - always show the version
        fLabel_Default = fLabel_Version
    };
    typedef int TLabelFlags;
    void GetLabel(string*     label,
                  ELabelType  type  = eDefault,
                  TLabelFlags flags = fLabel_Default) const;

    //Return seqid string with optional version for text seqid type
    string GetSeqIdString(bool with_version = false) const;

    // Get a string representation of the sequence IDs of a given bioseq.  This
    // function produces strings in a number of possible formats.
    enum EStringFormat {
        eFormat_FastA,              // FastA format
        eFormat_ForceGI,            // GI only, in FastA format
        eFormat_BestWithoutVersion, // 'Best' accession, without the version
        eFormat_BestWithVersion     // 'Best' accession, with version
    };
    static string GetStringDescr(const CBioseq& bioseq, EStringFormat fmt);

    // Numerical quality ranking; lower is better.
    // (Text)Score and WorstRank both basically correspond to the C
    // Toolkit's SeqIdFindWorst, which favors textual accessions,
    // whereas BestRank corresponds to the C Toolkit's SeqIdFindBest
    // and favors GIs.  All three give a slight bonus to accessions
    // that carry versions.

    int AdjustScore       (int base_score) const;
    int BaseTextScore     (void)           const;
    int BaseBestRankScore (void)           const;
    int BaseWorstRankScore(void)           const;

    int TextScore     (void) const { return AdjustScore(BaseTextScore()); }
    int BestRankScore (void) const { return AdjustScore(BaseBestRankScore()); }
    int WorstRankScore(void) const
        { return AdjustScore(BaseWorstRankScore()); }

    // Wrappers for use with FindBestChoice from <corelib/ncbiutil.hpp>
    static int Score(const CRef<CSeq_id>& id)
        { return id ? id->TextScore() : kMax_Int; }
    static int BestRank(const CRef<CSeq_id>& id)
        { return id ? id->BestRankScore() : kMax_Int; }
    static int WorstRank(const CRef<CSeq_id>& id)
        { return id ? id->WorstRankScore() : kMax_Int; }

    virtual void Assign(const CSerialObject& source,
                        ESerialRecursionMode how = eRecursive);
    virtual bool Equals(const CSerialObject& object,
                        ESerialRecursionMode how = eRecursive) const;

private:
    void x_Init
    (CSeq_id_Base::E_Choice the_type,
     // Just first string, as in text seqid, for unusual
     // cases (patents, pdb) not really an acc
     const string&          acc_in,
     const string&          name_in    = kEmptyStr,
     int                    version    = 0,
     const string&          release_in = kEmptyStr);

    // Prohibit copy constructor & assignment operator
    CSeq_id(const CSeq_id&);
    CSeq_id& operator= (const CSeq_id&);

    //CRef<CAbstractObjectManager> m_ObjectManager;

};


// Search the container of CRef<CSeq_id> for the id of given type.
// Return the id of requested type, or null CRef.
template<class container>
CRef<CSeq_id> GetSeq_idByType(const container& ids,
                                    CSeq_id::E_Choice choice)
{
    ITERATE (typename container, iter, ids) {
        if ((*iter)->Which() == choice){
            return *iter;
        }
    }
    return CRef<CSeq_id>(0);
}

//return gi from id list if exists, return 0 otherwise
template<class container>
int FindGi(const container& ids)
{
    CRef<CSeq_id> id = GetSeq_idByType(ids, CSeq_id::e_Gi);
    return id ? id->GetGi() : 0;
}


//return text seq-id from id list if exists, return 0 otherwise
template<class container>
CRef<CSeq_id> FindTextseq_id(const container& ids)
{
    ITERATE (typename container, iter, ids) {
        if ( (*iter)->GetTextseq_Id() ) {
            return *iter;
        }
    }
    return CRef<CSeq_id>(0);
}


/////////////////// CSeq_id inline methods

// Match - just uses Compare
inline
bool CSeq_id::Match (const CSeq_id& sid2) const
{
    return Compare(sid2) == e_YES;
}


inline
int CSeq_id::AdjustScore(int base_score) const
{
    int score = base_score * 10;
    const CTextseq_id* text_id = GetTextseq_Id();
    if (text_id) {
        if ( !text_id->IsSetVersion() ) {
            score += 4;
        }
        if ( !text_id->IsSetAccession() ) {
            score += 3;
        }
        if ( !text_id->IsSetName() ) {
            score += 2;
        }
    }
    return score;
}


inline
int CSeq_id::BaseTextScore(void) const
{
    switch (Which()) {
    case e_not_set:                                return kMax_Int;
    case e_Giim:    case e_Gi:                     return 20;
    case e_General: case e_Gibbsq: case e_Gibbmt:  return 15;
    case e_Local:   case e_Patent:                 return 10;
    case e_Other:                                  return 8;
    default:                                       return 5;
    }
}


inline
int CSeq_id::BaseBestRankScore(void) const
{
    switch (Which()) {
    case e_not_set:                               return 83;
    case e_General: case e_Local:                 return 80;
    case e_Gibbsq: case e_Gibbmt: case e_Giim:    return 70;
    case e_Patent:                                return 67;
    case e_Other:                                 return 65;

    case e_Ddbj: case e_Prf: case e_Pdb:
    case e_Tpe:  case e_Tpd: case e_Embl:
    case e_Pir:  case e_Swissprot:
    case e_Tpg:  case e_Genbank:                  return 60;

    case e_Gi:                                    return 51;
    default:                                      return 5;
    }
}


inline
int CSeq_id::BaseWorstRankScore(void) const
{
    switch (Which()) {
    case e_not_set:                               return 83;
    case e_Gi: case e_Giim:                       return 20;
    case e_General: case e_Gibbsq: case e_Gibbmt: return 15;
    case e_Local: case e_Patent:                  return 10;
    case e_Other:                                 return 8;

    case e_Ddbj: case e_Prf: case e_Pdb:
    case e_Tpe:  case e_Tpd: case e_Embl:
    case e_Pir:  case e_Swissprot:
    case e_Tpg:  case e_Genbank:                  return 5;

    default:                                      return 3;
    }
}


/////////////////// end of CSeq_id inline methods


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.48  2004/06/28 14:20:42  dicuccio
 * Added flag for GetLabel(): treat the 'db' portion of a general ID as content
 *
 * Revision 1.47  2004/05/11 14:34:12  ucko
 * Factored out and refined Textseq-id bonus calculation.
 *
 * Revision 1.46  2004/04/28 14:13:17  grichenk
 * Added FindTextseq_id()
 *
 * Revision 1.45  2004/04/19 18:20:16  grichenk
 * Added GetSeq_idByType() and FindGi()
 *
 * Revision 1.44  2004/04/01 16:36:34  ucko
 * Make the scoring functions wrappers around const methods (with slightly
 * different names to avoid ambiguity when passing CSeq_id::Score et al. to
 * FindBestChoice) and add a more verbose comment.  Also, WorstRank,
 * despite being "Worst," should still favor versioned accessions.
 *
 * Revision 1.43  2004/03/30 20:46:08  ucko
 * BestRank: demote gibb* and patent to 70 and 67 respectively per the C Toolkit.
 *
 * Revision 1.42  2004/03/25 15:58:41  gouriano
 * Added possibility to copy and compare serial object non-recursively
 *
 * Revision 1.41  2004/03/01 18:26:04  dicuccio
 * Modified CSeq_id::Score(), CSeq_id::BestRank(), and CSeq_id::WorstRank to
 * consider an accession's version if present.
 *
 * Revision 1.40  2004/01/22 21:03:58  dicuccio
 * Separated functionality of enums in GetLabel() into discrete mode and flags
 *
 * Revision 1.39  2004/01/22 18:45:45  dicuccio
 * Added new API: CSeq_id::GetLabel().  Rewired GetSeqIdString() to feed into
 * GetLabel().  Rewired GetStringDescr() to feed into GetLabel() directly instead
 * of feeding through GetSeqIdString().
 *
 * Revision 1.38  2004/01/21 18:04:20  dicuccio
 * Added ctor to create a seq-id from a given dbtag, performing conversion to
 * specific seq-id types where possible
 *
 * Revision 1.37  2004/01/16 22:11:15  ucko
 * Update for new CSerializable interface.
 *
 * Revision 1.36  2004/01/09 19:54:13  ucko
 * Give EXAMPLE prefixes to ease migration from hardcoded checks.
 *
 * Revision 1.35  2003/10/24 14:55:10  ucko
 * Merged eAcc_con and eAcc_segset (had been mistakenly separated);
 * renamed eAcc_gb_segset to eAcc_gb_con, but left the old name as a synoynm.
 *
 * Revision 1.34  2003/02/06 22:23:29  vasilche
 * Added CSeq_id::Assign(), CSeq_loc::Assign().
 * Added int CSeq_id::Compare() (not safe).
 * Added caching of CSeq_loc::GetTotalRange().
 *
 * Revision 1.33  2003/02/04 15:15:11  grichenk
 * Overrided Assign() for CSeq_loc and CSeq_id
 *
 * Revision 1.32  2003/01/18 08:40:04  kimelman
 * addes seqid constructor for numeric types
 *
 * Revision 1.31  2003/01/07 19:52:07  ucko
 * Add more refseq types (NR_, NS_, NW_) and list the prefixes as comments.
 *
 * Revision 1.30  2002/12/26 16:39:22  vasilche
 * Object manager class CSeqMap rewritten.
 *
 * Revision 1.29  2002/12/26 12:43:42  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.28  2002/11/26 15:12:15  dicuccio
 * Added general processing function to retrieve sequence id descriptions in a
 * number of formats.
 *
 * Revision 1.27  2002/10/23 18:22:57  ucko
 * Add self-classification (using known type information) and expand
 * EAccessionInfo accordingly.
 *
 * Revision 1.26  2002/10/22 20:18:30  jianye
 * Added GetSeqIdString()
 *
 * Revision 1.25  2002/10/03 18:51:11  clausen
 * Removed extra whitespace
 *
 * Revision 1.24  2002/10/03 17:06:13  clausen
 * Added BestRank() and WorstRank()
 *
 * Revision 1.23  2002/08/22 21:25:26  ucko
 * Added a standard score function for FindBestChoice corresponding to
 * the table in SeqIdFindWorst.
 *
 * Revision 1.22  2002/08/16 19:25:14  ucko
 * +eAcc_refseq_wgs_*
 *
 * Revision 1.21  2002/08/14 15:52:05  ucko
 * Add eAcc_refseq_ncrna(_predicted) [non-coding RNA, XR_].
 *
 * Revision 1.20  2002/08/01 20:32:52  ucko
 * s_IdentifyAccession -> IdentifyAccession; s_ is only for module-static names.
 *
 * Revision 1.19  2002/07/30 19:41:33  ucko
 * Add s_IdentifyAccession (along with EAccessionInfo and GetAccType).
 * Move CVS log to end.
 *
 * Revision 1.18  2002/06/07 11:16:57  clausen
 * Fixed comments
 *
 * Revision 1.17  2002/06/07 11:13:01  clausen
 * Added comment about util/sequence.hpp
 *
 * Revision 1.16  2002/06/06 20:32:01  clausen
 * Moved methods using object manager to objects/util
 *
 * Revision 1.15  2002/05/22 14:03:34  grichenk
 * CSerialUserOp -- added prefix UserOp_ to Assign() and Equals()
 *
 * Revision 1.14  2002/05/03 21:28:04  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.13  2002/01/10 18:43:34  clausen
 * Added GetLength
 *
 * Revision 1.12  2001/08/31 15:59:59  clausen
 * Added new constructors for FastA construction and added tpg, tpd, and tpe id types
 *
 * Revision 1.11  2001/07/25 19:11:09  grichenk
 * Equals() and Assign() re-declared as protected
 *
 * Revision 1.10  2001/07/16 16:22:44  grichenk
 * Added CSerialUserOp class to create Assign() and Equals() methods for
 * user-defind classes.
 * Added SerialAssign<>() and SerialEquals<>() functions.
 *
 * Revision 1.9  2001/06/25 18:52:01  grichenk
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.8  2001/04/17 04:13:03  vakatov
 * Utilize the redesigned "CSerializable" base class.
 * Completely get rid of the non-standard "AsFastaString()" method in
 * favor of more standard "DumpAsFasta()" one.
 *
 * Revision 1.7  2001/04/16 16:55:19  kholodov
 * Modified: Added implementation for the ISerializable interface.
 *
 * Revision 1.6  2001/01/03 16:38:53  vasilche
 * Added CAbstractObjectManager - stub for object manager.
 * CRange extracted to separate file.
 *
 * Revision 1.5  2000/12/26 17:28:34  vasilche
 * Simplified and formatted code.
 *
 * Revision 1.4  2000/12/08 22:18:41  ostell
 * changed MakeFastString to AsFastaString and to use ostream instead of string
 *
 * Revision 1.3  2000/12/08 20:45:56  ostell
 * added MakeFastaString()
 *
 * Revision 1.2  2000/11/27 20:36:39  vasilche
 * Enum should be defined in public area.
 *
 * Revision 1.1  2000/11/21 18:58:12  vasilche
 * Added Match() methods for CSeq_id, CObject_id and CDbtag.
 *
 * ===========================================================================
 */

#endif // OBJECTS_SEQLOC_SEQ_ID_HPP
