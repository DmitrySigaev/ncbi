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
 * Author:  Jian Ye
 *
 * File Description:
 *   Sequence alignment display
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/blast_format/showalign.hpp>

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>

#include <util/range.hpp>
#include <util/md5.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp> 
#include <serial/objistrasnb.hpp> 
#include <connect/ncbi_conn_stream.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Cdregion.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>

#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>

#include <stdio.h>
#include <util/tables/raw_scoremat.h>
#include <objtools/readers/getfeature.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE (sequence);

static const char k_IdentityChar = '.';
static const int k_NumFrame = 6;
static const string k_FrameConversion[k_NumFrame] = {"+1", "+2", "+3", "-1",
                                                     "-2", "-3"};
static const int k_GetSubseqThreshhold = 10000;

///threshhold to color mismatch. 98 means 98% 
static const int k_ColorMismatchIdentity = 0; 
static const int k_GetDynamicFeatureSeqLength = 200000;
static const string k_DumpGnlUrl = "/blast/dumpgnl.cgi";
static const int k_FeatureIdLen = 16;
static const int k_NumAsciiChar = 128;
const string color[]={"#000000", "#808080", "#FF0000"};
static const char k_PSymbol[CDisplaySeqalign::ePMatrixSize+1] =
"ARNDCQEGHILKMFPSTWYVBZX";

static const char k_IntronChar = '~';
static const int k_IdStartMargin = 2;
static const int k_SeqStopMargin = 2;
static const int k_StartSequenceMargin = 2;

static const string k_UncheckabeCheckbox = "<input type=\"checkbox\" \
name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment%d',\
 'getSeqMaster')\">";

static const string k_Checkbox = "<input type=\"checkbox\" \
name=\"getSeqGi\" value=\"%s\" onClick=\"synchronizeCheck(this.value, \
'getSeqAlignment%d', 'getSeqGi', this.checked)\">";

static string k_GetSeqSubmitForm[] = {"<FORM  method=\"post\" \
action=\"http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?SUBMIT=y\" \
name=\"%s%d\"><input type=button value=\"Get selected sequences\" \
onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s%d', %d)\"><input \
type=\"hidden\" name=\"db\" value=\"\"><input type=\"hidden\" name=\"term\" \
value=\"\"><input type=\"hidden\" name=\"doptcmdl\" value=\"docsum\"><input \
type=\"hidden\" name=\"cmd\" value=\"search\"></form>",
                                     
                                     "<FORM  method=\"POST\" \
action=\"http://www.ncbi.nlm.nih.gov/Traces/trace.cgi\" \
name=\"%s%d\"><input type=button value=\"Get selected sequences\" \
onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s%d', %d)\"><input \
type=\"hidden\" name=\"val\" value=\"\"><input \
type=\"hidden\" name=\"cmd\" value=\"retrieve\"></form>"
};

static string k_GetSeqSelectForm = "<FORM><input \
type=\"button\" value=\"Select all\" onClick=\"handleCheckAll('select', \
'getSeqAlignment%d', 'getSeqGi')\"></form></td><td><FORM><input \
type=\"button\" value=\"Deselect all\" onClick=\"handleCheckAll('deselect', \
'getSeqAlignment%d', 'getSeqGi')\"></form>";

CDisplaySeqalign::CDisplaySeqalign(const CSeq_align_set& seqalign, 
                                   CScope& scope,
                                   list <CRef<blast::CSeqLocInfo> >* mask_seqloc, 
                                   list <FeatureInfo*>* external_feature,
                                   const int matrix[][ePMatrixSize])
    : m_SeqalignSetRef(&seqalign),
      m_Seqloc(mask_seqloc),
      m_QueryFeature(external_feature),
      m_Scope(scope) 
{
    m_AlignOption = 0;
    m_SeqLocChar = eX;
    m_SeqLocColor = eBlack;
    m_LineLen = 60;
    m_IsDbNa = true;
    m_CanRetrieveSeq = false;
    m_DbName = NcbiEmptyString;
    m_NumAlignToShow = 1000000;
    m_AlignType = eNotSet;
    m_Rid = "0";
    m_CddRid = "0";
    m_EntrezTerm = NcbiEmptyString;
    m_QueryNumber = 0;
    m_BlastType = NcbiEmptyString;
    m_MidLineStyle = eBar;
    m_ConfigFile = NULL;
    m_Reg = NULL;
    m_DynamicFeature = NULL;    
    m_MasterGeneticCode = 1;
    m_SlaveGeneticCode = 1;

    SNCBIFullScoreMatrix blosumMatrix;
    NCBISM_Unpack(&NCBISM_Blosum62, &blosumMatrix);
 
    int** temp = new int*[k_NumAsciiChar];
    for(int i = 0; i<k_NumAsciiChar; ++i) {
        temp[i] = new int[k_NumAsciiChar];
    }
    for (int i=0; i<k_NumAsciiChar; i++){
        for (int j=0; j<k_NumAsciiChar; j++){
            temp[i][j] = -1000;
        }
    }
    for(int i = 0; i < ePMatrixSize; ++i){
        for(int j = 0; j < ePMatrixSize; ++j){
            if(matrix){
                temp[(size_t)k_PSymbol[i]][(size_t)k_PSymbol[j]] =
                    matrix[i][j];
            } else {
                temp[(size_t)k_PSymbol[i]][(size_t)k_PSymbol[j]] =
                    blosumMatrix.s[(size_t)k_PSymbol[i]][(size_t)k_PSymbol[j]];
            }
     
        }
    }
    for(int i = 0; i < ePMatrixSize; ++i) {
        temp[(size_t)k_PSymbol[i]]['*'] = temp['*'][(size_t)k_PSymbol[i]] = -4;
    }
    temp['*']['*'] = 1; 
    m_Matrix = temp;
}


CDisplaySeqalign::~CDisplaySeqalign()
{
    for(int i = 0; i<k_NumAsciiChar; ++i) {
        delete [] m_Matrix[i];
    }
    delete [] m_Matrix;
    if (m_ConfigFile) {
        delete m_ConfigFile;
    } 
    if (m_Reg) {
        delete m_Reg;
    }
    
    if(m_DynamicFeature){
        delete m_DynamicFeature;
    }
}

///show blast identity, positive etc.
///@param out: output stream
///@param aln_stop: stop in aln coords
///@param identity: identity
///@param positive: positives
///@param match: match
///@param gap: gap
///@param master_strand: plus strand = 1 and minus strand = -1
///@param slave_strand:  plus strand = 1 and minus strand = -1
///@param master_frame: frame for master
///@param slave_frame: frame for slave
///@param aln_is_prot: is protein alignment?
///
static void s_DisplayIdentityInfo(CNcbiOstream& out, int aln_stop, 
                                   int identity, int positive, int match,
                                   int gap, int master_strand, 
                                   int slave_strand, int master_frame, 
                                   int slave_frame, bool aln_is_prot)
{
    out<<" Identities = "<<match<<"/"<<(aln_stop+1)<<" ("<<identity<<"%"<<")";
    if(aln_is_prot){
        out<<", Positives = "<<(positive + match)<<"/"<<(aln_stop+1)
           <<" ("<<(((positive + match)*100)/(aln_stop+1))<<"%"<<")";
    }
    out<<", Gaps = "<<gap<<"/"<<(aln_stop+1)
       <<" ("<<((gap*100)/(aln_stop+1))<<"%"<<")"<<endl;
    if (!aln_is_prot){ 
        out<<" Strand="<<(master_strand==1 ? "Plus" : "Minus")
           <<"/"<<(slave_strand==1? "Plus" : "Minus")<<endl;
    }
    if(master_frame != 0 && slave_frame != 0) {
        out <<" Frame = " << ((master_frame > 0) ? "+" : "") 
            << master_frame <<"/"<<((slave_frame > 0) ? "+" : "") 
            << slave_frame<<endl;
    } else if (master_frame != 0){
        out <<" Frame = " << ((master_frame > 0) ? "+" : "") 
            << master_frame << endl;
    }  else if (slave_frame != 0){
        out <<" Frame = " << ((slave_frame > 0) ? "+" : "") 
            << slave_frame <<endl;
    } 
    out<<endl;
    
}

///wrap line
///@param out: output stream
///@param str: string to wrap
///
static void s_WrapOutputLine(CNcbiOstream& out, const string& str)
{
    const int line_len = 60;
    const int front_space = 12;
    bool do_wrap = false;
    int length = (int) str.size();
    if (length > line_len) {
        for (int i = 0; i < length; i ++){
            if(i > 0 && i % line_len == 0){
                do_wrap = true;
            }   
            out << str[i];
            if(do_wrap && isspace((unsigned char) str[i])){
                out << endl;  
                CBlastFormatUtil::AddSpace(out, front_space);
                do_wrap = false;
            }
        }
    } else {
        out << str;
    }
}

///To add color to bases other than identityChar
///@param seq: sequence
///@param identity_char: identity character
///@param out: output stream
///
static void s_ColorDifferentBases(string& seq, char identity_char,
                                  CNcbiOstream& out){
    string base_color = "#FF0000";
    bool tagOpened = false;
    for(int i = 0; i < (int)seq.size(); i ++){
        if(seq[i] != identity_char){
            if(!tagOpened){
                out << "<font color=\""+base_color+"\"><b>";
                tagOpened =  true;
            }
            
        } else {
            if(tagOpened){
                out << "</b></font>";
                tagOpened = false;
            }
        }
        out << seq[i];
        if(tagOpened && i == (int)seq.size() - 1){
            out << "</b></font>";
            tagOpened = false;
        }
    } 
}

///return the frame for a given strand
///Note that start is zero bases.  It returns frame +/-(1-3). 0 indicates error
///@param start: sequence start position
///@param strand: strand
///@param id: the seqid
///@param scope: the scope
///@return: the frame
///
static int s_GetFrame (int start, ENa_strand strand, const CSeq_id& id, 
                       CScope& sp) 
{
    int frame = 0;
    if (strand == eNa_strand_plus) {
        frame = (start % 3) + 1;
    } else if (strand == eNa_strand_minus) {
        frame = -(((int)sp.GetBioseqHandle(id).GetBioseqLength() - start - 1)
                  % 3 + 1);
        
    }
    return frame;
}

///reture the frame for master seq in stdseg
///@param ss: the input stdseg
///@param scope: the scope
///@return: the frame
///
static int s_GetStdsegMasterFrame(const CStd_seg& ss, CScope& scope)
{
    const CRef<CSeq_loc> slc = ss.GetLoc().front();
    ENa_strand strand = GetStrand(*slc);
    int frame = s_GetFrame(strand ==  eNa_strand_plus ?
                           GetStart(*slc, &scope) : GetStop(*slc, &scope),
                           strand ==  eNa_strand_plus ?
                           eNa_strand_plus : eNa_strand_minus,
                           *(ss.GetIds().front()), scope);
    return frame;
}

///return the get sequence table for html display
///@param form_name: form name
///@parm db_is_na: is the db of nucleotide type?
///@query_number: the query number
///@return: the from string
///
static string s_GetSeqForm(char* form_name, bool db_is_na, int query_number,
                           int db_type)
{
    char buf[2048] = {""};
    if(form_name){
        string template_str = "<table border=\"0\"><tr><td>" + \
            k_GetSeqSubmitForm[db_type] + \
            "</td><td>" + \
            k_GetSeqSelectForm +\
            "</td></tr></table>";
        sprintf(buf, template_str.c_str(), form_name, query_number, \
                db_is_na?1:0, query_number, form_name, query_number, db_type,
                query_number, query_number);      
        
    }
    return buf;
}

///return id type specified or null ref
///@param ids: the input ids
///@param choice: id of choice
///@return: the id with specified type
///
static CRef<CSeq_id> s_GetSeqIdByType(const list<CRef<CSeq_id> >& ids, 
                                      CSeq_id::E_Choice choice)
{
    CRef<CSeq_id> cid;
    
    for (CBioseq::TId::const_iterator iter = ids.begin(); iter != ids.end(); 
         iter ++){
        if ((*iter)->Which() == choice){
            cid = *iter;
            break;
        }
    }
    
    return cid;
}

///return gi from id list
///@param ids: the input ids
///@return: the gi if found
///
static int s_GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids)
{
    int gi = 0;
    CRef<CSeq_id> id = s_GetSeqIdByType(ids, CSeq_id::e_Gi);
    if (!(id.Empty())){
        return id->GetGi();
    }
    return gi;
}

///fill the cds start positions (1 based)
///@param line: the input cds line
///@param concat_exon: exon only string
///@param length_per_line: alignment length per line
///@param feat_aln_start_totalexon: feature aln pos in concat_exon
///@param strand: the alignment strand
///@param start: start list to be filled
///
static void s_FillCdsStartPosition(string& line, string& concat_exon,
                                   size_t length_per_line,
                                   TSeqPos feat_aln_start_totalexon,
                                   ENa_strand seq_strand, 
                                   ENa_strand feat_strand,
                                   list<TSeqPos>& start){
    size_t actual_line_len = 0;
    size_t aln_len = line.size();
    TSeqPos previous_num_letter = 0;
    
    //the number of amino acids preceeding this exon start position
    for (size_t i = 0; i <= feat_aln_start_totalexon; i ++){
        if(feat_strand == eNa_strand_minus){
            //remember the amino acid in this case goes backward
            //therefore we count backward too
            if(isalpha((unsigned char) concat_exon[concat_exon.size() -1 - i])){
                previous_num_letter ++;
            }
            
        } else {
            if(isalpha((unsigned char) concat_exon[i])){
                previous_num_letter ++;
            }
        }
    }
    
    
    TSeqPos prev_num = 0;
    //go through the entire feature line and get the amino acid position 
    //for each line
    for(size_t i = 0; i < aln_len; i += actual_line_len){
        //handle the last row which may be shorter
        if(aln_len - i< length_per_line) {
            actual_line_len = aln_len - i;
        } else {
            actual_line_len = length_per_line;
        }
        //the number of amino acids on this row
        TSeqPos cur_num = 0;
        bool has_intron = false;
        
        //go through each character on a row
        for(size_t j = i; j < actual_line_len + i; j ++){
            //don't count gap
            if(isalpha((unsigned char) line[j])){
                cur_num ++;
            } else if(line[j] == k_IntronChar){
                has_intron = true;
            }
        }
            
        if(cur_num > 0){
            if(seq_strand == eNa_strand_plus){
                if(feat_strand == eNa_strand_minus) {
                    start.push_back(previous_num_letter - prev_num); 
                } else {
                    start.push_back(previous_num_letter + prev_num);  
                }
            } else {
                if(feat_strand == eNa_strand_minus) {
                    start.push_back(previous_num_letter + prev_num);  
                } else {
                    start.push_back(previous_num_letter - prev_num);  
                } 
            }
        } else if (has_intron) {
            start.push_back(0);  //sentinal for now show
        }
        prev_num += cur_num;
    }
}


string CDisplaySeqalign::x_GetUrl(const list<CRef<CSeq_id> >& ids, int gi, 
                                  int row, int taxid) const
{
    string urlLink = NcbiEmptyString;
    
    char dopt[32], db[32];
    gi = (gi == 0) ? s_GetGiForSeqIdList(ids):gi;
    string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
    if(toolUrl == NcbiEmptyString || (gi > 0 && toolUrl.find("dumpgnl.cgi") 
                                      != string::npos)){ 
        //use entrez or dbtag specified
        if(m_IsDbNa) {
            strcpy(dopt, "GenBank");
            strcpy(db, "Nucleotide");
        } else {
            strcpy(dopt, "GenPept");
            strcpy(db, "Protein");
        }    
        
        char urlBuf[2048];
        if (gi > 0) {
            sprintf(urlBuf, kEntrezUrl.c_str(), db, gi, dopt, 
                    (m_AlignOption & eNewTargetWindow) ? 
                    "TARGET=\"EntrezView\"" : "");
            urlLink = urlBuf;
        } else {//seqid general, dbtag specified
            const CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
            if(wid->Which() == CSeq_id::e_General){
                const CDbtag& dtg = wid->GetGeneral();
                const string& dbName = dtg.GetDb();
                if(NStr::CompareNocase(dbName, "TI") == 0){
                    sprintf(urlBuf, kTraceUrl.c_str(), 
                            wid->GetSeqIdString().c_str());
                    urlLink = urlBuf;
                } else { //future use
                    
                }
            }
        }
    } else { //need to use url in configuration file
        string altUrl = NcbiEmptyString;
        urlLink = x_GetDumpgnlLink(ids, row, altUrl, taxid);
    }
    return urlLink;
}


void CDisplaySeqalign::x_AddLinkout(const CBioseq& cbsp, 
                                    const CBlast_def_line& bdl,
                                    int first_gi, int gi, 
                                    CNcbiOstream& out) const
{
    char molType[8]={""};
    if(cbsp.IsAa()){
        sprintf(molType, "[pgi]");
    }
    else {
        sprintf(molType, "[ngi]");
    }
    
    if (bdl.IsSetLinks()){
        for (list< int >::const_iterator iter = bdl.GetLinks().begin(); 
             iter != bdl.GetLinks().end(); iter ++){
            char buf[1024];
            
            if ((*iter) & eUnigene) {
                sprintf(buf, kUnigeneUrl.c_str(),  gi);
                out << buf;
            }
            if ((*iter) & eStructure){
                sprintf(buf, kStructureUrl.c_str(), m_Rid.c_str(), first_gi,
                        gi, m_CddRid.c_str(), "onepair", 
                        (m_EntrezTerm == NcbiEmptyString) ? 
                        "none":((char*) m_EntrezTerm.c_str()));
                out << buf;
            }
            if ((*iter) & eGeo){
                sprintf(buf, kGeoUrl.c_str(), gi);
                out << buf;
            }
            if((*iter) & eGene){
                sprintf(buf, kGeneUrl.c_str(), gi, cbsp.IsAa() ? 
                        "PUID" : "NUID");
                out << buf;
            }
        }
    }
}


void CDisplaySeqalign::x_DisplayAlnvec(CNcbiOstream& out)
{ 
    size_t maxIdLen=0, maxStartLen=0, startLen=0, actualLineLen=0;
    size_t aln_stop=m_AV->GetAlnStop();
    const int rowNum=m_AV->GetNumRows();   
    if(m_AlignOption & eMasterAnchored){
        m_AV->SetAnchor(0);
    }
    m_AV->SetGapChar('-');
    m_AV->SetEndChar(' ');
    vector<string> sequence(rowNum);
    CAlnMap::TSeqPosList* seqStarts = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* seqStops = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* insertStart = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* insertAlnStart = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* insertLength = new CAlnMap::TSeqPosList[rowNum];
    string* seqidArray=new string[rowNum];
    string middleLine;
    CAlnMap::TSignedRange* rowRng = new CAlnMap::TSignedRange[rowNum];
    int* frame = new int[rowNum];
    int* taxid = new int[rowNum];

    //Add external query feature info such as phi blast pattern
    list<SAlnFeatureInfo*>* bioseqFeature= x_GetQueryFeatureList(rowNum,
                                                                (int)aln_stop);
    //conver to aln coordinates for mask seqloc
    list<SAlnSeqlocInfo*> alnLocList;
    x_FillLocList(alnLocList);    
    //prepare data for each row
    for (int row=0; row<rowNum; row++) {
        string type_temp = NStr::ToLower(m_BlastType);
        if(type_temp.find("mapview") != string::npos || 
           type_temp.find("gsfasta") != string::npos){
            taxid[row] = CBlastFormatUtil::GetTaxidForSeqid(m_AV->GetSeqId(row),
                                                            m_Scope);
        } else {
            taxid[row] = 0;
        }
        rowRng[row] = m_AV->GetSeqAlnRange(row);
        frame[row] = (m_AV->GetWidth(row) == 3 ? 
                      s_GetFrame(m_AV->IsPositiveStrand(row) ? 
                                 m_AV->GetSeqStart(row) : 
                                 m_AV->GetSeqStop(row), 
                                 m_AV->IsPositiveStrand(row) ? 
                                 eNa_strand_plus : eNa_strand_minus, 
                                 m_AV->GetSeqId(row), m_Scope) : 0);        
        //make sequence
        m_AV->GetWholeAlnSeqString(row, sequence[row], &insertAlnStart[row],
                                   &insertStart[row], &insertLength[row],
                                   (int)m_LineLen, &seqStarts[row], &seqStops[row]);
        //make feature. Only for pairwise and untranslated for subject seq
        if(!(m_AlignOption & eMasterAnchored) && row > 0 &&
           !(m_AlignOption & eMultiAlign) && m_AV->GetWidth(row) != 3){
            if(m_AlignOption & eShowCdsFeature){
                x_GetFeatureInfo(bioseqFeature[row], *m_featScope, 
                                 CSeqFeatData::e_Cdregion, row, sequence[row]);
            }
            if(m_AlignOption & eShowGeneFeature){
                x_GetFeatureInfo(bioseqFeature[row], *m_featScope,
                                 CSeqFeatData::e_Gene, row, sequence[row]);
            }
        }
        //make id
        x_FillSeqid(seqidArray[row], row);
        maxIdLen=max<size_t>(seqidArray[row].size(), maxIdLen);
        size_t maxCood=max<size_t>(m_AV->GetSeqStart(row), m_AV->GetSeqStop(row));
        maxStartLen = max<size_t>(NStr::IntToString(maxCood).size(), maxStartLen);
    }
    for(int i = 0; i < rowNum; i ++){//adjust max id length for feature id 
        for (list<SAlnFeatureInfo*>::iterator iter=bioseqFeature[i].begin();
             iter != bioseqFeature[i].end(); iter++){
            maxIdLen=max<size_t>((*iter)->feature->feature_id.size(), maxIdLen );
        }
    }  //end of preparing row data
    bool colorMismatch = false; //color the mismatches
    //output identities info 
    if(m_AlignOption&eShowBlastInfo && !(m_AlignOption&eMultiAlign)) {
        int match = 0;
        int positive = 0;
        int gap = 0;
        int identity = 0;
        x_FillIdentityInfo(sequence[0], sequence[1], match, positive, middleLine);
        identity = (match*100)/((int)aln_stop+1);
        if(identity >= k_ColorMismatchIdentity && identity <100){
            colorMismatch = true;
        }
        gap = x_GetNumGaps();
        s_DisplayIdentityInfo(out, (int)aln_stop, identity, positive, match, gap,
                               m_AV->StrandSign(0), m_AV->StrandSign(1),
                               frame[0], frame[1], 
							   ((m_AlignType & eProt) != 0 ? true : false));
    }
    //output rows
    for(int j=0; j<=(int)aln_stop; j+=(int)m_LineLen){
        //output according to aln coordinates
        if(aln_stop-j+1<m_LineLen) {
            actualLineLen=aln_stop-j+1;
        } else {
            actualLineLen=m_LineLen;
        }
        CAlnMap::TSignedRange curRange(j, j+(int)actualLineLen-1);
        //here is each row
        for (int row=0; row<rowNum; row++) {
            bool hasSequence = true;   
            hasSequence = curRange.IntersectingWith(rowRng[row]);
            //only output rows that have sequence
            if (hasSequence){
                int start = seqStarts[row].front() + 1;  //+1 for 1 based
                int end = seqStops[row].front() + 1;
                list<string> inserts;
                string insertPosString;  //the one with "\" to indicate insert
                if(m_AlignOption & eMasterAnchored){
                    list<SInsertInformation*> insertList;
                    x_GetInserts(insertList, insertAlnStart[row], 
                                 insertStart[row], insertLength[row],  
                                 j + (int)m_LineLen);
                    x_FillInserts(row, curRange, j, inserts, insertPosString, 
                                  insertList);
                    ITERATE(list<SInsertInformation*>, iterINsert, insertList){
                        delete *iterINsert;
                    }
                }
                if(row == 0&&(m_AlignOption&eHtml)
                   &&(m_AlignOption&eMultiAlign) 
                   && (m_AlignOption&eSequenceRetrieval && m_CanRetrieveSeq)){
                    char checkboxBuf[200];
                    sprintf(checkboxBuf, k_UncheckabeCheckbox.c_str(), m_QueryNumber);
                    out << checkboxBuf;
                }
                string urlLink;
                //setup url link for seqid
                if(row>0&&(m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign)){
                    int gi = 0;
                    if(m_AV->GetSeqId(row).Which() == CSeq_id::e_Gi){
                        gi = m_AV->GetSeqId(row).GetGi();
                    }
                    if(!(gi > 0)){
                        gi = s_GetGiForSeqIdList(m_AV->GetBioseqHandle(row).\
                                                 GetBioseqCore()->GetId());
                    }
                    if(gi > 0){
                        out<<"<a name="<<gi<<"></a>";
                    } else {
                        out<<"<a name="<<seqidArray[row]<<"></a>";
                    }
                    //get sequence checkbox
                    if(m_AlignOption&eSequenceRetrieval && m_CanRetrieveSeq){
                        char checkBoxBuf[512];
                        sprintf(checkBoxBuf, k_Checkbox.c_str(), gi > 0 ?
                                NStr::IntToString(gi).c_str() : seqidArray[row].c_str(),                       
                                m_QueryNumber);
                        out << checkBoxBuf;        
                    }
                    urlLink = x_GetUrl(m_AV->GetBioseqHandle(row).\
                                       GetBioseqCore()->GetId(), gi, 
                                       row, taxid[row]);     
                    out << urlLink;                    
                }
                
                bool has_mismatch = false;
                //change the alignment line to identity style
                if (row>0 && m_AlignOption & eShowIdentity){
                    for (int index = j; index < j + (int)actualLineLen && 
                             index < (int)sequence[row].size(); index ++){
                        if (sequence[row][index] == sequence[0][index] &&
                            isalpha((unsigned char) sequence[row][index])) {
                            sequence[row][index] = k_IdentityChar;           
                        } else if (!has_mismatch) {
                            has_mismatch = true;
                        }        
                    }
                }
                
                //highlight the seqid for pairwise-with-identity format
                if(row>0 && m_AlignOption&eHtml && !(m_AlignOption&eMultiAlign)
                   && m_AlignOption&eShowIdentity && has_mismatch){
                    out<< "<font color = ff0000><b>";         
                }
                out<<seqidArray[row]; 
                if(row>0&& m_AlignOption&eHtml && m_AlignOption&eMultiAlign
                   && urlLink != NcbiEmptyString){
                    out<<"</a>";   
                }
                //highlight the seqid for pairwise-with-identity format
                if(row>0 && m_AlignOption&eHtml && !(m_AlignOption&eMultiAlign)
                   && m_AlignOption&eShowIdentity && has_mismatch){
                    out<< "</b></font>";         
                } 
                
                //adjust space between id and start
                CBlastFormatUtil::AddSpace(out, 
                                           maxIdLen-seqidArray[row].size()+
                                           k_IdStartMargin);
                out << start;
                startLen=NStr::IntToString(start).size();
                CBlastFormatUtil::AddSpace(out, maxStartLen-startLen+
                                           k_StartSequenceMargin);
                x_OutputSeq(sequence[row], m_AV->GetSeqId(row), j, 
                            (int)actualLineLen, frame[row], row,
                            (row > 0 && colorMismatch)?true:false,  
                            alnLocList, out);
                CBlastFormatUtil::AddSpace(out, k_SeqStopMargin);
                out << end;
                out<<endl;
                if(m_AlignOption & eMasterAnchored){//inserts for anchored view
                    bool insertAlready = false;
                    for(list<string>::iterator iter = inserts.begin(); 
                        iter != inserts.end(); iter ++){   
                        if(!insertAlready){
                            if((m_AlignOption&eHtml)
                               &&(m_AlignOption&eMultiAlign) 
                               && (m_AlignOption&eSequenceRetrieval 
                                   && m_CanRetrieveSeq)){
                                char checkboxBuf[200];
                                sprintf(checkboxBuf, 
                                        k_UncheckabeCheckbox.c_str(),
                                        m_QueryNumber);
                                out << checkboxBuf;
                            }
                            CBlastFormatUtil::AddSpace(out, 
                                                       maxIdLen
                                                       +k_IdStartMargin
                                                       +maxStartLen
                                                       +k_StartSequenceMargin);
                            out << insertPosString<<endl;
                        }
                        if((m_AlignOption&eHtml)
                           &&(m_AlignOption&eMultiAlign) 
                           && (m_AlignOption&eSequenceRetrieval && m_CanRetrieveSeq)){
                            char checkboxBuf[200];
                            sprintf(checkboxBuf, k_UncheckabeCheckbox.c_str(),
                                    m_QueryNumber);
                            out << checkboxBuf;
                        }
                        CBlastFormatUtil::AddSpace(out, maxIdLen
                                                   +k_IdStartMargin
                                                   +maxStartLen
                                                   +k_StartSequenceMargin);
                        out<<*iter<<endl;
                        insertAlready = true;
                    }
                } 
                //display feature. Feature, if set, 
                //will be displayed for query regardless
                CSeq_id no_id;
                for (list<SAlnFeatureInfo*>::iterator 
                         iter=bioseqFeature[row].begin(); 
                     iter != bioseqFeature[row].end(); iter++){
                    if ( curRange.IntersectingWith((*iter)->aln_range)){  
                        if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign)
                           && (m_AlignOption&eSequenceRetrieval && m_CanRetrieveSeq)){
                            char checkboxBuf[200];
                            sprintf(checkboxBuf,  k_UncheckabeCheckbox.c_str(),
                                    m_QueryNumber);
                            out << checkboxBuf;
                        }
                        out<<(*iter)->feature->feature_id;
                        if((*iter)->feature_start.empty()){
                            CBlastFormatUtil::
                                AddSpace(out, maxIdLen + k_IdStartMargin
                                         +maxStartLen + k_StartSequenceMargin
                                         -(*iter)->feature->feature_id.size());
                        } else {
                            int feat_start = (*iter)->feature_start.front();
                            if(feat_start > 0){
                                CBlastFormatUtil::
                                    AddSpace(out, maxIdLen + k_IdStartMargin
                                             -(*iter)->feature->feature_id.size());
                                out << feat_start;
                                CBlastFormatUtil::
                                    AddSpace(out, maxStartLen -
                                             NStr::IntToString(feat_start).size() +
                                             k_StartSequenceMargin);
                            } else { //no show start
                                CBlastFormatUtil::
                                    AddSpace(out, maxIdLen + k_IdStartMargin
                                             +maxStartLen + k_StartSequenceMargin
                                             -(*iter)->feature->feature_id.size());
                            }
                            
                            (*iter)->feature_start.pop_front();
                        }
                        x_OutputSeq((*iter)->feature_string, no_id, j,
                                    (int)actualLineLen, 0, row, false,  alnLocList, out);
                        out<<endl;
                    }
                }
                //display middle line
                if (row == 0 && ((m_AlignOption & eShowMiddleLine)) 
                    && !(m_AlignOption&eMultiAlign)) {
                    CBlastFormatUtil::
                        AddSpace(out, maxIdLen + k_IdStartMargin
                                 + maxStartLen + k_StartSequenceMargin);
                    x_OutputSeq(middleLine, no_id, j, (int)actualLineLen, 0, row, 
                                false,  alnLocList, out);
                    out<<endl;
                }
            }
            if(!seqStarts[row].empty()){ //shouldn't need this check
                seqStarts[row].pop_front();
            }
            if(!seqStops[row].empty()){
                seqStops[row].pop_front();
            }
        }
        out<<endl;
    }//end of displaying rows
   
    for(int i = 0; i < rowNum; i ++){ //free allocation
        ITERATE(list<SAlnFeatureInfo*>, iter, bioseqFeature[i]){
            delete (*iter)->feature;
            delete (*iter);
        }
    } 
    ITERATE(list<SAlnSeqlocInfo*>, iter, alnLocList){
        delete (*iter);
    }
    delete [] bioseqFeature;
    delete [] seqidArray;
    delete [] rowRng;
    delete [] seqStarts;
    delete [] seqStops;
    delete [] frame;
    delete [] insertStart;
    delete [] insertAlnStart;
    delete [] insertLength;
    delete [] taxid;
}


void CDisplaySeqalign::DisplaySeqalign(CNcbiOstream& out)
{   
    CSeq_align_set actual_aln_list;
    CBlastFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                     *m_SeqalignSetRef);
    if (actual_aln_list.Get().empty()){
        return;
    }
    //scope for feature fetching
    if(!(m_AlignOption & eMasterAnchored) 
       && (m_AlignOption & eShowCdsFeature || m_AlignOption 
           & eShowGeneFeature)){
        m_FeatObj = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_FeatObj);
        m_featScope = new CScope(*m_FeatObj);  //for seq feature fetch
        string name = CGBDataLoader::GetLoaderNameFromArgs();
        m_featScope->AddDataLoader(name);
    }
    //for whether to add get sequence feature
    m_CanRetrieveSeq = x_GetDbType(actual_aln_list) == eDbTypeNotSet ? false : true;
    if(m_AlignOption & eHtml || m_AlignOption & eDynamicFeature){
        //set config file
        m_ConfigFile = new CNcbiIfstream(".ncbirc");
        m_Reg = new CNcbiRegistry(*m_ConfigFile);
        string feat_file = m_Reg->Get("FEATURE_INFO", "FEATURE_FILE");
        string feat_file_index = m_Reg->Get("FEATURE_INFO",
                                            "FEATURE_FILE_INDEX");
        if(feat_file != NcbiEmptyString && feat_file_index != NcbiEmptyString){
            m_DynamicFeature = new CGetFeature(feat_file, feat_file_index);
        }
    }
    if(m_AlignOption & eHtml){  
        out<<"<script src=\"blastResult.js\"></script>";
    }
    //get sequence 
    if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_CanRetrieveSeq){ 
        out<<s_GetSeqForm((char*)"submitterTop", m_IsDbNa, m_QueryNumber, 
                          x_GetDbType(actual_aln_list));
        out<<"<form name=\"getSeqAlignment"<<m_QueryNumber<<"\">\n";
    }
    //begin to display
    int num_align = 0;
    string toolUrl = NcbiEmptyString;
    if(m_AlignOption & eHtml){
        toolUrl = m_Reg->Get(m_BlastType, "TOOL_URL");
    }
    auto_ptr<CObjectOStream> out2(CObjectOStream::Open(eSerial_AsnText, out));
    //*out2 << *m_SeqalignSetRef;
    if(!(m_AlignOption&eMultiAlign)){
        /*pairwise alignment. Note we can't just show each alnment as we go
          because we will need seg information form all hsp's with the same id
          for genome url link.  As a result we show hsp's with the same id 
          as a group*/
        list <SAlnInfo*> avList;        
        CConstRef<CSeq_id> previousId, subid;
        bool isFirstAln = true;
        for (CSeq_align_set::Tdata::const_iterator 
                 iter =  actual_aln_list.Get().begin(); 
             iter != actual_aln_list.Get().end() 
                 && num_align<m_NumAlignToShow; iter++, num_align++) {
            //make alnvector
            CRef<CAlnVec> avRef;
            CRef<CSeq_align> finalAln;
            if((*iter)->GetSegs().Which() == CSeq_align::C_Segs::e_Std){
                CRef<CSeq_align> densegAln = (*iter)->CreateDensegFromStdseg();
                if (m_AlignOption & eTranslateNucToNucAlignment) { 
                    finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
                } else {
                    finalAln = densegAln;
                }            
            } else if((*iter)->GetSegs().Which() == 
                      CSeq_align::C_Segs::e_Denseg){
                if (m_AlignOption & eTranslateNucToNucAlignment) { 
                    finalAln = (*iter)->CreateTranslatedDensegFromNADenseg();
                } else {
                    finalAln = (*iter);
                }
            } else if((*iter)->GetSegs().Which() == 
                      CSeq_align::C_Segs::e_Dendiag){
                CRef<CSeq_align> densegAln = 
                    CBlastFormatUtil::CreateDensegFromDendiag(**iter);
                if (m_AlignOption & eTranslateNucToNucAlignment) { 
                    finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
                } else {
                    finalAln = densegAln;
                }
            } else {
                NCBI_THROW(CException, eUnknown, 
                           "Seq-align should be Denseg, Stdseg or Dendiag!");
            }
            CRef<CDense_seg> finalDenseg(new CDense_seg);
            const CTypeIterator<CDense_seg> ds = Begin(*finalAln);
            if((ds->IsSetStrands() 
                && ds->GetStrands().front()==eNa_strand_minus) 
               && !(ds->IsSetWidths() && ds->GetWidths()[0] == 3)){
                //show plus strand if master is minus for non-translated case
                finalDenseg->Assign(*ds);
                finalDenseg->Reverse();
                avRef = new CAlnVec(*finalDenseg, m_Scope);   
            } else {
                avRef = new CAlnVec(*ds, m_Scope);
            }        
            if(!(avRef.Empty())){
                //Note: do not switch the set order per calnvec specs.
                avRef->SetGenCode(m_SlaveGeneticCode);
                avRef->SetGenCode(m_MasterGeneticCode, 0);
                try{
                    const CBioseq_Handle& handle = avRef->GetBioseqHandle(1);
                    if(handle){
                        subid=&(avRef->GetSeqId(1));
                    
                        if(!isFirstAln && !subid->Match(*previousId)) {
                            //this aln is a new id, show result for previous id
                            x_DisplayAlnvecList(out, avList);
                            
                            for(list<SAlnInfo*>::iterator 
                                    iterAv = avList.begin();
                                iterAv != avList.end(); iterAv ++){
                                delete(*iterAv);
                            }
                            avList.clear();   
                        }
                        //save the current alnment regardless
                        SAlnInfo* alnvecInfo = new SAlnInfo;
                        int sum_n;
                        int num_ident;
                        CBlastFormatUtil::GetAlnScores(**iter, 
                                                       alnvecInfo->score, 
                                                       alnvecInfo->bits, 
                                                       alnvecInfo->evalue, 
                                                       sum_n, 
                                                       num_ident,
                                                       alnvecInfo->use_this_gi,
                                                       alnvecInfo->comp_adj_method);
                        alnvecInfo->alnvec = avRef;
                        avList.push_back(alnvecInfo);
                        int gi = s_GetGiForSeqIdList(handle.\
                                                     GetBioseqCore()->GetId());
                        if(!(toolUrl == NcbiEmptyString 
                             || (gi > 0 
                                 && toolUrl.find("dumpgnl.cgi") 
                                 != string::npos)) 
                           || (m_AlignOption & eLinkout)){
                            /*need to construct segs for dumpgnl and
                              get sub-sequence for long sequences*/
                            string idString = avRef->GetSeqId(1).GetSeqIdString();
                            if(m_Segs.count(idString) > 0){ 
                                //already has seg, concatenate
                                /*Note that currently it's not necessary to 
                                  use map to store this information.  
                                  But I already implemented this way for 
                                  previous version.  Will keep this way as
                                  it's more flexible if we change something*/
                            
                                m_Segs[idString] += "," 
                                    + NStr::IntToString(avRef->GetSeqStart(1))
                                    + "-" + 
                                    NStr::IntToString(avRef->GetSeqStop(1));
                            } else {//new segs
                                m_Segs.insert(map<string, string>::
                                              value_type(idString, 
                                                         NStr::
                                                         IntToString\
                                                         (avRef->GetSeqStart(1))
                                                         + "-" + 
                                                         NStr::IntToString\
                                                         (avRef->GetSeqStop(1))));
                            }
                        }	    
                        isFirstAln = false;
                        previousId = subid;
                    }                
                } catch (const CException&){
                    continue;
                }
            }
        } 
        //Show here for the last one 
        if(!avList.empty()){
            x_DisplayAlnvecList(out, avList);
            for(list<SAlnInfo*>::iterator iterAv = avList.begin(); 
                iterAv != avList.end(); iterAv ++){
                delete(*iterAv);
            }
            avList.clear();
        }
    } else if(m_AlignOption&eMultiAlign){ //multiple alignment
        CRef<CAlnMix>* mix = new CRef<CAlnMix>[k_NumFrame]; 
        //each for one frame for translated alignment
        for(int i = 0; i < k_NumFrame; i++){
            mix[i] = new CAlnMix(m_Scope);
        }        
        num_align = 0;
        vector<CRef<CSeq_align_set> > alnVector(k_NumFrame);
        for(int i = 0; i <  k_NumFrame; i ++){
            alnVector[i] = new CSeq_align_set;
        }
        for (CSeq_align_set::Tdata::const_iterator 
                 alnIter = actual_aln_list.Get().begin(); 
             alnIter != actual_aln_list.Get().end() 
                 && num_align<m_NumAlignToShow; alnIter ++, num_align++) {
            const CBioseq_Handle& subj_handle = 
                m_Scope.GetBioseqHandle((*alnIter)->GetSeq_id(1));
            if(subj_handle){
                //need to convert to denseg for stdseg
                if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::e_Std) {
                    CTypeConstIterator<CStd_seg> ss = ConstBegin(**alnIter); 
                    CRef<CSeq_align> convertedDs = 
                        (*alnIter)->CreateDensegFromStdseg();
                    if((convertedDs->GetSegs().GetDenseg().IsSetWidths() 
                        && convertedDs->GetSegs().GetDenseg().GetWidths()[0] == 3)
                       || m_AlignOption & eTranslateNucToNucAlignment){
                        //only do this for translated master
                        int frame = s_GetStdsegMasterFrame(*ss, m_Scope);
                        switch(frame){
                        case 1:
                            alnVector[0]->Set().push_back(convertedDs);
                            break;
                        case 2:
                            alnVector[1]->Set().push_back(convertedDs);
                            break;
                        case 3:
                            alnVector[2]->Set().push_back(convertedDs);
                            break;
                        case -1:
                            alnVector[3]->Set().push_back(convertedDs);
                            break;
                        case -2:
                            alnVector[4]->Set().push_back(convertedDs);
                            break;
                        case -3:
                            alnVector[5]->Set().push_back(convertedDs);
                            break;
                        default:
                            break;
                        }
                    }
                    else {
                        alnVector[0]->Set().push_back(convertedDs);
                    }
                } else if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::
                          e_Denseg){
                    alnVector[0]->Set().push_back(*alnIter);
                } else if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::
                          e_Dendiag){
                    alnVector[0]->Set().\
                        push_back(CBlastFormatUtil::CreateDensegFromDendiag(**alnIter));
                } else {
                    NCBI_THROW(CException, eUnknown, 
                               "Input Seq-align should be Denseg, Stdseg or Dendiag!");
                }
            }
        }
        for(int i = 0; i < (int)alnVector.size(); i ++){
            bool hasAln = false;
            for(CTypeConstIterator<CSeq_align> 
                    alnRef = ConstBegin(*alnVector[i]); alnRef; ++alnRef){
                CTypeConstIterator<CDense_seg> ds = ConstBegin(*alnRef);
                //*out2 << *ds;      
                try{
                    if (m_AlignOption & eTranslateNucToNucAlignment) {	 
                        mix[i]->Add(*ds, CAlnMix::fForceTranslation);
                    } else {
                        mix[i]->Add(*ds);
                    }
                } catch (const CException& e){
                    out << "Warning: " << e.what();
                    continue;
                }
                hasAln = true;
            }
            if(hasAln){
                //    *out2<<*alnVector[i];
                mix[i]->Merge(CAlnMix::fGen2EST| CAlnMix::fMinGap 
                              | CAlnMix::fQuerySeqMergeOnly 
                              | CAlnMix::fFillUnalignedRegions);  
                //	*out2<<mix[i]->GetDenseg();
            }
        }
        
        int numDistinctFrames = 0;
        for(int i = 0; i < (int)alnVector.size(); i ++){
            if(!alnVector[i]->Get().empty()){
                numDistinctFrames ++;
            }
        }
        out<<endl;
        for(int i = 0; i < k_NumFrame; i ++){
            try{
                CRef<CAlnVec> avRef (new CAlnVec (mix[i]->GetDenseg(), 
                                                  m_Scope));
                avRef->SetGenCode(m_SlaveGeneticCode);
                avRef->SetGenCode(m_MasterGeneticCode, 0);
                m_AV = avRef;
                
                if(numDistinctFrames > 1){
                    out << "For reading frame " << k_FrameConversion[i] 
                        << " of query sequence:" << endl << endl;
                }
                x_DisplayAlnvec(out);
            } catch (CException e){
                continue;
            }
        } 
        delete [] mix;
    }
    if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_CanRetrieveSeq){
        out<<"</form>\n";
        out<<s_GetSeqForm((char*)"submitterBottom", m_IsDbNa, m_QueryNumber, 
                          x_GetDbType(actual_aln_list));
    }
}


void CDisplaySeqalign::x_FillIdentityInfo(const string& sequence_standard,
                                          const string& sequence , 
                                          int& match, int& positive, 
                                          string& middle_line) 
{
    match = 0;
    positive = 0;
    int min_length=min<int>((int)sequence_standard.size(), (int)sequence.size());
    if(m_AlignOption & eShowMiddleLine){
        middle_line = sequence;
    }
    for(int i=0; i<min_length; i++){
        if(sequence_standard[i]==sequence[i]){
            if(m_AlignOption & eShowMiddleLine){
                if(m_MidLineStyle == eBar ) {
                    middle_line[i] = '|';
                } else if (m_MidLineStyle == eChar){
                    middle_line[i] = sequence[i];
                }
            }
            match ++;
        } else {
            if ((m_AlignType&eProt) 
                && m_Matrix[sequence_standard[i]][sequence[i]] > 0){  
                positive ++;
                if(m_AlignOption & eShowMiddleLine){
                    if (m_MidLineStyle == eChar){
                        middle_line[i] = '+';
                    }
                }
            } else {
                if (m_AlignOption & eShowMiddleLine){
                    middle_line[i] = ' ';
                }
            }    
        }
    }  
}


void 
CDisplaySeqalign::x_PrintDefLine(const CBioseq_Handle& bsp_handle,
                                 list<int>& use_this_gi, 
                                 CNcbiOstream& out) const
{
    if(bsp_handle){
        const CRef<CSeq_id> wid =
            FindBestChoice(bsp_handle.GetBioseqCore()->GetId(), 
                           CSeq_id::WorstRank);
    
        const CRef<CBlast_def_line_set> bdlRef 
            =  CBlastFormatUtil::GetBlastDefline(bsp_handle);
        const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
        bool isFirst = true;
        int firstGi = 0;
    
        if(bdl.empty()){ //no blast defline struct, should be no such case now
            out << ">"; 
            wid->WriteAsFasta(out);
            out<<" ";
            s_WrapOutputLine(out, GetTitle(bsp_handle));
            out << endl;
        } else {
            //print each defline 
            for(list< CRef< CBlast_def_line > >::const_iterator 
                    iter = bdl.begin(); iter != bdl.end(); iter++){
                string urlLink;
                int gi =  s_GetGiForSeqIdList((*iter)->GetSeqid());
                int gi_in_use_this_gi = 0;
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(gi == *iter_gi){
                        gi_in_use_this_gi = *iter_gi;
                        break;
                    }
                }
                if(use_this_gi.empty() || gi_in_use_this_gi > 0) {
                
                    if(isFirst){
                        out << ">";
                        
                    } else{
                        out << " ";
                    }
                    const CRef<CSeq_id> wid2
                        = FindBestChoice((*iter)->GetSeqid(),
                                         CSeq_id::WorstRank);
                
                    if(isFirst){
                        firstGi = gi;
                    }
                    if ((m_AlignOption&eSequenceRetrieval)
                        && (m_AlignOption&eHtml) && m_CanRetrieveSeq && isFirst) {
                        char buf[512];
                        sprintf(buf, k_Checkbox.c_str(), gi > 0 ?
                                NStr::IntToString(gi).c_str() : wid2->GetSeqIdString().c_str(),
                                m_QueryNumber);
                        out << buf;
                    }
                
                    if(m_AlignOption&eHtml){
                        int taxid = 0;
                        string type_temp = m_BlastType;
                        if(NStr::ToLower(type_temp).find("mapview") 
                           != string::npos || 
                           NStr::ToLower(type_temp).find("gsfasta") != 
                           string::npos && 
                           (*iter)->IsSetTaxid() && 
                           (*iter)->CanGetTaxid()){
                            taxid = (*iter)->GetTaxid();
                        }
                        urlLink = x_GetUrl((*iter)->GetSeqid(), 
                                           gi_in_use_this_gi, 1, taxid);    
                        out<<urlLink;
                    }
                
                    if(m_AlignOption&eShowGi && gi > 0){
                        out<<"gi|"<<gi<<"|";
                    }     
                    if(!(wid2->AsFastaString().find("gnl|BL_ORD_ID") 
                         != string::npos)){
                        wid2->WriteAsFasta(out);
                    }
                    if(m_AlignOption&eHtml){
                        if(urlLink != NcbiEmptyString){
                            out<<"</a>";
                        }
                        if(gi != 0){
                            out<<"<a name="<<gi<<"></a>";
                        } else {
                            out<<"<a name="<<wid2->GetSeqIdString()<<"></a>";
                        }
                        if(m_AlignOption&eLinkout){
                            out <<" ";
                            x_AddLinkout(*(bsp_handle.GetBioseqCore()), (**iter),
                                         firstGi, gi, out);
                            if((int)bsp_handle.GetBioseqLength() 
                               > k_GetSubseqThreshhold){
                                string dumpGnlUrl
                                    = x_GetDumpgnlLink((*iter)->GetSeqid(), 1, 
                                                       k_DumpGnlUrl, 0);
                                out<<dumpGnlUrl
                                   <<"<img border=0 height=16 width=16\
 src=\"/blast/images/D.gif\" alt=\"Download subject sequence spanning the \
HSP\"></a>";
                            }
                        }
                    }
                
                    out <<" ";
                    if((*iter)->IsSetTitle()){
                        s_WrapOutputLine(out, (*iter)->GetTitle());     
                    }
                    out<<endl;
                    isFirst = false;
                }
            }
        }
    }
}


void CDisplaySeqalign::x_OutputSeq(string& sequence, const CSeq_id& id, 
                                   int start, int len, int frame, int row,
                                   bool color_mismatch, 
                                   list<SAlnSeqlocInfo*> loc_list, 
                                   CNcbiOstream& out) const 
{
    _ASSERT((int)sequence.size() > start);
    list<CRange<int> > actualSeqloc;
    string actualSeq = sequence.substr(start, len);
    
    if(id.Which() != CSeq_id::e_not_set){ 
        /*only do this for sequence but not for others like middle line,
          features*/
        //go through seqloc containing mask info.  Only for master sequence
        if(row == 0){
            for (list<SAlnSeqlocInfo*>::const_iterator iter = loc_list.begin();  
                 iter != loc_list.end(); iter++){
                int from=(*iter)->aln_range.GetFrom();
                int to=(*iter)->aln_range.GetTo();
                int locFrame = (*iter)->seqloc->GetFrame();
                if(id.Match((*iter)->seqloc->GetInterval().GetId()) 
                   && locFrame == frame){
                    bool isFirstChar = true;
                    CRange<int> eachSeqloc(0, 0);
                    //go through each residule and mask it
                    for (int i=max<int>(from, start); 
                         i<=min<int>(to, start+len); i++){
                        //store seqloc start for font tag below
                        if ((m_AlignOption & eHtml) && isFirstChar){         
                            isFirstChar = false;
                            eachSeqloc.Set(i, eachSeqloc.GetTo());
                        }
                        if (m_SeqLocChar==eX){
                            if(isalpha((unsigned char) actualSeq[i-start])){
                                actualSeq[i-start]='X';
                            }
                        } else if (m_SeqLocChar==eN){
                            actualSeq[i-start]='n';
                        } else if (m_SeqLocChar==eLowerCase){
                            actualSeq[i-start]=tolower((unsigned char) actualSeq[i-start]);
                        }
                        //store seqloc start for font tag below
                        if ((m_AlignOption & eHtml) 
                            && i == min<int>(to, start+len)){ 
                            eachSeqloc.Set(eachSeqloc.GetFrom(), i);
                        }
                    }
                    if(!(eachSeqloc.GetFrom()==0&&eachSeqloc.GetTo()==0)){
                        actualSeqloc.push_back(eachSeqloc);
                    }
                }
            }
        }
    }
    
    if(actualSeqloc.empty()){//no need to add font tag
        if((m_AlignOption & eColorDifferentBases) && (m_AlignOption & eHtml)
           && color_mismatch){
            //color the mismatches. Only for rows without mask. 
            //Otherwise it may confilicts with mask font tag.
            s_ColorDifferentBases(actualSeq, k_IdentityChar, out);
        } else {
            out<<actualSeq;
        }
    } else {//now deal with font tag for mask for html display    
        bool endTag = false;
        bool numFrontTag = 0;
        for (int i = 0; i < (int)actualSeq.size(); i ++){
            for (list<CRange<int> >::iterator iter=actualSeqloc.begin(); 
                 iter!=actualSeqloc.end(); iter++){
                int from = (*iter).GetFrom() - start;
                int to = (*iter).GetTo() - start;
                //start tag
                if(from == i){
                    out<<"<font color=\""+color[m_SeqLocColor]+"\">";
                    numFrontTag = 1;
                }
                //need to close tag at the end of mask or end of sequence
                if(to == i || i == (int)actualSeq.size() - 1 ){
                    endTag = true;
                }
            }
            out<<actualSeq[i];
            if(endTag && numFrontTag == 1){
                out<<"</font>";
                endTag = false;
                numFrontTag = 0;
            }
        }
    }
}


int CDisplaySeqalign::x_GetNumGaps() 
{
    int gap = 0;
    for (int row=0; row<m_AV->GetNumRows(); row++) {
        CRef<CAlnMap::CAlnChunkVec> chunk_vec 
            = m_AV->GetAlnChunks(row, m_AV->GetSeqAlnRange(0));
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
            if (chunk->IsGap()) {
                gap += (chunk->GetAlnRange().GetTo() 
                        - chunk->GetAlnRange().GetFrom() + 1);
            }
        }
    }
    return gap;
}


void CDisplaySeqalign::x_GetFeatureInfo(list<SAlnFeatureInfo*>& feature,
                                        CScope& scope, 
                                        CSeqFeatData::E_Choice choice,
                                        int row, string& sequence) const 
{
    //Only fetch features for seq that has a gi
    CRef<CSeq_id> id 
        = s_GetSeqIdByType(m_AV->GetBioseqHandle(row).GetBioseqCore()->GetId(),
                           CSeq_id::e_Gi);
    if(!(id.Empty())){
        const CBioseq_Handle& handle = scope.GetBioseqHandle(*id);
        if(handle){
            TSeqPos seq_start = m_AV->GetSeqPosFromAlnPos(row, 0);
            TSeqPos seq_stop = m_AV->GetSeqPosFromAlnPos(row, m_AV->GetAlnStop());
            CRef<CSeq_loc> loc_ref =
                handle.
                GetRangeSeq_loc(min(seq_start, seq_stop),
                                max(seq_start, seq_stop));
            for (CFeat_CI feat(scope, *loc_ref, choice); feat; ++feat) {
                const CSeq_loc& loc = feat->GetLocation();
                ENa_strand feat_strand = GetStrand(loc);
                string featLable = NcbiEmptyString;
                string featId;
                char feat_char = ' ';
                string alternativeFeatStr = NcbiEmptyString;            
                TSeqPos feat_aln_from = 0;
                TSeqPos feat_aln_to = 0;
                TSeqPos actual_feat_seq_start = 0, actual_feat_seq_stop = 0;
                feature::GetLabel(feat->GetOriginalFeature(), &featLable, 
                                  feature::eBoth, &scope);
                featId = featLable.substr(0, k_FeatureIdLen); //default
                TSeqPos aln_stop = m_AV->GetAlnStop();  
                SAlnFeatureInfo* featInfo = NULL;
                CRange<TSeqPos> feat_seq_range = loc.GetTotalRange();
                //find the actual feature sequence start and stop 
                if(m_AV->IsPositiveStrand(row)){
                    actual_feat_seq_start = 
                        max(feat_seq_range.GetFrom(), seq_start);
                    actual_feat_seq_stop = 
                        min(feat_seq_range.GetTo(), seq_stop);
                    
                } else {
                    actual_feat_seq_start = 
                        min(feat_seq_range.GetTo(), seq_start);
                    actual_feat_seq_stop =
                        max(feat_seq_range.GetFrom(), seq_stop);
                }
                //the feature alignment positions
                feat_aln_from = 
                    m_AV->GetAlnPosFromSeqPos(row, actual_feat_seq_start);
                feat_aln_to = 
                    m_AV->GetAlnPosFromSeqPos(row, actual_feat_seq_stop);
                if(choice == CSeqFeatData::e_Gene){
                    featInfo = new SAlnFeatureInfo;                 
                    feat_char = '^';
                    
                } else if(choice == CSeqFeatData::e_Cdregion &&
                          feat->IsSetProduct() &&
                          feat->GetProduct().IsWhole()){
                    
                    //show actual aa  if there is a cds product
                    
                    //line represents the amino acid line starting covering 
                    //the whole alignment.  The idea is if there is no feature
                    //in some range, then fill it with space and this won't
                    //be shown 
                    string line(aln_stop+1, ' ');   
                    string raw_cdr_product = NcbiEmptyString;    
                    const CSeq_id& productId = 
                        feat->GetProduct().GetWhole();
                    const CBioseq_Handle& productHandle 
                        = scope.GetBioseqHandle(productId );
                    featId = "CDS:" + 
                        GetTitle(productHandle).substr(0, k_FeatureIdLen);
                    productHandle.
                        GetSeqVector(CBioseq_Handle::eCoding_Iupac).
                        GetSeqData(0, productHandle.
                                   GetBioseqLength(), raw_cdr_product);
                    featInfo = new SAlnFeatureInfo;
                    
                    //pre-fill all cds region with intron char
                    for (TSeqPos i = feat_aln_from; i <= feat_aln_to; i ++){
                        line[i] = k_IntronChar;
                    }
                    
                    //get total coding length
                    TSeqPos total_coding_len = 0;
                    for(CSeq_loc_CI loc_it(loc); loc_it; ++loc_it){
                        total_coding_len += loc_it.GetRange().GetLength();
                    }    
                    //fill concatenated exon (excluding intron)
                    //with product
                    //this is will be later used to
                    //fill the feature line

                    string concat_exon(total_coding_len, ' ');
                    char gap_char = m_AV->GetGapChar(row);   
                    TSeqPos frame = 1;
                    const CCdregion& cdr = feat->GetData().GetCdregion();
                    if(cdr.IsSetFrame()){
                        frame = cdr.GetFrame();
                    }
                    TSeqPos num_base, num_coding_base;
                    TSeqPos coding_start_base;
                    if(feat_strand == eNa_strand_minus){
                        coding_start_base = total_coding_len - 1 - (frame -1);
                        num_base = total_coding_len - 1;
                        num_coding_base = 0;
                        
                    } else {
                        coding_start_base = 0; 
                        coding_start_base += frame - 1;
                        num_base = 0;
                        num_coding_base = 0;
                    }
                    
                    for(CSeq_loc_CI loc_it(loc); loc_it; ++loc_it){
                        //note that feature on minus strand needs to be
                        //filled backward.
                        if(feat_strand != eNa_strand_minus){
                            for(TSeqPos i = 0; i < loc_it.GetRange().GetLength(); i ++){
                                if(num_base >= coding_start_base){
                                    num_coding_base ++;
                                    if(num_coding_base % 3 == 2){
                                        //a.a to the 2nd base
                                        if(num_coding_base / 3 < raw_cdr_product.size()){
                                            //make sure the coding region is no
                                            //more than the protein seq as there
                                            //could errors in ncbi record
                                            concat_exon[num_base] 
                                                = raw_cdr_product[num_coding_base / 3];
                                        }                           
                                    }
                                }
                                num_base ++;
                            }    
                        } else {
                            
                            for(TSeqPos i = 0; i < 
                                    loc_it.GetRange().GetLength() &&
                                    num_base >= 0; i ++){
                                if(num_base <= coding_start_base){
                                    num_coding_base ++;
                                    if(num_coding_base % 3 == 2){
                                        //a.a to the 2nd base
                                        if(num_coding_base / 3 < 
                                           raw_cdr_product.size() &&
                                           coding_start_base > num_coding_base){
                                            //make sure the coding region is no
                                            //more than the protein seq as there
                                            //could errors in ncbi record
                                            concat_exon[num_base] 
                                                = raw_cdr_product[num_coding_base / 3];
                                        }                           
                                    }
                                }
                                num_base --;
                            }    
                        }
                    }
                 
                    TSeqPos feat_aln_start_totalexon = 0;
                    TSeqPos prev_feat_aln_start_totalexon = 0;
                    TSeqPos prev_feat_seq_stop = 0;
                    TSeqPos intron_size = 0;
                    bool is_first = true;
                    bool  is_first_exon_start = true;
                    
                    list<CSeq_loc_CI::TRange> isolated_range;
                    //here things get complicated a bit. The idea is fill the
                    //whole feature line in alignment coordinates with
                    //amino acid on the second base of a condon

                    //isolate the seqloc corresponding to feature
                    //as this is easier to manipulate
                    for(CSeq_loc_CI loc_it(loc); loc_it; ++loc_it){
                        isolated_range.push_back(loc_it.GetRange());
                    }

                    //Need to reverse the seqloc order for minus strand
                    if(feat_strand == eNa_strand_minus){
                        isolated_range.reverse(); 
                    }

                    //go through the feature seqloc and fill the feature line
                    ITERATE(list<CSeq_loc_CI::TRange>, iter, isolated_range){

                        //intron refers to the distance between two exons
                        //i.e. each seqloc is an exon
                        //intron needs to be skipped
                        if(!is_first){
                            intron_size += iter->GetFrom() 
                                - prev_feat_seq_stop - 1;
                        }
                        CRange<TSeqPos> actual_feat_seq_range =
                            loc_ref->GetTotalRange().
                            IntersectionWith(*iter);          
                        if(!actual_feat_seq_range.Empty()){
                            //the sequence start position in aln coordinates
                            //that has a feature
                            TSeqPos feat_aln_start;
                            TSeqPos feat_aln_stop;
                            if(m_AV->IsPositiveStrand(row)){
                                feat_aln_start = 
                                    m_AV->
                                    GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetFrom());
                                feat_aln_stop
                                    = m_AV->GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetTo());
                            } else {
                                feat_aln_start = 
                                    m_AV->
                                    GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetTo());
                                feat_aln_stop
                                    = m_AV->GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetFrom());
                            }
                            //put actual amino acid on feature line
                            //in aln coord 
                            for (TSeqPos i = feat_aln_start;
                                 i <= feat_aln_stop;  i ++){  
                                    if(sequence[i] != gap_char){
                                        //the amino acid position in 
                                        //concatanated exon that corresponds
                                        //to the sequence position
                                        //note intron needs to be skipped
                                        //as it does not have cds feature
                                        TSeqPos product_adj_seq_pos
                                            = m_AV->GetSeqPosFromAlnPos(row, i) - 
                                            intron_size - feat_seq_range.GetFrom();
                                        if(product_adj_seq_pos < 
                                           concat_exon.size()){
                                            //fill the cds feature line with
                                            //actual amino acids
                                            line[i] = 
                                                concat_exon[product_adj_seq_pos];
                                            //get the exon start position
                                            //note minus strand needs to be
                                            //counted backward
                                            if(m_AV->IsPositiveStrand(row)){
                                                //don't count gap 
                                                if(is_first_exon_start &&
                                                   isalpha((unsigned char) line[i])){
                                                    if(feat_strand == eNa_strand_minus){ 
                                                        feat_aln_start_totalexon = 
                                                            concat_exon.size()
                                                            - product_adj_seq_pos + 1;
                                                        is_first_exon_start = false;
                                                        
                                                    } else {
                                                        feat_aln_start_totalexon = 
                                                            product_adj_seq_pos;
                                                        is_first_exon_start = false;
                                                    }
                                                }
                                                
                                            } else {
                                                if(feat_strand == eNa_strand_minus){ 
                                                    if(is_first_exon_start && 
                                                       isalpha((unsigned char) line[i])){
                                                        feat_aln_start_totalexon = 
                                                            concat_exon.size()
                                                            - product_adj_seq_pos + 1;
                                                        is_first_exon_start = false;
                                                        prev_feat_aln_start_totalexon =
                                                        feat_aln_start_totalexon;
                                                    }
                                                    if(!is_first_exon_start){
                                                        //need to get the
                                                        //smallest start as
                                                        //seqloc list is
                                                        //reversed
                                                        feat_aln_start_totalexon =
                                                            min(TSeqPos(concat_exon.size()
                                                                        - product_adj_seq_pos + 1), 
                                                                prev_feat_aln_start_totalexon);
                                                        prev_feat_aln_start_totalexon =
                                                        feat_aln_start_totalexon;  
                                                    }
                                                } else {
                                                    feat_aln_start_totalexon = 
                                                        max(prev_feat_aln_start_totalexon,
                                                            product_adj_seq_pos); 
                                                    
                                                    prev_feat_aln_start_totalexon =
                                                        feat_aln_start_totalexon;
                                                }
                                            }
                                        }
                                    } else { //adding gap
                                        line[i] = ' '; 
                                    }                         
                               
                            }                      
                        }
                        
                        prev_feat_seq_stop = iter->GetTo();  
                        is_first = false;
                    }                 
                    alternativeFeatStr = line;
                    s_FillCdsStartPosition(line, concat_exon, m_LineLen,
                                           feat_aln_start_totalexon,
                                           m_AV->IsPositiveStrand(row) ?
                                           eNa_strand_plus : eNa_strand_minus,
                                           feat_strand, featInfo->feature_start); 
                }
                
                if(featInfo){
                    x_SetFeatureInfo(featInfo, *loc_ref,
                                     feat_aln_from, feat_aln_to, aln_stop, 
                                     feat_char, featId, alternativeFeatStr);  
                    feature.push_back(featInfo);
                }
            }
        }   
    }
}


void  CDisplaySeqalign::x_SetFeatureInfo(SAlnFeatureInfo* feat_info, 
                                         const CSeq_loc& seqloc, int aln_from, 
                                         int aln_to, int aln_stop, 
                                         char pattern_char, string pattern_id,
                                         string& alternative_feat_str) const
{
    FeatureInfo* feat = new FeatureInfo;
    feat->seqloc = &seqloc;
    feat->feature_char = pattern_char;
    feat->feature_id = pattern_id;
    
    if(alternative_feat_str != NcbiEmptyString){
        feat_info->feature_string = alternative_feat_str;
    } else {
        //fill feature string
        string line(aln_stop+1, ' ');
        for (int j = aln_from; j <= aln_to; j++){
            line[j] = feat->feature_char;
        }
        feat_info->feature_string = line;
    }
    
    feat_info->aln_range.Set(aln_from, aln_to); 
    feat_info->feature = feat;
}

///add a "|" to the current insert for insert on next rows and return the
///insert end position.
///@param seq: the seq string
///@param insert_aln_pos: the position of insert
///@param aln_start: alnment start position
///@return: the insert end position
///
static int x_AddBar(string& seq, int insert_alnpos, int aln_start){
    int end = (int)seq.size() -1 ;
    int barPos = insert_alnpos - aln_start + 1;
    string addOn;
    if(barPos - end > 1){
        string spacer(barPos - end - 1, ' ');
        addOn += spacer + "|";
    } else if (barPos - end == 1){
        addOn += "|";
    }
    seq += addOn;
    return max<int>((barPos - end), 0);
}


///Add new insert seq to the current insert seq and return the end position of
///the latest insert
///@param cur_insert: the current insert string
///@param new_insert: the new insert string
///@param insert_alnpos: insert position
///@param aln_start: alnment start
///@return: the updated insert end position
///
static int s_AdjustInsert(string& cur_insert, string& new_insert, 
                          int insert_alnpos, int aln_start)
{
    int insertEnd = 0;
    int curInsertSize = (int)cur_insert.size();
    int insertLeftSpace = insert_alnpos - aln_start - curInsertSize + 2;  
    //plus2 because insert is put after the position
    if(curInsertSize > 0){
        _ASSERT(insertLeftSpace >= 2);
    }
    int newInsertSize = (int)new_insert.size();  
    if(insertLeftSpace - newInsertSize >= 1){ 
        //can insert with the end position right below the bar
        string spacer(insertLeftSpace - newInsertSize, ' ');
        cur_insert += spacer + new_insert;
        
    } else { //Need to insert beyond the insert postion
        if(curInsertSize > 0){
            cur_insert += " " + new_insert;
        } else {  //can insert right at the firt position
            cur_insert += new_insert;
        }
    }
    insertEnd = aln_start + (int)cur_insert.size() -1 ; //-1 back to string position
    return insertEnd;
}


void CDisplaySeqalign::x_DoFills(int row, CAlnMap::TSignedRange& aln_range, 
                                 int  aln_start, 
                                 list<SInsertInformation*>& insert_list, 
                                 list<string>& inserts) const {
    if(!insert_list.empty()){
        string bar(aln_range.GetLength(), ' ');
        
        string seq;
        list<SInsertInformation*> leftOverInsertList;
        bool isFirstInsert = true;
        int curInsertAlnStart = 0;
        int prvsInsertAlnEnd = 0;
        
        //go through each insert and fills the seq if it can 
        //be filled on the same line.  If not, go to the next line
        for(list<SInsertInformation*>::iterator 
                iter = insert_list.begin(); iter != insert_list.end(); iter ++){
            curInsertAlnStart = (*iter)->aln_start;
            //always fill the first insert.  Also fill if there is enough space
            if(isFirstInsert || curInsertAlnStart - prvsInsertAlnEnd >= 1){
                bar[curInsertAlnStart-aln_start+1] = '|';  
                int seqStart = (*iter)->seq_start;
                int seqEnd = seqStart + (*iter)->insert_len - 1;
                string newInsert;
                newInsert = m_AV->GetSeqString(newInsert, row, seqStart,
                                               seqEnd);
                prvsInsertAlnEnd = s_AdjustInsert(seq, newInsert,
                                                  curInsertAlnStart, aln_start);
                isFirstInsert = false;
            } else { //if no space, save the chunk and go to next line 
                bar[curInsertAlnStart-aln_start+1] = '|'; 
                //indicate insert goes to the next line
                prvsInsertAlnEnd += x_AddBar(seq, curInsertAlnStart, aln_start); 
                //May need to add a bar after the current insert sequence 
                //to indicate insert goes to the next line.
                leftOverInsertList.push_back(*iter);    
            }
        }
        //save current insert.  Note that each insert has a bar and sequence
        //below it
        inserts.push_back(bar);
        inserts.push_back(seq);
        //here recursively fill the chunk that don't have enough space
        x_DoFills(row, aln_range, aln_start, leftOverInsertList, inserts);
    }
    
}


void CDisplaySeqalign::x_FillInserts(int row, CAlnMap::TSignedRange& aln_range,
                                     int aln_start, list<string>& inserts,
                                     string& insert_pos_string, 
                                     list<SInsertInformation*>& insert_list) const
{
    
    string line(aln_range.GetLength(), ' ');
    
    ITERATE(list<SInsertInformation*>, iter, insert_list){
        int from = (*iter)->aln_start;
        line[from - aln_start + 1] = '\\';
    }
    insert_pos_string = line; 
    //this is the line with "\" right after each insert position
    
    //here fills the insert sequence
    x_DoFills(row, aln_range, aln_start, insert_list, inserts);
}


void CDisplaySeqalign::x_GetInserts(list<SInsertInformation*>& insert_list,
                                    CAlnMap::TSeqPosList& insert_aln_start, 
                                    CAlnMap::TSeqPosList& insert_seq_start, 
                                    CAlnMap::TSeqPosList& insert_length, 
                                    int line_aln_stop)
{

    while(!insert_aln_start.empty() 
          && (int)insert_aln_start.front() < line_aln_stop){
        CDisplaySeqalign::SInsertInformation* insert
            = new CDisplaySeqalign::SInsertInformation;
        insert->aln_start = insert_aln_start.front() - 1; 
        //Need to minus one as we are inserting after this position
        insert->seq_start = insert_seq_start.front();
        insert->insert_len = insert_length.front();
        insert_list.push_back(insert);
        insert_aln_start.pop_front();
        insert_seq_start.pop_front();
        insert_length.pop_front();
    }
    
}


string CDisplaySeqalign::x_GetSegs(int row) const 
{
    string segs = NcbiEmptyString;
    if(m_AlignOption & eMultiAlign){ //only show this hsp
        segs = NStr::IntToString(m_AV->GetSeqStart(row))
            + "-" + NStr::IntToString(m_AV->GetSeqStop(row));
    } else { //for all segs
        string idString = m_AV->GetSeqId(1).GetSeqIdString();
        map<string, string>::const_iterator iter = m_Segs.find(idString);
        if ( iter != m_Segs.end() ){
            segs = iter->second;
        }
    }
    return segs;
}


///transforms a string so that it becomes safe to be used as part of URL
///the function converts characters with special meaning (such as
///semicolon -- protocol separator) to escaped hexadecimal (%xx)
///@param src: the input url
///@return: the safe url
///
static string s_MakeURLSafe(char* src){
    static char HEXDIGS[] = "0123456789ABCDEF";
    char* buf;
    size_t len;
    char* p;
    char c;
    string url = NcbiEmptyString;
    
    if (src){
        /* first pass to calculate required buffer size */
        for (p = src, len = 0; (c = *(p++)) != '\0'; ) {
            switch (c) {
            default:
                if (c < '0' || (c > '9' && c < 'A') ||
                    (c > 'Z' && c < 'a') || c > 'z') {
                    len += 3;
                    break;
                }
            case '-': case '_': case '.': case '!': case '~':
            case '*': case '\'': case '(': case ')':
                ++len;
            }
        }
        buf = new char[len + 1];
        /* second pass -- conversion */
        for (p = buf; (c = *(src++)) != '\0'; ) {
            switch (c) {
            default:
                if (c < '0' || (c > '9' && c < 'A') ||
                    (c > 'Z' && c < 'a') || c > 'z') {
                    *(p++) = '%';
                    *(p++) = HEXDIGS[(c >> 4) & 0xf];
                    *(p++) = HEXDIGS[c & 0xf];
                    break;
                }
            case '-': case '_': case '.': case '!': case '~':
            case '*': case '\'': case '(': case ')':
                *(p++) = c;
            }
        }
        *p = '\0';
        url = buf;
        delete [] buf;
    }
    return url;
}


string CDisplaySeqalign::x_GetDumpgnlLink(const list<CRef<CSeq_id> >& ids, 
                                          int row,
                                          const string& alternative_url,
                                          int taxid) const
{
    string link = NcbiEmptyString;  
    string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
    string passwd = m_Reg->Get(m_BlastType, "PASSWD");
    bool nodb_path =  false;
    CRef<CSeq_id> idGeneral = s_GetSeqIdByType(ids, CSeq_id::e_General);
    CRef<CSeq_id> idOther = s_GetSeqIdByType(ids, CSeq_id::e_Other);
    const CRef<CSeq_id> idAccession = FindBestChoice(ids, CSeq_id::WorstRank);
    string segs = x_GetSegs(row);
    int gi = s_GetGiForSeqIdList(ids);
    if(!idGeneral.Empty() 
       && idGeneral->AsFastaString().find("gnl|BL_ORD_ID") != string::npos){
        /* We do need to make security protected link to BLAST gnl */
        return NcbiEmptyString;
    }
    if(alternative_url != NcbiEmptyString){ 
        toolUrl = alternative_url;
    }
    /* dumpgnl.cgi need to use path  */
    if (toolUrl.find("dumpgnl.cgi") ==string::npos){
        nodb_path = true;
    }  
    int length = (int)m_DbName.size();
    string str;
    char  *chptr, *dbtmp;
    Char tmpbuff[256];
    char* dbname = new char[sizeof(char)*length + 2];
    strcpy(dbname, m_DbName.c_str());
    if(nodb_path) {
        int i, j;
        dbtmp = new char[sizeof(char)*length + 2]; /* aditional space and NULL */
        memset(dbtmp, '\0', sizeof(char)*length + 2);
        for(i = 0; i < length; i++) {            
            if(isspace((unsigned char) dbname[i]) || dbname[i] == ',') {/* Rolling spaces */
                continue;
            }
            j = 0;
            while (!isspace((unsigned char) dbname[i]) && j < 256  && i < length) { 
                tmpbuff[j] = dbname[i];
                j++; i++;
                if(dbname[i] == ',') { /* Comma is valid delimiter */
                    break;
                }
            }
            tmpbuff[j] = '\0';
            if((chptr = strrchr(tmpbuff, '/')) != NULL) { 
                strcat(dbtmp, (char*)(chptr+1));
            } else {
                strcat(dbtmp, tmpbuff);
            }
            strcat(dbtmp, " ");            
        }
    } else {
        dbtmp = dbname;
    }
    
    const CSeq_id* bestid = NULL;
    if (idGeneral.Empty()){
        bestid = idOther;
        if (idOther.Empty()){
            bestid = idAccession;
        }
    } else {
        bestid = idGeneral;
    }
    /*
     * Need to protect start and stop positions
     * to avoid web users sending us hand-made URLs
     * to retrive full sequences
     */
    char gnl[256];
    if (bestid && bestid->Which() !=  CSeq_id::e_Gi){
        strcpy(gnl, bestid->AsFastaString().c_str());
        
    } else {
        gnl[0] = '\0';
    }
    
    str = s_MakeURLSafe(dbtmp == NULL ? (char*) "nr" : dbtmp);
    link += "<a href=\"";
    if (toolUrl.find("?") == string::npos){
        link += toolUrl + "?" + "db=" + str + "&na=" + (m_IsDbNa? "1" : "0")
            + "&";
    } else {
        link += toolUrl + "&db=" + str + "&na=" + (m_IsDbNa? "1" : "0") + "&";
    }
    
    if (gnl[0] != '\0'){
        str = s_MakeURLSafe(gnl);
        link += "gnl=";
        link += str;
        link += "&";
    }
    if (gi > 0){
        link += "gi=" + NStr::IntToString(gi) + "&";
    }
    if(taxid > 0){
        link += "taxid=" + NStr::IntToString(taxid) +"&";
    }
    if (m_Rid != NcbiEmptyString){
        link += "RID=" + m_Rid +"&";
    }
    
    if ( m_QueryNumber > 0){
        link += "QUERY_NUMBER=" + NStr::IntToString(m_QueryNumber) + "&";
    }
    link += "segs=" + segs;
    link += "\">";
    if(nodb_path){
        delete [] dbtmp;
    }
    delete [] dbname;
    return link;
}


CDisplaySeqalign::DbType CDisplaySeqalign::x_GetDbType(const CSeq_align_set& actual_aln_list) 
{
    //determine if the database has gi by looking at the 1st hit.  
    //Could be wrong but simple for now
    DbType type = eDbTypeNotSet;
    CRef<CSeq_align> first_aln = actual_aln_list.Get().front();
    const CSeq_id& subject_id = first_aln->GetSeq_id(1);
    const CBioseq_Handle& handleTemp  = m_Scope.GetBioseqHandle(subject_id);
    if(handleTemp){
        int giTemp = FindGi(handleTemp.GetBioseqCore()->GetId());
        if (giTemp >0) { 
            type = eDbGi;
        } else if (subject_id.Which() == CSeq_id::e_General){
            const CDbtag& dtg = subject_id.GetGeneral();
            const string& dbName = dtg.GetDb();
            if(NStr::CompareNocase(dbName, "TI") == 0){
                type = eDbGeneral;
            }
        }   
    }
    return type;
}


CRef<CSeq_align_set> 
CDisplaySeqalign::PrepareBlastUngappedSeqalign(const CSeq_align_set& alnset) 
{
    CRef<CSeq_align_set> alnSetRef(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()){
        const CSeq_align::TSegs& seg = (*iter)->GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            if(seg.GetStd().size() > 1){ 
                //has more than one stdseg. Need to seperate as each 
                //is a distinct HSP
                ITERATE (CSeq_align::C_Segs::TStd, iterStdseg, seg.GetStd()){
                    CRef<CSeq_align> aln(new CSeq_align);
                    if((*iterStdseg)->IsSetScores()){
                        aln->SetScore() = (*iterStdseg)->GetScores();
                    }
                    aln->SetSegs().SetStd().push_back(*iterStdseg);
                    alnSetRef->Set().push_back(aln);
                }
                
            } else {
                alnSetRef->Set().push_back(*iter);
            }
        } else if(seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            if(seg.GetDendiag().size() > 1){ 
                //has more than one dendiag. Need to seperate as each is
                //a distinct HSP
                ITERATE (CSeq_align::C_Segs::TDendiag, iterDendiag,
                         seg.GetDendiag()){
                    CRef<CSeq_align> aln(new CSeq_align);
                    if((*iterDendiag)->IsSetScores()){
                        aln->SetScore() = (*iterDendiag)->GetScores();
                    }
                    aln->SetSegs().SetDendiag().push_back(*iterDendiag);
                    alnSetRef->Set().push_back(aln);
                }
                
            } else {
                alnSetRef->Set().push_back(*iter);
            }
        } else { //Denseg, doing nothing.
            
            alnSetRef->Set().push_back(*iter);
        }
    }
    
    return alnSetRef;
}


void CDisplaySeqalign::x_DisplayAlnvecList(CNcbiOstream& out, 
                                           list<SAlnInfo*>& av_list) 
{
    bool isFirstAlnInList = true;
    for(list<SAlnInfo*>::iterator iterAv = av_list.begin();
        iterAv != av_list.end(); iterAv ++){
        m_AV = (*iterAv)->alnvec;
        const CBioseq_Handle& bsp_handle=m_AV->GetBioseqHandle(1); 
        if(isFirstAlnInList && (m_AlignOption&eShowBlastInfo)) {
            x_PrintDefLine(bsp_handle, (*iterAv)->use_this_gi, out);
            out<<"          Length="<<bsp_handle.GetBioseqLength()<<endl;
            if((m_AlignOption&eHtml) && (m_AlignOption&eShowBlastInfo)
               && (m_AlignOption&eShowBl2seqLink)) {
                const CBioseq_Handle& query_handle=m_AV->GetBioseqHandle(0);
                const CBioseq_Handle& subject_handle=m_AV->GetBioseqHandle(1);
                CSeq_id_Handle query_seqid = GetId(query_handle, eGetId_Best);
                CSeq_id_Handle subject_seqid =
                    GetId(subject_handle, eGetId_Best);
                int query_gi = FindGi(query_handle.GetBioseqCore()->GetId());   
                int subject_gi = FindGi(subject_handle.GetBioseqCore()->GetId());
                
                char buffer[512];
                
                sprintf(buffer, kBl2seqUrl.c_str(), m_Rid.c_str(), 
                        query_gi > 0 ? 
                        NStr::IntToString(query_gi).c_str():query_seqid.\
                        GetSeqId()->AsFastaString().c_str(),
                        subject_gi > 0 ? 
                        NStr::IntToString(subject_gi).c_str():subject_seqid.\
                        GetSeqId()->AsFastaString().c_str()); 
                out << buffer << endl;
            }
            out << endl;
        }
        
        //output dynamic feature lines
        if(m_AlignOption&eShowBlastInfo && !(m_AlignOption&eMultiAlign) 
           && (m_AlignOption&eDynamicFeature) 
           && (int)m_AV->GetBioseqHandle(1).GetBioseqLength() 
           >= k_GetDynamicFeatureSeqLength){ 
            if(m_DynamicFeature){
                x_PrintDynamicFeatures(out);
            } 
        }
        if (m_AlignOption&eShowBlastInfo) {
            string evalue_buf, bit_score_buf;
            CBlastFormatUtil::GetScoreString((*iterAv)->evalue, 
                                             (*iterAv)->bits, evalue_buf, 
                                             bit_score_buf);
            //add id anchor for mapviewer link
            if(m_AlignOption&eHtml && NStr::ToUpper(m_BlastType).find("GENOME") != string::npos){
                string subj_id_str;
                char buffer[126];
                int master_start = m_AV->GetSeqStart(0) + 1;
                int master_stop = m_AV->GetSeqStop(0) + 1;
                int subject_start = m_AV->GetSeqStart(1) + 1;
                int subject_stop = m_AV->GetSeqStop(1) + 1;
                
                m_AV->GetSeqId(1).GetLabel(&subj_id_str, CSeq_id::eContent);
             
                sprintf(buffer, "<a name = %s_%d_%d_%d_%d_%d></a>",
                        subj_id_str.c_str(), (*iterAv)->score,
                        min(master_start, master_stop),
                        max(master_start, master_stop),
                        min(subject_start, subject_stop),
                        max(subject_start, subject_stop));
                        
                out << buffer << endl; 
            }
            out<<" Score = "<<bit_score_buf<<" ";
            out<<"bits ("<<(*iterAv)->score<<"),"<<"  ";
            out<<"Expect = "<<evalue_buf;
            if ((*iterAv)->comp_adj_method == 1)
                out << ", Method: Composition-based stats.";
            else if ((*iterAv)->comp_adj_method == 2)
                out << ", Method: Compositional matrix adjust.";
            out << endl;
        }
        
        x_DisplayAlnvec(out);
        out<<endl;
        isFirstAlnInList = false;
    }
}


void CDisplaySeqalign::x_PrintDynamicFeatures(CNcbiOstream& out) 
{
    const CSeq_id& subject_seqid = m_AV->GetSeqId(1);
    const CRange<TSeqPos>& range = m_AV->GetSeqRange(1);
    CRange<TSeqPos> actual_range = range;
    if(range.GetFrom() > range.GetTo()){
        actual_range.Set(range.GetTo(), range.GetFrom());
    }
    string id_str;
    subject_seqid.GetLabel(&id_str, CSeq_id::eBoth);
    const CBioseq_Handle& subject_handle=m_AV->GetBioseqHandle(1);
    int subject_gi = FindGi(subject_handle.GetBioseqCore()->GetId());
    char urlBuf[2048];
   
    SFeatInfo* feat5 = NULL;
    SFeatInfo* feat3 = NULL;
    vector<SFeatInfo*>& feat_list 
        =  m_DynamicFeature->GetFeatInfo(id_str, actual_range, feat5, feat3, 2);
    
    if(feat_list.size() > 0) { //has feature in this range
        out << " Features in this part of subject sequence:" << endl;
        ITERATE(vector<SFeatInfo*>, iter, feat_list){
            out << "   ";
            if(m_AlignOption&eHtml && subject_gi > 0){
                sprintf(urlBuf, kEntrezSubseqUrl.c_str(), subject_gi,
                        m_IsDbNa ? "Nucleotide" : "Protein",  
                        (*iter)->range.GetFrom() +1 , (*iter)->range.GetTo() + 1 );
                out << urlBuf;
            }  
            out << (*iter)->feat_str;
            if(m_AlignOption&eHtml && subject_gi > 0){
                out << "</a>";
            }  
            out << endl;
        }
    } else {  //show flank features
        if(feat5 || feat3){   
            out << " Features flanking this part of subject sequence:" << endl;
        }
        if(feat5){
            out << "   ";
            if(m_AlignOption&eHtml && subject_gi > 0){
                sprintf(urlBuf, kEntrezSubseqUrl.c_str(), subject_gi,
                        m_IsDbNa ? "Nucleotide" : "Protein",  
                        feat5->range.GetFrom() + 1 , feat5->range.GetTo() + 1 );
                out << urlBuf;
            }  
            out << actual_range.GetFrom() - feat5->range.GetTo() 
                << " bp at 5' side: " << feat5->feat_str;
            if(m_AlignOption&eHtml && subject_gi > 0){
                out << "</a>";
            }  
            out << endl;
        }
        if(feat3){
            out << "   ";
            if(m_AlignOption&eHtml && subject_gi > 0){
                sprintf(urlBuf, kEntrezSubseqUrl.c_str(), subject_gi,
                        m_IsDbNa ? "Nucleotide" : "Protein",  
                        feat3->range.GetFrom() + 1 , feat3->range.GetTo() + 1);
                out << urlBuf;
            }
            out << feat3->range.GetFrom() - actual_range.GetTo() 
                << " bp at 3' side: " << feat3->feat_str;
            if(m_AlignOption&eHtml){
                out << "</a>";
            }  
            out << endl;
        }
    }
    if(feat_list.size() > 0 || feat5 || feat3 ){
        out << endl;
    }
}


void CDisplaySeqalign::x_FillLocList(list<SAlnSeqlocInfo*>& loc_list) const 
{
    if(!m_Seqloc){
        return;
    }
    for (list<CRef<blast::CSeqLocInfo> >::iterator iter=m_Seqloc->begin(); 
         iter!=m_Seqloc->end(); iter++){
        SAlnSeqlocInfo* alnloc = new SAlnSeqlocInfo;    
        for (int i=0; i<m_AV->GetNumRows(); i++){
            if((*iter)->GetInterval().GetId().Match(m_AV->GetSeqId(i))){
                int actualAlnStart = 0, actualAlnStop = 0;
                if(m_AV->IsPositiveStrand(i)){
                    actualAlnStart =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetFrom());
                    actualAlnStop =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetTo());
                } else {
                    actualAlnStart =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetTo());
                    actualAlnStop =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetFrom());
                }
                alnloc->aln_range.Set(actualAlnStart, actualAlnStop);      
                break;
            }
        }
        alnloc->seqloc = *iter;   
        loc_list.push_back(alnloc);
    }
}


list<CDisplaySeqalign::SAlnFeatureInfo*>* 
CDisplaySeqalign::x_GetQueryFeatureList(int row_num, int aln_stop) const
{
    list<SAlnFeatureInfo*>* bioseqFeature= new list<SAlnFeatureInfo*>[row_num];
    if(m_QueryFeature){
        for (list<FeatureInfo*>::iterator iter=m_QueryFeature->begin(); 
             iter!=m_QueryFeature->end(); iter++){
            for(int i = 0; i < row_num; i++){
                if((*iter)->seqloc->GetInt().GetId().Match(m_AV->GetSeqId(i))){
                    int actualSeqStart = 0, actualSeqStop = 0;
                    if(m_AV->IsPositiveStrand(i)){
                        if((*iter)->seqloc->GetInt().GetFrom() 
                           < m_AV->GetSeqStart(i)){
                            actualSeqStart = m_AV->GetSeqStart(i);
                        } else {
                            actualSeqStart = (*iter)->seqloc->GetInt().GetFrom();
                        }
                        
                        if((*iter)->seqloc->GetInt().GetTo() >
                           m_AV->GetSeqStop(i)){
                            actualSeqStop = m_AV->GetSeqStop(i);
                        } else {
                            actualSeqStop = (*iter)->seqloc->GetInt().GetTo();
                        }
                    } else {
                        if((*iter)->seqloc->GetInt().GetFrom() 
                           < m_AV->GetSeqStart(i)){
                            actualSeqStart = (*iter)->seqloc->GetInt().GetFrom();
                        } else {
                            actualSeqStart = m_AV->GetSeqStart(i);
                        }
                        
                        if((*iter)->seqloc->GetInt().GetTo() > 
                           m_AV->GetSeqStop(i)){
                            actualSeqStop = (*iter)->seqloc->GetInt().GetTo();
                        } else {
                            actualSeqStop = m_AV->GetSeqStop(i);
                        }
                    }
                    int alnFrom = m_AV->GetAlnPosFromSeqPos(i, actualSeqStart);
                    int alnTo = m_AV->GetAlnPosFromSeqPos(i, actualSeqStop);
                    
                    SAlnFeatureInfo* featInfo = new SAlnFeatureInfo;
                    string tempFeat = NcbiEmptyString;
                    x_SetFeatureInfo(featInfo, *((*iter)->seqloc), alnFrom, 
                                     alnTo,  aln_stop, (*iter)->feature_char,
                                     (*iter)->feature_id, tempFeat);    
                    bioseqFeature[i].push_back(featInfo);
                }
            }
        }
    }
    return bioseqFeature;
}


void CDisplaySeqalign::x_FillSeqid(string& id, int row) const
{
    if(m_AlignOption & eShowBlastStyleId) {
        if(row==0){//query
            id="Query";
        } else {//hits
            if (!(m_AlignOption&eMultiAlign)){
                //hits for pairwise 
                id="Sbjct";
            } else {
                if(m_AlignOption&eShowGi){
                    int gi = 0;
                    if(m_AV->GetSeqId(row).Which() == CSeq_id::e_Gi){
                        gi = m_AV->GetSeqId(row).GetGi();
                    }
                    if(!(gi > 0)){
                        gi = s_GetGiForSeqIdList(m_AV->GetBioseqHandle(row).\
                                                 GetBioseqCore()->GetId());
                    }
                    if(gi > 0){
                        id=NStr::IntToString(gi);
                    } else {
                        const CRef<CSeq_id> wid 
                            = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                             GetBioseqCore()->GetId(), 
                                             CSeq_id::WorstRank);
                        id=wid->GetSeqIdString();
                    }
                } else {
                    const CRef<CSeq_id> wid 
                        = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                         GetBioseqCore()->GetId(), 
                                         CSeq_id::WorstRank);
                    id=wid->GetSeqIdString();
                }           
            }
        }
    } else {
        if(m_AlignOption&eShowGi){
            int gi = 0;
            if(m_AV->GetSeqId(row).Which() == CSeq_id::e_Gi){
                gi = m_AV->GetSeqId(row).GetGi();
            }
            if(!(gi > 0)){
                gi = s_GetGiForSeqIdList(m_AV->GetBioseqHandle(row).\
                                         GetBioseqCore()->GetId());
            }
            if(gi > 0){
                id=NStr::IntToString(gi);
            } else {
                const CRef<CSeq_id> wid 
                    = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                     GetBioseqCore()->GetId(),
                                     CSeq_id::WorstRank);
                id=wid->GetSeqIdString();
            }
        } else {
            const CRef<CSeq_id> wid 
                = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                 GetBioseqCore()->GetId(), 
                                 CSeq_id::WorstRank);
            id=wid->GetSeqIdString();
        }     
    }
}

END_NCBI_SCOPE

/* 
*============================================================
*$Log$
*Revision 1.89  2005/11/14 21:32:45  ucko
*Tweak CDisplaySeqalign::x_GetFeatureInfo not to assume size_type and
*TSeqPos are the same.
*
*Revision 1.88  2005/11/14 19:06:07  jianye
*added show cds product on minus strand
*
*Revision 1.87  2005/09/27 17:10:09  madden
*Print out compositional adjustment method if appropriate
*
*Revision 1.86  2005/08/29 16:10:26  camacho
*Fix to previous commit
*
*Revision 1.85  2005/08/29 14:40:47  camacho
*From Ilya Dondoshansky:
*SeqlocInfo structure changed to a CSeqLocInfo class, definition moved to
*the xblast library
*
*Revision 1.84  2005/08/11 15:30:26  jianye
*add taxid to user url
*
*Revision 1.83  2005/08/01 14:45:27  dondosha
*Removed meaningless const return types, fixing icc compiler warning
*
*Revision 1.82  2005/07/26 17:35:13  jianye
*Specify genetic code
*
*Revision 1.81  2005/07/20 18:18:03  dondosha
*Added const specifier to argument in PrepareBlastUngappedSeqalign
*
*Revision 1.80  2005/07/20 14:40:24  jianye
*attach query_number to form name
*
*Revision 1.79  2005/06/28 16:03:01  camacho
*Fix msvc warning
*
*Revision 1.78  2005/06/06 15:30:29  lavr
*Explicit (unsigned char) casts in ctype routines
*
*Revision 1.77  2005/06/03 16:58:24  lavr
*Explicit (unsigned char) casts in ctype routines
*
*Revision 1.76  2005/05/25 16:53:40  jianye
*fix warning under sun solaris
*
*Revision 1.75  2005/05/25 11:29:16  camacho
*Remove compiler warnings on solaris
*
*Revision 1.74  2005/05/13 14:21:37  jianye
*No showing internal blastdb id
*
*Revision 1.73  2005/05/12 14:47:47  jianye
*modify computing feature start pos
*
*Revision 1.72  2005/05/11 15:05:09  jianye
*gbloader change and some getfeature changes
*
*Revision 1.71  2005/05/02 17:34:33  dondosha
*Moved static function s_CreateDensegFromDendiag in showalign.cpp to a static method CreateDensegFromDendiag in CBlastFormatUtil class
*
*Revision 1.70  2005/04/20 20:20:55  ucko
*Use _ASSERT rather than assert
*
*Revision 1.69  2005/04/12 16:39:51  jianye
*Added subject seq retrieval for trace db, get rid of password seal and and gnl id to url
*
*Revision 1.68  2005/03/16 18:17:17  jianye
*Added the missing a tag
*
*Revision 1.67  2005/03/14 20:10:44  dondosha
*Moved static function s_ExtractSeqalign to a method in CblastFormatUtil class
*
*Revision 1.66  2005/03/14 15:49:06  jianye
*Added hsp anchor for mapviewer
*
*Revision 1.65  2005/03/07 22:20:12  jianye
*Not adding to multialnment if bioseq is withdrawn
*
*Revision 1.64  2005/03/02 18:19:58  jianye
*fixed some sun solaris compiler warnings
*
*Revision 1.63  2005/02/23 16:28:03  jianye
*change due to num_ident addition
*
*Revision 1.62  2005/02/22 15:58:55  jianye
*some style change
*
*Revision 1.61  2005/02/22 14:24:24  camacho
*Moved showalign.[hc]pp to objtools/blast_format
*
*Revision 1.60  2005/02/22 14:20:32  lebedev
*showalign.cpp restored
*
*Revision 1.58  2005/02/17 15:58:42  grichenk
*Changes sequence::GetId() to return CSeq_id_Handle
*
*Revision 1.57  2005/02/14 19:04:50  jianye
*changed constructor and other clean up
*
*Revision 1.56  2005/01/28 16:12:21  jianye
*use Assign() to copy object
*
*Revision 1.55  2005/01/25 15:34:53  jianye
*Show gap as - in masked region
*
*Revision 1.54  2005/01/03 21:05:56  jianye
*highlight subject seq for PairwiseWithIdentities format
*
*Revision 1.53  2004/12/28 16:59:19  jianye
*added empty seqalign check
*
*Revision 1.52  2004/12/21 19:32:16  jianye
*assert to _ASSERT
*
*Revision 1.51  2004/11/29 21:24:59  camacho
*use anonymous exceptions to avoid msvc warnings
*
*Revision 1.50  2004/11/18 21:27:40  grichenk
*Removed default value for scope argument in seq-loc related functions.
*
*Revision 1.49  2004/11/04 21:06:09  jianye
*fixed sign warning
*
*Revision 1.48  2004/11/01 19:33:09  grichenk
*Removed deprecated methods
*
*Revision 1.47  2004/09/27 21:12:33  jianye
*modify coordinates to get subseq
*
*Revision 1.46  2004/09/27 14:32:33  jianye
*modify use_this_gi logic
*
*Revision 1.45  2004/09/21 19:09:06  jianye
*modify significant digits for evalue and bit score
*
*Revision 1.44  2004/09/20 18:10:58  jianye
*Handles Disc alignment and some code clean up
*
*Revision 1.43  2004/09/09 19:36:58  jianye
*Added Gene linkout
*
*Revision 1.42  2004/08/10 17:27:08  jianye
*Added dynamic feature link
*
*Revision 1.41  2004/08/05 19:16:50  jianye
*checking dynamic feature better
*
*Revision 1.40  2004/08/05 16:58:10  jianye
*Added dynamic features
*
*Revision 1.39  2004/07/22 15:45:26  jianye
*Added Bl2seq link
*
*Revision 1.38  2004/07/21 15:51:26  grichenk
*CObjectManager made singleton, GetInstance() added.
*CXXXXDataLoader constructors made private, added
*static RegisterInObjectManager() and GetLoaderNameFromArgs()
*methods.
*
*Revision 1.37  2004/07/12 15:20:45  jianye
*handles use_this_gi in alignment score structure
*
*Revision 1.36  2004/05/21 21:42:51  gorelenk
*Added PCH ncbi_pch.hpp
*
*Revision 1.35  2004/04/26 16:50:48  ucko
*Don't try to pass temporary (dummy) CSeq_id objects, even by const
*reference, as CSeq_id has no public copy constructor.
*
*Revision 1.34  2004/04/14 16:29:03  jianye
*deprecated getbioseq
*
*Revision 1.33  2004/03/18 16:30:24  grichenk
*Changed type of seq-align containers from list to vector.
*
*Revision 1.32  2004/02/10 21:59:36  jianye
*Clean up some defs
*
*Revision 1.31  2004/01/29 23:13:20  jianye
*Wrap defline
*
*Revision 1.30  2004/01/27 17:11:43  ucko
*Allocate storage for m_NumAsciiChar to avoid link errors on AIX.
*
*Revision 1.29  2004/01/21 18:32:46  jianye
*initialize m_ConfigFile
*
*Revision 1.28  2004/01/13 17:58:02  jianye
*Added geo linkout
*
*Revision 1.27  2004/01/05 17:59:24  vasilche
*Moved genbank loader and its readers sources to new location in objtools.
*Genbank is now in library libncbi_xloader_genbank.
*Id1 reader is now in library libncbi_xreader_id1.
*OBJMGR_LIBS macro updated correspondingly.
*
*Old headers temporarily will contain redirection to new location
*for compatibility:
*objmgr/gbloader.hpp > objtools/data_loaders/genbank/gbloader.hpp
*objmgr/reader_id1.hpp > objtools/data_loaders/genbank/readers/id1/reader_id1.hpp
*
*Revision 1.26  2003/12/29 20:54:52  jianye
*change CanGet to IsSet
*
*Revision 1.25  2003/12/29 18:36:32  jianye
*Added nuc to nuc translation, show minus master as plus strand, etc
*
*Revision 1.24  2003/12/22 21:13:59  camacho
*Fix Seq-align-set type qualifier.
*Indenting left as-is.
*
*Revision 1.23  2003/12/16 19:21:23  jianye
*Set k_ColorMismatchIdentity to 0
*
*Revision 1.22  2003/12/11 22:27:18  jianye
*Use toolkit blosum matrix
*
*Revision 1.21  2003/12/09 19:40:24  ucko
*+<stdio.h> for sprintf
*
*Revision 1.20  2003/12/04 22:27:34  jianye
*Fixed accession problem for redundat hits
*
*Revision 1.19  2003/12/01 23:15:45  jianye
*Added showing CDR product
*
*Revision 1.18  2003/11/26 17:57:04  jianye
*Handling query set case for get sequence feature
*
*Revision 1.17  2003/10/28 22:41:06  jianye
*Added downloading sub seq capability for long seq
*
*Revision 1.16  2003/10/27 20:55:53  jianye
*Added color mismatches capability
*
*Revision 1.15  2003/10/08 17:48:54  jianye
*Fomat cvs log message area better
*
*Revision 1.14  2003/10/08 17:38:22  jianye
*fix comment sign generated by cvs

* Revision 1.13  2003/10/08 17:30:00  jianye
* get rid of some warnings under linux
*
*===========================================================
*/
