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

#include <objtools/alnmgr/util/showalign.hpp>

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


BEGIN_NCBI_SCOPE
BEGIN_SCOPE (objects)
USING_SCOPE (sequence);

static const char k_IdentityChar = '.';
static const int k_NumFrame = 6;
static const string k_FrameConversion[k_NumFrame] = {"+1", "+2", "+3", "-1", "-2", "-3"};
static const int k_GetSubseqThreshhold = 10000;
static const int k_ColorMismatchIdentity = 0;  /*threshhold to color mismatch.  98 means 98% */
static const string k_DumpGnlUrl = "/blast/dumpgnl.cgi";
static const int k_FeatureIdLen = 16;

#define ENTREZ_URL "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=%s&list_uids=%d&dopt=%s\" %s>"
#define TRACE_URL  "<a href=\"http://www.ncbi.nlm.nih.gov/Traces/trace.cgi?cmd=retrieve&dopt=fasta&val=%s\">"

/* url for linkout*/
#define URL_LocusLink "<a href=\"http://www.ncbi.nlm.nih.gov/LocusLink/list.cgi?Q=%d%s\"><img border=0 height=16 width=16 src=\"/blast/images/L.gif\" alt=\"LocusLink info\"></a>"
#define URL_Unigene "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=unigene&cmd=search&term=%d[Nucleotide+UID]\"><img border=0 height=16 width=16 src=\"/blast/images/U.gif\" alt=\"UniGene info\"></a>"

#define URL_Structure "<a href=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\"><img border=0 height=16 width=16 src=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/str_link.gif\" alt=\"Related structures\"></a>"

#define URL_Structure_Overview "<a href=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\">Related Structures</a>"

#define URL_Geo "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=geo&term=%d[gi]\"><img border=0 height=16 width=16 src=\"/blast/images/G.gif\" alt=\"Geo\"></a>"

const int CDisplaySeqalign::m_NumColor;
const string CDisplaySeqalign::color[m_NumColor]={"#000000", "#808080", "#FF0000"};
const int CDisplaySeqalign::m_PMatrixSize;
const char CDisplaySeqalign::m_PSymbol[m_PMatrixSize+1] = "ARNDCQEGHILKMFPSTWYVBZX";

//Constructor
CDisplaySeqalign::CDisplaySeqalign(const CSeq_align_set& seqalign, list <SeqlocInfo*>& maskSeqloc, list <FeatureInfo*>& externalFeature, const int matrix[][m_PMatrixSize], CScope& scope) : m_SeqalignSetRef(&seqalign), m_Seqloc(maskSeqloc), m_QueryFeature(externalFeature), m_Scope(scope) {
  m_AlignOption = 0;
  m_SeqLocChar = eX;
  m_SeqLocColor = eBlack;
  m_LineLen = 60;
  m_IdStartMargin = 2;
  m_StartSequenceMargin = 2;
  m_SeqStopMargin = 2;
  m_IsDbNa = true;
  m_IsQueryNa = true;
  m_IsDbGi = false;
  m_DbName = NcbiEmptyString;
  m_NumAlignToShow = 10000;
  m_AlignType = eNotSet;
  m_Rid = "0";
  m_CddRid = "0";
  m_EntrezTerm = NcbiEmptyString;
  m_QueryNumber = 0;
  m_BlastType = NcbiEmptyString;
  m_MidLineStyle = eBar;

  SNCBIFullScoreMatrix blosumMatrix;
  NCBISM_Unpack(&NCBISM_Blosum62, &blosumMatrix);
 
  int** temp = new int*[m_NumAsciiChar];
  for(int i = 0; i<m_NumAsciiChar; ++i) {
    temp[i] = new int[m_NumAsciiChar];
  }
  for (int i=0; i<m_NumAsciiChar; i++){
    for (int j=0; j<m_NumAsciiChar; j++){
      temp[i][j] = -1000;
    }
  }
  for(int i = 0; i < m_PMatrixSize; ++i){
    for(int j = 0; j < m_PMatrixSize; ++j){
      if(matrix){
	temp[m_PSymbol[i]][m_PSymbol[j]] = matrix[i][j];
      } else {
	temp[m_PSymbol[i]][m_PSymbol[j]] = blosumMatrix.s[m_PSymbol[i]][m_PSymbol[j]];
      }
     
    }
  }
  for(int i = 0; i < m_PMatrixSize; ++i) {
    temp[m_PSymbol[i]]['*'] = temp['*'][m_PSymbol[i]] = -4;
  }
  temp['*']['*'] = 1;
 
  m_Matrix = temp;
 
}

//Destructor
CDisplaySeqalign::~CDisplaySeqalign(){
  for(int i = 0; i<m_NumAsciiChar; ++i) {
    delete [] m_Matrix[i];
  }
  delete [] m_Matrix;
  if(m_AlignOption & eHtml){
    delete m_ConfigFile;
    delete m_Reg;
  }

}

static void AddSpace(CNcbiOstream& out, int number);

static string GetTaxNames(const CBioseq& cbsp, int taxid);

static string getNameInitials(string& name);
static string GetSeqForm(char* formName, bool dbIsNa, int queryNumber);
static const string GetSeqIdStringByFastaOrder(const CSeq_id& id, CScope& sp, bool with_version);
static int GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids);

static string MakeURLSafe(char* src);
static void getAlnScores(const CSeq_align& aln, int& score, double& bits, double& evalue);
template<class container> static bool s_GetBlastScore(const container& scoreList,  int& score, double& bits, double& evalue);
static bool s_canDoMultiAlign(const CSeq_align_set& aln, CScope& scope);
static CRef<CSeq_id> GetSeqIdByType(const list<CRef<CSeq_id> >& ids, CSeq_id::E_Choice choice);
static int s_getFrame (int start, ENa_strand strand, const CSeq_id& id, CScope& sp);
static CRef<CSeq_align> CreateDensegFromDendiag(const CSeq_align& aln);
static void x_ColorDifferentBases(string& seq, char identityChar, CNcbiOstream& out);

//To add color to bases other than identityChar
static void x_ColorDifferentBases(string& seq, char identityChar, CNcbiOstream& out){
  string color = "#FF0000";
  bool tagOpened = false;
  for(int i = 0; i < (int)seq.size(); i ++){
    if(seq[i] != identityChar){
      if(!tagOpened){
	out << "<font color=\""+color+"\"><b>";
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


template<class container> static bool s_GetBlastScore(const container&  scoreList,  int& score, double& bits, double& evalue){
  bool hasScore = false;
  ITERATE (typename container, iter, scoreList) {
    const CObject_id& id=(*iter)->GetId();
    if (id.IsStr()) {
      hasScore = true;
      if (id.GetStr()=="score"){
	score = (*iter)->GetValue().GetInt();
      
      } else if (id.GetStr()=="bit_score"){
	bits = (*iter)->GetValue().GetReal();
	
      } else if (id.GetStr()=="e_value" || id.GetStr()=="sum_e") {
	evalue = (*iter)->GetValue().GetReal();
      } 
    }
  }
  return hasScore;
}
static CRef<CSeq_align> CreateDensegFromDendiag(const CSeq_align& aln) {
  CRef<CSeq_align> sa(new CSeq_align);
  if ( !aln.GetSegs().IsDendiag()) {
    NCBI_THROW(CException, eUnknown, "Input Seq-align should be Dendiag!");
  }
  
  if(aln.IsSetType()){
    sa->SetType(aln.GetType());
  }
  if(aln.IsSetDim()){
    sa->SetDim(aln.GetDim());
  }
  if(aln.IsSetScore()){
    sa->SetScore() = aln.GetScore();
  }
  if(aln.IsSetBounds()){
    sa->SetBounds() = aln.GetBounds();
  }
  
  CDense_seg& ds = sa->SetSegs().SetDenseg();

  int counter = 0;
  ds.SetNumseg() = 0;
  ITERATE (CSeq_align::C_Segs::TDendiag, iter, aln.GetSegs().GetDendiag()){
     
    if(counter == 0){//assume all dendiag segments have same dim and ids
      if((*iter)->IsSetDim()){
	ds.SetDim((*iter)->GetDim());
      }
      if((*iter)->IsSetIds()){
	ds.SetIds() = (*iter)->GetIds();
      }
    }
    ds.SetNumseg() ++;
    if((*iter)->IsSetStarts()){
      ITERATE(CDense_diag::TStarts, iterStarts, (*iter)->GetStarts()){
	ds.SetStarts().push_back(*iterStarts);
      }
    }
    if((*iter)->IsSetLen()){
      ds.SetLens().push_back((*iter)->GetLen());
    }
    if((*iter)->IsSetStrands()){
      ITERATE(CDense_diag::TStrands, iterStrands, (*iter)->GetStrands()){
	ds.SetStrands().push_back(*iterStrands);
      }
    }
    if((*iter)->IsSetScores()){
      ITERATE(CDense_diag::TScores, iterScores, (*iter)->GetScores()){
	ds.SetScores().push_back(*iterScores); //this might not have right meaning
      }
    }
    counter ++;
  }
  
  return sa;
}
static int s_GetStdsegMasterFrame(const CStd_seg& ss, CScope& scope){
  const CRef<CSeq_loc> slc = ss.GetLoc().front();
  ENa_strand strand = GetStrand(*slc);
  int frame = s_getFrame(strand ==  eNa_strand_plus ? GetStart(*slc) : GetStop(*slc), strand ==  eNa_strand_plus? eNa_strand_plus : eNa_strand_minus, *(ss.GetIds().front()), scope);
  
  return frame;
}

/*Note that start is zero bases.  It returns frame +/1(1-3).  ) indicates erro*/
static int s_getFrame (int start, ENa_strand strand, const CSeq_id& id, CScope& sp) {
  int frame = 0;
  if (strand == eNa_strand_plus) {
    frame = (start % 3) + 1;
  } else if (strand == eNa_strand_minus) {
    frame = -((sp.GetBioseqHandle(id).GetBioseq().GetInst().GetLength() - start - 1) % 3 + 1);
   
  }
  return frame;
}
static bool s_canDoMultiAlign(const CSeq_align_set& aln, CScope& scope){
  bool multiAlign = true;
  int firstFrame = 0;
  bool isFirstSeqalign = true;
  /*Make sure that in case of stdseg, the master must be in same frame for all pairwise for multialign view */
  for (CTypeConstIterator<CSeq_align> seqalign = ConstBegin(aln); seqalign; ++ seqalign){
    if (seqalign->GetSegs().Which() == CSeq_align::C_Segs::e_Denseg){
      break;
    } else if (seqalign->GetSegs().Which() == CSeq_align::C_Segs::e_Std){
      CTypeConstIterator<CStd_seg> ss = ConstBegin(*seqalign);     
      int curFrame = s_GetStdsegMasterFrame(*ss, scope);
      if (isFirstSeqalign){
	firstFrame = curFrame;
	isFirstSeqalign = false;
      } else {
	if (firstFrame != curFrame){
	  multiAlign = false;
	  break;
	}
       
      }
    }
   
  }
  return multiAlign;
}


static void getAlnScores(const CSeq_align& aln, int& score, double& bits, double& evalue){
  bool hasScore = false;
  //look for scores at seqalign level first
  hasScore = s_GetBlastScore(aln.GetScore(),  score, bits, evalue);

  //look at the seg level
  if(!hasScore){
    const CSeq_align::TSegs& seg = aln.GetSegs();
    if(seg.Which() == CSeq_align::C_Segs::e_Std){
      s_GetBlastScore(seg.GetStd().front()->GetScores(),  score, bits, evalue);
    } else if (seg.Which() == CSeq_align::C_Segs::e_Dendiag){
      s_GetBlastScore(seg.GetDendiag().front()->GetScores(),  score, bits, evalue);
    }  else if (seg.Which() == CSeq_align::C_Segs::e_Denseg){
       s_GetBlastScore(seg.GetDenseg().GetScores(),  score, bits, evalue);
    }
  }	
}

string CDisplaySeqalign::getUrl(const list<CRef<CSeq_id> >& ids, int row) const{
  string urlLink = NcbiEmptyString;
 
  char dopt[32], db[32];
  int gi = GetGiForSeqIdList(ids);
  string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
  if(toolUrl == NcbiEmptyString || (gi > 0 && toolUrl.find("dumpgnl.cgi") != string::npos)){ //use entrez or dbtag specified
    if(m_IsDbNa) {
      strcpy(dopt, "GenBank");
      strcpy(db, "Nucleotide");
    } else {
      strcpy(dopt, "GenPept");
      strcpy(db, "Protein");
    }    
 
    char urlBuf[1024];
    if (gi > 0) {
      sprintf(urlBuf, ENTREZ_URL, db, gi, dopt, (m_AlignOption & eNewTargetWindow) ? "TARGET=\"EntrezView\"" : "");
      urlLink = urlBuf;
    } else {//seqid general, dbtag specified
      const CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
      if(wid->Which() == CSeq_id::e_General){
        const CDbtag& dtg = wid->GetGeneral();
        const string& dbName = dtg.GetDb();
        if(NStr::CompareNocase(dbName, "TI") == 0){
          sprintf(urlBuf, TRACE_URL, wid->GetSeqIdString().c_str());
          urlLink = urlBuf;
        } else { //future use

        }
      }
    }
  } else { //need to use url in configuration file
    string altUrl = NcbiEmptyString;
      urlLink = getDumpgnlLink(ids, row, altUrl);
  }
  return urlLink;
}

static string getNameInitials(string& name){
  vector<string> arr;
  string initials;
  NStr::Tokenize(name, " ", arr);

  for(vector<string>::iterator iter = arr.begin(); iter != arr.end(); iter ++){
    if (*iter != NcbiEmptyString){
      initials += (*iter)[0];
    }
  }
  return initials;
}

static void AddSpace(CNcbiOstream& out, int number){
  for(int i=0; i<number; i++){
    out<<" ";
  }
}


void CDisplaySeqalign::AddLinkout(const CBioseq& cbsp, const CBlast_def_line& bdl, int firstGi, int gi, CNcbiOstream& out) const{

  char molType[8]={""};
  if(cbsp.IsAa()){
    sprintf(molType, "[pgi]");
  }
  else {
    sprintf(molType, "[ngi]");
  }
 
  if (bdl.IsSetLinks()){
    for (list< int >::const_iterator iter = bdl.GetLinks().begin(); iter != bdl.GetLinks().end(); iter ++){
      char buf[1024];
      if((*iter) & eLocuslink){
        sprintf(buf, URL_LocusLink, gi, molType);
        out << buf;
      }
      if ((*iter) & eUnigene) {
	sprintf(buf, URL_Unigene,  gi);
	out << buf;
      }
      if ((*iter) & eStructure){
        sprintf(buf, URL_Structure, m_Rid.c_str(), firstGi, gi, m_CddRid.c_str(), "onepair", (m_EntrezTerm == NcbiEmptyString) ? "none":((char*) m_EntrezTerm.c_str()));
	out << buf;
      }
    }
  }
}

//return the get sequence table for html display
static string GetSeqForm(char* formName, bool dbIsNa, int queryNumber){
  char buf[2048] = {""};
  if(formName){
    sprintf(buf, "<table border=\"0\"><tr><td><FORM  method=\"post\" action=\"http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?SUBMIT=y\" name=\"%s\"><input type=button value=\"Get selected sequences\" onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s')\"><input type=\"hidden\" name=\"db\" value=\"\"><input type=\"hidden\" name=\"term\" value=\"\"><input type=\"hidden\" name=\"doptcmdl\" value=\"docsum\"><input type=\"hidden\" name=\"cmd\" value=\"search\"></form></td><td><FORM><input type=\"button\" value=\"Select all\" onClick=\"handleCheckAll('select', 'getSeqAlignment%d', 'getSeqGi')\"></form></td><td><FORM><input type=\"button\" value=\"Deselect all\" onClick=\"handleCheckAll('deselect', 'getSeqAlignment%d', 'getSeqGi')\"></form></td></tr></table>", formName, dbIsNa?1:0, queryNumber, formName, queryNumber, queryNumber);
   
  }
  return buf;
}

//Return the seqid in "fasta" orders
static const string GetSeqIdStringByFastaOrder(const CSeq_id& id, CScope& sp, bool with_version){
  string idString = NcbiEmptyString;
  static const int total_seqid_types=19;
  static int fasta_order[total_seqid_types];
  //fasta order.  See seqidwrite() in C library
  fasta_order[CSeq_id::e_not_set]=33;
  fasta_order[CSeq_id::e_Local]=20;
  fasta_order[CSeq_id::e_Gibbsq]=15;
  fasta_order[CSeq_id::e_Gibbmt]=16;
  fasta_order[CSeq_id::e_Giim]=30;
  fasta_order[CSeq_id::e_Genbank]=10;   
  fasta_order[CSeq_id::e_Embl]=10;
  fasta_order[CSeq_id::e_Pir]=10;
  fasta_order[CSeq_id::e_Swissprot]=10;
  fasta_order[CSeq_id::e_Patent]=15;
  fasta_order[CSeq_id::e_Other]=12;
  fasta_order[CSeq_id::e_General]=13;
  fasta_order[CSeq_id::e_Gi]=255;
  fasta_order[CSeq_id::e_Ddbj]=10;
  fasta_order[CSeq_id::e_Prf]=10;
  fasta_order[CSeq_id::e_Pdb]=12;
  fasta_order[CSeq_id::e_Tpg]=10;
  fasta_order[CSeq_id::e_Tpe]=10;
  fasta_order[CSeq_id::e_Tpd]=10;
 
  CRef<CSeq_id> idRef;
  const list<CRef<CSeq_id> >& ids=sp.GetBioseqHandle(id).GetBioseq().GetId();
 
  for (CBioseq::TId::const_iterator iter = ids.begin(); iter != ids.end(); iter ++){
    if(iter == ids.begin()){
      idRef = *iter;
    }
    else if(fasta_order[(*iter)->Which()]<fasta_order[idRef->Which()]){
      idRef = *iter;
    }
    
  }
  if(!(ids.empty())){
    idString = idRef->GetSeqIdString(with_version);
  }
  return idString;
}

static CRef<CSeq_id> GetSeqIdByType(const list<CRef<CSeq_id> >& ids, CSeq_id::E_Choice choice) {
  CRef<CSeq_id> cid;

  for (CBioseq::TId::const_iterator iter = ids.begin(); iter != ids.end(); iter ++){
    if ((*iter)->Which() == choice){
      cid = *iter;
      break;
    }
  }
 
  return cid;
}

static int GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids){
  int gi = 0;
  CRef<CSeq_id> id = GetSeqIdByType(ids, CSeq_id::e_Gi);
  if (!(id.Empty())){
    return id->GetGi();
  }
  return gi;
}

//To display the seqalign represented by internal alnvec
void CDisplaySeqalign::DisplayAlnvec(CNcbiOstream& out){ 

  int maxIdLen=0;
  int maxStartLen=0;
  int startLen=0;
  int actualLineLen=0;
  int aln_stop=m_AV->GetAlnStop();
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
  list<alnFeatureInfo*>* bioseqFeature= new list<alnFeatureInfo*>[rowNum];
  CAlnMap::TSignedRange* rowRng = new CAlnMap::TSignedRange[rowNum];
  int* frame = new int[rowNum];

  //conver to aln coordinates for mask seqloc
  list<alnSeqlocInfo*> alnLocList;
  for (list<SeqlocInfo*>::iterator iter=m_Seqloc.begin();  iter!=m_Seqloc.end(); iter++){
    alnSeqlocInfo* alnloc = new alnSeqlocInfo;    
   
    for (int i=0; i<rowNum; i++){
     
      if((*iter)->seqloc->GetInt().GetId().Match(m_AV->GetSeqId(i))){
	int actualAlnStart = 0, actualAlnStop = 0;
	if(m_AV->IsPositiveStrand(i)){
	  actualAlnStart = m_AV->GetAlnPosFromSeqPos(i, (*iter)->seqloc->GetInt().GetFrom());
	  actualAlnStop = m_AV->GetAlnPosFromSeqPos(i, (*iter)->seqloc->GetInt().GetTo());
	} else {
	  actualAlnStart = m_AV->GetAlnPosFromSeqPos(i, (*iter)->seqloc->GetInt().GetTo());
	  actualAlnStop = m_AV->GetAlnPosFromSeqPos(i, (*iter)->seqloc->GetInt().GetFrom());
	}
        alnloc->alnRange.Set(actualAlnStart, actualAlnStop);      
	break;
      }
    }
    alnloc->seqloc = *iter;   
    alnLocList.push_back(alnloc);
  }
  m_Alnloc = alnLocList;
  //Add external query feature info such as phi blast pattern
  for (list<FeatureInfo*>::iterator iter=m_QueryFeature.begin();  iter!=m_QueryFeature.end(); iter++){
    for(int i = 0; i < rowNum; i++){
      if((*iter)->seqloc->GetInt().GetId().Match(m_AV->GetSeqId(i))){
	int actualSeqStart = 0, actualSeqStop = 0;
	if(m_AV->IsPositiveStrand(i)){
	  if((*iter)->seqloc->GetInt().GetFrom() < m_AV->GetSeqStart(i)){
	    actualSeqStart = m_AV->GetSeqStart(i);
	  } else {
	    actualSeqStart = (*iter)->seqloc->GetInt().GetFrom();
	  }

	  if((*iter)->seqloc->GetInt().GetTo() > m_AV->GetSeqStop(i)){
	    actualSeqStop = m_AV->GetSeqStop(i);
	  } else {
	    actualSeqStop = (*iter)->seqloc->GetInt().GetTo();
	  }
	} else {
	  if((*iter)->seqloc->GetInt().GetFrom() < m_AV->GetSeqStart(i)){
	    actualSeqStart = (*iter)->seqloc->GetInt().GetFrom();
	  } else {
	    actualSeqStart = m_AV->GetSeqStart(i);
	  }
	  
	  if((*iter)->seqloc->GetInt().GetTo() > m_AV->GetSeqStop(i)){
	    actualSeqStop = (*iter)->seqloc->GetInt().GetTo();
	  } else {
	    actualSeqStop = m_AV->GetSeqStop(i);
	  }
	}
	int alnFrom = m_AV->GetAlnPosFromSeqPos(i, actualSeqStart);
	int alnTo = m_AV->GetAlnPosFromSeqPos(i, actualSeqStop);
	
	alnFeatureInfo* featInfo = new alnFeatureInfo;
	string tempFeat = NcbiEmptyString;
	setFeatureInfo(featInfo, *((*iter)->seqloc), alnFrom, alnTo, aln_stop, (*iter)->featureChar, (*iter)->featureId, tempFeat);    
	bioseqFeature[i].push_back(featInfo);
      }
    }
  }

  //prepare data for each row
  for (int row=0; row<rowNum; row++) {
    rowRng[row] = m_AV->GetSeqAlnRange(row);
    frame[row] = (m_AV->GetWidth(row) == 3 ? s_getFrame(m_AV->IsPositiveStrand(row) ? m_AV->GetSeqStart(row) : m_AV->GetSeqStop(row), m_AV->IsPositiveStrand(row) ? eNa_strand_plus : eNa_strand_minus, m_AV->GetSeqId(row), m_Scope) : 0);
 
       
    //make sequence
    m_AV->GetWholeAlnSeqString(row, sequence[row],  &insertAlnStart[row], &insertStart[row], &insertLength[row], m_LineLen, &seqStarts[row], &seqStops[row]);
    
    //make feature.  Only for pairwise, non-query-anchored and untranslated
    if(!(m_AlignOption & eMasterAnchored) && !(m_AlignOption & eMultiAlign) && m_AV->GetWidth(row) != 3){
      if(m_AlignOption & eShowCdsFeature){
	getFeatureInfo(bioseqFeature[row], *m_featScope, CSeqFeatData::e_Cdregion, row, sequence[row]);
      }
      if(m_AlignOption & eShowGeneFeature){
	getFeatureInfo(bioseqFeature[row], *m_featScope, CSeqFeatData::e_Gene, row, sequence[row]);
      }
    }
 
    //make id
    if(m_AlignOption & eShowBlastStyleId) {
       if(row==0){//query
         seqidArray[row]="Query";
       } else {//hits
         if (!(m_AlignOption&eMultiAlign)){
         //hits for pairwise 
           seqidArray[row]="Sbjct";
         } else {
           int gi = GetGiForSeqIdList(m_AV->GetBioseqHandle(row).GetBioseq().GetId());
           if (m_AlignOption&eShowGi && gi > 0){
             seqidArray[row]=NStr::IntToString(gi);
           } else {
             const CBioseq& bsp=m_AV->GetBioseqHandle(row).GetBioseq();
             const CRef<CSeq_id> wid = FindBestChoice(bsp.GetId(), CSeq_id::WorstRank);
             seqidArray[row]=wid->GetSeqIdString();
           }           
         }
       }

    } else {
      int gi = GetGiForSeqIdList(m_AV->GetBioseqHandle(row).GetBioseq().GetId());
      if (m_AlignOption&eShowGi && gi > 0){
        seqidArray[row]=NStr::IntToString(gi);
      } else {
        const CBioseq& bsp=m_AV->GetBioseqHandle(row).GetBioseq();
        const CRef<CSeq_id> wid = FindBestChoice(bsp.GetId(), CSeq_id::WorstRank);
        seqidArray[row]=wid->GetSeqIdString();
      }      
    }

    //max id length
    maxIdLen=max<int>(seqidArray[row].size(), maxIdLen);
    //max start length
    int maxCood=max<int>(m_AV->GetSeqStart(row), m_AV->GetSeqStop(row));
    maxStartLen = max<int>(NStr::IntToString(maxCood).size(), maxStartLen);
  }
  //adjust max id length for feature id 
  for(int i = 0; i < rowNum; i ++){
    for (list<alnFeatureInfo*>::iterator iter=bioseqFeature[i].begin();  iter != bioseqFeature[i].end(); iter++){
      maxIdLen=max<int>((*iter)->feature->featureId.size(), maxIdLen );
    }
  }  //end of preparing row data
  
  bool colorMismatch = false; //color the mismatches
  //output identities info 
  if(m_AlignOption&eShowBlastInfo && !(m_AlignOption&eMultiAlign)) {
    int match = 0;
    int positive = 0;
    int gap = 0;
    int identity = 0;
    fillIdentityInfo(sequence[0], sequence[1],  match,  positive, middleLine);
    identity = (match*100)/(aln_stop+1);
    if(identity >= k_ColorMismatchIdentity && identity <100){
      colorMismatch = true;
    }
    out<<" Identities = "<<match<<"/"<<(aln_stop+1)<<" ("<<identity<<"%"<<")";
    if(m_AlignType&eProt) {
      out<<", Positives = "<<(positive + match)<<"/"<<(aln_stop+1)<<" ("<<(((positive + match)*100)/(aln_stop+1))<<"%"<<")";
    }
    gap = getNumGaps();
    out<<", Gaps = "<<gap<<"/"<<(aln_stop+1)<<" ("<<((gap*100)/(aln_stop+1))<<"%"<<")"<<endl;
    if (m_AlignType&eNuc){ 
      out<<" Strand="<<(m_AV->StrandSign(0)==1 ? "Plus" : "Minus")<<"/"<<(m_AV->StrandSign(1)==1? "Plus" : "Minus")<<endl;
    }
    if(frame[0] != 0 && frame[1] != 0) {
      out <<" Frame = " << ((frame[0] > 0) ? "+" : "") << frame[0] <<"/"<<((frame[1] > 0) ? "+" : "") << frame[1]<<endl;
    } else if (frame[0] != 0){
      out <<" Frame = " << ((frame[0] > 0) ? "+" : "") << frame[0] <<endl;
    }  else if (frame[1] != 0){
      out <<" Frame = " << ((frame[1] > 0) ? "+" : "") << frame[1] <<endl;
    } 
    out<<endl;
  }
  //output rows
  for(int j=0; j<=aln_stop; j+=m_LineLen){
    //output according to aln coordinates
    if(aln_stop-j+1<m_LineLen) {
      actualLineLen=aln_stop-j+1;
    } else {
      actualLineLen=m_LineLen;
    }
    CAlnMap::TSignedRange curRange(j, j+actualLineLen-1);
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
	  list<insertInformation*> insertList;
	  GetInserts(insertList, insertAlnStart[row], insertStart[row], insertLength[row],  j + m_LineLen);
	  fillInserts(row, curRange, j, inserts, insertPosString, insertList);
	  ITERATE(list<insertInformation*>, iterINsert, insertList){
	    delete *iterINsert;
	  }
	}
        if(row == 0&&(m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
          char checkboxBuf[200];
          sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment%d', 'getSeqMaster')\">", m_QueryNumber);
          out << checkboxBuf;
        }
        string urlLink;
        //setup url link for seqid
        if(row>0&&(m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign)){
        
          int gi = GetGiForSeqIdList(m_AV->GetBioseqHandle(row).GetBioseq().GetId());
          if(gi > 0){
            out<<"<a name="<<gi<<"></a>";
          } else {
            out<<"<a name="<<seqidArray[row]<<"></a>";
          }
          //get sequence checkbox
          if(m_AlignOption&eSequenceRetrieval && m_IsDbGi){
            char checkBoxBuf[512];
            sprintf(checkBoxBuf, "<input type=\"checkbox\" name=\"getSeqGi\" value=\"%d\" onClick=\"synchronizeCheck(this.value, 'getSeqAlignment%d', 'getSeqGi', this.checked)\">", gi, m_QueryNumber);
            out << checkBoxBuf;        
          }
          urlLink = getUrl(m_AV->GetBioseqHandle(row).GetBioseq().GetId(), row);         
          out << urlLink;
         
        }
        out<<seqidArray[row]; 
        if(row>0&& m_AlignOption&eHtml && m_AlignOption&eMultiAlign && urlLink != NcbiEmptyString){
          out<<"</a>";
         
        }
        //adjust space between id and start
        AddSpace(out, maxIdLen-seqidArray[row].size()+m_IdStartMargin);
	out << start;
        startLen=NStr::IntToString(start).size();
        AddSpace(out, maxStartLen-startLen+m_StartSequenceMargin);
	if (row>0 && m_AlignOption & eShowIdentity){
	  for (int index = j; index < j + actualLineLen && index < (int)sequence[row].size(); index ++){
	    if (sequence[row][index] == sequence[0][index] && isalpha(sequence[row][index])) {
	      sequence[row][index] = k_IdentityChar;           
	    }         
	  }
	}
	OutputSeq(sequence[row], m_AV->GetSeqId(row), j, actualLineLen, frame[row], (row > 0 && colorMismatch)?true:false, out);
        AddSpace(out, m_SeqStopMargin);
	out << end;
        
        out<<endl;
     
	//display inserts for anchored type
	if(m_AlignOption & eMasterAnchored){
	  bool insertAlready = false;
	  for(list<string>::iterator iter = inserts.begin(); iter != inserts.end(); iter ++){
	   
	    if(!insertAlready){
	      if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
		char checkboxBuf[200];
		sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment%d', 'getSeqMaster')\">", m_QueryNumber);
		out << checkboxBuf;
	      }
	      AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin);
	      out << insertPosString<<endl;
	    }
	    if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
	      char checkboxBuf[200];
	      sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment%d', 'getSeqMaster')\">", m_QueryNumber);
	      out << checkboxBuf;
	    }
	    AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin);
	    out<<*iter<<endl;
	    
	    insertAlready = true;
	  }
	} 
	//display feature. Feature, if set, will be displayed for query regardless

	for (list<alnFeatureInfo*>::iterator iter=bioseqFeature[row].begin();  iter != bioseqFeature[row].end(); iter++){
	  if ( curRange.IntersectingWith((*iter)->alnRange)){  
	    if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
	      char checkboxBuf[200];
	      sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment%d', 'getSeqMaster')\">", m_QueryNumber);
	      out << checkboxBuf;
	    }
	    out<<(*iter)->feature->featureId;
	    AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin-(*iter)->feature->featureId.size());
	    OutputSeq((*iter)->featureString, CSeq_id(), j, actualLineLen, 0, false, out);
	    out<<endl;
	  }
	}
	
	//display middle line
	if (row == 0 && ((m_AlignOption & eShowMiddleLine)) && !(m_AlignOption&eMultiAlign)) {
	  AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin);
	  OutputSeq(middleLine, CSeq_id(), j, actualLineLen, 0, false, out);
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
 
  //free allocation
  for(int i = 0; i < rowNum; i ++){
    for (list<alnFeatureInfo*>::iterator iter=bioseqFeature[i].begin();  iter != bioseqFeature[i].end(); iter++){
      delete (*iter)->feature;
      delete (*iter);
    }
  } 

  for (list<alnSeqlocInfo*>::const_iterator iter = alnLocList.begin();  iter != alnLocList.end(); iter++){
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
}

//To display the seqalign
void CDisplaySeqalign::DisplaySeqalign(CNcbiOstream& out){
  
  if(m_SeqalignSetRef->Get().empty()){
    return;
  }

  //scope for feature fetching
  if(!(m_AlignOption & eMasterAnchored) && (m_AlignOption & eShowCdsFeature || m_AlignOption & eShowGeneFeature)){
    m_FeatObj = new CObjectManager();
    m_FeatObj->RegisterDataLoader(*new CGBDataLoader("ID", NULL, 2), CObjectManager::eDefault); 
    m_featScope = new CScope(*m_FeatObj);  //for seq feature fetch
    m_featScope->AddDefaults();	     
  }	
 
  setDbGi(); //for whether to add get sequence feature
  if(m_AlignOption & eHtml){
    //set config file
    m_ConfigFile = new CNcbiIfstream(".ncbirc");
    m_Reg = new CNcbiRegistry(*m_ConfigFile);
    out<<"<script src=\"blastResult.js\"></script>";
  }
   //get sequence 
  if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_IsDbGi){ 
        out<<GetSeqForm((char*)"submitterTop", m_IsDbNa, m_QueryNumber);
        out<<"<form name=\"getSeqAlignment"<<m_QueryNumber<<"\">\n";
      }

  //begin to display
  int num_align = 0;
  string toolUrl = NcbiEmptyString;
  if(m_AlignOption & eHtml){
    toolUrl = m_Reg->Get(m_BlastType, "TOOL_URL");
  }
  auto_ptr<CObjectOStream> out2(CObjectOStream::Open(eSerial_AsnText, out));

  if(!(m_AlignOption&eMultiAlign)){/*pairwise alignment. Note we can't just show each alnment as we go because we will need seg information form all hsp's with the same id for genome url link.  As a result we show hsp's with the same id as a group*/
    list<alnInfo*> avList;        
    string previousId = NcbiEmptyString, subid = NcbiEmptyString;
    bool isFirstAln = true;
    for (CSeq_align_set::Tdata::const_iterator iter = m_SeqalignSetRef->Get().begin(); iter != m_SeqalignSetRef->Get().end()&&num_align<m_NumAlignToShow; iter++, num_align++) {
      
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

      } else if((*iter)->GetSegs().Which() == CSeq_align::C_Segs::e_Denseg){
	if (m_AlignOption & eTranslateNucToNucAlignment) { 
	  finalAln = (*iter)->CreateTranslatedDensegFromNADenseg();
	} else {
	  finalAln = (*iter);
	}

      } else if((*iter)->GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag){
	CRef<CSeq_align> densegAln = CreateDensegFromDendiag(**iter);
	if (m_AlignOption & eTranslateNucToNucAlignment) { 
	  finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
	} else {
	  finalAln = densegAln;
	}

      } else {
	NCBI_THROW(CException, eUnknown, "Seq-align should be Denseg, Stdseg or Dendiag!");
      }
      CRef<CDense_seg> finalDenseg(new CDense_seg);
      const CTypeIterator<CDense_seg> ds = Begin(*finalAln);
      if((ds->IsSetStrands() && ds->GetStrands().front()==eNa_strand_minus) && !(ds->IsSetWidths() && ds->GetWidths()[0] == 3)){
	//show plus strand if master is minus for non-translated case
	memcpy(&*finalDenseg, &(*ds), sizeof(CDense_seg));
	finalDenseg->Reverse();
	avRef = new CAlnVec(*finalDenseg, m_Scope);
	
      } else {
	avRef = new CAlnVec(*ds, m_Scope);
      }
    
      if(!(avRef.Empty())){
	try{
	  const CBioseq_Handle& handle = avRef->GetBioseqHandle(1);	
	  if(handle){
	    subid=avRef->GetSeqId(1).GetSeqIdString();
	    
	    if(!isFirstAln && subid != previousId) {//this aln is a new id, show result for previous id
	      x_DisplayAlnvecList(out, avList);
	    
	      for(list<alnInfo*>::iterator iterAv = avList.begin(); iterAv != avList.end(); iterAv ++){
		delete(*iterAv);
	      }
	      avList.clear();
	      
	    }
	    //save the current alnment regardless
	    alnInfo* alnvecInfo = new alnInfo;
	    getAlnScores(**iter, alnvecInfo->score, alnvecInfo->bits, alnvecInfo->eValue);
	    alnvecInfo->alnVec = avRef;
	    avList.push_back(alnvecInfo);
	    int gi = GetGiForSeqIdList(handle.GetBioseq().GetId());
	    if(!(toolUrl == NcbiEmptyString || (gi > 0 && toolUrl.find("dumpgnl.cgi") != string::npos)) || (m_AlignOption & eLinkout)){ /*need to construct segs for dumpgnl and get sub-sequence for long sequences*/
	      string idString = avRef->GetSeqId(1).GetSeqIdString();
	      if(m_Segs.count(idString) > 0){ 	//already has seg, concatenate
		/*Note that currently it's not necessary to use map to store this information.  But I already implemented this way for previous version.  Will keep this way as it's more flexible if we change something*/
	
		m_Segs[idString] += "," + NStr::IntToString(avRef->GetSeqStart(1)) + "-" + NStr::IntToString(avRef->GetSeqStop(1));
	      } else {//new segs
		m_Segs.insert(map<string, string>::value_type(idString, NStr::IntToString(avRef->GetSeqStart(1)) + "-" + NStr::IntToString(avRef->GetSeqStop(1))));
	      }
	    }	    
	    isFirstAln = false;
	    previousId = subid;
	  }
	 
	} catch (CException& e){
	continue;
	}
	
      }
    } 
  
    //Show here for the last one 
    if(!avList.empty()){
      x_DisplayAlnvecList(out, avList);
      for(list<alnInfo*>::iterator iterAv = avList.begin(); iterAv != avList.end(); iterAv ++){
	delete(*iterAv);
      }
      avList.clear();
    }
  	     
  } else if(m_AlignOption&eMultiAlign){ //multiple alignment
       
    CRef<CAlnMix>* mix = new CRef<CAlnMix>[k_NumFrame]; //each for one frame for translated alignment
    for(int i = 0; i < k_NumFrame; i++){
      mix[i] = new CAlnMix(m_Scope);
    }

    num_align = 0;
    vector<CRef<CSeq_align_set> > alnVector(k_NumFrame);
    for(int i = 0; i <  k_NumFrame; i ++){
      alnVector[i] = new CSeq_align_set;
    }
    for (CSeq_align_set::Tdata::const_iterator alnIter = m_SeqalignSetRef->Get().begin(); alnIter != m_SeqalignSetRef->Get().end()&&num_align<m_NumAlignToShow; alnIter ++, num_align++) {
      //need to convert to denseg for stdseg
      if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::e_Std) {
	CTypeConstIterator<CStd_seg> ss = ConstBegin(**alnIter); 
	CRef<CSeq_align> convertedDs = (*alnIter)->CreateDensegFromStdseg();
	if((convertedDs->GetSegs().GetDenseg().IsSetWidths() && convertedDs->GetSegs().GetDenseg().GetWidths()[0] == 3) || m_AlignOption & eTranslateNucToNucAlignment){//only do this for translated master
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
      } else if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::e_Denseg){
	alnVector[0]->Set().push_back(*alnIter);
      } else if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag){
	alnVector[0]->Set().push_back(CreateDensegFromDendiag(**alnIter));
      } else {
	NCBI_THROW(CException, eUnknown, "Input Seq-align should be Denseg, Stdseg or Dendiag!");
      }
    }
    
    for(int i = 0; i < (int)alnVector.size(); i ++){
      bool hasAln = false;
      for(CTypeConstIterator<CSeq_align> alnRef = ConstBegin(*alnVector[i]); alnRef; ++alnRef){
	CTypeConstIterator<CDense_seg> ds = ConstBegin(*alnRef);
	//	*out2 << *ds;
	try{
	  if (m_AlignOption & eTranslateNucToNucAlignment) {	 
	    mix[i]->Add(*ds, CAlnMix::fForceTranslation);
	  } else {
	    mix[i]->Add(*ds);
	  }
	} catch (CException& e){
	  continue;
	}
	 hasAln = true;
      }
      if(hasAln){
	//	*out2<<*alnVector[i];
	  mix[i]->Merge(CAlnMix::fGen2EST| CAlnMix::fMinGap | CAlnMix::fQuerySeqMergeOnly | CAlnMix::fFillUnalignedRegions);  
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
	CRef<CAlnVec> avRef (new CAlnVec (mix[i]->GetDenseg(), m_Scope));
	m_AV = avRef;
	if(numDistinctFrames > 1){
	 out << "For reading frame " << k_FrameConversion[i] << " of query sequence:" << endl << endl;
	}
	DisplayAlnvec(out);
      } catch (CException e){
	continue;
      }
    } 
    delete [] mix;
  }
  if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_IsDbGi){
    out<<"</form>\n";
    out<<GetSeqForm((char*)"submitterBottom", m_IsDbNa, m_QueryNumber);
  }
}

//compute number of identical and positive residues; set middle line accordingly
const void CDisplaySeqalign::fillIdentityInfo(const string& sequenceStandard, const string& sequence , int& match, int& positive, string& middleLine) {
  match = 0;
  positive = 0;
  int min_length=min<int>(sequenceStandard.size(), sequence.size());
  if(m_AlignOption & eShowMiddleLine){
    middleLine = sequence;
  }
  for(int i=0; i<min_length; i++){
    if(sequenceStandard[i]==sequence[i]){
      if(m_AlignOption & eShowMiddleLine){
	if(m_MidLineStyle == eBar ) {
	  middleLine[i] = '|';
	} else if (m_MidLineStyle == eChar){
	  middleLine[i] = sequence[i];
	}
      }
      match ++;
    } else {
      if ((m_AlignType&eProt) && m_Matrix[sequenceStandard[i]][sequence[i]] > 0){  
	positive ++;
	if(m_AlignOption & eShowMiddleLine){
	  if (m_MidLineStyle == eChar){
	    middleLine[i] = '+';
	  }
	}
      } else {
	if (m_AlignOption & eShowMiddleLine){
	  middleLine[i] = ' ';
	}
      }    
    }
  }  
}


const void CDisplaySeqalign::PrintDefLine(const CBioseq_Handle& bspHandle, CNcbiOstream& out) const
{
  if(bspHandle){
    const CRef<CSeq_id> wid = FindBestChoice(bspHandle.GetBioseq().GetId(), CSeq_id::WorstRank);
 
    const CRef<CBlast_def_line_set> bdlRef = GetBlastDefline(bspHandle.GetBioseq());
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    bool isFirst = true;
    int firstGi = 0;
  
    if(bdl.empty()){ //no blast defline struct, should be no such case now
      out << ">"; 
      wid->WriteAsFasta(out);
      out<<" ";
      out << GetTitle(bspHandle);
      out << endl;
    } else {
      //print each defline 
      for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin(); iter != bdl.end(); iter++){
	string urlLink;
	if(isFirst){
	  out << ">";
   
	} else{
	  out << " ";
	}
	const CRef<CSeq_id> wid2 = FindBestChoice((*iter)->GetSeqid(), CSeq_id::WorstRank);
	int gi =  GetGiForSeqIdList((*iter)->GetSeqid());
	if(isFirst){
	  firstGi = gi;
	}
	if ((m_AlignOption&eSequenceRetrieval) && (m_AlignOption&eHtml) && m_IsDbGi && isFirst) {
	  char buf[512];
	  sprintf(buf, "<input type=\"checkbox\" name=\"getSeqGi\" value=\"%d\" onClick=\"synchronizeCheck(this.value, 'getSeqAlignment%d', 'getSeqGi', this.checked)\">", gi, m_QueryNumber);
	  out << buf;
	}
 
	if(m_AlignOption&eHtml){
     
	  urlLink = getUrl((*iter)->GetSeqid(), 1);    
	  out<<urlLink;
	}
    
	if(m_AlignOption&eShowGi && gi > 0){
	  out<<"gi|"<<gi<<"|";
	}       
   
	wid2->WriteAsFasta(out);
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
	    AddLinkout(bspHandle.GetBioseq(), (**iter), firstGi, gi, out);
	    if((int)bspHandle.GetBioseq().GetInst().GetLength() > k_GetSubseqThreshhold){
	      string dumpGnlUrl = getDumpgnlLink((*iter)->GetSeqid(), 1, k_DumpGnlUrl);
	      out<<dumpGnlUrl<<"<img border=0 height=16 width=16 src=\"/blast/images/D.gif\" alt=\"Download subject sequence spanning the HSP\"></a>";
	    }
	  }
	}
 
	out <<" ";
	if((*iter)->IsSetTitle()){
	  out << (*iter)->GetTitle();     
	}
	out<<endl;
	isFirst = false;
      }
    }
  }
}

//Output sequence and mask sequences if any
const void CDisplaySeqalign::OutputSeq(string& sequence, const CSeq_id& id, int start, int len, int frame, bool colorMismatch, CNcbiOstream& out) const {
  int actualSize = sequence.size();
  assert(actualSize > start);
  list<CRange<int> > actualSeqloc;
  string actualSeq = sequence.substr(start, len);
  
  if(id.Which() != CSeq_id::e_not_set){ /*only do this for sequence but not for others like middle line, features*/
    //go through seqloc containing mask info
    for (list<alnSeqlocInfo*>::const_iterator iter = m_Alnloc.begin();  iter != m_Alnloc.end(); iter++){
      int from=(*iter)->alnRange.GetFrom();
      int to=(*iter)->alnRange.GetTo();
      int locFrame = (*iter)->seqloc->frame;
      if(id.Match((*iter)->seqloc->seqloc->GetInt().GetId()) && locFrame == frame){
	bool isFirstChar = true;
	CRange<int> eachSeqloc(0, 0);
	//go through each residule and mask it
	for (int i=max<int>(from, start); i<=min<int>(to, start+len); i++){
	  //store seqloc start for font tag below
	  if ((m_AlignOption & eHtml) && isFirstChar){         
	    isFirstChar = false;
	    eachSeqloc.Set(i, eachSeqloc.GetTo());
	  }
	  if (m_SeqLocChar==eX){
	    actualSeq[i-start]='X';
	  } else if (m_SeqLocChar==eN){
	    actualSeq[i-start]='n';
	  } else if (m_SeqLocChar==eLowerCase){
	    actualSeq[i-start]=tolower(actualSeq[i-start]);
	  }
	  //store seqloc start for font tag below
	  if ((m_AlignOption & eHtml) && i == min<int>(to, start+len)){ 
	    eachSeqloc.Set(eachSeqloc.GetFrom(), i);
	  }
	}
	if(!(eachSeqloc.GetFrom()==0&&eachSeqloc.GetTo()==0)){
	  actualSeqloc.push_back(eachSeqloc);
	}
      }
    }
  }

  if(actualSeqloc.empty()){//no need to add font tag
    if((m_AlignOption & eColorDifferentBases) && (m_AlignOption & eHtml) && colorMismatch){
      //color the mismatches. Only for rows without mask.  Otherwise it may confilicts with mask font tag.
      x_ColorDifferentBases(actualSeq, k_IdentityChar, out);
    } else {
      out<<actualSeq;
    }
  } else {//now deal with font tag for mask for html display    
    bool endTag = false;
    bool numFrontTag = 0;
    for (int i = 0; i < (int)actualSeq.size(); i ++){
      for (list<CRange<int> >::iterator iter=actualSeqloc.begin();  iter!=actualSeqloc.end(); iter++){
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

int CDisplaySeqalign::getNumGaps() {
  int gap = 0;
  for (int row=0; row<m_AV->GetNumRows(); row++) {
    CRef<CAlnMap::CAlnChunkVec> chunk_vec = m_AV->GetAlnChunks(row, m_AV->GetSeqAlnRange(0));
    for (int i=0; i<chunk_vec->size(); i++) {
      CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
      if (chunk->IsGap()) {
        gap += (chunk->GetAlnRange().GetTo() - chunk->GetAlnRange().GetFrom() + 1);
      }
    }
  }
  return gap;
}


const CRef<CBlast_def_line_set>  CDisplaySeqalign::GetBlastDefline (const CBioseq& cbsp) const {
  CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set());
  if(cbsp.IsSetDescr()){
    const CSeq_descr& desc = cbsp.GetDescr();
    const list< CRef< CSeqdesc > >& descList = desc.Get();
    for (list<CRef< CSeqdesc > >::const_iterator iter = descList.begin(); iter != descList.end(); iter++){
      
      if((*iter)->IsUser()){
        const CUser_object& uobj = (*iter)->GetUser();
        const CObject_id& uobjid = uobj.GetType();
        if(uobjid.IsStr()){
   
          const string& label = uobjid.GetStr();
          if (label == kAsnDeflineObjLabel){
           const vector< CRef< CUser_field > >& usf = uobj.GetData();
           string buf;
          
	   if(usf.front()->GetData().IsOss()){ //only one user field
             typedef const CUser_field::TData::TOss TOss;
             const TOss& oss = usf.front()->GetData().GetOss();
             int size = 0;
             //determine the octet string length
             ITERATE (TOss, iter3, oss) {
	       size += (**iter3).size();
             }
             
             int i =0;
             char* temp = new char[size];
             //retrive the string
             ITERATE (TOss, iter3, oss) {
      
               for(vector< char >::iterator iter4 = (**iter3).begin(); iter4 !=(**iter3).end(); iter4++){
                 temp[i] = *iter4;
                 i++;
               }
             }            
	    
	     CConn_MemoryStream stream;
             stream.write(temp, i);
             auto_ptr<CObjectIStream> ois(CObjectIStream::Open(eSerial_AsnBinary, stream));
             *ois >> *bdls;
	     delete [] temp;
           }         
          }
        }
      }
    }
  }
  return bdls;
}

static string GetTaxNames(const CBioseq& cbsp, int taxid){
  string name;
  if(cbsp.IsSetDescr()){  
    const CSeq_descr& desc = cbsp.GetDescr();
    const list< CRef< CSeqdesc > >& descList = desc.Get();   
    for (list<CRef< CSeqdesc > >::const_iterator iter = descList.begin(); iter != descList.end(); iter++){
      if((*iter)->IsUser()){
        const CUser_object& uobj = (*iter)->GetUser();
        const CObject_id& uobjid = uobj.GetType();
        if(uobjid.IsStr()){   
          const string& label = uobjid.GetStr();
          if (label == kTaxDataObjLabel){
            const vector< CRef< CUser_field > >& usf = uobj.GetData();        
            for (vector< CRef< CUser_field > >::const_iterator iter2 = usf.begin(); iter2 != usf.end(); iter2 ++){
              const CObject_id& oid = (*iter2)->GetLabel();
              if (oid.GetId() == taxid){
                (**iter2).GetData().Which();
                name = (**iter2).GetData().GetStrs().front();
                break;
              }
            }
          }
        }
      }
    }
  }
  return name;
}

void CDisplaySeqalign::getFeatureInfo(list<alnFeatureInfo*>& feature, CScope& scope, CSeqFeatData::E_Choice choice, int row, string& sequence) const {
  //Only fetch features for seq that has a gi
  CRef<CSeq_id> id = GetSeqIdByType(m_AV->GetBioseqHandle(row).GetBioseq().GetId(), CSeq_id::e_Gi);
  if(!(id.Empty())){
    const CBioseq_Handle& handle = scope.GetBioseqHandle(*id);
    //cds feature
    for  (CFeat_CI feat (handle, m_AV->GetSeqStart(row), m_AV->GetSeqStop(row), choice); feat;  ++feat) {
      
      const CSeq_loc& loc = feat->GetLocation();
      string featLable = NcbiEmptyString;
      string featId;
      string alternativeFeatStr = NcbiEmptyString;

      feature::GetLabel(feat->GetOriginalFeature(), &featLable, feature::eBoth, &(m_AV->GetScope()));
      featId = featLable.substr(0, k_FeatureIdLen); //default
	  
      int alnStop = m_AV->GetAlnStop();      
      if(loc.IsInt()){
	alnFeatureInfo* featInfo = new alnFeatureInfo;
	int featSeqFrom = loc.GetInt().GetFrom();
	int featSeqTo = loc.GetInt().GetTo();
	int actualFeatSeqStart = 0, actualFeatSeqStop = 0;
	if(m_AV->IsPositiveStrand(row)){
	  if(featSeqFrom < (int)m_AV->GetSeqStart(row)){
	    actualFeatSeqStart = m_AV->GetSeqStart(row);
	  } else {
	    actualFeatSeqStart = featSeqFrom;
	  }

	  if(featSeqTo > (int)m_AV->GetSeqStop(row)){
	    actualFeatSeqStop = m_AV->GetSeqStop(row);
	  } else {
	    actualFeatSeqStop = featSeqTo;
	  }
	} else {
	  if(featSeqFrom < (int)m_AV->GetSeqStart(row)){
	    actualFeatSeqStart = featSeqFrom;
	  } else {
	    actualFeatSeqStart = m_AV->GetSeqStart(row); 
	  }

	  if(featSeqTo > (int)m_AV->GetSeqStop(row)){
	    actualFeatSeqStop = featSeqTo;
	  } else {
	    actualFeatSeqStop = m_AV->GetSeqStop(row);
	  }
	}

	int alnFrom = m_AV->GetAlnPosFromSeqPos(row, actualFeatSeqStart);
	int alnTo = m_AV->GetAlnPosFromSeqPos(row, actualFeatSeqStop);
	char featChar = ' ';
	if(choice == CSeqFeatData::e_Gene){
	  featChar = '^';
	} else if (choice == CSeqFeatData::e_Cdregion){
	  featChar = '~';
	}

	//need to construct the protein seq aligned to nucleotide seq 
	if (choice == CSeqFeatData::e_Cdregion){
	  string rawCdrProduct = NcbiEmptyString;
	  if(feat->IsSetProduct()){
	    const CSeq_loc& productLoc = feat->GetProduct(); 
	    //only show first k_FeatureIdLen letters
	    if(productLoc.IsWhole()){
	      const CSeq_id& productId = productLoc.GetWhole();
	      const CBioseq_Handle& productHandle = scope.GetBioseqHandle(productId );
	      featId = "CDS:" + GetTitle(productHandle).substr(0, k_FeatureIdLen);
	    }
	  }
	  //show protein product only if the row is plus strand
	  if(m_AV->IsPositiveStrand(row)){
	    string line(alnStop+1, ' ');  
	    CCdregion_translate::TranslateCdregion (rawCdrProduct, handle, loc, feat->GetData().GetCdregion(), true, false);
	    
	    bool firstBase = true;
	    char gapChar = m_AV->GetGapChar(row);
	    int marginAdjuster = 0;
	    int featStartSeqPos = 0;
	    int firstFeatStringPos = 0;
	    int numBase = 0;
	    
	    //put actual amino acid to the cdr product line in aln coord
	    for (int i = alnFrom; i < alnTo; i++){
	      if(sequence[i] != gapChar){
		numBase ++;
		if(firstBase){
		  firstBase = false;
		  featStartSeqPos = m_AV->GetSeqPosFromAlnPos(row, i);
		  const CCdregion& cdr = feat->GetData().GetCdregion();
		  int frame = 1;
		  if(cdr.IsSetFrame()){
		    frame = cdr.GetFrame();
		  }
		  int numBaseFromFeatStart = (featStartSeqPos - (featSeqFrom + (frame -1) )); //Number of bases between feature start and current base. adjust using frame
		  if(numBaseFromFeatStart % 3 == 0){ //this base is the 1st base
		    marginAdjuster = 0; //aa aligned to 2nd base of a condon
		    firstFeatStringPos = numBaseFromFeatStart / 3;  
		  } else if (numBaseFromFeatStart % 3 == 1) {
		    marginAdjuster = 1;
		    firstFeatStringPos = numBaseFromFeatStart / 3;
		  } else {
		    marginAdjuster = -1;
		    firstFeatStringPos = (numBaseFromFeatStart / 3) + 1;
		  }
		}
		if((numBase + marginAdjuster) % 3 == 2){
		  int stringPos = firstFeatStringPos + (numBase + marginAdjuster) / 3;
		  if(stringPos < (int)rawCdrProduct.size()){//should not need this check
		    line[i] = rawCdrProduct[stringPos];
		  }
		}
	      }
	    }
	    alternativeFeatStr = line;
	  }
	} 

	setFeatureInfo(featInfo, loc, alnFrom, alnTo, alnStop, featChar, featId, alternativeFeatStr);     
	feature.push_back(featInfo);
      
      }
    }
  }
}

void  CDisplaySeqalign::setFeatureInfo(alnFeatureInfo* featInfo, const CSeq_loc& seqloc, int alnFrom, int alnTo, int alnStop, char patternChar, string patternId, string& alternativeFeatStr) const{
  FeatureInfo* feat = new FeatureInfo;
  feat->seqloc = &seqloc;
  feat->featureChar = patternChar;
  feat->featureId = patternId;
    
  if(alternativeFeatStr != NcbiEmptyString){
    featInfo->featureString = alternativeFeatStr;
  } else {
    //fill feature string
    string line(alnStop+1, ' ');
    for (int j = alnFrom; j <= alnTo; j++){
      line[j] = feat->featureChar;
    }
    featInfo->featureString = line;
  }
 
  featInfo->alnRange.Set(alnFrom, alnTo); 
  featInfo->feature = feat;
}

//May need to add a "|" to the current insert for insert on next rows
static int addBar(string& seq, int insertAlnPos, int alnStart){
  int end = seq.size() -1 ;
  int barPos = insertAlnPos - alnStart + 1;
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

//Add new insert seq to the current insert seq and return the end position of the latest insert
static int adjustInsert(string& curInsert, string& newInsert, int insertAlnPos, int alnStart){
  int insertEnd = 0;
  int curInsertSize = curInsert.size();
  int insertLeftSpace = insertAlnPos - alnStart - curInsertSize + 2;  //plus2 because insert is put after the position
  if(curInsertSize > 0){
    assert(insertLeftSpace >= 2);
  }
  int newInsertSize = newInsert.size();  
  if(insertLeftSpace - newInsertSize >= 1){ //can insert with the end position right below the bar
    string spacer(insertLeftSpace - newInsertSize, ' ');
    curInsert += spacer + newInsert;
    
  } else { //Need to insert beyond the insert postion
    if(curInsertSize > 0){
      curInsert += " " + newInsert;
    } else {  //can insert right at the firt position
      curInsert += newInsert;
    }
  }
  insertEnd = alnStart + curInsert.size() -1 ; //-1 back to string position
  return insertEnd;
}

//recusively fill the insert
void CDisplaySeqalign::doFills(int row, CAlnMap::TSignedRange& alnRange, int  alnStart, list<insertInformation*>& insertList, list<string>& inserts) const {
  if(!insertList.empty()){
    string bar(alnRange.GetLength(), ' ');
    
    string seq;
    list<insertInformation*> leftOverInsertList;
    bool isFirstInsert = true;
    int curInsertAlnStart = 0;
    int prvsInsertAlnEnd = 0;
    
    //go through each insert and fills the seq if it can  be filled on the same line.  If not, go to the next line
    for(list<insertInformation*>::iterator iter = insertList.begin(); iter != insertList.end(); iter ++){
      curInsertAlnStart = (*iter)->alnStart;
      //always fill the first insert.  Also fill if there is enough space
      if(isFirstInsert || curInsertAlnStart - prvsInsertAlnEnd >= 1){
	bar[curInsertAlnStart-alnStart+1] = '|';  
	int seqStart = (*iter)->seqStart;
	int seqEnd = seqStart + (*iter)->insertLen - 1;
	string newInsert;
	newInsert = m_AV->GetSeqString(newInsert, row, seqStart, seqEnd);
	prvsInsertAlnEnd = adjustInsert(seq, newInsert, curInsertAlnStart, alnStart);
	isFirstInsert = false;
      } else { //if no space, save the chunk and go to next line 
	bar[curInsertAlnStart-alnStart+1] = '|';  //indicate insert goes to the next line
	prvsInsertAlnEnd += addBar(seq, curInsertAlnStart, alnStart);   //May need to add a bar after the current insert sequence to indicate insert goes to the next line.
	leftOverInsertList.push_back(*iter);    
      }
    }
    //save current insert.  Note that each insert has a bar and sequence below it
    inserts.push_back(bar);
    inserts.push_back(seq);
    //here recursively fill the chunk that don't have enough space
    doFills(row, alnRange, alnStart, leftOverInsertList, inserts);
  }
 
}

/*fill a list of inserts for a particular row*/
void CDisplaySeqalign::fillInserts(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<string>& inserts, string& insertPosString, list<insertInformation*>& insertList) const{

  string line(alnRange.GetLength(), ' ');
 
  ITERATE(list<insertInformation*>, iter, insertList){
    int from = (*iter)->alnStart;
    line[from - alnStart + 1] = '\\';
  }
  insertPosString = line; //this is the line with "\" right after each insert position
    
  //here fills the insert sequence
  doFills(row, alnRange, alnStart, insertList, inserts);
}

void CDisplaySeqalign::GetInserts(list<insertInformation*>& insertList, CAlnMap::TSeqPosList& insertAlnStart, CAlnMap::TSeqPosList& insertSeqStart, CAlnMap::TSeqPosList& insertLength, int lineAlnStop){

  while(!insertAlnStart.empty() && (int)insertAlnStart.front() < lineAlnStop){
    CDisplaySeqalign::insertInformation* insert = new CDisplaySeqalign::insertInformation;
    insert->alnStart = insertAlnStart.front() - 1; //Need to minus one as we are inserting after this position
    insert->seqStart = insertSeqStart.front();
    insert->insertLen = insertLength.front();
    insertList.push_back(insert);
    insertAlnStart.pop_front();
    insertSeqStart.pop_front();
    insertLength.pop_front();
  }
 
}


//segments starts and stops used for map viewer 
string CDisplaySeqalign::getSegs(int row) const {
  string segs = NcbiEmptyString;
  if(m_AlignOption & eMultiAlign){ //only show this hsp
    segs = NStr::IntToString(m_AV->GetSeqStart(row)) + "-" + NStr::IntToString(m_AV->GetSeqStop(row));
  } else { //for all segs
    string idString = m_AV->GetSeqId(1).GetSeqIdString();
    map<string, string>::const_iterator iter = m_Segs.find(idString);
    if ( iter != m_Segs.end() ){
      segs = iter->second;
    }
  }
  return segs;
}



/* transforms a string so that it becomes safe to be used as part of URL
 * the function converts characters with special meaning (such as
 * semicolon -- protocol separator) to escaped hexadecimal (%xx)
 */
static string MakeURLSafe(char* src){
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

//make url for dumpgnl.cgi
string CDisplaySeqalign::getDumpgnlLink(const list<CRef<CSeq_id> >& ids, int row, const string& alternativeUrl)const {
  string link = NcbiEmptyString;  
  string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
  string passwd = m_Reg->Get(m_BlastType, "PASSWD");
  bool nodb_path =  false;
  CRef<CSeq_id> idGeneral = GetSeqIdByType(ids, CSeq_id::e_General);
  CRef<CSeq_id> idOther = GetSeqIdByType(ids, CSeq_id::e_Other);
  const CRef<CSeq_id> idAccession = FindBestChoice(ids, CSeq_id::WorstRank);
  string segs = getSegs(row);
  int gi = GetGiForSeqIdList(ids);
  if(!idGeneral.Empty() && idGeneral->AsFastaString().find("gnl|BL_ORD_ID")){ /* We do need to make security protected link to BLAST gnl */
    return NcbiEmptyString;
}
  if(alternativeUrl != NcbiEmptyString){ 
    toolUrl = alternativeUrl;
  }
  /* dumpgnl.cgi need to use path  */
  if (toolUrl.find("dumpgnl.cgi") ==string::npos){
    nodb_path = true;
  }  
  int length = m_DbName.size();
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
       if(isspace(dbname[i]) || dbname[i] == ',') {/* Rolling spaces */
	 continue;
       }
       j = 0;
       while (!isspace(dbname[i]) && j < 256  && i < length) { 
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
  }
  /*
   * Need to protect start and stop positions
   * to avoid web users sending us hand-made URLs
   * to retrive full sequences
   */
  char gnl[256];
  unsigned char buf[32];
  CMD5 urlHash;
  if (bestid && bestid->Which() !=  CSeq_id::e_Gi){
    length = passwd.size();
    urlHash.Update(passwd.c_str(), length);
    strcpy(gnl, bestid->AsFastaString().c_str());
    urlHash.Update(gnl, strlen(gnl));
    urlHash.Update(segs.c_str(), segs.size());
    urlHash.Update(passwd.c_str(), length);
    urlHash.Finalize(buf);
    
  } else {
    gnl[0] = '\0';
  }
  
  str = MakeURLSafe(dbtmp == NULL ? (char*) "nr" : dbtmp);
  link += "<a href=\"";
  if (toolUrl.find("?") == string::npos){
    link += toolUrl + "?" + "db=" + str + "&na=" + (m_IsDbNa? "1" : "0") + "&";
  } else {
    link += toolUrl + "&db=" + str + "&na=" + (m_IsDbNa? "1" : "0") + "&";
  }
  
  if (gnl[0] != '\0'){
    str = MakeURLSafe(gnl);
    link += "gnl=";
    link += str;
    link += "&";
  }
  if (gi > 0){
    link += "gi=" + NStr::IntToString(gi) + "&";
  }
  if (m_Rid != NcbiEmptyString){
    link += "RID=" + m_Rid +"&";
  }
  
  if ( m_QueryNumber > 0){
    link += "QUERY_NUMBER=" + NStr::IntToString(m_QueryNumber) + "&";
  }
  link += "segs=" + segs + "&";
  
  char tempBuf[128];
  
  sprintf(tempBuf,
	  "seal=%02X%02X%02X%02X"
	  "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
	  buf[0], buf[1], buf[2], buf[3],
	  buf[4], buf[5], buf[6], buf[7],
	  buf[8], buf[9], buf[10], buf[11],
	  buf[12], buf[13], buf[14], buf[15]);
  
  link += tempBuf;
  link += "\">";
  if(nodb_path){
    delete [] dbtmp;
  }
  delete [] dbname;
  return link;
}

void CDisplaySeqalign::setDbGi() {
  //determine if the database has gi by looking at the 1st hit.  Could be wrong but simple for now
 
  CTypeConstIterator<CSeq_align> saTemp = ConstBegin(*m_SeqalignSetRef);
  if(saTemp->IsSetSegs()){ 
    if(saTemp->GetSegs().Which() == CSeq_align::C_Segs::e_Denseg){
      CTypeConstIterator<CDense_seg> dsTemp = ConstBegin(*saTemp); 
      const vector< CRef< CSeq_id > >& idTemp = (dsTemp->GetIds());
      vector< CRef< CSeq_id > >::const_iterator iterTemp = idTemp.begin();
      iterTemp++;
      const CBioseq_Handle& handleTemp = m_Scope.GetBioseqHandle(**iterTemp);
      if(handleTemp){
	int giTemp = GetGiForSeqIdList(handleTemp.GetBioseq().GetId());
	if(giTemp >0 ) { 
	  m_IsDbGi = true;
	}
      }
    } else if (saTemp->GetSegs().Which() == CSeq_align::C_Segs::e_Std){
      CTypeConstIterator<CStd_seg> dsTemp = ConstBegin(*saTemp); 
      const list< CRef< CSeq_id > >& idTemp = (dsTemp->GetIds());
      list< CRef< CSeq_id > >::const_iterator iterTemp = idTemp.begin();
      iterTemp++;
      const CBioseq_Handle& handleTemp = m_Scope.GetBioseqHandle(**iterTemp);
      if(handleTemp){
	int giTemp = GetGiForSeqIdList(handleTemp.GetBioseq().GetId());
	if(giTemp >0 ) { 
	  m_IsDbGi = true;
	}
      }
    }
  }

}
//Need to call this if the seqalign is stdseg or dendiag for ungapped blast alignment display as each stdseg ro dendiag is a distinct alignment.  Don't call it for other case as it's a waste of time.
CRef<CSeq_align_set>CDisplaySeqalign::PrepareBlastUngappedSeqalign(CSeq_align_set& alnset) {
  CRef<CSeq_align_set> alnSetRef(new CSeq_align_set);

  ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()){
    const CSeq_align::TSegs& seg = (*iter)->GetSegs();
    if(seg.Which() == CSeq_align::C_Segs::e_Std){
      if(seg.GetStd().size() > 1){ //has more than one stdseg. Need to seperate as each is a distinct HSP
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
      if(seg.GetDendiag().size() > 1){ //has more than one dendiag. Need to seperate as each is a distinct HSP
	ITERATE (CSeq_align::C_Segs::TDendiag, iterDendiag, seg.GetDendiag()){
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

void CDisplaySeqalign::x_DisplayAlnvecList(CNcbiOstream& out, list<alnInfo*>& avList) {
  bool isFirstAlnInList = true;
  for(list<alnInfo*>::iterator iterAv = avList.begin(); iterAv != avList.end(); iterAv ++){
    m_AV = (*iterAv)->alnVec;
    const CBioseq_Handle& bsp_handle=m_AV->GetBioseqHandle(1); 
    if(isFirstAlnInList && (m_AlignOption&eShowBlastInfo)) {
      PrintDefLine(bsp_handle,  out);
      out<<"          Length="<<bsp_handle.GetBioseq().GetInst().GetLength()<<endl<<endl;
      
    }
    
    if (m_AlignOption&eShowBlastInfo) {
      
      out<<" Score = "<<(*iterAv)->bits<<" ";
      out<<"bits ("<<(*iterAv)->score<<"),"<<"  ";
      out<<"Expect = "<<(*iterAv)->eValue<<endl;
    }
    DisplayAlnvec(out);
    out<<endl;
    isFirstAlnInList = false;
  }
  
}

END_SCOPE(objects)
END_NCBI_SCOPE

/* 
*============================================================
*$Log$
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
