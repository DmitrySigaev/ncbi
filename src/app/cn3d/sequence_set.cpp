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
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to hold sets of sequences
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.43  2002/01/11 15:49:01  thiessen
* update for Mac CW7
*
* Revision 1.42  2001/11/30 14:59:55  thiessen
* add netscape launch for Mac
*
* Revision 1.41  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.40  2001/11/27 16:26:09  thiessen
* major update to data management system
*
* Revision 1.39  2001/09/27 15:37:59  thiessen
* decouple sequence import and BLAST
*
* Revision 1.38  2001/08/21 01:10:31  thiessen
* fix lit. launch for nucleotides
*
* Revision 1.37  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.36  2001/07/24 15:02:59  thiessen
* use ProSite syntax for pattern searches
*
* Revision 1.35  2001/07/23 20:09:23  thiessen
* add regex pattern search
*
* Revision 1.34  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.33  2001/07/10 16:39:55  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.32  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.31  2001/06/18 21:48:27  thiessen
* add [PACC] to Entrez query for PDB id's
*
* Revision 1.30  2001/06/02 17:22:46  thiessen
* fixes for GCC
*
* Revision 1.29  2001/05/31 18:47:09  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.28  2001/05/31 14:32:03  thiessen
* better netscape launch for unix
*
* Revision 1.27  2001/05/25 01:38:16  thiessen
* minor fixes for compiling on SGI
*
* Revision 1.26  2001/05/24 13:32:32  thiessen
* further tweaks for GTK
*
* Revision 1.25  2001/05/15 23:48:37  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.24  2001/05/02 16:35:15  thiessen
* launch entrez web page on sequence identifier
*
* Revision 1.23  2001/04/20 18:03:22  thiessen
* add ncbistdaa parsing
*
* Revision 1.22  2001/04/18 15:46:53  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.21  2001/03/28 23:02:17  thiessen
* first working full threading
*
* Revision 1.20  2001/02/16 00:40:01  thiessen
* remove unused sequences from asn data
*
* Revision 1.19  2001/02/13 01:03:57  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.18  2001/02/08 23:01:50  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.17  2001/01/25 20:21:18  thiessen
* fix ostrstream memory leaks
*
* Revision 1.16  2001/01/09 21:45:00  thiessen
* always use pdbID as title if known
*
* Revision 1.15  2001/01/04 18:20:52  thiessen
* deal with accession seq-id
*
* Revision 1.14  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.13  2000/12/28 20:43:09  vakatov
* Do not use "set" as identifier.
* Do not include <strstream>. Use "CNcbiOstrstream" instead of "ostrstream".
*
* Revision 1.12  2000/12/21 23:42:15  thiessen
* load structures from cdd's
*
* Revision 1.11  2000/12/20 23:47:48  thiessen
* load CDD's
*
* Revision 1.10  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.9  2000/11/17 19:48:13  thiessen
* working show/hide alignment row
*
* Revision 1.8  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.7  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.6  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.5  2000/08/30 23:46:27  thiessen
* working alignment display
*
* Revision 1.4  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.3  2000/08/28 23:47:19  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.2  2000/08/28 18:52:42  thiessen
* start unpacking alignments
*
* Revision 1.1  2000/08/27 18:52:22  thiessen
* extract sequence information
*
* ===========================================================================
*/

#if defined(__WXMSW__)
#include <windows.h>
#include <shellapi.h>   // for ShellExecute, needed to launch browser

#elif defined(__WXGTK__)
#include <unistd.h>

#elif defined(__WXMAC__)
#include <Files.h>      // stuff for sending URL apple event
#include <Processes.h>
#include <AppleEvents.h>
#include <Gestalt.h>
#include <Navigation.h>
#include <Folders.h>

#endif

#include <corelib/ncbistd.hpp> // must come first to avoid NCBI type clashes
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/PDB_block.hpp>

#include <regex.h>  // regex from C-toolkit

#include <memory>

#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static void UnpackSeqSet(const CBioseq_set& bss, SequenceSet *parent, SequenceSet::SequenceList& seqlist)
{
    CBioseq_set::TSeq_set::const_iterator q, qe = bss.GetSeq_set().end();
    for (q=bss.GetSeq_set().begin(); q!=qe; q++) {
        if (q->GetObject().IsSeq()) {

            // only store amino acid or nucleotide sequences
            if (q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_aa &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_dna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_rna &&
                q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_na)
                continue;

            const Sequence *sequence = new Sequence(parent, q->GetObject().SetSeq());
            seqlist.push_back(sequence);

        } else { // Bioseq-set
            UnpackSeqSet(q->GetObject().GetSet(), parent, seqlist);
        }
    }
}

static void UnpackSeqEntry(CSeq_entry& seqEntry, SequenceSet *parent, SequenceSet::SequenceList& seqlist)
{
    if (seqEntry.IsSeq()) {
        const Sequence *sequence = new Sequence(parent, seqEntry.SetSeq());
        seqlist.push_back(sequence);
    } else { // Bioseq-set
        UnpackSeqSet(seqEntry.GetSet(), parent, seqlist);
    }
}

SequenceSet::SequenceSet(StructureBase *parent, const SeqEntryList& seqEntries) :
    StructureBase(parent)
{
    SeqEntryList::const_iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; s++)
        UnpackSeqEntry(s->GetObject(), this, sequences);
    TESTMSG("number of sequences: " << sequences.size());
}

#define FIRSTOF2(byte) (((byte) & 0xF0) >> 4)
#define SECONDOF2(byte) ((byte) & 0x0F)

static void StringFrom4na(const std::vector< char >& vec, std::string *str, bool isDNA)
{
    if (SECONDOF2(vec.back()) > 0)
        str->resize(vec.size() * 2);
    else
        str->resize(vec.size() * 2 - 1);

    // first, extract 4-bit values
    int i;
    for (i=0; i<vec.size(); i++) {
        str->at(2*i) = FIRSTOF2(vec[i]);
        if (SECONDOF2(vec[i]) > 0) str->at(2*i + 1) = SECONDOF2(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); i++) {
        switch (str->at(i)) {
            case 1: str->at(i) = 'A'; break;
            case 2: str->at(i) = 'C'; break;
            case 4: str->at(i) = 'G'; break;
            case 8: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
            default:
                str->at(i) = 'X';
        }
    }
}

#define FIRSTOF4(byte) (((byte) & 0xC0) >> 6)
#define SECONDOF4(byte) (((byte) & 0x30) >> 4)
#define THIRDOF4(byte) (((byte) & 0x0C) >> 2)
#define FOURTHOF4(byte) ((byte) & 0x03)

static void StringFrom2na(const std::vector< char >& vec, std::string *str, bool isDNA)
{
    str->resize(vec.size() * 4);

    // first, extract 4-bit values
    int i;
    for (i=0; i<vec.size(); i++) {
        str->at(4*i) = FIRSTOF4(vec[i]);
        str->at(4*i + 1) = SECONDOF4(vec[i]);
        str->at(4*i + 2) = THIRDOF4(vec[i]);
        str->at(4*i + 3) = FOURTHOF4(vec[i]);
    }

    // then convert 4-bit values to ascii characters
    for (i=0; i<str->size(); i++) {
        switch (str->at(i)) {
            case 0: str->at(i) = 'A'; break;
            case 1: str->at(i) = 'C'; break;
            case 2: str->at(i) = 'G'; break;
            case 3: isDNA ? str->at(i) = 'T' : str->at(i) = 'U'; break;
        }
    }
}

static void StringFromStdaa(const std::vector < char >& vec, std::string *str)
{
    static const char *stdaaMap = "-ABCDEFGHIKLMNPQRSTVWXYZU*";

    str->resize(vec.size());
    for (int i=0; i<vec.size(); i++)
        str->at(i) = stdaaMap[vec[i]];
}

Sequence::Sequence(StructureBase *parent, ncbi::objects::CBioseq& bioseq) :
    StructureBase(parent), bioseqASN(&bioseq), molecule(NULL), isProtein(false)
{
    int gi = MoleculeIdentifier::VALUE_NOT_SET, mmdbID = MoleculeIdentifier::VALUE_NOT_SET,
        pdbChain = MoleculeIdentifier::VALUE_NOT_SET;
    std::string pdbID, accession;

    // get Seq-id info
    CBioseq::TId::const_iterator s, se = bioseq.GetId().end();
    for (s=bioseq.GetId().begin(); s!=se; s++) {
        if (s->GetObject().IsGi()) {
            gi = s->GetObject().GetGi();
        } else if (s->GetObject().IsPdb()) {
            pdbID = s->GetObject().GetPdb().GetMol().Get();
            if (s->GetObject().GetPdb().IsSetChain())
                pdbChain = s->GetObject().GetPdb().GetChain();
            else
                pdbChain = ' ';
        } else if (s->GetObject().IsLocal() && s->GetObject().GetLocal().IsStr()) {
            accession = s->GetObject().GetLocal().GetStr();
            // special case where local accession is actually a PDB chain
            if (pdbID.size() == 0 && accession.size() >= 6 && accession[4] == ' ' && isalpha(accession[5])) {
                pdbID = accession.substr(0, 4);
                pdbChain = accession[5];
            }
        } else if (s->GetObject().IsGenbank() && s->GetObject().GetGenbank().IsSetAccession()) {
            accession = s->GetObject().GetGenbank().GetAccession();
        } else if (s->GetObject().IsSwissprot() && s->GetObject().GetSwissprot().IsSetAccession()) {
            accession = s->GetObject().GetSwissprot().GetAccession();
        }
    }
    if (gi == MoleculeIdentifier::VALUE_NOT_SET && pdbID.size() == 0 && accession.size() == 0) {
        ERR_POST(Error << "Sequence::Sequence() - can't parse SeqId");
        return;
    }

    // try to get description from title or compound
    if (bioseq.IsSetDescr()) {
        CSeq_descr::Tdata::const_iterator d, de = bioseq.GetDescr().Get().end();
        for (d=bioseq.GetDescr().Get().begin(); d!=de; d++) {
            if (d->GetObject().IsTitle()) {
                description = d->GetObject().GetTitle();
                break;
            } else if (d->GetObject().IsPdb() && d->GetObject().GetPdb().GetCompound().size() > 0) {
                description = d->GetObject().GetPdb().GetCompound().front();
                break;
            }
        }
    }

    // get link to MMDB id - mainly for CDD's where Biostrucs have to be loaded separately
    if (bioseq.IsSetAnnot()) {
        CBioseq::TAnnot::const_iterator a, ae = bioseq.GetAnnot().end();
        for (a=bioseq.GetAnnot().begin(); a!=ae; a++) {
            if (a->GetObject().GetData().IsIds()) {
                CSeq_annot::C_Data::TIds::const_iterator i, ie = a->GetObject().GetData().GetIds().end();
                for (i=a->GetObject().GetData().GetIds().begin(); i!=ie; i++) {
                    if (i->GetObject().IsGeneral() &&
                        i->GetObject().GetGeneral().GetDb() == "mmdb" &&
                        i->GetObject().GetGeneral().GetTag().IsId()) {
                        mmdbID = i->GetObject().GetGeneral().GetTag().GetId();
                        break;
                    }
                }
                if (i != ie) break;
            }
        }
    }
    if (mmdbID != MoleculeIdentifier::VALUE_NOT_SET)
        TESTMSG("sequence gi " << gi << ", PDB '" << pdbID << "' chain '" << (char) pdbChain <<
            "', is from MMDB id " << mmdbID);

    // get sequence string
    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_raw && bioseq.GetInst().IsSetSeq_data()) {

        // protein formats
        if (bioseq.GetInst().GetSeq_data().IsNcbieaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetNcbieaa().Get();
            isProtein = true;
        } else if (bioseq.GetInst().GetSeq_data().IsIupacaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();
            isProtein = true;
        } else if (bioseq.GetInst().GetSeq_data().IsNcbistdaa()) {
            StringFromStdaa(bioseq.GetInst().GetSeq_data().GetNcbistdaa().Get(), &sequenceString);
            isProtein = true;
        }

        // nucleotide formats
        else if (bioseq.GetInst().GetSeq_data().IsIupacna()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacna().Get();
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi4na()) {
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi4na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi8na()) {  // same repr. for non-X as 4na
            StringFrom4na(bioseq.GetInst().GetSeq_data().GetNcbi8na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
        } else if (bioseq.GetInst().GetSeq_data().IsNcbi2na()) {
            StringFrom2na(bioseq.GetInst().GetSeq_data().GetNcbi2na().Get(), &sequenceString,
                (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna));
            if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() < sequenceString.length())
                sequenceString.resize(bioseq.GetInst().GetLength());
        }

        else {
            ERR_POST(Critical << "Sequence::Sequence() - sequence " << gi
                << ": confused by sequence string format");
            return;
        }

        // check length
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != sequenceString.length()) {
            ERR_POST(Critical << "Sequence::Sequence() - sequence string length mismatch");
            return;
        }

        // force uppercase
        for (int i=0; i<sequenceString.length(); i++)
            sequenceString[i] = toupper(sequenceString[i]);

    } else {
        ERR_POST(Critical << "Sequence::Sequence() - sequence " << gi
                << ": confused by sequence representation");
        return;
    }

    // get identifier
    identifier = MoleculeIdentifier::GetIdentifier(this, pdbID, pdbChain, mmdbID, gi, accession);
}

CSeq_id * Sequence::CreateSeqId(void) const
{
    CSeq_id *sid = new CSeq_id();
    FillOutSeqId(sid);
    return sid;
}

void Sequence::FillOutSeqId(ncbi::objects::CSeq_id *sid) const
{
    if (identifier->pdbID.size() > 0 && identifier->pdbChain != MoleculeIdentifier::VALUE_NOT_SET) {
        sid->SetPdb().SetMol().Set(identifier->pdbID);
        if (identifier->pdbChain != ' ') sid->SetPdb().SetChain(identifier->pdbChain);
    } else if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) { // use gi
        sid->SetGi(identifier->gi);
    } else if (identifier->accession.size() > 0) {
        CRef<CObject_id> oid(new CObject_id());
        oid->SetStr(identifier->accession);
        sid->SetLocal(oid);
    } else {
        ERR_POST(Error << "Sequence::FillOutSeqId() - can't do Seq-id on sequence "
            << identifier->ToString());
    }
}

void Sequence::AddCSeqId(SeqIdPtr *id, bool addAllTypes) const
{
    if (identifier->pdbID.size() > 0) {
        PDBSeqIdPtr pdbid = PDBSeqIdNew();
        pdbid->mol = StrSave(identifier->pdbID.c_str());
        pdbid->chain = (Uint1) identifier->pdbChain;
        ValNodeAddPointer(id, SEQID_PDB, pdbid);
        if (!addAllTypes) return;
    }
    if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
        ValNodeAddInt(id, SEQID_GI, identifier->gi);
        if (!addAllTypes) return;
    }
    if (identifier->accession.size() > 0) {
        TextSeqIdPtr gbid = TextSeqIdNew();
        gbid->accession = StrSave(identifier->accession.c_str());
        ValNodeAddPointer(id, SEQID_GENBANK, gbid);
        if (!addAllTypes) return;
    }
}

int Sequence::GetOrSetMMDBLink(void) const
{
    if (molecule) {
        const StructureObject *object;
        if (!molecule->GetParentOfType(&object)) return identifier->mmdbID;
        if (identifier->mmdbID != MoleculeIdentifier::VALUE_NOT_SET &&
            identifier->mmdbID != object->mmdbID)
            ERR_POST(Error << "Sequence::GetOrSetMMDBLink() - mismatched MMDB ID: identifier says "
                << identifier->mmdbID << ", StructureObject says " << object->mmdbID);
        else
            const_cast<MoleculeIdentifier*>(identifier)->mmdbID = object->mmdbID;
    }
    return identifier->mmdbID;
}


#ifdef __WXMSW__
// code borrowed (and modified) from Nlm_MSWin_OpenDocument() in vibutils.c
static bool MSWin_OpenDocument(const char* doc_name)
{
  int status = (int) ShellExecute(0, "open", doc_name, NULL, NULL, SW_SHOWNORMAL);
  if (status <= 32) {
    ERR_POST(Error << "Unable to open document \"" << doc_name << "\", error = " << status);
    return false;
  }
  return true;
}
#endif

#ifdef __WXMAC__
// code borrowed from vibutils.c
Nlm_Boolean Nlm_LaunchAppEx (Nlm_CharPtr fileName, Nlm_VoidPtr serialNumPtr, Nlm_CharPtr sig)
{
  OSErr                   err;
  FSSpec                  fsspec;
  long                    gval;
  LaunchParamBlockRec     myLaunchParams;
  Nlm_Boolean             okay = FALSE;
  unsigned char           pathname [256];
  ProcessSerialNumberPtr  psnp;
  Nlm_CharPtr             ptr;
  Nlm_Boolean             rsult;
  OSType                  theSignature;
  ProcessSerialNumber     psn;
  ProcessInfoRec          pir;
  Nlm_Boolean             found;
  DTPBRec                 dtpb;
  short                   ioDTRefNum;
  long                    dirID;
  short                   vRefNum;
  long                    ioAPPLParID;

  rsult = FALSE;
  if (Gestalt (gestaltSystemVersion, &gval) == noErr && (short) gval >= 7 * 256) {
    if (fileName != NULL && fileName [0] != '\0') {
      if (Nlm_StringChr (fileName, DIRDELIMCHR) != NULL) {
        Nlm_StringNCpy_0((Nlm_CharPtr)pathname, fileName, sizeof(pathname));
      } else {
        Nlm_ProgramPath ((Nlm_CharPtr) pathname, sizeof (pathname));
        ptr = Nlm_StringRChr ((Nlm_CharPtr) pathname, DIRDELIMCHR);
        if (ptr != NULL) {
          *ptr = '\0';
        }
        Nlm_FileBuildPath ((Nlm_CharPtr) pathname, NULL, fileName);
      }
      Nlm_CtoPstr ((Nlm_CharPtr) pathname);
      err = FSMakeFSSpec (0, 0, pathname, &(fsspec));
      okay = (Nlm_Boolean) (err == noErr);
    } else if (sig != NULL && sig [0] != '\0') {
      theSignature = *(OSType*) sig;
      psn.highLongOfPSN = 0;
      psn.lowLongOfPSN = kNoProcess;
      found = FALSE;
      while (GetNextProcess (&psn) != noErr) {
        if (GetProcessInformation (&psn, &pir) != noErr) {
          if (pir.processSignature == theSignature) {
            found = TRUE;
          }
        }
      }
      if (found) return TRUE; /* no need to launch if already running */
      if (! found) {
        err = FindFolder (kOnSystemDisk, kTemporaryFolderType,
                          kCreateFolder, &vRefNum, &dirID);
        if (err == noErr) {
          memset (&dtpb, 0, sizeof (DTPBRec));
          dtpb.ioNamePtr = NULL;
          dtpb.ioVRefNum = vRefNum;
          err = PBDTGetPath (&dtpb);
          if (err == noErr) {
            ioDTRefNum = dtpb.ioDTRefNum;
            if (ioDTRefNum != 0) {
              memset (&dtpb, 0, sizeof (DTPBRec));
              dtpb.ioDTRefNum = ioDTRefNum;
              dtpb.ioFileCreator = theSignature;
              dtpb.ioNamePtr = (StringPtr) pathname;
              dtpb.ioIndex = 0;
              memset (&pathname, 0, sizeof (pathname));
              err = PBDTGetAPPL (&dtpb, FALSE);
              if (err == noErr) {
                ioAPPLParID = dtpb.ioAPPLParID;
                if (pathname [0] != '\0') {
                  err = FSMakeFSSpec (vRefNum, ioAPPLParID,
                                      (ConstStr255Param) pathname, &fsspec);
                  okay = (Nlm_Boolean) (err == noErr);
                }
              }
            }
          }
        }
      }
    }
    if (okay) {
      myLaunchParams.launchBlockID = extendedBlock;
      myLaunchParams.launchEPBLength = extendedBlockLen;
      myLaunchParams.launchFileFlags = 0;
      myLaunchParams.launchControlFlags = launchContinue + launchNoFileFlags;
      myLaunchParams.launchAppParameters = NULL;
      myLaunchParams.launchAppSpec = &fsspec;
      rsult = (Nlm_Boolean) (LaunchApplication (&myLaunchParams) == noErr);
      if (serialNumPtr != NULL) {
        psnp = (ProcessSerialNumberPtr) serialNumPtr;
        *psnp = myLaunchParams.launchProcessSN;
      }
      SetFrontProcess (&myLaunchParams.launchProcessSN);
    }
  }
  return rsult;
}

void Nlm_SendURLAppleEvent (Nlm_CharPtr urlString, Nlm_CharPtr sig, Nlm_CharPtr prog)
{
  long gval;
  OSErr theErr = noErr;
  AEAddressDesc theAddress;
  AEDesc urlEvent;
  AppleEvent theEvent;
  AppleEvent theReply;
  OSType theSignature;
  ProcessSerialNumber  psn;
  Nlm_Boolean  okay = FALSE;

  if (Gestalt (gestaltSystemVersion, &gval) == noErr && (short) gval >= 7 * 256) {
    theSignature = 'RSML';
    if (sig != NULL && sig [0] != '\0') {
      theSignature = *(OSType*) sig;
    }
    if (prog != NULL && prog [0] != '\0') {
      if (Nlm_LaunchAppEx (prog, (Nlm_VoidPtr) &psn, NULL)) {
        theErr = AECreateDesc(typeProcessSerialNumber, (Ptr)&psn,
                              sizeof(psn), &theAddress);
        okay = (Nlm_Boolean) (theErr == noErr);
      }
    } else if (sig != NULL && sig [0] != '\0') {
      if (Nlm_LaunchAppEx (prog, NULL, sig)) {
        theSignature = *(OSType*) sig;
        theErr = AECreateDesc(typeApplSignature, (Ptr)&theSignature,
                              sizeof(theSignature), &theAddress);
        okay = (Nlm_Boolean) (theErr == noErr);
      }
    }
    if (okay) {
      theErr = AECreateAppleEvent('WWW!', 'OURL', &theAddress,
                                  kAutoGenerateReturnID, kAnyTransactionID, &theEvent);
      if (theErr == noErr) {
        theErr = AECreateDesc(typeChar, urlString, Nlm_StringLen (urlString), &urlEvent);
        if (theErr == noErr) {
          theErr = AEPutParamDesc(&theEvent, keyDirectObject, &urlEvent);
          if (theErr == noErr) {
            AESend (&theEvent, &theReply, kAEWaitReply, kAEHighPriority, 500, NULL, NULL);
            AEDisposeDesc(&theReply);
          }
          AEDisposeDesc(&urlEvent);
        }
        AEDisposeDesc(&theEvent);
      }
      AEDisposeDesc(&theAddress);
    }
  }
}
#endif

// code borrowed (and modified a lot) from Nlm_LaunchWebPage in bspview.c
void LaunchWebPage(const char *url)
{
    if(!url) return;
    TESTMSG("launching url " << url);

#if defined(__WXMSW__)
    if (!MSWin_OpenDocument(url)) {
        ERR_POST(Error << "Unable to launch browser");
    }

#elif defined(__WXGTK__)
    CNcbiOstrstream oss;
    oss << "( netscape -raise -remote 'openURL(" << url << ")' || netscape '" << url
        << "' ) >/dev/null 2>&1 &" << '\0';
    system(oss.str());
    delete oss.str();
    
#elif defined(__WXMAC__)
    Nlm_SendURLAppleEvent (url, "MOSS", NULL);
#endif
}

void Sequence::LaunchWebBrowserWithInfo(void) const
{
    char dbChar = isProtein ? 'P' : 'N';
    CNcbiOstrstream oss;
    oss << "http://www.ncbi.nlm.nih.gov/entrez/utils/qmap.cgi?form=6&Dopt=g&db="
        << dbChar << "&uid=";
    if (identifier->pdbID.size() > 0) {
        oss << identifier->pdbID.c_str();
        if (identifier->pdbChain != ' ')
            oss << (char) identifier->pdbChain;
        oss << "%5B" << dbChar << "ACC%5D";
    } else if (identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
        oss << identifier->gi;
    } else if (identifier->accession.size() > 0) {
        if (identifier->accession == "query" || identifier->accession == "consensus") return;
        oss << identifier->accession.c_str();
    }
    oss << '\0';
    LaunchWebPage(oss.str());
    delete oss.str();
}

static bool Prosite2Regex(const std::string& prosite, std::string *regex, int *nGroups)
{
    try {
        // check allowed characters
        static const std::string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.";
        int i;
        for (i=0; i<prosite.size(); i++)
            if (allowed.find(toupper(prosite[i])) == std::string::npos) break;
        if (i != prosite.size()) throw "invalid ProSite character";
        if (prosite[prosite.size() - 1] != '.') throw "ProSite pattern must end with '.'";

        // translate into real regex syntax;
        regex->erase();
        *nGroups = 0;

        bool inGroup = false;
        for (int i=0; i<prosite.size(); i++) {

            // handle grouping and termini
            bool characterHandled = true;
            switch (prosite[i]) {
                case '-': case '.': case '>':
                    if (inGroup) {
                        *regex += ')';
                        inGroup = false;
                    }
                    if (prosite[i] == '>') *regex += '$';
                    break;
                case '<':
                    *regex += '^';
                    break;
                default:
                    characterHandled = false;
                    break;
            }
            if (characterHandled) continue;
            if (!inGroup && (
                    (isalpha(prosite[i]) && toupper(prosite[i]) != 'X') ||
                    prosite[i] == '[' || prosite[i] == '{')) {
                *regex += '(';
                (*nGroups)++;
                inGroup = true;
            }

            // translate syntax
            switch (prosite[i]) {
                case '(':
                    *regex += '{';
                    break;
                case ')':
                    *regex += '}';
                    break;
                case '{':
                    *regex += "[^";
                    break;
                case '}':
                    *regex += ']';
                    break;
                case 'X': case 'x':
                    *regex += '.';
                    break;
                default:
                    *regex += toupper(prosite[i]);
                    break;
            }
        }
    }

    catch (const char *err) {
        ERR_POST(Error << "Prosite2Regex() - " << err);
        return false;
    }

    return true;
}

bool Sequence::HighlightPattern(const std::string& prositePattern) const
{
    // setup regex syntax
    reg_syntax_t newSyntax = RE_CONTEXT_INDEP_ANCHORS | RE_CONTEXT_INVALID_OPS | RE_INTERVALS |
        RE_LIMITED_OPS | RE_NO_BK_BRACES | RE_NO_BK_PARENS | RE_NO_EMPTY_RANGES;
    reg_syntax_t oldSyntax = re_set_syntax(newSyntax);

    bool retval = true;
    try {
        // allocate structures
        static re_pattern_buffer *patternBuffer = NULL;
        static re_registers *registers = NULL;
        if (!patternBuffer) {
            // new pattern initialized to zero
            patternBuffer = (re_pattern_buffer *) calloc(1, sizeof(re_pattern_buffer));
            if (!patternBuffer) throw "can't allocate pattern buffer";
            patternBuffer->fastmap = (char *) calloc(256, sizeof(char));
            if (!patternBuffer->fastmap) throw "can't allocate fastmap";
            registers = (re_registers *) calloc(1, sizeof(re_registers));
            if (!registers) throw "can't allocate registers";
            patternBuffer->regs_allocated = REGS_UNALLOCATED;
        }

        // update pattern buffer if not the same pattern as before
        static std::string previousPrositePattern;
        static int nGroups;
        int i;
        if (prositePattern != previousPrositePattern) {

            // convert from ProSite syntax
            std::string regexPattern;
            if (!Prosite2Regex(prositePattern, &regexPattern, &nGroups))
                throw "error converting ProSite to regex syntax";

            // create pattern buffer
            TESTMSG("compiling pattern '" << regexPattern << "'");
            const char *error = re_compile_pattern(regexPattern.c_str(), regexPattern.size(), patternBuffer);
            if (error) throw error;

            // optimize pattern buffer
            int err = re_compile_fastmap(patternBuffer);
            if (err) throw "re_compile_fastmap internal error";

            previousPrositePattern = prositePattern;
        }

        // do the search, finding all non-overlapping matches
        int start = 0;
        while (start < sequenceString.size()) {

            int result = re_search(patternBuffer, sequenceString.c_str(),
                sequenceString.size(), start, sequenceString.size(), registers);
            if (result == -1)
                break;
            else if (result == -2)
                throw "re_search internal error";

            // re_search gives the longest hit, but we want the shortest; so try to find the
            // shortest hit within the hit already found by limiting the length of the string
            // allowed to be included in the search. (This isn't very efficient! but
            // the regex API doesn't have an option for finding the shortest hit...)
            int stringSize = start + 1;
            while (stringSize <= sequenceString.size()) {
                result = re_search(patternBuffer, sequenceString.c_str(),
                    stringSize, start, stringSize, registers);
                if (result >= 0) break;
                stringSize++;
            }

            // parse the match registers, highlight ranges
//            TESTMSG("found match starting at " << identifier->ToString() << " loc " << result+1);
            int lastMatched = result;
            for (i=1; i<registers->num_regs; i++) {
                int from = registers->start[i], to = registers->end[i] - 1;
                if (from >= 0 && to >= 0) {
                    if (to > lastMatched) lastMatched = to;

                    // highlight this ranage
//                    TESTMSG("register " << i << ": from " << from+1 << " to " << to+1);
                    GlobalMessenger()->AddHighlights(this, from, to);
                }
            }

            start = lastMatched + 1;
        }

    } catch (const char *err) {
        ERR_POST(Error << "Sequence::HighlightPattern() - " << err);
        retval = false;
    }

    // cleanup
    re_set_syntax(oldSyntax);

    return retval;
}

END_SCOPE(Cn3D)
