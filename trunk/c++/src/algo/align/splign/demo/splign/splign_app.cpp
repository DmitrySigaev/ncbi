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
 * Author:  Yuri Kapustin
 *
 * File Description: Splign application
 *                   
*/

#include "splign_app.hpp"
#include "splign_app_exception.hpp"
#include "seq_loader.hpp"

#include <corelib/ncbistd.hpp>
#include <algo/align/nw_spliced_aligner16.hpp>
#include <algo/align/nw_spliced_aligner32.hpp>
#include <algo/align/splign.hpp>
#include <algo/align/splign_formatter.hpp>

#include <iostream>
#include <memory>


BEGIN_NCBI_SCOPE

void CSplignApp::Init()
{
  HideStdArgs( fHideLogfile | fHideConffile | fHideVersion);
  
  auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);

  string program_name ("Splign v.1.03");
#ifdef GENOME_PIPELINE
  program_name += 'p';
#endif
  argdescr->SetUsageContext(GetArguments().GetProgramName(), program_name);

  argdescr->AddKey
    ("index", "index",
     "Batch mode index file (use -mkidx to generate).",
     CArgDescriptions::eString);

  argdescr->AddFlag ("mkidx", "Generate batch mode index and quit.", true);
  
  argdescr->AddKey
    ("hits", "hits",
     "Batch mode hit file. "
     "This file defines the set of sequences to align and "
     "is also used to guide alignments.",
     CArgDescriptions::eString);

  argdescr->AddDefaultKey
    ("log", "log", "Splign log file",
     CArgDescriptions::eString, "splign.log");

  argdescr->AddDefaultKey
    ("quality", "quality", "Genomic sequence quality.", CArgDescriptions::eString, "high");
  
  argdescr->AddDefaultKey
    ("strand", "strand", "Spliced sequence's strand.", CArgDescriptions::eString, "plus");

  argdescr->AddFlag ("noendgaps",
		     "Skip detection of unaligning regions at the ends.",
                      true);

  argdescr->AddFlag ("nopolya", "Assume no Poly(A) tail.",  true);

  argdescr->AddDefaultKey
    ("compartment_penalty", "compartment_penalty",
     "Penalty to open a new compartment.",
     CArgDescriptions::eDouble,
     "0.75");
 
 argdescr->AddDefaultKey
    ("min_query_cov", "min_query_cov",
     "Min query coverage by initial Blast hits. "
     "Queries with less coverage will not be aligned.",
     CArgDescriptions::eDouble, "0.25");

  argdescr->AddDefaultKey
    ("min_idty", "identity",
     "Minimal exon identity. Lower identity segments "
     "will be marked as gaps.",
     CArgDescriptions::eDouble, "0.75");

#ifdef GENOME_PIPELINE

  argdescr->AddDefaultKey
    ("Wm", "match", "match score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWm()).c_str());
  
  argdescr->AddDefaultKey
    ("Wms", "mismatch", "mismatch score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWms()).c_str());
  
  argdescr->AddDefaultKey
    ("Wg", "gap", "gap opening score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWg()).c_str());
  
  argdescr->AddDefaultKey
    ("Ws", "space", "gap extension (space) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWs()).c_str());
  
  argdescr->AddDefaultKey
    ("Wi0", "Wi0", "Conventional intron (GT/AG) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(0)).c_str());
  
  argdescr->AddDefaultKey
    ("Wi1", "Wi1", "Conventional intron (GC/AG) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(1)).c_str());
  
  argdescr->AddDefaultKey
    ("Wi2", "Wi2", "Conventional intron (AT/AC) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(2)).c_str());
  
  argdescr->AddDefaultKey
    ("Wi3", "Wi3", "Arbitrary intron score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(3)).c_str());
  
#endif
    
  // restrictions

  CArgAllow_Strings* constrain_errlevel = new CArgAllow_Strings;
  constrain_errlevel->Allow("low")->Allow("high");
  argdescr->SetConstraint("quality", constrain_errlevel);

  CArgAllow_Strings* constrain_strand = new CArgAllow_Strings;
  constrain_strand->Allow("plus")->Allow("minus");
  argdescr->SetConstraint("strand", constrain_strand);

  CArgAllow* constrain01 = new CArgAllow_Doubles(0,1);
  argdescr->SetConstraint("min_idty", constrain01);
  argdescr->SetConstraint("compartment_penalty", constrain01);
  argdescr->SetConstraint("min_query_cov", constrain01);
    
  SetupArgDescriptions(argdescr.release());
}


bool CSplignApp::x_GetNextPair(ifstream* ifs, vector<CHit>* hits)
{
  hits->clear();
  if(!m_pending.size() && (!ifs || !*ifs) ) {
    return false;
  }

  if(!m_pending.size()) {

    string query, subj;
    if(m_firstline.size()) {
      CHit hit (m_firstline.c_str());
      query = hit.m_Query;
      subj  = hit.m_Subj;
      m_pending.push_back(hit);
    }

    char buf [1024];
    while(ifs) {
      buf[0] = 0;
      CT_POS_TYPE pos0 = ifs->tellg();
      ifs->getline(buf, sizeof buf, '\n');
      CT_POS_TYPE pos1 = ifs->tellg();
      if(pos1 == pos0) break; // GCC hack
      if(buf[0] == '#') continue; // skip comments
      const char* p = buf; // skip leading spaces
      while(*p == ' ' || *p == '\t') ++p;
      if(*p == 0) continue; // skip empty lines

      CHit hit (p);
      if(query.size() == 0) {
	query = hit.m_Query;
      }
      if(subj.size() == 0) {
	subj = hit.m_Subj;
      }
      if(hit.m_ai[0] > hit.m_ai[1]) {
        NCBI_THROW(CSplignAppException,
                   eBadData,
                   "Hit with reversed query coordinates detected." );
      }
      if(hit.m_ai[2] == hit.m_ai[3]) { // skip single bases
        continue;
      }

      if(hit.m_Query != query || hit.m_Subj != subj) {
	m_firstline = p;
	break;
      }

      m_pending.push_back(hit);
    }
  }

  const size_t pending_size = m_pending.size();
  if(pending_size) {
    const string& query = m_pending[0].m_Query;
    const string& subj  = m_pending[0].m_Subj;
    size_t i = 1;
    for(; i < pending_size; ++i) {
      const CHit& h = m_pending[i];
      if(h.m_Query != query || h.m_Subj != subj) {
        break;
      }
    }
    hits->resize(i);
    copy(m_pending.begin(), m_pending.begin() + i, hits->begin());
    m_pending.erase(m_pending.begin(), m_pending.begin() + i);
  }

  return hits->size() > 0;
}


void CSplignApp::x_LogStatus(size_t model_id, const string& query,
                             const string& subj,
			     bool error, const string& msg)
{
  string error_tag (error? "Error: ": "");
  if(model_id == 0) {
    m_logstream << '-';
  }
  else {
    m_logstream << model_id;
  }

  m_logstream << '\t' << query << '\t' << subj 
              << '\t' << error_tag << msg << endl;
}


int CSplignApp::Run()
{ 
  const CArgs& args = GetArgs();    

  // open log stream

  m_logstream.open( args["log"].AsString().c_str() );

  // prepare input hit stream

  auto_ptr<fstream> hit_stream;
  const string filename (args["hits"].AsString());
  hit_stream.reset((fstream*)new ifstream (filename.c_str()));

  vector<CHit> hits;
  ifstream* ifs_hits = (ifstream*) (hit_stream.get());

  // setup splign and formatter objects

  CSplign splign;

  CRef<CSplicedAligner> aligner ( args["quality"].AsString() == "high" ?
    static_cast<CSplicedAligner*> (new CSplicedAligner16):
    static_cast<CSplicedAligner*> (new CSplicedAligner32) );

  splign.SetPolyaDetection(!args["nopolya"]);
  splign.SetMinExonIdentity(args["min_idty"].AsDouble());
  splign.SetCompartmentPenalty(args["compartment_penalty"].AsDouble());
  splign.SetMinQueryCoverage(args["min_query_cov"].AsDouble());
  splign.SetStrand(args["strand"].AsString() == "plus");
  splign.SetEndGapDetection(!(args["noendgaps"]));

#if GENOME_PIPELINE

  aligner->SetWm(args["Wm"].AsInteger());
  aligner->SetWms(args["Wms"].AsInteger());
  aligner->SetWg(args["Wg"].AsInteger());
  aligner->SetWs(args["Ws"].AsInteger());

  for(size_t i = 0, n = aligner->GetSpliceTypeCount(); i < n; ++i) {
      string arg_name ("Wi");
      arg_name += NStr::IntToString(i);
      aligner->SetWi(i, args[arg_name.c_str()].AsInteger());
  }

#endif

  splign.SetAligner(aligner);

  CSeqLoader* pseqloader = new CSeqLoader;
  pseqloader->Open(args["index"].AsString());
  CRef<CSplignSeqAccessor> seq_loader (pseqloader);
  pseqloader = 0;
  splign.SetSeqAccessor(seq_loader);

  CSplignFormatter formatter (splign);

  splign.SetStartModelId(1);

  while(x_GetNextPair(ifs_hits, &hits) ) {

    if(hits.size() == 0) {
      continue;
    }

    const string query (hits[0].m_Query);
    const string subj (hits[0].m_Subj);

    splign.Run(&hits);

    cout << formatter.AsText();

    ITERATE(vector<CSplign::SAlignedCompartment>, ii, splign.GetResult()) {
      x_LogStatus(ii->m_id, query, subj, ii->m_error, ii->m_msg);
    }
    if(splign.GetResult().size() == 0) {
      x_LogStatus(0, query, subj, true, "No feasible compartment found");
    }
  }

  return 0;
}


END_NCBI_SCOPE
                     

USING_NCBI_SCOPE;


// make Splign index file
void MakeIDX( istream* inp_istr, const size_t file_index, ostream* out_ostr )
{
  istream * inp = inp_istr? inp_istr: &cin;
  ostream * out = out_ostr? out_ostr: &cout;
  inp->unsetf(IOS_BASE::skipws);
  char c0 = '\n', c;
  while(inp->good()) {
    c = inp->get();
    if(c0 == '\n' && c == '>') {
      CT_OFF_TYPE pos = inp->tellg() - CT_POS_TYPE(1);
      string s;
      *inp >> s;
      *out << s << '\t' << file_index << '\t' << pos << endl;
    }
    c0 = c;
  }
}



int main(int argc, const char* argv[]) 
{
  // pre-scan for mkidx
  for(int i = 1; i < argc; ++i) {
    if(0 == strcmp(argv[i], "-mkidx")) {
      
      if(i + 1 == argc) {
        char err_msg [] = 
          "ERROR: No FastA files specified to index. "
          "Please specify one or more FastA files after -mkidx. "
          "Your system may support wildcards to specify multiple files.";
	cerr << err_msg << endl;
        return 1;
      }
      else {
	++i;
      }
      vector<string> fasta_filenames;
      for(; i < argc; ++i) {
	fasta_filenames.push_back(argv[i]);
	// test
	ifstream ifs (argv[i]);
	if(ifs.fail()) {
	  cerr << "ERROR: Unable to open " << argv[i] << endl;
	  return 1;
	}
      }

      // write the list of files
      const size_t files_count = fasta_filenames.size();
      cout << "# This file was generated by Splign. Edit with care." << endl;
      cout << "$$$FI" << endl;
      for(size_t k = 0; k < files_count; ++k) {
	cout << fasta_filenames[k] << '\t' << k << endl;
      }
      cout << "$$$SI" << endl;
      for(size_t k = 0; k < files_count; ++k) {
	ifstream ifs (fasta_filenames[k].c_str());
	MakeIDX(&ifs, k, &cout);
      }

      return 0;
    }
  }

  return CSplignApp().AppMain(argc, argv, 0, eDS_Default, 0);

}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2004/04/26 19:26:22  kapustin
 * Count models from one
 *
 * Revision 1.18  2004/04/26 15:38:46  kapustin
 * Add model_id as a CSplign member
 *
 * Revision 1.17  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.15  2004/02/19 22:57:55  ucko
 * Accommodate stricter implementations of CT_POS_TYPE.
 *
 * Revision 1.14  2004/02/18 16:04:53  kapustin
 * Do not print PolyA and gap contents
 *
 * Revision 1.13  2003/12/15 20:16:58  kapustin
 * GetNextQuery() ->GetNextPair()
 *
 * Revision 1.12  2003/12/09 13:21:47  ucko
 * +<memory> for auto_ptr
 *
 * Revision 1.11  2003/12/04 20:08:22  kapustin
 * Remove endgaps argument
 *
 * Revision 1.10  2003/12/04 19:26:37  kapustin
 * Account for zero-length segment vector
 *
 * Revision 1.9  2003/12/03 19:47:02  kapustin
 * Increase exon search scope at the ends
 *
 * Revision 1.8  2003/11/26 16:13:21  kapustin
 * Determine subject after filtering for more accurate log reporting
 *
 * Revision 1.7  2003/11/21 16:04:18  kapustin
 * Enable RLE for Poly(A) tail.
 *
 * Revision 1.6  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.5  2003/11/20 14:38:10  kapustin
 * Add -nopolya flag to suppress Poly(A) detection.
 *
 * Revision 1.4  2003/11/10 19:20:26  kapustin
 * Generate encoded alignment transcript by default
 *
 * Revision 1.3  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */
