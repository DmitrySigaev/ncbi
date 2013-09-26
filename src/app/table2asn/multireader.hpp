#ifndef TABLE2ASN_MULTIREADER_HPP
#define TABLE2ASN_MULTIREADER_HPP

#include <util/format_guess.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class IMessageListener;
class CIdMapper;
class CBioseq;
};

class CTable2AsnContext;
class CSerialObject;

//  ============================================================================
class CMultiReader
//  ============================================================================
{
public:
   CMultiReader(const CTable2AsnContext& context);
   ~CMultiReader();

   CRef<objects::CSeq_entry> LoadFile(const string& ifname);
   void Cleanup(CRef<objects::CSeq_entry>);
   void WriteObject(CSerialObject&, ostream& );
   void ApplyAdditionalProperties(objects::CSeq_entry& entry);
   void LoadTemplate(CTable2AsnContext& context, const string& ifname);
   void LoadDescriptors(const string& ifname, CRef<objects::CSeq_descr> & out_desc);
   CRef<objects::CSeq_descr> GetSeqDescr(CSerialObject* obj);
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeq_descr & source);
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeqdesc & source);
   void ApplyDescriptors(objects::CSeq_entry & obj, const objects::CSeq_descr & source);


protected:
private:
    CRef<objects::CSeq_entry> xReadFile(const string& ifname);
    CRef<objects::CSeq_entry> xReadFasta(CNcbiIstream& instream);
    CRef<objects::CSeq_entry> xReadASN1(CNcbiIstream& instream);
    auto_ptr<CObjectIStream> xCreateASNStream(CNcbiIstream& instream);
    CRef<objects::CSeq_entry> CreateNewSeqFromTemplate(const CTable2AsnContext& context, objects::CBioseq& bioseq) const;

    void xSetFormat(const CTable2AsnContext&, CNcbiIstream&);
    void xSetFlags(const CTable2AsnContext&, CNcbiIstream&);
    void xSetMapper(const CTable2AsnContext&);
    void xSetErrorContainer(const CTable2AsnContext&);

//    void xDumpErrors(CNcbiOstream& );

    CFormatGuess::EFormat m_uFormat;
    bool m_bDumpStats;
    int  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;

    auto_ptr<objects::CIdMapper> m_pMapper;
    const CTable2AsnContext& m_context;
};

END_NCBI_SCOPE

#endif
