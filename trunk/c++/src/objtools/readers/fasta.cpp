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
* Authors:  Aaron Ucko, NCBI;  Anatoliy Kuznetsov, NCBI.
*
* File Description:
*   Reader for FASTA-format sequences.  (The writer is CFastaOStream, in
*   src/objmgr/util/sequence.cpp.)
*
* ===========================================================================
*/

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <corelib/ncbiutil.hpp>
#include <util/format_guess.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Bioseq_set.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static SIZE_TYPE s_EndOfFastaID(const string& str, SIZE_TYPE pos)
{
    SIZE_TYPE vbar = str.find('|', pos);
    if (vbar == NPOS) {
        return NPOS; // bad
    }

    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(str.substr(pos, vbar - pos).c_str());

#if 1
    if (choice != CSeq_id::e_not_set) {
        SIZE_TYPE vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            if (vbar == NPOS) {
                break;
            }
            choice = CSeq_id::WhichInverseSeqId(
                str.substr(vbar_prev + 1, vbar - vbar_prev - 1).c_str());
            if (choice != CSeq_id::e_not_set) {
                vbar = vbar_prev;
                break;
            }
        }
    } else {
        return NPOS; // bad
    }
#else
    switch (choice) {
    case CSeq_id::e_Patent: case CSeq_id::e_Other: // 3 args
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Genbank:   case CSeq_id::e_Embl:    case CSeq_id::e_Pir:
    case CSeq_id::e_Swissprot: case CSeq_id::e_General: case CSeq_id::e_Ddbj:
    case CSeq_id::e_Prf:       case CSeq_id::e_Pdb:     case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:       case CSeq_id::e_Tpd:
        // 2 args
        if (vbar == NPOS) {
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Local: case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:  case CSeq_id::e_Gi:
        // 1 arg
        if (vbar == NPOS) {
            if (choice == CSeq_id::e_Other) {
                // this is acceptable - member is optional
                break;
            }
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        break;

    default: // unrecognized or not set
        return NPOS; // bad
    }
#endif

    return (vbar == NPOS) ? str.size() : vbar;
}


static void s_FixSeqData(CBioseq* seq)
{
    _ASSERT(seq);
    CSeq_inst& inst = seq->SetInst();
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_delta:
    {
        TSeqPos length = 0;
        NON_CONST_ITERATE (CDelta_ext::Tdata, it,
                           inst.SetExt().SetDelta().Set()) {
            if ((*it)->IsLiteral()) {
                CSeq_literal& lit  = (*it)->SetLiteral();
                CSeq_data&    data = lit.SetSeq_data();
                if (data.IsIupacna()) {
                    lit.SetLength(data.GetIupacna().Get().size());
                    CSeqportUtil::Pack(&data);
                } else {
                    lit.SetLength(data.GetIupacaa().Get().size());
                }
                length += lit.GetLength();
            }
        }
        break;
    }
    case CSeq_inst::eRepr_raw:
    {
        CSeq_data& data = inst.SetSeq_data();
        if (data.IsIupacna()) {
            inst.SetLength(data.GetIupacna().Get().size());
            CSeqportUtil::Pack(&data);
        } else {
            inst.SetLength(data.GetIupacaa().Get().size());
        }        
        break;
    }
    default: // especially not_set!
        break;
    }
}


static CSeq_data& s_LastData(CSeq_inst& inst)
{
    if (inst.IsSetExt()  &&  inst.GetExt().IsDelta()) {
        CDelta_ext::Tdata& delta_data = inst.SetExt().SetDelta().Set();
        if (delta_data.empty()  ||  !delta_data.back()->IsLiteral()) {
            CRef<CDelta_seq> delta_seq(new CDelta_seq);
            delta_data.push_back(delta_seq);
            return delta_seq->SetLiteral().SetSeq_data();
        } else {
            return delta_data.back()->SetLiteral().SetSeq_data();
        }
    } else {
        return inst.SetSeq_data();
    }
}


static CSeq_inst::EMol s_ParseFastaDefline(CBioseq::TId& ids, string& title,
                                           const string& line,
                                           TReadFastaFlags flags)
{
    SIZE_TYPE       start = 0;
    CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
    do {
        ++start;
        SIZE_TYPE space = line.find_first_of(" \t", start);
        string    name  = line.substr(start, space - start), local;

        if (flags & fReadFasta_NoParseID) {
            local = name;
        } else {
            // try to parse out IDs
            SIZE_TYPE pos = 0;
            while (pos < name.size()) {
                SIZE_TYPE end = s_EndOfFastaID(name, pos);
                if (end == NPOS) {
                    if (pos > 0) {
                        NCBI_THROW2(CObjReaderParseException, eFormat,
                                    "s_ParseFastaDefline: Bad ID "
                                    + name.substr(pos),
                                    pos);
                    } else {
                        local = name;
                        break;
                    }
                }

                CRef<CSeq_id> id(new CSeq_id(name.substr(pos, end - pos)));
                ids.push_back(id);
                if (mol == CSeq_inst::eMol_not_set
                    &&  !(flags & fReadFasta_ForceType)) {
                    CSeq_id::EAccessionInfo ai = id->IdentifyAccession();
                    if (ai & CSeq_id::fAcc_nuc) {
                        mol = CSeq_inst::eMol_na;
                    } else if (ai & CSeq_id::fAcc_prot) {
                        mol = CSeq_inst::eMol_aa;
                    }
                }
                pos = end + 1;
            }
        }
            
        if ( !local.empty() ) {
            ids.push_back(CRef<CSeq_id>
                          (new CSeq_id(CSeq_id::e_Local, local, kEmptyStr)));
        }

        start = line.find('\1', start);
        if (space != NPOS  &&  title.empty()) {
            title.assign(line, space + 1,
                         (start == NPOS) ? NPOS : (start - space - 1));
        }
    } while (start != NPOS  &&  (flags & fReadFasta_AllSeqIds));
    return mol;
}


static void s_GuessMol(CSeq_inst::EMol& mol, const string& data,
                       TReadFastaFlags flags, istream& in)
{
    if (mol != CSeq_inst::eMol_not_set) {
        return; // already known; no need to guess
    }

    if (mol == CSeq_inst::eMol_not_set  &&  !(flags & fReadFasta_ForceType)) {
        switch (CFormatGuess::SequenceType(data.c_str(), data.size())) {
        case CFormatGuess::eNucleotide:  mol = CSeq_inst::eMol_na;  return;
        case CFormatGuess::eProtein:     mol = CSeq_inst::eMol_aa;  return;
        default:                         break;
        }
    }

    // ForceType was set, or CFormatGuess failed, so we have to rely on
    // explicit assumptions
    if (flags & fReadFasta_AssumeNuc) {
        _ASSERT(!(flags & fReadFasta_AssumeProt));
        mol = CSeq_inst::eMol_na;
    } else if (flags & fReadFasta_AssumeProt) {
        mol = CSeq_inst::eMol_aa;
    } else { 
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: unable to deduce molecule type"
                    " from IDs, flags, or sequence",
                    in.tellg());
    }
}


CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags,
                           CSeq_loc* lowercase)
{
    CRef<CSeq_entry>       entry(new CSeq_entry);
    CBioseq_set::TSeq_set& sset  = entry->SetSet().SetSeq_set();
    CRef<CBioseq>          seq(0); // current Bioseq
    string                 line;
    TSeqPos                pos, lc_start;
    bool                   was_lc;
    CRef<CSeq_id>          best_id;

    while ( !in.eof() ) {
        if ((flags & fReadFasta_OneSeq)  &&  seq.NotEmpty()
            &&  (in.peek() == '>')) {
            break;
        }
        NcbiGetlineEOL(in, line);
        if (in.eof()  &&  line.empty()) {
            break;
        }
        if (line[0] == '>') {
            // new sequence
            if (seq) {
                s_FixSeqData(seq);
                if (lowercase  &&  was_lc) {
                    lowercase->SetMix().AddInterval(*best_id, lc_start, pos);
                }
            }
            seq = new CBioseq;
            if (flags & fReadFasta_NoSeqData) {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
            } else {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
            }
            {{
                CRef<CSeq_entry> entry2(new CSeq_entry);
                entry2->SetSeq(*seq);
                sset.push_back(entry2);
            }}
            string          title;
            CSeq_inst::EMol mol = s_ParseFastaDefline(seq->SetId(), title,
                                                      line, flags);
            if (mol == CSeq_inst::eMol_not_set
                &&  (flags & fReadFasta_NoSeqData)) {
                if (flags & fReadFasta_AssumeNuc) {
                    _ASSERT(!(flags & fReadFasta_AssumeProt));
                    mol = CSeq_inst::eMol_na;
                } else if (flags & fReadFasta_AssumeProt) {
                    mol = CSeq_inst::eMol_aa;
                }
            }
            seq->SetInst().SetMol(mol);

            if ( !title.empty() ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetTitle(title);
                seq->SetDescr().Set().push_back(desc);
            }

            if (lowercase) {
                pos     = 0;
                was_lc  = false;
                best_id = FindBestChoice(seq->GetId(), CSeq_id::Score);
            }
        } else if (line[0] == '#'  ||  line[0] == '!') {
            continue; // comment
        } else if (!(flags & fReadFasta_NoSeqData)) {
            // actual data; may contain embedded junk
            CSeq_inst& inst = seq->SetInst();
            string residues;
            for (SIZE_TYPE i = 0;  i < line.size();  ++i) {
                char c = line[i];
                if (isalpha(c)) {
                    if (lowercase) {
                        bool is_lc = islower(c) ? true : false;
                        if (is_lc && !was_lc) {
                            lc_start = pos;
                        } else if (was_lc && !is_lc) {
                            lowercase->SetMix().AddInterval
                                (*best_id, lc_start, pos);
                        }
                        was_lc = is_lc;
                        ++pos;
                    }
                    residues += (char)toupper(c);
                } else if (c == '-'  &&  (flags & fReadFasta_ParseGaps)) {
                    CDelta_ext::Tdata& d = inst.SetExt().SetDelta().Set();
                    if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
                        CRef<CDelta_seq> ds(new CDelta_seq);
                        inst.SetRepr(CSeq_inst::eRepr_delta);
                        if (inst.IsSetSeq_data()) {
                            ds->SetLiteral().SetSeq_data(inst.SetSeq_data());
                            d.push_back(ds);
                            inst.ResetSeq_data();
                        }
                    }
                    if ( !residues.empty() ) {
                        if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                            s_GuessMol(inst.SetMol(), residues, flags, in);
                        }
                        CSeq_data& data = s_LastData(inst);
                        if (inst.GetMol() == CSeq_inst::eMol_aa) {
                            data.SetIupacaa().Set() += residues;
                        } else {
                            data.SetIupacna().Set() += residues;
                        }                        
                    }
                    {{
                        CRef<CDelta_seq> gap(new CDelta_seq);
                        gap->SetLoc().SetNull();
                        d.push_back(gap);
                    }}
                } else if (c == ';') {
                    continue; // skip rest of line
                }
            }

            if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                s_GuessMol(inst.SetMol(), residues, flags, in);
            }
            
            // Add the accumulated data...
            {{
                CSeq_data& data = s_LastData(inst);
                if (inst.GetMol() == CSeq_inst::eMol_aa) {
                    if (data.IsIupacaa()) {
                        data.SetIupacaa().Set() += residues;
                    } else {
                        data.SetIupacaa().Set(residues);
                    }
                } else {
                    if (data.IsIupacna()) {
                        data.SetIupacna().Set() += residues;
                    } else {
                        data.SetIupacna().Set(residues);
                    }
                }
            }}
        }
    }

    if (seq) {
        s_FixSeqData(seq);
        if (lowercase && was_lc) {
            lowercase->SetMix().AddInterval(*best_id, lc_start, pos);
        }
    }
    // simplify if possible
    if (sset.size() == 1) {
        entry->SetSeq(*seq);
    }
    return entry;
}


void ReadFastaFileMap(SFastaFileMap* fasta_map, CNcbiIfstream& input)
{
    _ASSERT(fasta_map);

    fasta_map->file_map.resize(0);

    if (!input.is_open()) 
        return;

    while (!input.eof()) {
        SFastaFileMap::SFastaEntry  fasta_entry;
        fasta_entry.stream_offset = input.tellg();

        CRef<CSeq_entry> se;
        se = ReadFasta(input, fReadFasta_AssumeNuc | fReadFasta_OneSeq);

        if (!se->IsSeq()) 
            continue;

        const CSeq_entry::TSeq& bioseq = se->GetSeq();
        const CSeq_id* sid = bioseq.GetFirstId();
        fasta_entry.seq_id = sid->AsFastaString();
        if (bioseq.CanGetDescr()) {
            const CSeq_descr& d = bioseq.GetDescr();
            if (d.CanGet()) {
                const CSeq_descr_Base::Tdata& data = d.Get();
                if (!data.empty()) {
                    CSeq_descr_Base::Tdata::const_iterator it = 
                                                      data.begin();
                    if (it != data.end()) {
                        CRef<CSeqdesc> ref_desc = *it;
                        ref_desc->GetLabel(&fasta_entry.description, 
                                           CSeqdesc::eContent);
                    }                                
                }
            }
        }
        fasta_map->file_map.push_back(fasta_entry);
        

    } // while

}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2003/06/23 20:49:11  kuznets
* Changed to use Seq_id::AsFastaString() when reading fasta file map.
*
* Revision 1.2  2003/06/08 16:17:00  lavr
* Heed MSVC int->bool performance warning
*
* Revision 1.1  2003/06/04 17:26:22  ucko
* Split out from Seq_entry.cpp.
*
*
* ===========================================================================
*/
