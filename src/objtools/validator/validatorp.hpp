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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDATORP__HPP
#define VALIDATOR___VALIDATORP__HPP

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <util/strsearch.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/valid/Comment_set.hpp>

#include <objtools/validator/validator.hpp>

#include <objects/misc/sequence_macros.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CCit_art;
class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;
class CPub_set;
class CAuth_list;
class CTitle;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CSeq_graph;
class CMappedGraph;
class CDense_diag;
class CDense_seg;
class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
class CByte_graph;
class CDelta_seq;
class CSeqFeatData;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;
class CSeq_feat_Handle;
class CCountries;
class CInferencePrefixList;
class CComment_set;

BEGIN_SCOPE(validator)


// =============================================================================
//                            Validation classes                          
// =============================================================================
class CValidError_desc;
class CValidError_descr;
class CCountryBlock;
class CCountryLatLonMap;

// ==================== for validating lat-lon versus country ================
class CCountryBlock
{
public:
	CCountryBlock (string country_name, double min_x, double min_y, double max_x, double max_y);
	~CCountryBlock (void);

	string GetCountry(void)            const { return m_CountryName; }
    double GetMinX(void)               const { return m_MinX; }
    double GetMinY(void)               const { return m_MinY; }
    double GetMaxX(void)               const { return m_MaxX; }
    double GetMaxY(void)               const { return m_MaxY; }
	bool IsLatLonInCountryBlock (double x, double y);
    bool DoesOverlap(const CCountryBlock* other_block) const;

private:
	string m_CountryName;
	double m_MinX;
	double m_MinY;
	double m_MaxX;
	double m_MaxY;
};


// note - once I get this working at a basic level, want to have sorted
// indices for country name, for x then y, and for y then x
class CCountryLatLonMap
{
public:
	CCountryLatLonMap(void);
	~CCountryLatLonMap(void);

	bool IsCountryInLatLon(string country, double x, double y);
	string GuessCountryForLatLon(double x, double y);
	bool HaveLatLonForCountry (string country);
	static bool DoesStringContainBodyOfWater(const string& country);
    bool DoCountryBoxesOverlap (string country1, string country2);

private:
    typedef vector <CCountryBlock *> TCountryBlockList;
    typedef TCountryBlockList::const_iterator TCountryBlockList_iter; 

    TCountryBlockList m_CountryBlockList;

	static const string sm_BodiesOfWater[];
};


// ===========================  Central Validation  ==========================

// CValidError_imp provides the entry point to the validation process.
// It calls upon the various validation classes to perform validation of
// each part.
// The class holds all the data for the validation process. 
class CValidError_imp
{
public:
    // Interface to be used by the CValidError class

    // Constructor & Destructor
    CValidError_imp(CObjectManager& objmgr, CValidError* errors, 
        Uint4 options = 0);
    virtual ~CValidError_imp(void);

    // Validation methods
    bool Validate(const CSeq_entry& se, const CCit_sub* cs = 0,
        CScope* scope = 0);
    bool Validate(const CSeq_entry_Handle& seh, const CCit_sub* cs = 0);
    void Validate(const CSeq_submit& ss, CScope* scope = 0);
    void Validate(const CSeq_annot_Handle& sa);

	void Validate(const CSeq_feat& feat);
	void Validate(const CBioSource& src);
	void Validate(const CPubdesc& pubdesc);
    void ValidateSubAffil(const CAffil::TStd& std, const CSerialObject& obj, const CSeq_entry *ctx);

    void SetProgressCallback(CValidator::TProgressCallback callback,
        void* user_data);
public:
    // interface to be used by the various validation classes

    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;
    typedef const CSeq_graph& TGraph;
    typedef const CSeq_align& TAlign;
    typedef const CSeq_entry& TEntry;
    typedef const CSeq_feat::TDbxref TDbtags;
    typedef map < const CSeq_feat*, const CSeq_annot* >& TFeatAnnotMap;

    // Posts errors.
    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry ctx,
        TDesc ds);
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);*/
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);*/
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CBioSource& src);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const COrg_ref& org);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CPubdesc& src);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_submit& ss);
    void PostObjErr (EDiagSev sv, EErrType et, const string& msg, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void PostBadDateError (EDiagSev sv, const string& msg, int flags, const CSerialObject& obj, const CSeq_entry *ctx = 0);

    // General use validation methods
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateSubSource(const CSubSource& subsrc, const CSerialObject& obj, const CSeq_entry *ctx = 0);
	void ValidateOrgRef(const COrg_ref& orgref, const CSerialObject& obj, const CSeq_entry *ctx);
	void ValidateOrgName(const COrgName& orgname, const CSerialObject& obj, const CSeq_entry *ctx);
	void ValidateOrgModVoucher(const COrgMod& orgmod, const CSerialObject& obj, const CSeq_entry *ctx);
	void ValidateBioSourceForSeq(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx, const CBioseq_Handle& bsh);
    bool IsSyntheticConstruct (const CBioSource& src);
    bool IsArtificial (const CBioSource& src);
    bool IsOrganelle (int genome);
    bool IsOrganelle (CBioseq_Handle seq);
	bool IsOtherDNA(const CBioseq_Handle& bsh) const;
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq_Handle& seq,
        const string& prefix, const CSerialObject& obj);
    void ValidateSeqLocIds(const CSeq_loc& loc, const CSerialObject& obj);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj,
        bool biosource = false, const CSeq_entry *ctx = 0);
    void ValidateDbxref(TDbtags& xref_list, const CSerialObject& obj,
        bool biosource = false, const CSeq_entry *ctx = 0);
    void ValidateCitSub(const CCit_sub& cs, const CSerialObject& obj, const CSeq_entry *ctx = 0);
	void ValidateTaxonomy(const CSeq_entry& se);
	void ValidateSpecificHost (const vector<CConstRef<CSeqdesc> > & src_descs, const vector<CConstRef<CSeq_entry> > & desc_ctxs, const vector<CConstRef<CSeq_feat> > & src_feats);
    void ValidateTaxonomy(const COrg_ref& org, int genome = CBioSource::eGenome_unknown);
    void ValidateCitations (const CSeq_entry_Handle& seh);

        
    // getters
    inline CScope* GetScope(void) { return m_Scope; }

    // flags derived from options parameter
    bool IsNonASCII(void)             const { return m_NonASCII; }
    bool IsSuppressContext(void)      const { return m_SuppressContext; }
    bool IsValidateAlignments(void)   const { return m_ValidateAlignments; }
    bool IsValidateExons(void)        const { return m_ValidateExons; }
    bool IsOvlPepErr(void)            const { return m_OvlPepErr; }
    bool IsRequireTaxonID(void)       const { return m_RequireTaxonID; }
    bool IsRequireISOJTA(void)        const { return m_RequireISOJTA; }
    bool IsValidateIdSet(void)        const { return m_ValidateIdSet; }
    bool IsRemoteFetch(void)          const { return m_RemoteFetch; }
    bool IsFarFetchMRNAproducts(void) const { return m_FarFetchMRNAproducts; }
    bool IsFarFetchCDSproducts(void)  const { return m_FarFetchCDSproducts; }
    bool IsLocusTagGeneralMatch(void) const { return m_LocusTagGeneralMatch; }
    bool DoRubiscoTest(void)          const { return m_DoRubiscoText; }
    bool IsIndexerVersion(void)       const { return m_IndexerVersion; }
	bool UseEntrez(void)              const { return m_UseEntrez; }
	bool ValidateInferenceAccessions(void) const { return m_ValidateInferenceAccessions; }
    bool IgnoreExceptions(void) const { return m_IgnoreExceptions; }
    bool ReportSpliceAsError(void) const { return m_ReportSpliceAsError; }
    bool IsLatLonCheckState(void)     const { return m_LatLonCheckState; }
    bool IsLatLonIgnoreWater(void)    const { return m_LatLonIgnoreWater; }


    // flags calculated by examining data in record
    inline bool IsStandaloneAnnot(void) const { return m_IsStandaloneAnnot; }
    inline bool IsNoPubs(void) const { return m_NoPubs; }
    inline bool IsNoBioSource(void) const { return m_NoBioSource; }
    inline bool IsGPS(void) const { return m_IsGPS; }
    inline bool IsGED(void) const { return m_IsGED; }
    inline bool IsPDB(void) const { return m_IsPDB; }
    inline bool IsPatent(void) const { return m_IsPatent; }
    inline bool IsRefSeq(void) const { return m_IsRefSeq; }
    inline bool IsEmbl(void) const { return m_IsEmbl; }
    inline bool IsDdbj(void) const { return m_IsDdbj; }
    inline bool IsNC(void) const { return m_IsNC; }
    inline bool IsNG(void) const { return m_IsNG; }
    inline bool IsNM(void) const { return m_IsNM; }
    inline bool IsNP(void) const { return m_IsNP; }
    inline bool IsNR(void) const { return m_IsNR; }
    inline bool IsNS(void) const { return m_IsNS; }
    inline bool IsNT(void) const { return m_IsNT; }
    inline bool IsNW(void) const { return m_IsNW; }
    inline bool IsXR(void) const { return m_IsXR; }
    inline bool IsGI(void) const { return m_IsGI; }
    bool IsNoncuratedRefSeq(const CBioseq& seq, EDiagSev& sev);
    inline bool IsGenbank(void) const { return m_IsGB; }
    inline bool DoesAnyFeatLocHaveGI(void) const { return m_FeatLocHasGI; }
    inline bool DoesAnyProductLocHaveGI(void) const { return m_ProductLocHasGI; }
    inline bool DoesAnyGeneHaveLocusTag(void) const { return m_GeneHasLocusTag; }
    inline bool DoesAnyProteinHaveGeneralID(void) const { return m_ProteinHasGeneralID; }
    inline bool IsINSDInSep(void) const { return m_IsINSDInSep; }

    // counting number of misplaced features
    inline void ResetMisplacedFeatureCount (void) { m_NumMisplacedFeatures = 0; }
    inline void IncrementMisplacedFeatureCount (void) { m_NumMisplacedFeatures++; }
    inline void AddToMisplacedFeatureCount (SIZE_TYPE num) { m_NumMisplacedFeatures += num; }

    // counting number of misplaced graphs
    inline void ResetMisplacedGraphCount (void) { m_NumMisplacedGraphs = 0; }
    inline void IncrementMisplacedGraphCount (void) { m_NumMisplacedGraphs++; }
    inline void AddToMisplacedGraphCount (SIZE_TYPE num) { m_NumMisplacedGraphs += num; }

    // counting number of genes and gene xrefs
    inline void ResetGeneCount (void) { m_NumGenes = 0; }
    inline void IncrementGeneCount (void) { m_NumGenes++; }
    inline void AddToGeneCount (SIZE_TYPE num) { m_NumGenes += num; }
    inline void ResetGeneXrefCount (void) { m_NumGeneXrefs = 0; }
    inline void IncrementGeneXrefCount (void) { m_NumGeneXrefs++; }
    inline void AddToGeneXrefCount (SIZE_TYPE num) { m_NumGeneXrefs += num; }

    // counting sequences with and without TPA history
    inline void ResetTpaWithHistoryCount (void) { m_NumTpaWithHistory = 0; }
    inline void IncrementTpaWithHistoryCount (void) { m_NumTpaWithHistory++; }
    inline void AddToTpaWithHistoryCount (SIZE_TYPE num) { m_NumTpaWithHistory += num; }
    inline void ResetTpaWithoutHistoryCount (void) { m_NumTpaWithoutHistory = 0; }
    inline void IncrementTpaWithoutHistoryCount (void) { m_NumTpaWithoutHistory++; }
    inline void AddToTpaWithoutHistoryCount (SIZE_TYPE num) { m_NumTpaWithoutHistory += num; }

    const CSeq_entry& GetTSE(void) { return *m_TSE; }
    CSeq_entry_Handle GetTSEH(void) { return m_TSEH; }

    TFeatAnnotMap GetFeatAnnotMap(void);

    void AddBioseqWithNoPub(const CBioseq& seq);
    void AddBioseqWithNoBiosource(const CBioseq& seq);
    void AddProtWithoutFullRef(const CBioseq_Handle& seq);
	static bool IsWGSIntermediate(const CBioseq& seq);
	static bool IsWGSIntermediate(const CSeq_entry& se);
    void ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs);
    void ReportMissingBiosource(const CSeq_entry& se);

    bool IsNucAcc(const string& acc);
    bool IsFarLocation(const CSeq_loc& loc);
    CConstRef<CSeq_feat> GetCDSGivenProduct(const CBioseq& seq);
    CConstRef<CSeq_feat> GetmRNAGivenProduct(const CBioseq& seq);
    const CSeq_entry* GetAncestor(const CBioseq& seq, CBioseq_set::EClass clss);
    bool IsSerialNumberInComment(const string& comment);

    bool CheckSeqVector(const CSeqVector& vec);
    bool IsSequenceAvaliable(const CSeqVector& vec);

    bool IsTransgenic(const CBioSource& bsrc);

    CRef<CComment_set> GetStructuredCommentRules(void);

private:
    // Prohibit copy constructor & assignment operator
    CValidError_imp(const CValidError_imp&);
    CValidError_imp& operator= (const CValidError_imp&);

    void Setup(const CSeq_entry_Handle& seh);
    void Setup(const CSeq_annot_Handle& sa);
    void SetScope(const CSeq_entry& se);

    void InitializeSourceQualTags();
    void ValidateSourceQualTags(const string& str, const CSerialObject& obj, const CSeq_entry *ctx = 0);

    bool IsMixedStrands(const CSeq_loc& loc);

    void ValidatePubGen(const CCit_gen& gen, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidatePubArticle(const CCit_art& art, int uid, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void x_ValidatePages(const string& pages, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateAuthorList(const CAuth_list::C_Names& names, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidateAuthorsInPubequiv (const CPub_equiv& pe, const CSerialObject& obj, const CSeq_entry *ctx = 0);
    void ValidatePubHasAuthor(const CPubdesc& pubdesc, const CSerialObject& obj, const CSeq_entry *ctx = 0);
        
    bool HasName(const CAuth_list& authors);
    bool HasTitle(const CTitle& title);
    bool HasIsoJTA(const CTitle& title);

    bool ValidateDescriptorInSeqEntry (const CSeq_entry& se, CValidError_desc * descval);
    bool ValidateDescriptorInSeqEntry (const CSeq_entry& se);

    bool ValidateSeqDescrInSeqEntry (const CSeq_entry& se, CValidError_descr *descr_val);
    bool ValidateSeqDescrInSeqEntry (const CSeq_entry& se);

    void FindEmbeddedScript (const CSerialObject& obj);
    void FindCollidingSerialNumbers (const CSerialObject& obj);

	void GatherSources (const CSeq_entry& se, vector<CConstRef<CSeqdesc> >& src_descs, vector<CConstRef<CSeq_entry> >& desc_ctxs, vector<CConstRef<CSeq_feat> >& src_feats);


    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
    CConstRef<CSeq_entry>   m_TSE;
    CSeq_entry_Handle       m_TSEH;

    // validation data read from external files
	CCountryLatLonMap lat_lon_map;
    CRef<CComment_set> m_StructuredCommentRules;

    // error repoitory
    CValidError*       m_ErrRepository;

    // flags derived from options parameter
    bool m_NonASCII;             // User sets if Non ASCII char found
    bool m_SuppressContext;      // Include context in errors if true
    bool m_ValidateAlignments;   // Validate Alignments if true
    bool m_ValidateExons;        // Check exon feature splice sites
    bool m_OvlPepErr;            // Peptide overlap error if true, else warn
    bool m_RequireTaxonID;       // BioSource requires taxonID dbxref
    bool m_RequireISOJTA;        // Journal requires ISO JTA
    bool m_ValidateIdSet;        // validate update against ID set in database
    bool m_RemoteFetch;          // Remote fetch enabled?
    bool m_FarFetchMRNAproducts; // Remote fetch mRNA products
    bool m_FarFetchCDSproducts;  // Remote fetch proteins
    bool m_LatLonCheckState;
    bool m_LatLonIgnoreWater;
    bool m_LocusTagGeneralMatch;
    bool m_DoRubiscoText;
    bool m_IndexerVersion;
	bool m_UseEntrez;
    bool m_IgnoreExceptions;             // ignore exceptions when validating translation
    bool m_ValidateInferenceAccessions;  // check that accessions in inferences are valid
    bool m_ReportSpliceAsError;

    // flags calculated by examining data in record
    bool m_IsStandaloneAnnot;
    bool m_NoPubs;                  // Suppress no pub error if true
    bool m_NoBioSource;             // Suppress no organism error if true
    bool m_IsGPS;
    bool m_IsGED;
    bool m_IsPDB;
    bool m_IsPatent;
    bool m_IsRefSeq;
    bool m_IsEmbl;
    bool m_IsDdbj;
    bool m_IsNC;
    bool m_IsNG;
    bool m_IsNM;
    bool m_IsNP;
    bool m_IsNR;
    bool m_IsNS;
    bool m_IsNT;
    bool m_IsNW;
    bool m_IsXR;
    bool m_IsGI;
    bool m_IsGB;
    bool m_FeatLocHasGI;
    bool m_ProductLocHasGI;
    bool m_GeneHasLocusTag;
    bool m_ProteinHasGeneralID;
    bool m_IsINSDInSep;

    bool m_IsTbl2Asn;

    // seq ids contained within the orignal seq entry. 
    // (used to check for far location)
    vector< CConstRef<CSeq_id> >    m_InitialSeqIds;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> >    m_BioseqWithNoSource;

    // list of publication serial numbers
    vector< int > m_PubSerialNumbers;

    // legal dbxref database strings
    static const string legalDbXrefs[];
    static const string legalRefSeqDbXrefs[];

    // source qulalifiers prefixes
    static const string sm_SourceQualPrefixes[];
    static auto_ptr<CTextFsa> m_SourceQualTags;

    CValidator::TProgressCallback m_PrgCallback;
    CValidator::CProgressInfo     m_PrgInfo;
    SIZE_TYPE   m_NumAlign;
    SIZE_TYPE   m_NumAnnot;
    SIZE_TYPE   m_NumBioseq;
    SIZE_TYPE   m_NumBioseq_set;
    SIZE_TYPE   m_NumDesc;
    SIZE_TYPE   m_NumDescr;
    SIZE_TYPE   m_NumFeat;
    SIZE_TYPE   m_NumGraph;

    SIZE_TYPE   m_NumMisplacedFeatures;
    SIZE_TYPE   m_NumMisplacedGraphs;
    SIZE_TYPE   m_NumGenes;
    SIZE_TYPE   m_NumGeneXrefs;

    size_t      m_NumTpaWithHistory;
    size_t      m_NumTpaWithoutHistory;
};


// =============================================================================
//                         Specific validation classes
// =============================================================================



class CValidError_base
{
protected:
    // typedefs:
    typedef CValidError_imp::TFeat TFeat;
    typedef CValidError_imp::TBioseq TBioseq;
    typedef CValidError_imp::TSet TSet;
    typedef CValidError_imp::TDesc TDesc;
    typedef CValidError_imp::TAnnot TAnnot;
    typedef CValidError_imp::TGraph TGraph;
    typedef CValidError_imp::TAlign TAlign;
    typedef CValidError_imp::TEntry TEntry;
    typedef CValidError_imp::TDbtags TDbtags;

    CValidError_base(CValidError_imp& imp);
    virtual ~CValidError_base();

    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    //void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry ctx,
        TDesc ds);
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);*/
    /*void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);*/
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);

    CValidError_imp& m_Imp;
    CScope* m_Scope;
};


// ============================  Validate SeqGraph  ============================


class CValidError_graph : private CValidError_base
{
public:
    CValidError_graph(CValidError_imp& imp);
    virtual ~CValidError_graph(void);

    void ValidateSeqGraph(const CSeq_graph& graph);
    void ValidateSeqGraph(CSeq_entry_Handle seh);
    void ValidateSeqGraph(const CBioseq& seq);
    void ValidateSeqGraph(const CBioseq_set& set);

    void ValidateSeqGraphContext (const CSeq_graph& graph, const CBioseq_set& set);
    void ValidateSeqGraphContext (const CSeq_graph& graph, const CBioseq& seq);
   

private:

};


// ============================  Validate SeqAlign  ============================


class CValidError_align : private CValidError_base
{
public:
    CValidError_align(CValidError_imp& imp);
    virtual ~CValidError_align(void);

    void ValidateSeqAlign(const CSeq_align& align);

private:
    typedef CSeq_align::C_Segs::TDendiag    TDendiag;
    typedef CSeq_align::C_Segs::TDenseg     TDenseg;
    typedef CSeq_align::C_Segs::TPacked     TPacked;
    typedef CSeq_align::C_Segs::TStd        TStd;
    typedef CSeq_align::C_Segs::TDisc       TDisc;

    void x_ValidateAlignPercentIdentity (const CSeq_align& align, bool internal_gaps);
    void x_ValidateDendiag(const TDendiag& dendiags, const CSeq_align& align);
    void x_ValidateDenseg(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStd(const TStd& stdsegs, const CSeq_align& align);
    void x_ValidatePacked(const TPacked& packed, const CSeq_align& align);
    size_t x_CountBits(const CPacked_seg::TPresent& present);

    // Check if dimension is valid
    template <typename T>
    bool x_ValidateDim(T& obj, const CSeq_align& align, size_t part = 0);

    // Check if the  strand is consistent in SeqAlignment of global 
    // or partial type
    void x_ValidateStrand(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStrand(const TPacked& packed, const CSeq_align& align);
    void x_ValidateStrand(const TStd& std_segs, const CSeq_align& align);

    // Check if an alignment is FASTA-like. 
    // Alignment is FASTA-like if all gaps are at the end with dimensions > 2.
    void x_ValidateFastaLike(const TDenseg& denseg, const CSeq_align& align);

    // Check if there is a gap for all sequences in a segment.
    void x_ValidateSegmentGap(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSegmentGap(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSegmentGap(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSegmentGap(const TDendiag& dendiags, const CSeq_align& align);

    // Validate SeqId in sequence alignment.
    void x_ValidateSeqId(const CSeq_align& align);
    void x_GetIds(const CSeq_align& align, vector< CRef< CSeq_id > >& ids);

    // Check segment length, start and end point in Dense_seg, Dense_diag 
    // and Std_seg
    void x_ValidateSeqLength(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSeqLength(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSeqLength(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSeqLength(const CDense_diag& dendiag, size_t dendiag_num,
        const CSeq_align& align);
};


// =============================  Validate SeqFeat  ============================


class CValidError_feat : private CValidError_base
{
public:
    CValidError_feat(CValidError_imp& imp);
    virtual ~CValidError_feat(void);

    void ValidateSeqFeat(const CSeq_feat& feat);
    void ValidateSeqFeatContext(const CSeq_feat& feat, const CBioseq& seq);

    enum EInferenceValidCode {
        eInferenceValidCode_valid = 0,
        eInferenceValidCode_empty,
        eInferenceValidCode_bad_prefix,
        eInferenceValidCode_bad_body,
        eInferenceValidCode_single_field,
        eInferenceValidCode_spaces,
        eInferenceValidCode_same_species_misused,
        eInferenceValidCode_bad_accession,
        eInferenceValidCode_bad_accession_version,
        eInferenceValidCode_accession_version_not_public,
        eInferenceValidCode_bad_accession_type
    };

    EInferenceValidCode ValidateInferenceAccession (string accession, string separator, bool fetch_accession);
    EInferenceValidCode ValidateInference(string inference, bool fetch_accession);

    // functions expected to be used in Discrepancy Report
    bool DoesCDSHaveShortIntrons(const CSeq_feat& feat);
    bool IsIntronShort(const CSeq_feat& feat);

private:
    void x_ValidateSeqFeatLoc(const CSeq_feat& feat);
    void ValidateSeqFeatData(const CSeqFeatData& data, const CSeq_feat& feat);
    void ValidateSeqFeatProduct(const CSeq_loc& prod, const CSeq_feat& feat);
    void ValidateGene(const CGene_ref& gene, const CSeq_feat& feat);
    void ValidateGeneXRef(const CSeq_feat& feat);
    void ValidateOperon(const CSeq_feat& feat);

    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
    void ValidateCdTrans(const CSeq_feat& feat);
    bool ValidateCdRegionTranslation (const CSeq_feat& feat, const string& transl_prot, bool report_errors, bool unclassified_except, bool& has_errors, bool& other_than_mismatch, bool& reported_bad_start_codon, bool& prot_ok);
    void ValidateCdsProductId(const CSeq_feat& feat);
    void ValidateCdConflict(const CCdregion& cdregion, const CSeq_feat& feat);
    void ReportCdTransErrors(const CSeq_feat& feat,
        bool show_stop, bool got_stop, bool no_end, int ragged,
        bool report_errors, bool& has_errors);
    void ValidateDonor (ENa_strand strand, TSeqPos stop, CSeqVector vec, TSeqPos seq_len,
                        bool rare_consensus_not_expected, 
                        string label, bool report_errors, bool relax_to_warning, bool &has_errors, const CSeq_feat& feat);
    void ValidateAcceptor (ENa_strand strand, TSeqPos start, CSeqVector vec, TSeqPos seq_len,
                        bool rare_consensus_not_expected, 
                        string label, bool report_errors, bool relax_to_warning, bool &has_errors, const CSeq_feat& feat);
    void ValidateSplice(const CSeq_feat& feat, bool check_all = false);
    void ValidateBothStrands(const CSeq_feat& feat);
    void ValidateCommonCDSProduct(const CSeq_feat& feat);
    void ValidateBadMRNAOverlap(const CSeq_feat& feat);
    void ValidateBadGeneOverlap(const CSeq_feat& feat);
    void ValidateCDSPartial(const CSeq_feat& feat);
    bool x_ValidateCodeBreakNotOnCodon(const CSeq_feat& feat,const CSeq_loc& loc,
        const CCdregion& cdregion, bool report_erros);
    void x_ValidateCdregionCodebreak(const CCdregion& cds, const CSeq_feat& feat);

    void ValidateProt(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ReportUninformativeNames(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ValidateProtECNumbers(const CProt_ref& prot, const CSeq_feat& feat);

    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateAnticodon(const CSeq_loc& anticodon, const CSeq_feat& feat);
    void ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat);
    void ValidateMrnaTrans(const CSeq_feat& feat);
    void ValidateCommonMRNAProduct(const CSeq_feat& feat);
    void ValidatemRNAGene (const CSeq_feat &feat);
    void ValidateRnaProductType(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateIntron(const CSeq_feat& feat);

    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateNonImpFeat (const CSeq_feat& feat);
    void ValidateImpGbquals(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateNonImpFeatGbquals(const CSeq_feat& feat);

    // these functions are for validating individual genbank qualifier values
    void ValidateRptUnitVal (const string& val, const string& key, const CSeq_feat& feat);
    void ValidateRptUnitSeqVal (const string& val, const string& key, const CSeq_feat& feat);
    void ValidateRptUnitRangeVal (const string& val, const CSeq_feat& feat);
    void ValidateLabelVal (const string& val, const CSeq_feat& feat);
    void ValidateCompareVal (const string& val, const CSeq_feat& feat);

    void ValidateGapFeature (const CSeq_feat& feat);

    void ValidatePeptideOnCodonBoundry(const CSeq_feat& feat, 
        const string& key);

    bool SplicingNotExpected(const CSeq_feat& feat);
    void ValidateFeatPartialness(const CSeq_feat& feat);
    void ValidateExcept(const CSeq_feat& feat);
    void ValidateExceptText(const string& text, const CSeq_feat& feat);
    void ValidateSeqFeatXref (const CSeqFeatXref& xref, const CSeq_feat& feat, CTSE_Handle tse);
    void ValidateExtUserObject (const CUser_object& user_object, const CSeq_feat& feat);
    void ValidateGoTerms (CUser_object::TData field_list, const CSeq_feat& feat, vector<pair<string, string> >& id_terms);

    void ValidateFeatCit(const CPub_set& cit, const CSeq_feat& feat);
    void ValidateFeatComment(const string& comment, const CSeq_feat& feat);
    void ValidateFeatBioSource(const CBioSource& bsrc, const CSeq_feat& feat);

    bool IsPlastid(int genome);
    bool IsOverlappingGenePseudo(const CSeq_feat& feat);
    unsigned char Residue(unsigned char res);
    int  CheckForRaggedEnd(const CSeq_loc&, const CCdregion& cdr);
    bool SuppressCheck(const string& except_text);
    string MapToNTCoords(const CSeq_feat& feat, const CSeq_loc& product,
        TSeqPos pos);

    bool IsPartialAtSpliceSite(const CSeq_loc& loc, unsigned int errtype, bool& bad_seq);
    bool ArePartialsAtSpliceSitesOrGaps(const CSeq_loc& loc);
    bool IsSameAsCDS(const CSeq_feat& feat);
    bool IsCDDFeat(const CSeq_feat& feat) const;

    int x_SeqIdToGiNumber(const string& seq_id, const string database_name );

    void ValidateCharactersInField (string value, string field_name, const CSeq_feat& feat);

};


// ============================  Validate SeqAnnot  ============================


class CValidError_annot : private CValidError_base
{
public:
    CValidError_annot(CValidError_imp& imp);
    virtual ~CValidError_annot(void);

    void ValidateSeqAnnot(const CSeq_annot_Handle& annot);

    void ValidateSeqAnnot (const CSeq_annot& annot);
    void ValidateSeqAnnotContext(const CSeq_annot& annot, const CBioseq& seq);
    void ValidateSeqAnnotContext(const CSeq_annot& annot, const CBioseq_set& set);

private:
    CValidError_graph m_GraphValidator;
    CValidError_align m_AlignValidator;
    CValidError_feat m_FeatValidator;

};


// =============================  Validate SeqDesc  ============================

class CValidError_desc : private CValidError_base
{
public:
    CValidError_desc(CValidError_imp& imp);
    virtual ~CValidError_desc(void);

    void ValidateSeqDesc(const CSeqdesc& desc, const CSeq_entry& ctx);

    void ResetModifCounters(void);
private:

    void ValidateComment(const string& comment, const CSeqdesc& desc);
    void ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc);
    void ValidateStructuredComment(const CUser_object& usr, const CSeqdesc& desc, const CComment_rule& rule);
    void ValidateUser(const CUser_object& usr, const CSeqdesc& desc);
    void ValidateMolInfo(const CMolInfo& minfo, const CSeqdesc& desc);

    CConstRef<CSeq_entry> m_Ctx;
};


// ============================  Validate SeqDescr  ============================


class CValidError_descr : private CValidError_base
{
public:
    CValidError_descr(CValidError_imp& imp);
    virtual ~CValidError_descr(void);

    void ValidateSeqDescr(const CSeq_descr& descr, const CSeq_entry& ctx);
private:

    CValidError_desc m_DescValidator;
};


// =============================  Validate Bioseq  =============================


class CValidError_bioseq : private CValidError_base
{
public:
    CValidError_bioseq(CValidError_imp& imp);
    virtual ~CValidError_bioseq(void);

    void ValidateBioseq(const CBioseq& seq);
    void ValidateSeqIds(const CBioseq& seq);
    void ValidateSeqId(const CSeq_id& id, const CBioseq& ctx);
    void ValidateInst(const CBioseq& seq);
    void ValidateBioseqContext(const CBioseq& seq);
    void ValidateHistory(const CBioseq& seq);

private:
    typedef multimap<string, const CSeq_feat*, PNocase> TStrFeatMap;
    typedef vector<CMappedFeat>                         TMappedFeatVec;
    
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    void ValidateDeltaLoc(const CSeq_loc& loc, const CBioseq& seq, TSeqPos& len);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void x_ValidateTitle(const CBioseq& seq);
    void x_ValidateBarcode(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    void ValidateNsAndGaps(const CBioseq& seq);
    
    void ValidateMultiIntervalGene (const CBioseq& seq);
    void ValidateMultipleGeneOverlap (const CBioseq_Handle& bsh);
    void ValidateSeqFeatContext(const CBioseq& seq);
    EDiagSev x_DupFeatSeverity (const CSeq_feat& curr, const CSeq_feat& prev, bool is_fruitfly, bool viral, bool htgs, bool same_annot, bool same_label);
    void x_ReportDupOverlapFeaturePair (CSeq_feat_Handle f1, CSeq_feat_Handle f2, bool fruit_fly, bool viral, bool htgs);
    void x_ReportOverlappingPeptidePair (CSeq_feat_Handle f1, CSeq_feat_Handle f2, const CBioseq& bioseq, bool& reported_last_peptide);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateCollidingGenes(const CBioseq& seq);
    void x_CompareStrings(const TStrFeatMap& str_feat_map, const string& type,
        EErrType err, EDiagSev sev);
    void x_ValidateCompletness(const CBioseq& seq, const CMolInfo& mi);
    void x_ValidateAbuttingUTR(const CBioseq_Handle& seq);
    bool x_IsRangeGap (const CBioseq_Handle& seq, int start, int stop);
    void x_ValidateAbuttingRNA(const CBioseq_Handle& seq);
    void x_ValidateCDSmRNAmatch(const CBioseq_Handle& seq, int numgene, int numcds, int nummrna);
    unsigned int x_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna);
    bool x_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna);
    void x_ValidateLocusTagGeneralMatch(const CBioseq_Handle& seq);

    void ValidateSeqDescContext(const CBioseq& seq);
	void ValidateGBBlock (const CGB_block& gbblock, const CBioseq& seq, const CSeqdesc& desc);
    void ValidateMolInfoContext(const CMolInfo& minfo, int& seq_biomol, int& tech, int& completeness,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateMolTypeContext(const EGIBB_mol& gibb, EGIBB_mol& seq_biomol,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateUpdateDateContext(const CDate& update,const CDate& create,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateOrgContext(const CSeqdesc_CI& iter, const COrg_ref& this_org,
        const COrg_ref& org, const CBioseq& seq, const CSeqdesc& desc);
    void ReportModifInconsistentError (int new_mod, int& old_mod, const CSeqdesc& desc, const CSeq_entry& ctx);
    void ValidateModifDescriptors (const CBioseq& seq);
    void ValidateMoltypeDescriptors (const CBioseq& seq);

    void ValidateGraphsOnBioseq(const CBioseq& seq);
    void ValidateGraphOrderOnBioseq (const CBioseq& seq, vector <CRef <CSeq_graph> > graph_list);
    void ValidateByteGraphOnBioseq(const CSeq_graph& graph, const CBioseq& seq);
    void ValidateGraphOnDeltaBioseq(const CBioseq& seq);
    bool ValidateGraphLocation (const CSeq_graph& graph);
    void ValidateGraphValues(const CSeq_graph& graph, const CBioseq& seq,
        int& first_N, int& first_ACGT, size_t& num_bases, size_t& Ns_with_score, size_t& gaps_with_score,
        size_t& ACGTs_without_score, size_t& vals_below_min, size_t& vals_above_max);
    void ValidateMinValues(const CByte_graph& bg, const CSeq_graph& graph);
    void ValidateMaxValues(const CByte_graph& bg, const CSeq_graph& graph);
    bool GetLitLength(const CDelta_seq& delta, TSeqPos& len);
    bool IsSupportedGraphType(const CSeq_graph& graph) const;
    SIZE_TYPE GetSeqLen(const CBioseq& seq);

    void ValidateSecondaryAccConflict(const string& primary_acc,
        const CBioseq& seq, int choice);
    void ValidateIDSetAgainstDb(const CBioseq& seq);
    void x_ValidateSourceFeatures(const CBioseq_Handle& bsh);
    void x_ValidatePubFeatures(const CBioseq_Handle& bsh);
	void x_ReportDuplicatePubLabels (const CBioseq& seq, vector<string>& labels);
    void x_ValidateMultiplePubs(const CBioseq_Handle& bsh);

    void CheckForPubOnBioseq(const CBioseq& seq);
    void CheckSoureDescriptor(const CBioseq_Handle& bsh);
    void CheckForMolinfoOnBioseq(const CBioseq& seq);
    void CheckTpaHistory(const CBioseq& seq);

    TSeqPos GetDataLen(const CSeq_inst& inst);
    bool CdError(const CBioseq_Handle& bsh);
    bool IsMrna(const CBioseq_Handle& bsh);
    bool IsPrerna(const CBioseq_Handle& bsh);
    size_t NumOfIntervals(const CSeq_loc& loc);
    bool LocOnSeg(const CBioseq& seq, const CSeq_loc& loc);
    //bool NotPeptideException(const CFeat_CI& curr, const CFeat_CI& prev);
    //bool IsSameSeqAnnot(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool x_IsSameSeqAnnotDesc(const CSeq_feat_Handle& f1, const CSeq_feat_Handle& f2);
    bool IsIdIn(const CSeq_id& id, const CBioseq& seq);
    bool SuppressTrailingXMsg(const CBioseq& seq);
    CRef<CSeq_loc> GetLocFromSeq(const CBioseq& seq);
    bool IsDifferentDbxrefs(const TDbtags& dbxref1,
                            const TDbtags& dbxref2);
    bool IsHistAssemblyMissing(const CBioseq& seq);
    bool IsFlybaseDbxrefs(const TDbtags& dbxrefs);
    bool GraphsOnBioseq(const CBioseq& seq) const;
    bool IsSynthetic(const CBioseq& seq) const;
    bool x_IsArtificial(const CBioseq& seq) const;
    bool x_IsActiveFin(const CBioseq& seq) const;
    bool x_IsMicroRNA(const CBioseq& seq) const;
    bool x_IsDeltaLitOnly(const CSeq_inst& inst) const;
    
    size_t x_CountAdjacentNs(const CSeq_literal& lit);

    //internal validators
    CValidError_annot m_AnnotValidator;
    CValidError_descr m_DescrValidator;
    CValidError_feat  m_FeatValidator;
};


// ===========================  Validate Bioseq_set  ===========================


class CValidError_bioseqset : private CValidError_base
{
public:
    CValidError_bioseqset(CValidError_imp& imp);
    virtual ~CValidError_bioseqset(void);

    void ValidateBioseqSet(const CBioseq_set& seqset);

private:

    void ValidateNucProtSet(const CBioseq_set& seqset, int nuccnt, int protcnt, int segcnt);
    void ValidateSegSet(const CBioseq_set& seqset, int segcnt);
    void ValidatePartsSet(const CBioseq_set& seqset);
    void ValidateGenbankSet(const CBioseq_set& seqset);
    void ValidatePopSet(const CBioseq_set& seqset);
    void ValidatePhyMutEcoWgsSet(const CBioseq_set& seqset);
    void ValidateGenProdSet(const CBioseq_set& seqset);
    void CheckForInconsistentBiomols (const CBioseq_set& seqset);

    bool IsMrnaProductInGPS(const CBioseq& seq); 
    bool IsCDSProductInGPS(const CBioseq& seq, const CBioseq_set& gps); 

    //internal validators
    CValidError_annot m_AnnotValidator;
    CValidError_descr m_DescrValidator;
    CValidError_bioseq m_BioseqValidator;
};


// ===========================  for handling PCR primer subtypes on BioSource ==

class CPCRSet
{
public:
	CPCRSet(size_t pos);
	virtual ~CPCRSet(void);

    string GetFwdName(void)            const { return m_FwdName; }
    string GetFwdSeq(void)             const { return m_FwdSeq; }
    string GetRevName(void)            const { return m_RevName; }
    string GetRevSeq(void)             const { return m_RevSeq; }
	size_t GetOrigPos(void)            const { return m_OrigPos; }

	void SetFwdName(string fwd_name) { m_FwdName = fwd_name; }
	void SetFwdSeq(string fwd_seq)   { m_FwdSeq = fwd_seq; }
	void SetRevName(string rev_name) { m_RevName = rev_name; }
	void SetRevSeq(string rev_seq)   { m_RevSeq = rev_seq; }

private:
	string m_FwdName;
    string m_FwdSeq;
	string m_RevName;
	string m_RevSeq;
	size_t m_OrigPos;
};

class CPCRSetList
{
public:
	CPCRSetList(void);
	~CPCRSetList(void);

	void AddFwdName (string name);
	void AddRevName (string name);
	void AddFwdSeq (string name);
	void AddRevSeq (string name);

	bool AreSetsUnique(void);

private:
	vector <CPCRSet *> m_SetList;
};

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDATORP__HPP */
