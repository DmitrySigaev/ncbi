#ifndef __FEATURE_TABLE_READER_HPP_INCLUDED__
#define __FEATURE_TABLE_READER_HPP_INCLUDED__


#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeq_entry;
    class ILineErrorListener;
    class CSeq_entry_Handle;
    class CScope;
    class CDelta_seq;
    class CSeq_feat;
};
class CSerialObject;
class ILineReader;
class CTable2AsnContext;

class CFeatureTableReader
{
public:
   CFeatureTableReader(CTable2AsnContext& context);

   bool CheckIfNeedConversion(const objects::CSeq_entry& entry) const;
   void ConvertSeqIntoSeqSet(objects::CSeq_entry& entry, bool nuc_prod_set) const;
   void ConvertNucSetToSet(CRef<objects::CSeq_entry>& entry) const;
   // MergeCDSFeatures looks for cdregion features in the feature tables
//    in sequence annotations and creates protein sequences based on them
//    as well as converting the sequence or a seq-set into nuc-prot-set
   void MergeCDSFeatures(objects::CSeq_entry& obj);
   void ParseCdregions(objects::CSeq_entry& entry);
   // This method reads 5 column table and attaches these features
//    to corresponding sequences
// This method requires certain postprocessing of plain features added
   void ReadFeatureTable(objects::CSeq_entry& obj, ILineReader& line_reader);
   void FindOpenReadingFrame(objects::CSeq_entry& entry) const;
   CRef<objects::CSeq_entry> ReadProtein(ILineReader& line_reader);
   void AddProteins(const objects::CSeq_entry& possible_proteins, objects::CSeq_entry& entry);
   CRef<objects::CSeq_entry> m_replacement_protein;

   void MakeGapsFromFeatures(objects::CSeq_entry_Handle seh);
   CRef<objects::CDelta_seq> MakeGap(objects::CBioseq_Handle bsh, const objects::CSeq_feat& feature_gap);
   static 
   void RemoveEmptyFtable(objects::CBioseq& bioseq);
   static
   void GenerateECNumbers(objects::CSeq_entry_Handle seh, const string& fname, auto_ptr<CNcbiOfstream>& ostream);
private:
    void MergeCDSFeatures_impl(objects::CSeq_entry& entry);
    CRef<objects::CSeq_entry> TranslateProtein(
       objects::CScope& scope, objects::CSeq_entry_Handle top_entry_h, 
       const objects::CBioseq& bioseq,
       objects::CSeq_feat& cd_feature);

   int m_local_id_counter;
   bool AddProteinToSeqEntry(const objects::CSeq_entry* protein, objects::CSeq_entry_Handle seh);
   void MoveCdRegions(objects::CSeq_entry_Handle entry_h, const objects::CBioseq& bioseq, objects::CSeq_annot::TData::TFtable& seq_ftable, objects::CSeq_annot::TData::TFtable& set_ftable);

   CTable2AsnContext& m_context;
};

END_NCBI_SCOPE



#endif
