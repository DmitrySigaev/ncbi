#ifndef _DiscRep_Test_HPP
#define _DiscRep_Test_HPP

/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Tests and item data collection for Cpp Discrepany Report
 */

#include <objects/biblio/Auth_list.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/macro/Constraint_choice.hpp>
#include <objects/macro/Feat_qual_choice.hpp>
#include <objects/macro/Macro_feature_type_.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>      
#include <objects/pub/Pub.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include "hDiscRep_app.hpp"

using namespace ncbi;
using namespace objects;

BEGIN_NCBI_SCOPE

namespace DiscRepNmSpc {
  typedef map <string, vector <int> > Str2Ints;
  typedef map <string, int> Str2Int;
  typedef map <int, int> Int2Int;
  typedef map <string, Str2Strs> Str2MapStr2Strs;

  class CDiscTestInfo 
  {
    public:
      static bool   is_AllAnnot_run;  // checked
      static bool   is_BASES_N_run;
      static bool   is_BIOSRC_run;
      static bool   is_Biosrc_Orgmod_run;
      static bool   is_BIOSRC1_run;
      static bool   is_CdTransl_run;
      static bool   is_DESC_user_run; // checked
      static bool   is_FEAT_DESC_biosrc_run;
      static bool   is_GP_Set_run;
      static bool   is_MolInfo_run;
      static bool   is_MRNA_run;
      static bool   is_Prot_run;
      static bool   is_Rna_run;
      static bool   is_SHORT_run;
  };


  template < typename T >
  struct SCompareCRefs
  {
     bool operator() (const CRef < T >& x, const CRef < T >& y) const {
         if ( !(y.GetPointer()) ) return false;
         else if ( (!x.GetPointer()) ) return true;
         else return ((*x).str) <  ((*y).str) ;
     }
  };

  struct qualvlu_distribute {
         Str2Strs qual_vlu2src;
         vector <string> missing_item, multi_same, multi_dup, multi_all_dif;
  };
  typedef map <string, qualvlu_distribute > Str2QualVlus;

  class GeneralDiscSubDt: public CObject
  {
     public:
        GeneralDiscSubDt (const string& new_text, 
                          const string& new_str)
                           : obj_text(1, new_text), 
                             str(new_str), 
                             sf0_added(false), 
                             biosrc(0) {};

        virtual ~GeneralDiscSubDt () {};

        vector < string > obj_text;  // e.g., seq_feat list
        string str;                       // prop. of the list;
        bool  sf0_added;
        bool  isAa;
        const CBioSource* biosrc;
        map <string, int> feat_count_list;
  };

  enum ECommentTp {
     e_IsComment = 0,
     e_HasComment,
     e_OtherComment
  };


  class CRuleProperties : public CObject
  {
    public:
      CRuleProperties () {};
      ~CRuleProperties () {};

      static vector <bool>                         srch_func_empty;
      static vector <bool>                         except_empty;

      bool IsSearchFuncEmpty(const CSearch_func& func);
      class CSearchFuncMatcher : public CObject 
      {
        public:
          CSearchFuncMatcher () {};
          virtual ~CSearchFuncMatcher () {};

          //CRef <CSearchFuncMatcher> factory()  { return 0; };

          virtual void MarchFunc(const string& sch_str, const CString_constraint& str_cst) {};
          virtual void MarchFunc(const string& sch_str, const string& rule_str) {};
      };

      static vector <CRef < CSearchFuncMatcher > > srch_func_tp;
  };


  class CTestAndRepData : public CObject
  {
    public:
      CTestAndRepData() {};
      virtual ~CTestAndRepData() {};

      virtual void TestOnObj(const CBioseq_set& bioseq_set) = 0;
      virtual void TestOnObj(const CSeq_entry& seq_entry) = 0;
      virtual void TestOnObj(const CBioseq& bioseq) = 0;
      virtual void TestOnObj(const CSeq_feat& seq_feat) = 0;

      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;

      virtual string GetName() const =0;

      string GetIsComment(unsigned cnt, const string& str);
      string GetHasComment(unsigned cnt, const string& str);
      string GetDoesComment(unsigned cnt, const string& str);
      string GetOtherComment(unsigned cnt, const string& single_str, const string& plural_str);

 //  GetDiscrepancyItemTextEx() 
      string GetDiscItemText(const CBioseq& obj);
      string GetDiscItemTextForBioseqSet(const CBioseq_set& obj);
      string GetDiscItemText(const CBioseq_set& obj);
      string GetDiscItemText(const CSeq_feat& obj);
      string GetDiscItemText(const CSeqdesc& obj, const CSeq_entry& seq_entry);
      string GetDiscItemText(const CSeqdesc& obj, const CBioseq& bioseq);
      string GetDiscItemText(const CPerson_id& obj, const CSeq_entry& seq_entry);

      CBioseq* GetRepresentativeBioseqFromBioseqSet(const CBioseq_set& bioseq_set);

      string ListAuthNames(const CAuth_list& auths);
      string ListAllAuths(const CPubdesc& pubdesc);

      const CSeq_id& BioseqToBestSeqId(const CBioseq& bioseq, CSeq_id::E_Choice);
      string BioseqToBestSeqIdString(const CBioseq& bioseq, CSeq_id::E_Choice);
      string SeqLocPrintUseBestID(const CSeq_loc& seq_loc, bool range_only = false);
      string GetLocusTagForFeature(const CSeq_feat& seq_feat);
      string GetProdNmForCD(const CSeq_feat& cd_feat);
      bool IsWholeWord(const string& str, const string& phrase);
      bool IsAllCaps(const string& str);
      bool IsAllLowerCase(const string& str);
      bool IsAllPunctuation(const string& str);

      static bool IsmRNASequenceInGenProdSet(const CBioseq& bioseq);
      static bool IsProdBiomol(const CBioseq& bioseq);

      void RmvChar (string& in_out_str, string rm_chars);

      bool IsBioseqHasLineage(const CBioseq& bioseq, const string& type);
      bool HasLineage(const CBioSource& biosrc, const string& type);

      static vector <const CSeq_feat*> mix_feat, gene_feat, cd_feat, rna_feat, prot_feat;
      static vector <const CSeq_feat*> pub_feat, biosrc_feat, biosrc_orgmod_feat, rbs_feat;
      static vector <const CSeq_feat*> rna_not_mrna_feat, intron_feat, all_feat, non_prot_feat;
      static vector <const CSeq_feat*> rrna_feat, miscfeat_feat, otherRna_feat;
      static vector <const CSeq_feat*> utr3_feat, utr5_feat, exon_feat, promoter_feat;
      static vector <const CSeq_feat*> mrna_feat, trna_feat, bioseq_biosrc_feat;

      static vector <const CSeqdesc*>  pub_seqdesc,comm_seqdesc, biosrc_seqdesc, title_seqdesc;
      static vector <const CSeqdesc*>  biosrc_orgmod_seqdesc, user_seqdesc;
      static vector <const CSeqdesc*>  bioseq_biosrc_seqdesc, bioseq_molinfo, bioseq_title;

      static vector <const CSeq_entry*> pub_seqdesc_seqentry, comm_seqdesc_seqentry;
      static vector <const CSeq_entry*> biosrc_seqdesc_seqentry, title_seqdesc_seqentry;
      static vector <const CSeq_entry*> biosrc_orgmod_seqdesc_seqentry, user_seqdesc_seqentry;

    protected:
      void GetSeqFeatLabel(const CSeq_feat& seq_feat, string* label);      
      void GetProperCItem(CRef <CClickableItem>& c_item, bool* citem1_used);
      void AddSubcategories(CRef <CClickableItem>& c_item, const string& setting_name, 
           const vector <string>& itemlist, const string& desc1, const string& desc2, 
           bool copy2parent = true, ECommentTp comm=e_IsComment, const string& desc3="",
           bool halfsize = false);
      void GetTestItemList(const vector <string>& itemlist, Str2Strs& setting2list, 
                                                                   const string& delim = "$");
      void RmvRedundancy(vector <string>& item_list); //all CSeqEntry_Feat_desc tests need this
      CConstRef <CSeq_feat> GetGeneForFeature(const CSeq_feat& seq_feat);
  };



// CBioseqSet_
  class CBioseqSetTestAndRepData : public CTestAndRepData
  {
     public:
       virtual ~CBioseqSetTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) = 0;
       virtual void TestOnObj(const CSeq_entry& seq_entry) {};
       virtual void TestOnObj(const CBioseq& bioseq) {};
       virtual void TestOnObj(const CSeq_feat& seq_feat) {};

       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;
  };


  class CBioseqSet_DISC_NONWGS_SETS_PRESENT : public CBioseqSetTestAndRepData
  {
    public:
      virtual ~CBioseqSet_DISC_NONWGS_SETS_PRESENT () {};

      virtual void TestOnObj(const CBioseq_set& bioseq_set);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_NONWGS_SETS_PRESENT");}
  };



  // CSeqEntry
  class CSeqEntryTestAndRepData : public CTestAndRepData
  {
  //     friend class CTestAndRepData;

     public:
       CSeqEntryTestAndRepData () {};
       virtual ~CSeqEntryTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) {};
       virtual void TestOnObj(const CSeq_entry& seq_entry) = 0;
       virtual void TestOnObj(const CBioseq& bioseq) {};
       virtual void TestOnObj(const CSeq_feat& seq_feat) {};

       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;

     protected:
       void AddBioseqsOfSetToReport(const CBioseq_set& bioseq_set, const string& setting_name, 
                                                bool be_na = true, bool be_aa = true);
       void TestOnFeatDesc_Biosrc();
       string GetName_iden_nm() const {return ("MORE_OR_SPEC_NAMES_IDENTIFIED_BY"); } 
       string GetName_col_nm() const {return ("MORE_NAMES_COLLECTED_BY"); } 
       bool HasMoreOrSpecNames(const CBioSource& biosrc, CSubSource::ESubtype subtype, 
                                 vector <string>& submit_text, bool check_mul_nm_only = false);
       void GetSubmitText(const CAuth_list& authors, vector <string>& submit_text);
       void FindSpecSubmitText(vector <string>& submit_text);
       void AddBioseqsInSeqentryToReport(const CSeq_entry* seq_entry, 
                              const string& setting_name, bool be_na = true, bool be_aa=true);

       void TestOnBiosrc(const CSeq_entry& seq_entry);
       string GetName_multi() const {return string("ONCALLER_MULTISRC"); }
       string GetName_iso() const {
                             return string("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"); }
       bool IsBacterialIsolate(const CBioSource& biosrc);
       bool HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& biosrc);
  }; // CSeqEntryTest


  enum eMultiQual {
        e_not_multi = 0,
        e_same,
        e_dup,
        e_all_dif
  };

/* not yet ready to imple.
  class CSeqEntry_DISC_FLATFILE_FIND_ONCALLER : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_FLATFILE_FIND_ONCALLER () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("DISC_FLATFILE_FIND_ONCALLER");}
  };
*/


//  new comb!!
/*
  class CSeqEntry_test_on_quals : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_quals () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      string GetName_asn1() const {return string("DISC_SOURCE_QUALS_ASNDISC"); }
      string GetName_asn1_oncall() const {return string("DISC_SOURCE_QUALS_ASNDISC"); }
  };
*/


  class CSeqEntry_test_on_biosrc_orgmod : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_biosrc_orgmod () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;
 
    protected:
      string GetName_mism() const {return string("DISC_BACTERIAL_TAX_STRAIN_MISMATCH");}
      string GetName_cul() const {return 
                                    string("ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH");}
      string GetName_cbs() const {return string("DUP_DISC_CBS_CULTURE_CONFLICT");}
      string GetName_atcc() const {return string("DUP_DISC_ATCC_CULTURE_CONFLICT");}

      bool IsStrainInCultureCollectionForBioSource(const CBioSource& biosrc, 
                        const string& strain_head, const string& culture_head);
      void RunTests(const CBioSource& biosrc, const string& desc);
      void BiosrcHasConflictingStrainAndCultureCollectionValues(const CBioSource& biosrc, 
                                                                         const string& desc);
      void BacterialTaxShouldEndWithStrain(const CBioSource& biosrc, const string& desc);

      bool HasConflict(const list <CRef <COrgMod> >& mods, const string& subname_rest, 
                               const COrgMod::ESubtype& check_type, const string& check_head);
  };


  class CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_atcc();}
  };

 
  class CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_cbs();}
  };

  
  class CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_mism();}
  }; 

 
  class CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH : public CSeqEntry_test_on_biosrc_orgmod
  {
    public:
      virtual ~CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc_orgmod::GetName_cul();}
  };



  class CSeqEntry_test_on_user : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_user () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      string GetName_comm() const {return string("MISSING_GENOMEASSEMBLY_COMMENTS");}
      string GetName_proj() const {return string("TEST_HAS_PROJECT_ID"); }
      string GetName_scomm() const {return string("ONCALLER_MISSING_STRUCTURED_COMMENTS"); }
      string GetName_mproj() const {return string("MISSING_PROJECT"); }
      string GetName_bproj() const {return string("ONCALLER_BIOPROJECT_ID"); }
      void GroupAllBioseqs(const CBioseq_set& bioseq_set, const int& id);
      void CheckCommentCountForSet(const CBioseq_set& set, const unsigned& cnt, 
                                                                        Str2Int& bioseq2cnt);
      Str2Int m_bioseq2geno_comm;
      const CBioseq& Get1stBioseqOfSet(const CBioseq_set& bioseq_set);
      void AddBioseqsOfSet2Map(const CBioseq_set& bioseq_set);
      void RmvBioseqsOfSetOutMap(const CBioseq_set& bioseq_set);
  };

  class CSeqEntry_ONCALLER_BIOPROJECT_ID : public CSeqEntry_test_on_user
  {
     public:
      virtual ~CSeqEntry_ONCALLER_BIOPROJECT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_bproj();}
  };


  class CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS : public CSeqEntry_test_on_user
  {
     public:
      virtual ~CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_comm();}
  };

  class CSeqEntry_TEST_HAS_PROJECT_ID : public CSeqEntry_test_on_user
  {
    public:
      virtual ~CSeqEntry_TEST_HAS_PROJECT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_user::GetName_proj();}
  };


  class CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS : public CSeqEntry_test_on_user
  {
    public:
      virtual ~CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_scomm();}
  };


  class CSeqEntry_MISSING_PROJECT : public CSeqEntry_test_on_user
  {
    public:
      virtual ~CSeqEntry_MISSING_PROJECT () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_user::GetName_mproj();}
  };



  class CSeqEntry_test_on_biosrc : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_test_on_biosrc () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const =0;

    protected:
      string GetName_iso() const {return string("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1"); }
      string GetName_mult() const {return string("ONCALLER_MULTISRC"); }
      string GetName_tmiss() const {return string("TAX_LOOKUP_MISSING"); }
      string GetName_tbad () const {return string("TAX_LOOKUP_MISMATCH"); }
      string GetName_flu() const {return string("DISC_INFLUENZA_DATE_MISMATCH"); }
      string GetName_quals() const {return string("DISC_MISSING_VIRAL_QUALS"); }

      bool HasMulSrc(const CBioSource& biosrc);
      bool IsBacterialIsolate(const CBioSource& biosrc);
      bool HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& biosrc);
      bool DoTaxonIdsMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool DoInfluenzaStrainAndCollectionDateMisMatch(const CBioSource& biosrc);
      void AddMissingViralQualsDiscrepancies(const CBioSource& biosrc, const string& desc);

      void RunTests(const CBioSource& biosrc, const string& desc);
  };


  class CSeqEntry_DISC_MISSING_VIRAL_QUALS : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_MISSING_VIRAL_QUALS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CSeqEntry_test_on_biosrc::GetName_quals();}
  };


  class CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_flu();}
  };


  class CSeqEntry_TAX_LOOKUP_MISSING : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_TAX_LOOKUP_MISSING () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_tmiss();}
  };


  class CSeqEntry_TAX_LOOKUP_MISMATCH : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_TAX_LOOKUP_MISMATCH () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_tbad();}
  };


  class CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1 : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1 () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_iso();}
  };


  class CSeqEntry_ONCALLER_MULTISRC : public CSeqEntry_test_on_biosrc
  {
    public:
      virtual ~CSeqEntry_ONCALLER_MULTISRC () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return CSeqEntry_test_on_biosrc::GetName_mult();}
  };

  
// new comb


  class CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const { return string("DISC_CITSUB_AFFIL_DUP_TEXT");}

    protected:
      CConstRef <CCit_sub> CitSubFromPubEquiv(const list <CRef <CPub> >& pubs);
      bool AffilStreetContainsDuplicateText(const CAffil& affil);       
      bool AffilStreetEndsWith(const string& street, const string& end_str);
  };



  class CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE : public CSeqEntryTestAndRepData
  {
      friend class CSeqEntryTestAndRepData;
    public:
      virtual ~CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {
                               return string("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE");}
      virtual void Check(const string& desc) {cerr << "check1 " << desc << endl;}
  };


  class CSeqEntry_MORE_NAMES_COLLECTED_BY : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_MORE_NAMES_COLLECTED_BY () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("MORE_NAMES_COLLECTED_BY");}
  };


  class CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("MORE_OR_SPEC_NAMES_IDENTIFIED_BY");}
  };



  class CSeqEntry_DISC_REQUIRED_STRAIN : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_REQUIRED_STRAIN () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_REQUIRED_STRAIN");}
 
    protected:
      bool IsMissingRequiredStrain(const CBioSource& biosrc); 
  };


  class CSeqEntry_DIVISION_CODE_CONFLICTS : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DIVISION_CODE_CONFLICTS () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DIVISION_CODE_CONFLICTS");}

    protected:
      bool HasDivCode(const CBioSource& biosrc, string& div_code);
  };


  class CSeqEntry_INCONSISTENT_BIOSOURCE : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_INCONSISTENT_BIOSOURCE () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("INCONSISTENT_BIOSOURCE"); }
   
    protected:
      bool SynonymsMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool DbtagMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool OrgModSetMatch(const COrgName& nm1, const COrgName& nm2);
      bool OrgNameMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool OrgRefMatch(const COrg_ref& org1, const COrg_ref& org2);
      bool SubSourceSetMatch(const CBioSource& biosrc1, const CBioSource& biosrc2);
      bool BioSourceMatch( const CBioSource& biosrc1, const CBioSource& biosrc2);
      string DescribeOrgRefDifferences(const COrg_ref& org1, const COrg_ref& org2);
      string DescribeBioSourceDifferences(const CBioSource& biosrc1,const CBioSource& biosrc2);
      string DescribeOrgNameDifferences(const COrg_ref& org1, const COrg_ref& org2);
      string OrgModSetDifferences(const COrgName& nm1, const COrgName& nm2);
      void  AddAllBioseqToList(const CBioseq_set& bioseq_set,
                        CRef <GeneralDiscSubDt>& item_list, const string& biosrc_txt);
      CRef <GeneralDiscSubDt> AddSeqEntry(const CSeqdesc& seqdesc,const CSeq_entry& seq_entry);
      bool HasNaInSet(const CBioseq_set& bioseq_set);
  };


  class CSeqEntry_DISC_SOURCE_QUALS : public CSeqEntryTestAndRepData
  {
    public:
      CSeqEntry_DISC_SOURCE_QUALS () {};
      virtual ~CSeqEntry_DISC_SOURCE_QUALS () {}; 

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const = 0;

    protected:
      bool isOncaller;

/*
      struct qualvlu_distribute {
         Str2Strs qual_vlu2src;
         vector <string> missing_item, multi_same, multi_dup, multi_all_dif;
      };
      typedef map <string, qualvlu_distribute > Str2QualVlus;
*/

      Str2Ints m_qual2src_idx_feat, m_qual2src_idx_seqdesc;
      Str2MapStr2Strs  m_biosrc2qualvlu_nm;

      void GetQual2SrcIdx(const CBioSource& biosrc,Str2Ints& qual2src_idx,const unsigned& idx);
      string GetSrcQualValue(const string& qual_type, const int& cur_idx, bool is_subsrc);
      string GetSubSrcValue(const CBioSource& biosrc, const string& type_name);
      void GetMultiSubSrcVlus(const CBioSource& biosrc, const string& type_name, 
                                                                  vector <string>& multi_vlus);
      string GetOrgModValue(const CBioSource& biosrc, const string& type_name);
      void GetMultiOrgModVlus(const CBioSource& biosrc, const string& type_name,
                                                                  vector <string>& multi_vlus);
      void GetMultiPrimerVlus(const CBioSource& biosrc, const string& qual_name,
                                                                  vector <string>& multi_vlus);
      CRef <CClickableItem> MultiItem(const string& qual_name, 
                                  const vector <string>& multi_list, const string& ext_desc);
      void CheckForMultiQual(const string& qual_name, const CBioSource& biosrc, 
                                                      eMultiQual& multi_type, bool is_subsrc);
      void GetMultiQualVlus(const string& qual_name, const CBioSource& biosrc, 
                                                  vector <string>& multi_vlus, bool is_subsrc);
      Str2QualVlus m_qual_nm2qual_vlus;
      void GetQualDistribute(const Str2Ints& qual2src_idx, bool IsFromFeat = false);
  };

  class CSeqEntry_DISC_SOURCE_QUALS_ASNDISC : public CSeqEntry_DISC_SOURCE_QUALS
  {
      friend class CSeqEntry_DISC_SOURCE_QUALS;

    public:
      CSeqEntry_DISC_SOURCE_QUALS_ASNDISC () { isOncaller = false;}
      virtual ~CSeqEntry_DISC_SOURCE_QUALS_ASNDISC () {};

      virtual void GetReport(CRef <CClickableItem>& c_item){
            CSeqEntry_DISC_SOURCE_QUALS :: GetReport(c_item);
      }
      virtual string GetName() const {return string("DISC_SOURCE_QUALS_ASNDISC");}
  }; 


  class CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller : public CSeqEntry_DISC_SOURCE_QUALS
  {
      friend class CSeqEntry_DISC_SOURCE_QUALS;

    public:
      CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller() { isOncaller = true;}
      virtual ~CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller () {};

      virtual void GetReport(CRef <CClickableItem>& c_item){
            CSeqEntry_DISC_SOURCE_QUALS :: GetReport(c_item);
      }
      virtual string GetName() const {return string("DISC_SOURCE_QUALS_ASNDISC");}
  };


  class CSeqEntry_ONCALLER_DEFLINE_ON_SET : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_ONCALLER_DEFLINE_ON_SET () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ONCALLER_DEFLINE_ON_SET");}
  };
   

  class CSeqEntry_DISC_FEATURE_COUNT : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_FEATURE_COUNT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEATURE_COUNT");}
  };



  class CSeqEntry_ONCALLER_COMMENT_PRESENT : public CSeqEntryTestAndRepData
  {
    public:
      CSeqEntry_ONCALLER_COMMENT_PRESENT () { all_same = true; };
      virtual ~CSeqEntry_ONCALLER_COMMENT_PRESENT () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ONCALLER_COMMENT_PRESENT");}

    protected:
      bool all_same;
  };


  class CSeqEntry_DISC_CHECK_AUTH_CAPS : public CSeqEntryTestAndRepData
  {
    public:
      virtual ~CSeqEntry_DISC_CHECK_AUTH_CAPS () {};

      virtual void TestOnObj(const CSeq_entry& seq_entry);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_CHECK_AUTH_CAPS");}

    protected:
      bool IsNameCapitalizationOk(const string& name);
      bool IsAuthorInitialsCapitalizationOk(const string& nm_init);
      bool NameIsBad(const CRef <CAuthor> nm_std);

      bool HasBadAuthorName(const CAuth_list& auths);
      bool AreBadAuthCapsInPubdesc(const CPubdesc& pubdesc);
      bool AreAuthCapsOkInSubmitBlock(const CSubmit_block& submit_block);
  };



// class CBioseq_
  class CBioseqTestAndRepData : public CTestAndRepData
  {
     public: 
       virtual ~CBioseqTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) {};
       virtual void TestOnObj(const CSeq_entry& seq_entry) {};
       virtual void TestOnObj(const CBioseq& bioseq) = 0;
       virtual void TestOnObj(const CSeq_feat& seq_feat) {}; 
         
       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;

    protected:
       bool StrandOk(ENa_strand strand1, ENa_strand strand2);
       bool IsUnknown(const string& known_items, const unsigned idx);
       bool IsPseudoSeqFeatOrXrefGene(const CSeq_feat* seq_feat);
       string GetRNAProductString(const CSeq_feat& seq_feat);

       void TestOverlapping_ed_Feats(const vector <const CSeq_feat*>& feat, 
                     const string& setting_name, bool isGene = true, bool isOverlapped = true);

       void TestExtraMissingGenes(const CBioseq& bioseq);
       bool GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref);

       void TestProteinID(const CBioseq& bioseq);
       string GetName_pid () const {return string("MISSING_PROTEIN_ID"); }
       string GetName_prefix() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX"); }

       void TestOnBasesN(const CBioseq& bioseq);
       bool IsDeltaSeqWithFarpointers(const CBioseq& bioseq);
       void AddNsReport(CRef <CClickableItem>& c_item, bool is_n10=true);
       string GetName_n10() const {return string("N_RUNS"); }
       string GetName_n14() const {return string("N_RUNS_14"); }
       string GetName_0() const {return string("ZERO_BASECOUNT"); }
       string GetName_5perc() const {return string("DISC_PERCENT_N"); }
       string GetName_10perc() const {return string("DISC_10_PERCENTN"); }
       string GetName_non_nt() const {return string("TEST_UNUSUAL_NT"); }

       void TestOnShortContigSequence(const CBioseq& bioseq);
       string GetName_contig() const {return string("SHORT_CONTIG"); }
       string GetName_200seq() const {return string("SHORT_SEQUENCES_200"); }
       string GetName_50seq() const {return string("SHORT_SEQUENCES"); }

       void TestOnMRna(const CBioseq& bioseq);
       string GetName_no_mrna() const {return string("DISC_CDS_WITHOUT_MRNA");}
       string GetName_pid_tid() const {return string("MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS");}
       string GetFieldValueForObject(const CSeq_feat& seq_feat, const CFeat_qual_choice& feat_qual);
       CSeqFeatData::ESubtype GetFeatdefFromFeatureType(EMacro_feature_type feat_field_type);
       string GetQualFromFeature(const CSeq_feat& seq_feat, const CField_type& field_type);
       CConstRef <CSeq_feat> GetmRNAforCDS(const CSeq_feat& cd_feat, const CSeq_entry& seq_entry);
       CConstRef <CProt_ref> GetProtRefForFeature(const CSeq_feat& seq_feat, 
                                                                        bool look_xref=true);
       bool IsLocationOrganelle(const CBioSource::EGenome& genome);
       bool ProductsMatchForRefSeq(const string& feat_prod, const string& mRNA_prod);
       bool DoesStringContainPhrase(const string& str, const string& phrase, 
                                           bool case_sensitive = true, bool whole_word = true);
       bool IsMrnaSequence();
  }; // CBioseqTest:


// new comb.
  class CBioseq_test_on_genprod_set : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_genprod_set() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_mprot() const {return string("MISSING_GENPRODSET_PROTEIN");}
      string GetName_dprot() const {return string("DUP_GENPRODSET_PROTEIN");}
      string GetName_mtid() const {return string("MISSING_GENPRODSET_TRANSCRIPT_ID");}
      string GetName_dtid() const {return string("DUP_GENPRODSET_TRANSCRIPT_ID");}

      void GetReport_dup(CRef <CClickableItem>& c_item, const string& setting_name, 
                              const string& desc1, const string& desc2, const string& desc3);
  };


  class CBioseq_MISSING_GENPRODSET_PROTEIN : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_MISSING_GENPRODSET_PROTEIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_mprot(); }
  };


  class CBioseq_DUP_GENPRODSET_PROTEIN : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_DUP_GENPRODSET_PROTEIN () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_dprot(); }
  };


  class CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_mtid(); }
  };


  class CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID : public CBioseq_test_on_genprod_set
  {
    public:
      virtual ~CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_genprod_set::GetName_dtid(); }
  };


  class CBioseq_test_on_prot : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_prot() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_cnt () const {return string("COUNT_PROTEINS"); }
      string GetName_id() const {return string("MISSING_PROTEIN_ID1"); }
      string GetName_prefix() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX1"); }
  };


  class CBioseq_COUNT_PROTEINS : public CBioseq_test_on_prot
  {
    public:
      virtual ~CBioseq_COUNT_PROTEINS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_prot::GetName_cnt(); }
  };

 
  class CBioseq_MISSING_PROTEIN_ID1 : public CBioseq_test_on_prot
  {
    public:
      virtual ~CBioseq_MISSING_PROTEIN_ID1 () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_prot::GetName_id(); }
  };


  class CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX1 : public CBioseq_test_on_prot
  {
    public:
      virtual ~CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX1 () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_prot::GetName_prefix(); }

    protected:
      void MakeRep(const Str2Strs& item_map, const string& desc1, const string& desc2);
  };


  class CBioseq_test_on_cd_4_transl : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_cd_4_transl() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;
    
    protected:
      string GetName_note() const {return string("TRANSL_NO_NOTE"); }
      string GetName_transl() const {return string("NOTE_NO_TRANSL"); }
      string GetName_long() const {return string("TRANSL_TOO_LONG"); }
      void  TranslExceptOfCDs (const CSeq_feat& cd, bool& has_transl, bool& too_long);
  };

  class CBioseq_TRANSL_NO_NOTE : public CBioseq_test_on_cd_4_transl
  {
    public:
      virtual ~CBioseq_TRANSL_NO_NOTE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_cd_4_transl::GetName_note(); }
  };


  class CBioseq_NOTE_NO_TRANSL : public CBioseq_test_on_cd_4_transl
  {
    public:
      virtual ~CBioseq_NOTE_NO_TRANSL () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_cd_4_transl::GetName_transl(); }
  };

 
  class CBioseq_TRANSL_TOO_LONG : public CBioseq_test_on_cd_4_transl
  {
    public:
      virtual ~CBioseq_TRANSL_TOO_LONG () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_cd_4_transl::GetName_long(); }
  };


  class CBioseq_test_on_rna : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_rna() {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_tcnt() const {return string("COUNT_TRNAS"); }
      string GetName_rcnt() const {return string("COUNT_RRNAS"); }
      string GetName_rdup() const {return string("FIND_DUP_RRNAS"); }
      string GetName_tdup() const {return string("FIND_DUP_TRNAS"); }
      string GetName_len() const {return string("FIND_BADLEN_TRNAS"); }
      string GetName_strand() const {return string("FIND_STRAND_TRNAS"); }

      void FindMissingRNAsInList();
      bool RRnaMatch(const CRNA_ref& rna1, const CRNA_ref& rna2);
      void FindDupRNAsInList();
      void GetReport_trna(CRef <CClickableItem>& c_item);
      void FindtRNAsOnSameStrand();

      string m_bioseq_desc, m_best_id_str;
  };


  class CBioseq_FIND_STRAND_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_STRAND_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_strand(); }
  };


  class CBioseq_FIND_BADLEN_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_BADLEN_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_len(); }
  };


  class CBioseq_FIND_DUP_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_DUP_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_tdup();}
  };

  class CBioseq_COUNT_TRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_COUNT_TRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_tcnt();}
  };


  class CBioseq_COUNT_RRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_COUNT_RRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_rcnt();}
  };


  class CBioseq_FIND_DUP_RRNAS : public CBioseq_test_on_rna
  {
    public:
      virtual ~CBioseq_FIND_DUP_RRNAS () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_rna::GetName_rdup();}
  };


  
  class CBioseq_test_on_molinfo : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_molinfo () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;

    protected:
      string GetName_mrna() const {return string("MOLTYPE_NOT_MRNA"); }
      string GetName_tsa() const {return string("TECHNIQUE_NOT_TSA"); }
      string GetName_part() const { return string("PARTIAL_CDS_COMPLETE_SEQUENCE"); }
  };


  class CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_part();}
  };


  class CBioseq_MOLTYPE_NOT_MRNA : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_MOLTYPE_NOT_MRNA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_mrna();}
  };


  class CBioseq_TECHNIQUE_NOT_TSA : public CBioseq_test_on_molinfo
  {
    public:
      virtual ~CBioseq_TECHNIQUE_NOT_TSA () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_molinfo::GetName_tsa();}
  };


  class CBioseq_test_on_all_annot : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_test_on_all_annot () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item) = 0;
      virtual string GetName() const = 0;
    
    protected:
       string GetName_no() const {return string("NO_ANNOTATION");}
       string GetName_long_no() const {return string("DISC_LONG_NO_ANNOTATION");}
       string GetName_joined() const {return string("JOINED_FEATURES");}
  };


  class CBioseq_JOINED_FEATURES : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_JOINED_FEATURES () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_joined();}
  };

 
  class CBioseq_NO_ANNOTATION : public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_NO_ANNOTATION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_no();}
  };

  class CBioseq_DISC_LONG_NO_ANNOTATION: public CBioseq_test_on_all_annot
  {
    public:
      virtual ~CBioseq_DISC_LONG_NO_ANNOTATION () {};

      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return CBioseq_test_on_all_annot::GetName_long_no();}
  };

// new comb: CBioseq_

 
  class CBioseq_ADJACENT_PSEUDOGENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_ADJACENT_PSEUDOGENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ADJACENT_PSEUDOGENES"); }

    protected:
      string GetGeneStringMatch (const string& str1, const string& str2);
  };


  class CBioseq_DISC_FEAT_OVERLAP_SRCFEAT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_FEAT_OVERLAP_SRCFEAT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEAT_OVERLAP_SRCFEAT"); }
  };


  class CBioseq_CDS_TRNA_OVERLAP : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_CDS_TRNA_OVERLAP () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("CDS_TRNA_OVERLAP"); }
  };


  class CBioseq_INCONSISTENT_SOURCE_DEFLINE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_INCONSISTENT_SOURCE_DEFLINE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("INCONSISTENT_SOURCE_DEFLINE"); }
  };


  class CBioseq_CONTAINED_CDS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_CONTAINED_CDS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("CONTAINED_CDS"); }
  };


  class CBioseq_PSEUDO_MISMATCH : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_PSEUDO_MISMATCH () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("PSEUDO_MISMATCH"); }

    protected:
      void FindPseudoDiscrepancies(const CSeq_feat& seq_feat);
  };


  class CBioseq_EC_NUMBER_NOTE : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_EC_NUMBER_NOTE () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("EC_NUMBER_NOTE"); }
  };



  class CBioseq_NON_GENE_LOCUS_TAG : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_NON_GENE_LOCUS_TAG () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("NON_GENE_LOCUS_TAG"); }
  };


  class CBioseq_SHOW_TRANSL_EXCEPT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHOW_TRANSL_EXCEPT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHOW_TRANSL_EXCEPT");}
  };


  class CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA");}
  };


  class CBioseq_DISC_GENE_PARTIAL_CONFLICT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_GENE_PARTIAL_CONFLICT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_GENE_PARTIAL_CONFLICT");}

    protected:
      bool Is5EndInUTRList(const unsigned& start);
      bool Is3EndInUTRList(const unsigned& stop);
      void ReportPartialConflictsForFeatureType(vector <const CSeq_feat*>& seq_feats, 
                                             string label, bool check_for_utrs = false);
  };



  class CBioseq_SHORT_PROT_SEQUENCES :  public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHORT_PROT_SEQUENCES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHORT_PROT_SEQUENCES");}
  };


  class CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS");}
  };



  class CBioseq_RRNA_NAME_CONFLICTS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_RRNA_NAME_CONFLICTS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("RRNA_NAME_CONFLICTS");}

    protected:
      bool NameNotStandard(const string& nm);
  };
  


  class CBioseq_DISC_CDS_WITHOUT_MRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_CDS_WITHOUT_MRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_CDS_WITHOUT_MRNA");}
  };



  class CBioseq_GENE_PRODUCT_CONFLICT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_GENE_PRODUCT_CONFLICT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("GENE_PRODUCT_CONFLICT");}
  };


  class CBioseq_TEST_UNUSUAL_MISC_RNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_UNUSUAL_MISC_RNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_UNUSUAL_MISC_RNA");}

    protected:
      string GetTrnaProductString(const CTrna_ext& trna_ext);
      string GetRnaRefProductString(const CRNA_ref& rna_ref);
  };


  class CBioseq_DISC_PARTIAL_PROBLEMS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_PARTIAL_PROBLEMS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_PARTIAL_PROBLEMS");}

    protected:
      int DistanceToUpstreamGap(const unsigned& pos, const CBioseq& bioseq);
      bool CouldExtendLeft(const CBioseq& bioseq, const unsigned& pos);
      int DistanceToDownstreamGap (const int& pos, const CBioseq& bioseq);
      bool CouldExtendRight(const CBioseq& bioseq, const int& pos);
  };


  class CBioseq_DISC_SUSPICIOUS_NOTE_TEXT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SUSPICIOUS_NOTE_TEXT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SUSPICIOUS_NOTE_TEXT");}

    protected:
      bool HasSuspiciousStr(const string& str, string& sus_str);
  };



  class CBioseq_SHORT_SEQUENCES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHORT_SEQUENCES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHORT_SEQUENCES");}
  };



  class CBioseq_SHORT_SEQUENCES_200 : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHORT_SEQUENCES_200 () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHORT_CSEQUENCES_200");}
  };



  class CBioseq_SHORT_CONTIG : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_SHORT_CONTIG () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("SHORT_CONTIG");}
  };



  class CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("HYPOTHETICAL_CDS_HAVING_GENE_NAME");}
  };


  
  class CBioseq_TEST_OVERLAPPING_RRNAS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_OVERLAPPING_RRNAS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_OVERLAPPING_RRNAS");}
  };



  class CBioseq_TEST_LOW_QUALITY_REGION : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_LOW_QUALITY_REGION () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_LOW_QUALITY_REGION");}
  };


 

  class CBioseq_TEST_BAD_GENE_NAME : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_BAD_GENE_NAME () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_BAD_GENE_NAME");}
    
    protected:
      bool GeneNameHas4Numbers(const string& locus);
      string GetName_bad() const {return string("BAD_BACTERIAL_GENE_NAME"); }
  };


  class CBioseq_DISC_SHORT_RRNA : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SHORT_RRNA () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SHORT_RRNA");}
  };


  class CBioseq_DISC_BAD_GENE_STRAND : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_BAD_GENE_STRAND () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_BAD_GENE_STRAND");}

    protected:
      bool AreIntervalStrandsOk(const CSeq_loc& g_loc, const CSeq_loc& f_loc);
  };



  class CBioseq_DISC_SHORT_INTRON : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_SHORT_INTRON () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_SHORT_INTRON");}

    protected:
      bool PosIsAt3End(const unsigned pos, CConstRef <CBioseq>& bioseq);
      bool PosIsAt5End(unsigned pos, CConstRef <CBioseq>& bioseq);
  };


  class CBioseq_TEST_UNUSUAL_NT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_UNUSUAL_NT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_UNUSUAL_NT");}
  };


  class CBioseq_N_RUNS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_N_RUNS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("N_RUNS");}
  };


  class CBioseq_N_RUNS_14 : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_N_RUNS_14 () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("N_RUNS_14");}
  };



  class CBioseq_ZERO_BASECOUNT : public CBioseqTestAndRepData
  {   
    public:
      virtual ~CBioseq_ZERO_BASECOUNT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("ZERO_BASECOUNT");}
  };


  class CBioseq_DISC_PERCENT_N : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_PERCENT_N () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_PERCENT_N");}
  };


  class CBioseq_DISC_10_PERCENTN : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_10_PERCENTN () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_10_PERCENTN");}
  };



  class CBioseq_RNA_NO_PRODUCT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_RNA_NO_PRODUCT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("RNA_NO_PRODUCT");}
  };



  class CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("EC_NUMBER_ON_UNKNOWN_PROTEIN");}
  };



  class CBioseq_FEATURE_LOCATION_CONFLICT :  public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_FEATURE_LOCATION_CONFLICT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("FEATURE_LOCATION_CONFLICT");}

    protected:
      void CheckFeatureTypeForLocationDiscrepancies(const vector <const CSeq_feat*>& seq_feat,
                                                                const string& feat_type);
      bool IsGeneLocationOk(const CSeq_feat* seq_feat, const CSeq_feat* gene);
      bool IsMixStrand(const CSeq_feat* seq_feat);
      bool IsMixedStrandGeneLocationOk(const CSeq_loc& feat_loc, const CSeq_loc& gene_loc);
      bool GeneRefMatch(const CGene_ref& gene1, const CGene_ref& gene2);
      bool StrandOk(const int& strand1, const int& strand2);
  };



  class CBioseq_LOCUS_TAGS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_LOCUS_TAGS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("LOCUS_TAGS");}

    protected:
      bool IsLocationDirSub(const CSeq_loc& seq_location);
      string GetName_missing() const { return string("MISSING_LOCUS_TAGS"); };
      string GetName_dup() const { return string("DUPLICATE_LOCUS_TAGS"); }
      string GetName_incons() const { return string("INCONSISTENT_LOCUS_TAG_PREFIX"); }
      string GetName_badtag() const { return string("BAD_LOCUS_TAG_FORMAT"); }
  };



  class CBioseq_DISC_FEATURE_COUNT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_FEATURE_COUNT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_FEATURE_COUNT");}
  };



  class CBioseq_OVERLAPPING_GENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_OVERLAPPING_GENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("OVERLAPPING_GENES");}
  };


  class CBioseq_FIND_OVERLAPPED_GENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_FIND_OVERLAPPED_GENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("FIND_OVERLAPPED_GENES");}
  };



  class CBioseq_MISSING_PROTEIN_ID : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_MISSING_PROTEIN_ID () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("MISSING_PROTEIN_ID");}

    protected:
      string GetName_prefix() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX");}
  };


  class CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("INCONSISTENT_PROTEIN_ID_PREFIX");}

   protected:
      string GetName_pid() const {return string("MISSING_PROTEIN_ID"); }
      void MakeRep(const Str2Strs& item_map, const string& desc1, const string& desc2);
  };



  class CBioseq_TEST_DEFLINE_PRESENT : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_TEST_DEFLINE_PRESENT () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("TEST_DEFLINE_PRESENT");}
  };

 

  class CBioseq_DISC_QUALITY_SCORES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_QUALITY_SCORES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_QUALITY_SCORES");}
  };


  class CBioseq_RNA_CDS_OVERLAP : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_RNA_CDS_OVERLAP () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("RNA_CDS_OVERLAP");}
  };


  class CBioseq_DISC_COUNT_NUCLEOTIDES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DISC_COUNT_NUCLEOTIDES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DISC_COUNT_NUCLEOTIDES");}
  };



  class CBioseq_EXTRA_MISSING_GENES : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_EXTRA_MISSING_GENES () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("EXTRA_MISSING_GENES");}

    protected:
      bool GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref);
      string GetName_extra() const {return string("EXTRA_GENES"); }
      string GetName_extra_pseudodp() const {return string("EXTRA_GENES_pseudodp"); }
      string GetName_extra_frameshift() const {return string("EXTRA_GENES_frameshift"); }
      string GetName_extra_nonshift() const {return string("EXTRA_GENES_nonshift"); }
      string GetName_missing() const {return string("MISSING_GENES"); }
  };



  class CBioseq_DUPLICATE_GENE_LOCUS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_DUPLICATE_GENE_LOCUS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("DUPLICATE_GENE_LOCUS");}

      CRef <GeneralDiscSubDt> AddLocus(const CSeq_feat& seq_feat);
  };


  class CBioseq_OVERLAPPING_CDS : public CBioseqTestAndRepData
  {
    public:
      virtual ~CBioseq_OVERLAPPING_CDS () {};

      virtual void TestOnObj(const CBioseq& bioseq);
      virtual void GetReport(CRef <CClickableItem>& c_item);
      virtual string GetName() const {return string("OVERLAPPING_CDS");}

    protected:
      bool OverlappingProdNmSimilar(const string& prod_nm1, const string& prod_nm2);
      void AddToDiscRep(const CSeq_feat* seq_feat);
      bool HasNoSuppressionWords(const CSeq_feat* seq_feat);
  };


/*
  class CBioseq_SUSPECT_PRODUCT_NAMES : public CBioseqTestAndRepData
  {
    public:
       virtual ~CBioseq_SUSPECT_PRODUCT_NAMES () {};

       virtual void TestOnObj(const CBioseq& Bioseq);
       virtual void GetReport(CRef <CClickableItem>& c_item);
       virtual string GetName() const {return string("SUSPECT_PRODUCT_NAMES");}

    protected: 
       string SummarizeSuspectRule(const CSuspect_rule& rule);
       string SummarizeStringConstraint(const CString_constraint& constraint);

       void MatchesSrchFunc(vector <string>& str, vector <bool> match_ls, const CSearch_func& func);
       void DoesStringMatchConstraint(vector <string>& str, vector <bool>& match_ls, const CString_constraint& str_cst);
       string SkipWeasel(const string& str);

       void DoesObjectMatchConstraint(const CRef < CConstraint_choice>& cst, vector <const CSeq_feat*> seq_feats, vector <bool> match_ls);
       void DoesObjectMatchStringConstraint (vector <CSeq_feat*> seq_feats, vector <bool> match_ls, const CString_constraint& str_cst);
       void DoesObjectMatchConstraintChoiceSet(const CConstraint_choice_set& feat_cst, vector <const CSeq_feat*>& seq_feats, vector <bool>& match_ls);

  };

*/


// CSeqFeat
  class CSeqFeatTestAndRepData : public CTestAndRepData
  {
      friend class CTestAndRepData;
  
    public:
       virtual ~CSeqFeatTestAndRepData() {};

       virtual void TestOnObj(const CBioseq_set& bioseq_set) {};
       virtual void TestOnObj(const CSeq_entry& seq_entry) {};
       virtual void TestOnObj(const CBioseq& bioseq) {};
       virtual void TestOnObj(const CSeq_feat& seq_feat) = 0; 

       virtual void GetReport(CRef <CClickableItem>& c_item)=0; 

       virtual string GetName() const  = 0;
  };


/*
  class CSeqFeat_SUSPECT_PRODUCT_NAMES : public CSeqFeatTestAndRepData
  {
    public:
       virtual ~CSeqFeat_SUSPECT_PRODUCT_NAMES () {};

       virtual void TestOnObj(const CSeq_feat& seq_feat);
       virtual void GetReport(CRef <CClickableItem>& c_item);
       virtual string GetName() const {return string("SUSPECT_PRODUCT_NAMES");}

    private:
       bool DoesStringMatchSuspectRule(const string& str, const CSeq_feat* seq_feat, const CRef <CSuspect_rule> rule);

       string SummarizeStringConstraint(const CString_constraint& constraint)
  };
*/


// CSourceDesc
/*
  class CSourceDesc : public CTestAndRepData
  {
      friend class CTestAndRepData;

    public:
       virtual ~CSourceDesc() {};

       virtual void TestOnObj(const CBioseq& bioseq) = 0;
       virtual void TestOnObj(const CSeq_feat& seq_feat) {};

       virtual void GetReport(CRef <CClickableItem>& c_item)=0;

       virtual string GetName() const = 0;

      
  };
*/

};


END_NCBI_SCOPE

#endif
