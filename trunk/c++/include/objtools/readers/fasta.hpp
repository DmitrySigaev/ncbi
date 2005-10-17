#ifndef OBJTOOLS_READERS___FASTA__HPP
#define OBJTOOLS_READERS___FASTA__HPP

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
* Authors:  Aaron Ucko, NCBI
*
* File Description:
*   Reader for FASTA-format sequences.  (The writer is CFastaOStream, in
*   <objmgr/util/sequence.hpp>.)
*
*/

#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EReadFastaFlags {
    fReadFasta_AssumeNuc  = 0x1,  // type to use if no revealing accn found
    fReadFasta_AssumeProt = 0x2,
    fReadFasta_ForceType  = 0x4,  // force type regardless of accession
    fReadFasta_NoParseID  = 0x8,  // treat name as local ID regardless of |s
    fReadFasta_ParseGaps  = 0x10, // make a delta sequence if gaps found
    fReadFasta_OneSeq     = 0x20, // just read the first sequence found
    fReadFasta_AllSeqIds  = 0x40, // read Seq-ids past the first ^A (see note)
    fReadFasta_NoSeqData  = 0x80, // parse the deflines but skip the data
    fReadFasta_RequireID  = 0x100 // reject deflines that lack IDs
};
typedef int TReadFastaFlags; // binary OR of EReadFastaFlags

// Note on fReadFasta_AllSeqIds: some databases (notably nr) have
// merged identical sequences, stringing their deflines together with
// control-As.  Normally, the reader stops at the first control-A;
// however, this flag makes it parse all the IDs.

// keeps going until EOF or parse error (-> CParseException) unless
// fReadFasta_OneSeq is set
// see also CFastaOstream in <objmgr/util/sequence.hpp> (-lxobjutil)
NCBI_XOBJREAD_EXPORT
CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags = 0,
                           int* counter = 0,
                           vector<CConstRef<CSeq_loc> >* lcv = 0);



//////////////////////////////////////////////////////////////////
//
// Class - description of multi-entry FASTA file,
// to keep list of offsets on all molecules in the file.
//
struct SFastaFileMap
{
    struct SFastaEntry
    {
        SFastaEntry() : stream_offset(0) {}
        /// List of qll sequence ids
        typedef list<string>  TFastaSeqIds;

        string         seq_id;        ///< Primary sequence Id
        string         description;   ///< Molecule description
        CNcbiStreampos stream_offset; ///< Molecule offset in file
        TFastaSeqIds   all_seq_ids;   ///< List of all seq.ids
    };

    typedef vector<SFastaEntry>  TMapVector;

    TMapVector   file_map; // vector keeps list of all molecule entries
};

/// Callback interface to scan fasta file for entries
class NCBI_XOBJREAD_EXPORT IFastaEntryScan
{
public:
    virtual ~IFastaEntryScan();

    /// Callback function, called after reading the fasta entry
    virtual void EntryFound(CRef<CSeq_entry> se, 
                            CNcbiStreampos stream_position) = 0;
};


/// Function reads input stream (assumed that it is FASTA format) one
/// molecule entry after another filling the map structure describing and
/// pointing on molecule entries. Fasta map can be used later for quick
/// CSeq_entry retrival
void NCBI_XOBJREAD_EXPORT ReadFastaFileMap(SFastaFileMap* fasta_map, 
                                           CNcbiIfstream& input);

/// Scan FASTA files, call IFastaEntryScan::EntryFound (payload function)
void NCBI_XOBJREAD_EXPORT ScanFastaFile(IFastaEntryScan* scanner, 
                                        CNcbiIfstream&   input,
                                        TReadFastaFlags  fread_flags);



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2005/10/17 12:19:35  rsmith
* initialize streampos members.
*
* Revision 1.9  2005/09/29 19:35:51  kuznets
* Added callback based fasta reader
*
* Revision 1.8  2005/09/26 15:14:54  kuznets
* Added list of ids to fasta map
*
* Revision 1.7  2005/09/23 12:41:56  kuznets
* Minor comment change
*
* Revision 1.6  2004/11/24 19:27:42  ucko
* +fReadFasta_RequireID
*
* Revision 1.5  2004/01/20 16:27:53  ucko
* Fix a stray reference to sequence.hpp's old location.
*
* Revision 1.4  2003/08/08 21:31:37  dondosha
* Changed type of lcase_mask in ReadFasta to vector of CConstRefs
*
* Revision 1.3  2003/08/07 21:12:56  ucko
* Support a counter for assigning local IDs to sequences with no ID given.
*
* Revision 1.2  2003/08/06 19:08:28  ucko
* Slight interface tweak to ReadFasta: report lowercase locations in a
* vector with one entry per Bioseq rather than a consolidated Seq_loc_mix.
*
* Revision 1.1  2003/06/04 17:26:08  ucko
* Split out from Seq_entry.hpp.
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_READERS___FASTA__HPP */
