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
* File Description:
*
*   Translate HGVS expression to Variation-ref seq-feats.
*   HGVS nomenclature rules: http://www.hgvs.org/mutnomen/
*
* ===========================================================================
*/

#ifndef VARIATION_UTL_HPP_
#define VARIATION_UTIL_HPP_

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Seq_data.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CVariationUtil : public CObject
{
public:
    CVariationUtil(CScope& scope)
       : m_scope(&scope)
    {}

    /*!
     * Create remapped copy of the variation feat via given alignment
     * The seq-locs that are
     * in scope for remapping: feat.location, variation-ref.location
     * and the recursive counterparts, Variation-inst.delta.seq.loc
     * where the id of the loc is also represented in the location
     * of the parent variation-ref.
     */
    CRef<CSeq_feat> Remap(const CSeq_feat& variation_feat, const CSeq_align& aln);


    /// Convert protein-variation (single-AA missense/nonsense) to nuc-variation on the parent.
    CRef<CSeq_feat> ProtToPrecursor(const CSeq_feat& prot_variation_feat);

    /// Convert to nuc-variation on the parent to protein-variation (single-AA missense/nonsense)
    /// Only a subset of sequence edits is supported (mismatch, ins, del, indel)
    /// The location must fall within annotated CDS
    CRef<CSeq_feat> PrecursorToProt(const CSeq_feat& prot_variation_feat);


    /// todo: implement when schema captures asserted allele
    bool ValidateAllele(const CSeq_feat& variation_feat);

    /// todo: implement when decide on representation in the schema
    void FlipStrand(CSeq_feat& variation_feat);

    /// Calculate upstream (first) and downstream(second) flanks for loc
    struct SFlankLocs
    {
        CRef<CSeq_loc> upstream;
        CRef<CSeq_loc> downstream;
    };
    SFlankLocs CreateFlankLocs(const CSeq_loc& loc, TSeqPos len);

    /*!
     * Calculate location of variation-sets as union of the members.
     * If all members have the same location, their locations are reset
     * (recorded only at the set level)
     */
    static void s_FactorOutLocsInPlace(CVariation_ref& v);

    /// Propagate parent variation location to the members of set, unles they have their own location set.
    static void s_PropagateLocsInPlace(CVariation_ref& v);

private:
    void s_UntranslateProt(const string& prot_str, vector<string>& codons);

    size_t CVariationUtil::s_CountMatches(const string& a, const string& b);

    void s_CalcPrecursorVariationCodon(
            const string& codon_from, //codon on cDNA
            const string& prot_to,    //missense/nonsense AA
            vector<string>& codons_to);           //calculated variation-codon

    string s_CollapseAmbiguities(const vector<string>& seqs);

    /*!
     * Apply offsets to the variation's location (variation must be inst)
     * E.g. the original variation could be a transcript location with offsets specifying intronic location.
     * After original transcript lociation is remapped, the offsets are to be applied to the remapped location
     * to get absolute offset-free genomic location
     */
    static void s_ResolveIntronicOffsets(CVariation_ref& v);

    /*!
     * If start|stop don't fall into an exon, adjust start|stop to the closest exon boundary and add offset to inst.
     * This is to be applied before remapping a genomic variation to transcript coordinates
     */
    static void s_AddIntronicOffsets(CVariation_ref& v, const CSpliced_seg& ss);
    CRef<CScope> m_scope;
};

END_NCBI_SCOPE;
#endif
