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
 */

/// @file Variation_ref.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'seqfeat.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Variation_ref_.hpp


#ifndef OBJECTS_SEQFEAT_VARIATION_REF_HPP
#define OBJECTS_SEQFEAT_VARIATION_REF_HPP


// generated includes
#include <objects/seqfeat/Variation_ref_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_SEQFEAT_EXPORT CVariation_ref : public CVariation_ref_Base
{
    typedef CVariation_ref_Base Tparent;
public:
    // constructor
    CVariation_ref(void);
    // destructor
    ~CVariation_ref(void);

    /// Enum governing sequence types for supplied sequence strings
    enum ESeqType {
        eSeqType_na,
        eSeqType_aa
    };

    /// PostRead() hooks to establish correct, non-deprecated data layout
    void PostRead();


    void SetSNV(const CSeq_data& nucleotide, 
                CRef<CDelta_item> offset=null);

    void SetMNP(const CSeq_data& nucleotide, 
                TSeqPos length,
                CRef<CDelta_item> offset=null);

    void SetMissense(const CSeq_data& amino_acid);

    /// Set a standard single nucleotide variant.  The replaces set can include
    /// empty strings and/or '-' as a character to indicate a deletion.
    void SetSNV(const vector<string>& replaces,
                ESeqType seq_type);
    bool IsSNV() const;

    /// Set a standard multinucleotide variant.  The replaces set can include
    /// empty strings and/or '-' as a character to indicate a deletion.
    void SetMNP(const vector<string>& replaces,
                ESeqType seq_type);
    bool IsMNP() const;

    /// Make this variant a deletion
    void SetDeletion();
    bool IsDeletion() const;

    /// Make this variant an insertion
    void SetInsertion(const string& sequence, ESeqType seq_type);
    bool IsInsertion() const;

    /// Make this variant an insertion of unknown sequence
    void SetInsertion();

    /// Make this variant an insertion
    void SetDeletionInsertion(const string& sequence, ESeqType seq_type);
    bool IsDeletionInsertion() const;

    /// Set the standard fields for a microsatellite.  This API establishes a
    /// microsatellite with a range of possible observed repeats.
    void SetMicrosatellite(const string& nucleotide_seq,
                           TSeqPos min_repeats, TSeqPos max_repeats);
    bool IsMicrosatellite() const;

    /// Set the standard fields for a microsatellite.  This API establishes a
    /// microsatellite with a fixed set of possible observed repeats
    void SetMicrosatellite(const string& nucleotide_seq,
                           const vector<TSeqPos>& observed_repeats);

    /// Make this variant a copy number variant.  NOTE: This API variant
    /// establishes a CNV of unknown copy number
    void SetCNV();
    bool IsCNV() const;

    /// Special subtype of CNV: 'gain' - an unspecified increase in copy number
    void SetGain();
    bool IsGain() const;

    /// Special subtype of CNV: 'loss' - an unspecified decrease in copy number
    void SetLoss();
    bool IsLoss() const;

    /// Make this variant a copy number variant.  NOTE: This API variant
    /// establishes a CNV with a range of possible copies
    void SetCNV(TSeqPos min_copy, TSeqPos max_copy);

    /// Make this variant a copy number variant.  NOTE: This API variant
    /// establishes a CNV with a fixed set of possible copies
    void SetCNV(const vector<TSeqPos>& observed_copies);

    /// The feature represents an inversion at the specified location
    /// The provided location should be upstream and on the opposite strand
    void SetInversion(const CSeq_loc& other_loc);
    bool IsInversion() const;

    /// The feature represents an eversion at the specified location
    /// The provided location should be downstream and on the opposite strand
    void SetEversion(const CSeq_loc& other_loc);
    bool IsEversion() const;

    /// The feature represents a translocation event
    /// The provided location can be anywhere; a special case exists when the
    /// provided location is on a different chromosome, in which case the
    /// feature is considered a transchromosomal rearrangement
    void SetTranslocation(const CSeq_loc& other_loc);
    bool IsTranslocation() const;

    /// Establish a uniparental disomy mark-up
    void SetUniparentalDisomy();
    bool IsUniparentalDisomy() const;

    /// Create a complex undescribed variant
    void SetComplex();
    bool IsComplex() const;

    /// Create a variant of unknown type
    void SetUnknown();
    bool IsUnknown() const;

    /// Create a variant of type 'other'
    void SetOther();
    bool IsOther() const;

    /// Validate that all semantic fields are correct
    void Validate();

    /// @name Deprecated Methods
    /// @{

    /// NOTE: THESE ARE GOING AWAY SOON!!
    NCBI_DEPRECATED bool IsSetLocation(void) const;
    NCBI_DEPRECATED bool CanGetLocation(void) const;
    NCBI_DEPRECATED void ResetLocation(void);
    NCBI_DEPRECATED const TLocation& GetLocation(void) const;
    NCBI_DEPRECATED void SetLocation(TLocation& value);
    NCBI_DEPRECATED TLocation& SetLocation(void);

    NCBI_DEPRECATED bool IsSetExt_locs(void) const;
    NCBI_DEPRECATED bool CanGetExt_locs(void) const;
    NCBI_DEPRECATED void ResetExt_locs(void);
    NCBI_DEPRECATED const TExt_locs& GetExt_locs(void) const;
    NCBI_DEPRECATED TExt_locs& SetExt_locs(void);

    NCBI_DEPRECATED bool IsSetExt(void) const;
    NCBI_DEPRECATED bool CanGetExt(void) const;
    NCBI_DEPRECATED void ResetExt(void);
    NCBI_DEPRECATED const TExt& GetExt(void) const;
    NCBI_DEPRECATED void SetExt(TExt& value);
    NCBI_DEPRECATED TExt& SetExt(void);



    NCBI_DEPRECATED bool IsSetValidated(void) const;
    NCBI_DEPRECATED bool CanGetValidated(void) const;
    NCBI_DEPRECATED void ResetValidated(void);
    NCBI_DEPRECATED TValidated GetValidated(void) const;
    NCBI_DEPRECATED void SetValidated(TValidated value);
    NCBI_DEPRECATED TValidated& SetValidated(void);

    NCBI_DEPRECATED bool IsSetClinical_test(void) const;
    NCBI_DEPRECATED bool CanGetClinical_test(void) const;
    NCBI_DEPRECATED void ResetClinical_test(void);
    NCBI_DEPRECATED const TClinical_test& GetClinical_test(void) const;
    NCBI_DEPRECATED TClinical_test& SetClinical_test(void);

    NCBI_DEPRECATED bool IsSetAllele_origin(void) const;
    NCBI_DEPRECATED bool CanGetAllele_origin(void) const;
    NCBI_DEPRECATED void ResetAllele_origin(void);
    NCBI_DEPRECATED TAllele_origin GetAllele_origin(void) const;
    NCBI_DEPRECATED void SetAllele_origin(TAllele_origin value);
    NCBI_DEPRECATED TAllele_origin& SetAllele_origin(void);

    NCBI_DEPRECATED bool IsSetAllele_state(void) const;
    NCBI_DEPRECATED bool CanGetAllele_state(void) const;
    NCBI_DEPRECATED void ResetAllele_state(void);
    NCBI_DEPRECATED TAllele_state GetAllele_state(void) const;
    NCBI_DEPRECATED void SetAllele_state(TAllele_state value);
    NCBI_DEPRECATED TAllele_state& SetAllele_state(void);

    NCBI_DEPRECATED bool IsSetAllele_frequency(void) const;
    NCBI_DEPRECATED bool CanGetAllele_frequency(void) const;
    NCBI_DEPRECATED void ResetAllele_frequency(void);
    NCBI_DEPRECATED TAllele_frequency GetAllele_frequency(void) const;
    NCBI_DEPRECATED void SetAllele_frequency(TAllele_frequency value);
    NCBI_DEPRECATED TAllele_frequency& SetAllele_frequency(void);

    NCBI_DEPRECATED bool IsSetIs_ancestral_allele(void) const;
    NCBI_DEPRECATED bool CanGetIs_ancestral_allele(void) const;
    NCBI_DEPRECATED void ResetIs_ancestral_allele(void);
    NCBI_DEPRECATED TIs_ancestral_allele GetIs_ancestral_allele(void) const;
    NCBI_DEPRECATED void SetIs_ancestral_allele(TIs_ancestral_allele value);
    NCBI_DEPRECATED TIs_ancestral_allele& SetIs_ancestral_allele(void);
    /// @}

private:
    /// Prohibited deprecated functions
    /// Due to limitations in ASN spec-land, we cannot officially remove these
    /// data elements from the spec.  We can, however, hide them from client
    /// access.
    /// @{
    NCBI_DEPRECATED bool IsSetPopulation_data(void) const;
    NCBI_DEPRECATED bool CanGetPopulation_data(void) const;
    NCBI_DEPRECATED void ResetPopulation_data(void);
    NCBI_DEPRECATED const TPopulation_data& GetPopulation_data(void) const;
    NCBI_DEPRECATED TPopulation_data& SetPopulation_data(void);

    NCBI_DEPRECATED bool IsSetPub(void) const;
    NCBI_DEPRECATED bool CanGetPub(void) const;
    NCBI_DEPRECATED void ResetPub(void);
    NCBI_DEPRECATED const TPub& GetPub(void) const;
    NCBI_DEPRECATED void SetPub(TPub& value);
    NCBI_DEPRECATED TPub& SetPub(void);

    /// @}

private:
    // Prohibit copy constructor and assignment operator
    CVariation_ref(const CVariation_ref& value);
    CVariation_ref& operator=(const CVariation_ref& value);

};

NCBISER_HAVE_POST_READ(CVariation_ref)

/////////////////// CVariation_ref inline methods

// constructor
inline
CVariation_ref::CVariation_ref(void)
{
}

/////////////////// end of CVariation_ref inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQFEAT_VARIATION_REF_HPP
/* Original file checksum: lines: 86, chars: 2492, CRC32: ef8e854f */
