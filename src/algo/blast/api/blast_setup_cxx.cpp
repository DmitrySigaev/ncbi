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
* Author:  Christiam Camacho
*
* File Description:
*   Auxiliary setup functions for Blast objects interface
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbifile.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIstdaa.hpp>

#include <algo/blast/api/blast_exception.hpp>
#include "blast_setup.hpp"

// NewBlast includes
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blastkar.h>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(ncbi::objects);


#define LAST2BITS 0x03

// Compresses sequence data on vector to buffer, which should have been
// allocated and have the right size.
static void PackDNA(const CSeqVector& vec, Uint1* buffer, const int buflen)
{
    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence

    ASSERT(vec.GetCoding() == CSeq_data::e_Ncbi2na);

    for (ci = 0, i = 0; ci < (TSeqPos) buflen-1; ci++, i += COMPRESSION_RATIO) {
        buffer[ci] = ((vec[i+0] & LAST2BITS)<<6) |
                     ((vec[i+1] & LAST2BITS)<<4) |
                     ((vec[i+2] & LAST2BITS)<<2) |
                     ((vec[i+3] & LAST2BITS)<<0);
    }

    buffer[ci] = 0;
    for (; i < vec.size(); i++) {
            Uint1 bit_shift = 0;
            switch (i%COMPRESSION_RATIO) {
               case 0: bit_shift = 6; break;
               case 1: bit_shift = 4; break;
               case 2: bit_shift = 2; break;
               default: abort();   // should never happen
            }
            buffer[ci] |= ((vec[i] & LAST2BITS)<<bit_shift);
    }
    buffer[ci] |= vec.size()%COMPRESSION_RATIO;    // Number of bases in the last byte.
   
}

Uint1*
BLASTGetSequence(const CSeq_loc& sl, Uint1 encoding, int* len, CScope* scope,
        ENa_strand strand, bool add_nucl_sentinel)
{
    Uint1* buf,* buf_var;       // buffers to write sequence
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    Uint1 sentinel;             // sentinel byte

    CBioseq_Handle handle = scope->GetBioseqHandle(sl);
    if (!handle) {
        ERR_POST(Error << "Could not retrieve bioseq_handle");
        return NULL;
    }

    // Retrieves the correct strand (plus or minus), but not both
    CSeqVector sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi, strand);

    switch (encoding) {
    // Protein sequences (query & subject) always have sentinels around sequence
    case BLASTP_ENCODING:
        sentinel = NULLB;
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        buflen = sv.size()+2;
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        *buf_var++ = sentinel;
        for (i = 0; i < sv.size(); i++)
            *buf_var++ = sv[i];
        *buf_var++ = sentinel;
        break;

    case NCBI4NA_ENCODING:
    case BLASTNA_ENCODING: // Used for nucleotide blastn queries
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        sentinel = 0xF;
        buflen = add_nucl_sentinel ? sv.size() + 2 : sv.size();
        if (strand == eNa_strand_both)
            buflen = add_nucl_sentinel ? buflen * 2 - 1 : buflen * 2;

        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        if (add_nucl_sentinel)
            *buf_var++ = sentinel;
        for (i = 0; i < sv.size(); i++) {
            if (encoding == BLASTNA_ENCODING)
                *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
            else
                *buf_var++ = sv[i];
        }
        if (add_nucl_sentinel)
            *buf_var++ = sentinel;

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                    eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi4na);
            for (i = 0; i < sv.size(); i++) {
                if (encoding == BLASTNA_ENCODING)
                    *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
                else
                    *buf_var++ = sv[i];
            }
            if (add_nucl_sentinel)
                *buf_var++ = sentinel;
        }
        break;

    // Used only in Blast2Sequences for the subject sequence. No sentinels are
    // required. As in readdb, remainder (sv.size()%4 != 0) goes in the last 
    // 2 bits of the last byte.
    case NCBI2NA_ENCODING:
        ASSERT(add_nucl_sentinel == false);
        sv.SetCoding(CSeq_data::e_Ncbi2na);
        buflen = (sv.size()/COMPRESSION_RATIO) + 1;
        if (strand == eNa_strand_both)
            buflen *= 2;

        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        PackDNA(sv, buf_var, (strand == eNa_strand_both) ? buflen/2 : buflen);

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                    eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi2na);
            buf_var += buflen/2;
            PackDNA(sv, buf_var, buflen/2);
        }
        break;

    default:
        ERR_POST(Error << "Invalid encoding " << encoding);
        return NULL;
    }

	if (len) {
		*len = buflen;
	}

    return buf;
}

#if 0
// Not used right now, need to complete implementation
void
BLASTGetTranslation(const Uint1* seq, const Uint1* seq_rev,
        const int nucl_length, const short frame, Uint1* translation)
{
    TSeqPos ni = 0;     // index into nucleotide sequence
    TSeqPos pi = 0;     // index into protein sequence

    const Uint1* nucl_seq = frame >= 0 ? seq : seq_rev;
    translation[0] = NULLB;
    for (ni = ABS(frame)-1; ni < (TSeqPos) nucl_length-2; ni += CODON_LENGTH) {
        Uint1 residue = CGen_code_table::CodonToIndex(nucl_seq[ni+0], 
                                                      nucl_seq[ni+1],
                                                      nucl_seq[ni+2]);
        if (IS_residue(residue))
            translation[pi++] = residue;
    }
    translation[pi++] = NULLB;

    return;
}
#endif

unsigned char*
BLASTFindGeneticCode(int genetic_code)
{
    unsigned char* retval = NULL;
    CSeq_data gc_ncbieaa(CGen_code_table::GetNcbieaa(genetic_code),
            CSeq_data::e_Ncbieaa);
    CSeq_data gc_ncbistdaa;

    TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
            CSeq_data::e_Ncbistdaa);

    ASSERT(gc_ncbistdaa.IsNcbistdaa());
    ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

    try {
        retval = new unsigned char[nconv];
    } catch (bad_alloc& ba) {
        return NULL;
    }

    for (unsigned int i = 0; i < nconv; i++)
        retval[i] = gc_ncbistdaa.GetNcbistdaa().Get()[i];

    return retval;
}

char* 
BLASTGetMatrixPath(const char* matrix_name, bool is_prot)
{
    char* retval = NULL, *p = NULL;
    string full_path;       // full path to matrix file

    if (!matrix_name)
        return NULL;

    string mtx(matrix_name);
    transform(mtx.begin(), mtx.end(), mtx.begin(), (int (*)(int))toupper);

    // Look for matrix file in local directory
    full_path = mtx;
    if (CFile(full_path).Exists()) {
        return retval;
    }

    // Obtain the matrix path from the ncbi configuration file
    CMetaRegistry::SEntry sentry;
    sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    string path = sentry.registry ? sentry.registry->Get("NCBI", "Data") : "";

    full_path = CFile::MakePath(path, mtx);
    if (CFile(full_path).Exists()) {
        retval = strdup(full_path.c_str());
        p = strstr(retval, matrix_name);
        *p = NULLB;
        return retval;
    }

    // Try appending "aa" or "nt" 
    full_path = path;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = strdup(full_path.c_str());
        p = strstr(retval, matrix_name);
        *p = NULLB;
        return retval;
    }

    // Try using local "data" directory
    full_path = "data";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = strdup(full_path.c_str());
        p = strstr(retval, matrix_name);
        *p = NULLB;
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app)
        return NULL;

    const string& blastmat_env = app->GetEnvironment().Get("BLASTMAT");
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += is_prot ? "aa" : "nt";
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = strdup(full_path.c_str());
            p = strstr(retval, matrix_name);
            *p = NULLB;
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = strdup(full_path.c_str());
        p = strstr(retval, matrix_name);
        *p = NULLB;
        return retval;
    }
#endif

    // Try again without the "aa" or "nt"
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = strdup(full_path.c_str());
            p = strstr(retval, matrix_name);
            *p = NULLB;
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = strdup(full_path.c_str());
        p = strstr(retval, matrix_name);
        *p = NULLB;
        return retval;
    }
#endif

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.20  2003/09/05 19:06:31  camacho
* Use regular new to allocate genetic code string
*
* Revision 1.19  2003/09/03 19:36:27  camacho
* Fix include path for blast_setup.hpp
*
* Revision 1.18  2003/08/28 22:42:54  camacho
* Change BLASTGetSequence signature
*
* Revision 1.17  2003/08/28 15:49:02  madden
* Fix for packing DNA as well as correct buflen
*
* Revision 1.16  2003/08/25 16:24:14  camacho
* Updated BLASTGetMatrixPath
*
* Revision 1.15  2003/08/19 17:39:07  camacho
* Minor fix to use of metaregistry class
*
* Revision 1.14  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.13  2003/08/14 13:51:24  camacho
* Use CMetaRegistry class to load the ncbi config file
*
* Revision 1.12  2003/08/11 15:17:39  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.11  2003/08/11 14:00:41  dicuccio
* Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
* BEGIN_NCBI_SCOPE block)
*
* Revision 1.10  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.9  2003/08/04 15:18:23  camacho
* Minor fixes
*
* Revision 1.8  2003/08/01 22:35:02  camacho
* Added function to get matrix path (fixme)
*
* Revision 1.7  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.4  2003/07/25 13:55:58  camacho
* Removed unnecessary #includes
*
* Revision 1.3  2003/07/24 18:22:50  camacho
* #include blastkar.h
*
* Revision 1.2  2003/07/23 21:29:06  camacho
* Update BLASTFindGeneticCode to get genetic code string with C++ toolkit
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
