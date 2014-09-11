#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CSeq_descr;
class CDate;
class IMessageListener;
class CUser_object;
class CBioSource;
class CScope;
class CObjectManager;
class CSeq_entry_EditHandle;
class CSeq_feat;
};

#include <objects/seq/Seqdesc.hpp>

// command line parameters are mapped into the context
// those with only only symbol still needs to be implemented

class CAutoAddDesc
{
public:
    CAutoAddDesc(objects::CSeq_descr& descr, objects::CSeqdesc::E_Choice which)
    {
        m_descr.Reset(&descr);
        m_which = which;
    }
    static
    CRef<objects::CSeqdesc> LocateDesc(objects::CSeq_descr& descr, objects::CSeqdesc::E_Choice which);
    const objects::CSeqdesc& Get();
    objects::CSeqdesc& Set(bool skip_loopup = false);
    bool IsNull();

private:
    CAutoAddDesc();
    CAutoAddDesc(const CAutoAddDesc&);

    objects::CSeqdesc::E_Choice m_which;
    CRef<objects::CSeq_descr> m_descr;
    CRef<objects::CSeqdesc>   m_desc;
};


class CTable2AsnContext
{
public:
    string m_current_file;
    string m_ResultsDirectory;
    CNcbiOstream* m_output;
    string m_accession;
    string m_OrganismName;
    string m_source_qualifiers;
    string m_Comment;
    string m_single_table5_file;
    string m_find_open_read_frame;
    string m_strain;
    string m_url;
    CRef<objects::CDate> m_HoldUntilPublish;
    string F;
    string A;
    string m_genome_center_id;
    string m_validate;
    bool   m_delay_genprodset;
    bool   m_copy_genid_to_note;
    string G;
    bool   R;
    bool   S;
    string Q;
    bool   m_remove_unnec_xref;
    bool   L;
    bool   W;
    bool   m_save_bioseq_set;
    string c;
    string ZOutFile;
    string zOufFile;
    string X;
    string m_master_genome_flag;
    string m;
    string m_cleanup;
    string m_single_structure_cmt;   
    int    m_ProjectVersionNumber;
    bool   m_flipped_struc_cmt;
    bool   m_RemoteTaxonomyLookup;
    bool   m_RemotePubLookup;
    bool   m_HandleAsSet;
    bool   m_GenomicProductSet;
    bool   m_SetIDFromFile;
    bool   m_NucProtSet;
    int    m_taxid;
    bool   m_avoid_orf_lookup;
    TSeqPos m_gapNmin;
    TSeqPos m_gap_Unknown_length;
    TSeqPos m_minimal_sequence_length;
    int    m_gaps_evidence;
    int    m_gaps_type;
    bool   m_fcs_trim;
    bool   m_avoid_submit_block;


    CRef<objects::CSeq_descr>  m_descriptors;

    //string conffile;

/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data) const;
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;
    void ApplySourceQualifiers(CSerialObject& obj, const string& src_qualifiers) const;
    void ApplySourceQualifiers(objects::CSeq_entry_EditHandle& obj, const string& src_qualifiers) const;

    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const string& type);
    static
    objects::CBioSource& SetBioSource_old(objects::CSeq_descr& SD);
    bool ApplyCreateDate(objects::CSeq_entry& entry) const;
    void ApplyUpdateDate(objects::CSeq_entry& entry) const;
    void ApplyAccession(objects::CSeq_entry& entry) const;
    CRef<CSerialObject>
        CreateSubmitFromTemplate(CRef<objects::CSeq_entry> object, const string& toolname) const;
    CRef<CSerialObject>
        CreateSeqEntryFromTemplate(CRef<objects::CSeq_entry> object) const;

    typedef void (*BioseqVisitorMethod)(CTable2AsnContext& context, objects::CBioseq& bioseq);
    typedef void (*FeatureVisitorMethod)(CTable2AsnContext& context, objects::CSeq_feat& bioseq);
    void VisitAllBioseqs(objects::CSeq_entry& entry, BioseqVisitorMethod m);
    void VisitAllBioseqs(objects::CSeq_entry_EditHandle& entry_h, BioseqVisitorMethod m);
    void VisitAllFeatures(objects::CSeq_entry_EditHandle& entry_h, FeatureVisitorMethod m);

    void MergeSeqDescr(objects::CSeq_descr& dest, const objects::CSeq_descr& src) const;
    void MergeWithTemplate(objects::CSeq_entry& entry) const;
    void SetSeqId(objects::CSeq_entry& entry) const;
    void CopyFeatureIdsToComments(objects::CSeq_entry& entry) const;
    void RemoveUnnecessaryXRef(objects::CSeq_entry& entry) const;
    void SmartFeatureAnnotation(objects::CSeq_entry& entry) const;

    void MakeGenomeCenterId(objects::CSeq_entry_EditHandle& entry);
    static void MakeGenomeCenterId(CTable2AsnContext& context, objects::CBioseq& bioseq);
    static void MakeDelayGenProdSet(CTable2AsnContext& context, objects::CSeq_feat& feature);

    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    CRef<objects::CSeq_descr>  m_descr_template;
    objects::IMessageListener* m_logger;

    CRef<objects::CScope>      m_scope;
    CRef<objects::CObjectManager> m_ObjMgr;
private:
};


END_NCBI_SCOPE


#endif
