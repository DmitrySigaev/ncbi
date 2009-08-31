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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <math.h>
#include <hash_map.h>
#include <algo/align/ngalign/sequence_set.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <algo/align/util/score_builder.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>

#include <objtools/readers/fasta.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>

#include <algo/dustmask/symdust.hpp>

using namespace ncbi;
using namespace objects;
using namespace blast;
using namespace std;



CBlastDbSet::CBlastDbSet(const string& BlastDb) : m_BlastDb(BlastDb)
{
    ;
}


CSearchDatabase::TFilteringAlgorithms& CBlastDbSet::SetSoftFiltering()
{
    return m_Filters;
}


CRef<IQueryFactory> CBlastDbSet::CreateQueryFactory(
            CRef<CScope> Scope, CConstRef<CBlastOptionsHandle> BlastOpts)
{
    NCBI_THROW(CException, CException::eInvalid,
                    "CreateQueryFactory is not supported for type BlastDb");
    return CRef<IQueryFactory>();
}


CRef<CLocalDbAdapter> CBlastDbSet::CreateLocalDbAdapter(
            CRef<CScope> Scope, CConstRef<CBlastOptionsHandle> BlastOpts)
{
    if(m_BlastDb.empty()) {
        NCBI_THROW(CException, CException::eInvalid, "CBLastDb::CreateLocalDbAdapter: BlastDb is empty.");
    }

    CRef<CSearchDatabase> SearchDb;
    SearchDb.Reset(new CSearchDatabase(m_BlastDb, CSearchDatabase::eBlastDbIsNucleotide));

    if(!m_Filters.empty()) {
        SearchDb->SetFilteringAlgorithm(m_Filters.front());
    }

    CRef<CLocalDbAdapter> Result;
    Result.Reset(new CLocalDbAdapter(*SearchDb));
    return Result;
}



CSeqIdListSet::CSeqIdListSet() : m_SeqMasker(NULL)
{
    ;
}


list<CRef<CSeq_id> >& CSeqIdListSet::SetIdList()
{
    return m_SeqIdList;
}


void CSeqIdListSet::SetSeqMasker(CSeqMasker* SeqMasker)
{
    m_SeqMasker = SeqMasker;
}


CRef<CSeq_loc> s_GetMaskLoc(const CSeq_id& Id, CSeqMasker* SeqMasker, CRef<CScope> Scope)
{
    auto_ptr<CSeqMasker::TMaskList> Masks, DustMasks;

    CBioseq_Handle Handle = Scope->GetBioseqHandle(Id);
    CSeqVector Vector = Handle.GetSeqVector(Handle.eCoding_Iupac, Handle.eStrand_Plus);


    CSymDustMasker DustMasker;

    try {
        Masks.reset((*SeqMasker)(Vector));
        DustMasks = DustMasker(Vector);
    } catch(CException& e) {
        ERR_POST(Info << "CSeqIdListSet::CreateQueryFactory Masking Failure: " << e.ReportAll());
        return CRef<CSeq_loc>();
    }

    if(!DustMasks->empty()) {
        copy(DustMasks->begin(), DustMasks->end(),
            insert_iterator<CSeqMasker::TMaskList>(*Masks, Masks->end()));
    }

    if(Masks->empty()) {
        return CRef<CSeq_loc>();
    }

    CRef<CSeq_loc> MaskLoc(new CSeq_loc);
    ITERATE(CSeqMasker::TMaskList, IntIter, *Masks) {
        CSeq_loc IntLoc;
        IntLoc.SetInt().SetId().Assign(Id);
        IntLoc.SetInt().SetFrom() = IntIter->first;
        IntLoc.SetInt().SetTo() = IntIter->second;
        MaskLoc->Add(IntLoc);
    }

    MaskLoc = sequence::Seq_loc_Merge(*MaskLoc, CSeq_loc::fSortAndMerge_All, Scope);
    MaskLoc->ChangeToPackedInt();

    return MaskLoc;
}

CRef<IQueryFactory> CSeqIdListSet::CreateQueryFactory(
            CRef<CScope> Scope, CConstRef<CBlastOptionsHandle> BlastOpts)
{
    if(m_SeqIdList.empty()) {
        NCBI_THROW(CException, CException::eInvalid, "CSeqIdListSet::CreateQueryFactory: Id List is empty.");
    }


    TSeqLocVector FastaLocVec;
    ITERATE(list<CRef<CSeq_id> >, IdIter, m_SeqIdList) {

        if(m_SeqMasker == NULL) {
            CRef<CSeq_loc> WholeLoc(new CSeq_loc);
            WholeLoc->SetWhole().Assign(**IdIter);
            SSeqLoc WholeSLoc(*WholeLoc, *Scope);
            FastaLocVec.push_back(WholeSLoc);
        } else {

            CRef<CSeq_loc> WholeLoc(new CSeq_loc), MaskLoc;
            WholeLoc->SetWhole().Assign(**IdIter);

            MaskLoc = s_GetMaskLoc(**IdIter, m_SeqMasker, Scope);

            if(MaskLoc.IsNull() /* || Vec.size() < 100*/ ) {
                SSeqLoc WholeSLoc(*WholeLoc, *Scope);
                FastaLocVec.push_back(WholeSLoc);
            } else {
                SSeqLoc MaskSLoc(*WholeLoc, *Scope, *MaskLoc);
                FastaLocVec.push_back(MaskSLoc);
            }
        }
    }

    CRef<IQueryFactory> Result(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<CLocalDbAdapter> CSeqIdListSet::CreateLocalDbAdapter(
            CRef<CScope> Scope, CConstRef<CBlastOptionsHandle> BlastOpts)
{
    if(m_SeqIdList.empty()) {
        NCBI_THROW(CException, CException::eInvalid, "CSeqIdListSet::CreateLocalDbAdapter: Id List is empty.");
    }

    CRef<CLocalDbAdapter> Result;
    CRef<IQueryFactory> QueryFactory = CreateQueryFactory(Scope, BlastOpts);
    Result.Reset(new CLocalDbAdapter(QueryFactory, BlastOpts));
    return Result;
}



CFastaFileSet::CFastaFileSet(CNcbiIstream* FastaStream)
            : m_FastaStream(FastaStream), m_LowerCaseMasking(true)
{
    ;
}


void CFastaFileSet::EnableLowerCaseMasking(bool LowerCaseMasking)
{
    m_LowerCaseMasking = LowerCaseMasking;
}


CRef<IQueryFactory> CFastaFileSet::CreateQueryFactory(
            CRef<CScope> Scope, CConstRef<CBlastOptionsHandle> BlastOpts)
{
    if(m_FastaStream == NULL) {
        NCBI_THROW(CException, CException::eInvalid, "CFastaFileSet::CreateQueryFactory: Fasta Stream is NULL.");
    }

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);
    CFastaReader FastaReader(*m_FastaStream);
    Scope->AddTopLevelSeqEntry(*(FastaReader.ReadSet()));

    SDataLoaderConfig LoaderConfig(false);
    CBlastInputSourceConfig InputConfig(LoaderConfig);
    InputConfig.SetLowercaseMask(m_LowerCaseMasking);
    InputConfig.SetBelieveDeflines(true);

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);
    CBlastFastaInputSource FastaSource(*m_FastaStream, InputConfig);
    const EProgram kProgram = eBlastn;
    CBlastInput Input(&FastaSource, GetQueryBatchSize(kProgram));

    TSeqLocVector FastaLocVec = Input.GetAllSeqLocs(*Scope);

    m_FastaStream->clear();
    m_FastaStream->seekg(0, std::ios::beg);

    CRef<IQueryFactory> Result(new CObjMgr_QueryFactory(FastaLocVec));
    return Result;
}


CRef<CLocalDbAdapter> CFastaFileSet::CreateLocalDbAdapter(
            CRef<CScope> Scope, CConstRef<CBlastOptionsHandle> BlastOpts)
{
    CRef<CLocalDbAdapter> Result;
    CRef<IQueryFactory> QueryFactory = CreateQueryFactory(Scope, BlastOpts);
    Result.Reset(new CLocalDbAdapter(QueryFactory, BlastOpts));
    return Result;
}







//end
