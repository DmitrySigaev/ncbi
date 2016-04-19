#ifndef _NA_IREP_TO_SEQFEAT_HPP_ 
#define _NA_IREP_TO_SEQFEAT_HPP_

#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objmgr/scope.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objtools/readers/hgvs/id_resolver.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CHgvsNaDeltaHelper 
{
public:
    static CRef<CDelta_item> GetIntronOffset(const CNtSite& nt_site);
    static CRef<CDelta_item> GetIntronOffset(const CNtSiteRange& nt_site_range);
    static CRef<CDelta_item> GetStartIntronOffset(const CNtLocation& nt_loc);
    static CRef<CDelta_item> GetStopIntronOffset(const CNtLocation& nt_loc);
    static CRef<CDelta_item> GetStartIntronOffset(const CNtInterval& nt_int);
    static CRef<CDelta_item> GetStopIntronOffset(const CNtInterval& nt_int);
};


class CNtSeqlocHelper
{
public:

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CSimpleVariant& var_desc,
                                       const CSeqVariants::TSeqtype& seq_type,
                                       CScope& scope,
                                       CVariationIrepMessageListener& listener);


    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtSite& nt_site,
                                       const CSeqVariants::TSeqtype& seq_type,
                                       CScope& scope,
                                       CVariationIrepMessageListener& listener);


    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtSiteRange& nt_range,
                                       const CSeqVariants::TSeqtype& seq_type,
                                       CScope& scope,
                                       CVariationIrepMessageListener& listener);


    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtInterval& nt_int,
                                       const CSeqVariants::TSeqtype& seq_type,
                                       CScope& scope,
                                       CVariationIrepMessageListener& listener);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtLocation& nt_loc,
                                       const CSeqVariants::TSeqtype& seq_type,
                                       CScope& scope,
                                       CVariationIrepMessageListener& listener);
private:

    static bool x_ComputeSiteIndex(const CSeq_id& seq_id,
                                   const CNtIntLimit& nt_limit,
                                   const CSeqVariants::TSeqtype& seq_type,
                                   CScope& scope,
                                   CVariationIrepMessageListener& listener,
                                   TSeqPos& site_index);

    static bool x_ComputeSiteIndex(const CSeq_id& seq_id,
                                   const CNtSite& nt_site,
                                   const CSeqVariants::TSeqtype& seq_type,
                                   CScope& scope,
                                   CVariationIrepMessageListener& listener,
                                   TSeqPos& site_index);

    static bool x_ComputeSiteIndex(const CSeq_id& seq_id,
                                   const CNtSiteRange& nt_range,
                                   const CSeqVariants::TSeqtype& seq_type,
                                   CScope& scope,
                                   CVariationIrepMessageListener& listener,
                                   TSeqPos& site_index);

    static CRef<CInt_fuzz> x_CreateIntFuzz(const CSeq_id& seq_id,
                                           const CNtSiteRange& nt_range,
                                           const CSeqVariants::TSeqtype& seq_type,
                                           CScope& scope,
                                           CVariationIrepMessageListener& listener);
};


class CHgvsNaIrepReader 
{
public:
        using TSet = CVariation_ref::C_Data::C_Set;
        using TDelta = CVariation_inst::TDelta::value_type;
       
        CHgvsNaIrepReader(CScope& scope, CVariationIrepMessageListener& message_listener) : 
            m_Scope(scope),
            m_IdResolver(Ref(new CIdResolver(scope))),
            m_MessageListener(message_listener) {}

 private:

        //CRef<CScope> m_Scope;
        CScope& m_Scope;

        CRef<CIdResolver> m_IdResolver;

        CVariationIrepMessageListener& m_MessageListener;

        bool x_LooksLikePolymorphism(const CDelins& delins) const;

        CRef<CVariation_ref> x_CreateNaIdentityVarref(const CNaIdentity& identity,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateNaIdentityVarref(const CNtLocation& nt_loc,
                                                      const string& nucleotide,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateNaSubVarref(const CNaSub& sub,
                                                 const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateNaSubVarref(const CNtLocation& nt_loc,
                                                 const string& initial_nt,
                                                 const string& final_nt,
                                                 const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDuplicationVarref(const CDuplication& dup,
                                                       const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateInsertionVarref(const CInsertion& ins,
                                                     const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDeletionVarref(const CDeletion& del,
                                                    const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDelinsVarref(const CDelins& delins,
                                                  const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateInversionVarref(const CInversion& inv,
                                                     const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateConversionVarref(const CConversion& conv, 
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateSSRVarref(const CRepeat& ssr, 
                                               const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateVarref(const string& var_name, 
                                            const CSimpleVariant& var_desc) const;

        CRef<CSeq_literal> x_CreateNtSeqLiteral(const string& raw_seq) const;

        CRef<CSeq_literal> x_CreateNtSeqLiteral(TSeqPos length) const;

        CRef<CSeq_feat> x_CreateSeqfeat(const string& var_name,
                                        const string& identifier,
                                        const CSeqVariants::TSeqtype& seq_type,
                                        const CSimpleVariant& var_desc) const;

public:
        CRef<CSeq_feat> CreateSeqfeat(CRef<CVariantExpression>& variant_expr) const;
        list<CRef<CSeq_feat>> CreateSeqfeats(CRef<CVariantExpression>& variant_expr) const;
}; // CHgvsNaIrepReader

END_objects_SCOPE
END_NCBI_SCOPE

#endif // _NA_IREP_TO_SEQFEAT_HPP_
