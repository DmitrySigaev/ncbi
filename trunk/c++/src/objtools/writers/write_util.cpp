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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objtools/writers/write_util.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CRef<CUser_object> CWriteUtil::GetDescriptor(
    const CSeq_annot& annot,
    const string& strType )
//  ----------------------------------------------------------------------------
{
    CRef< CUser_object > pUser;
    if (!annot.IsSetDesc()) {
        return pUser;
    }

    const list<CRef<CAnnotdesc> > descriptors = annot.GetDesc().Get();
    list<CRef<CAnnotdesc> >::const_iterator it;
    for (it = descriptors.begin(); it != descriptors.end(); ++it) {
        if (!(*it)->IsUser()) {
            continue;
        }
        const CUser_object& user = (*it)->GetUser();
        if (user.GetType().GetStr() == strType) {
            pUser.Reset(new CUser_object);
            pUser->Assign(user);
            return pUser;
        }
    }    
    return pUser;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetGenomeString( 
    const CBioSource& bs,
    string& genome_str )
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { genome_str = str; return true; }

    if (!bs.IsSetGenome()) {
        return false;
    }
    switch (bs.GetGenome()) {
        default:
            return false;
        case CBioSource::eGenome_apicoplast: EMIT("apicoplast");
        case CBioSource::eGenome_chloroplast: EMIT("chloroplast");
        case CBioSource::eGenome_chromatophore: EMIT("chromatophore");
        case CBioSource::eGenome_chromoplast: EMIT("chromoplast");
        case CBioSource::eGenome_chromosome: EMIT("chromosome");
        case CBioSource::eGenome_cyanelle: EMIT("cyanelle");
        case CBioSource::eGenome_endogenous_virus: EMIT("endogenous_virus");
        case CBioSource::eGenome_extrachrom: EMIT("extrachrom");
        case CBioSource::eGenome_genomic: EMIT("genomic");
        case CBioSource::eGenome_hydrogenosome: EMIT("hydrogenosome");
        case CBioSource::eGenome_insertion_seq: EMIT("insertion_seq");
        case CBioSource::eGenome_kinetoplast: EMIT("kinetoplast");
        case CBioSource::eGenome_leucoplast: EMIT("leucoplast");
        case CBioSource::eGenome_macronuclear: EMIT("macronuclear");
        case CBioSource::eGenome_mitochondrion: EMIT("mitochondrion");
        case CBioSource::eGenome_nucleomorph: EMIT("nucleomorph");
        case CBioSource::eGenome_plasmid: EMIT("plasmid");
        case CBioSource::eGenome_plastid: EMIT("plastid");
        case CBioSource::eGenome_proplastid: EMIT("proplastid");
        case CBioSource::eGenome_proviral: EMIT("proviral");
        case CBioSource::eGenome_transposon: EMIT("transposon");
        case CBioSource::eGenome_unknown: EMIT("unknown");
        case CBioSource::eGenome_virion: EMIT("virion");
    }
}
#undef EMIT

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetIdType(
    CBioseq_Handle bsh,
    string& id_type )
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { id_type = str; return true; }

    try {
        CSeq_id_Handle best_idh = sequence::GetId(bsh, sequence::eGetId_Best);
        if ( !best_idh ) {
            best_idh = sequence::GetId(bsh, sequence::eGetId_Canonical);
        }
        switch ( best_idh.Which() ) {
            default:
                break;
            case CSeq_id::e_Local: EMIT("Local");
            case CSeq_id::e_Gibbsq:
            case CSeq_id::e_Gibbmt:
            case CSeq_id::e_Giim:
            case CSeq_id::e_Gi: EMIT("GenInfo");
            case CSeq_id::e_Genbank: EMIT("Genbank");
            case CSeq_id::e_Swissprot: EMIT("SwissProt");
            case CSeq_id::e_Patent: EMIT("Patent");
            case CSeq_id::e_Other: EMIT("RefSeq");
            case CSeq_id::e_General: 
                EMIT(best_idh.GetSeqId()->GetGeneral().GetDb());
        }
        id_type = CSeq_id::SelectionName( best_idh.Which() );
        NStr::ToUpper( id_type );
        return true;
    }
    catch(...) {
        return false;
    }
#undef EMIT
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetOrgModSubType(
    const COrgMod& mod,
    string& subtype,
    string& subname)
//  ----------------------------------------------------------------------------
{
    if (!mod.IsSetSubtype() || !mod.IsSetSubname()) {
        return false;
    }
    subtype = COrgMod::GetSubtypeName(mod.GetSubtype());
    subname = mod.GetSubname();
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetSubSourceSubType(
    const CSubSource& sub,
    string& subtype,
    string& subname)
//  ----------------------------------------------------------------------------
{
    if (!sub.IsSetSubtype() || !sub.IsSetName()) {
        return false;
    }
    subtype = CSubSource::GetSubtypeName(sub.GetSubtype());
    if (sub.GetSubtype() == CSubSource::eSubtype_environmental_sample) {
        subname = "true";
    }
    else {
        subname = sub.GetName();
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWriteUtil::GetBiomolString(
    CBioseq_Handle bsh,
    string& mol_str)
//  ----------------------------------------------------------------------------
{
#define EMIT(str) { mol_str = str; return true; }
    CSeqdesc_CI md(bsh.GetParentEntry(), CSeqdesc::e_Molinfo, 0);
    if (!md) {
        return false;
    }
    const CMolInfo& molinfo = md->GetMolinfo();
    if (!molinfo.IsSetBiomol()) {
        return false;
    }
 
    int inst = bsh.GetInst_Mol();
    int mol = molinfo.GetBiomol();

    switch( mol ) {
        default:
            break;
        case CMolInfo::eBiomol_genomic: {
            switch (inst) {
                default:
                    EMIT("genomic");
                case CSeq_inst::eMol_dna:
                    EMIT("genomic DNA");
                case CSeq_inst::eMol_rna:
                    EMIT("genomic RNA");
            }
        }
        case CMolInfo::eBiomol_mRNA: 
            EMIT("mRNA");
        case CMolInfo::eBiomol_rRNA: 
            EMIT("rRNA");
        case CMolInfo::eBiomol_tRNA: 
            EMIT("tRNA");
        case CMolInfo::eBiomol_pre_RNA:
        case CMolInfo::eBiomol_snRNA:
        case CMolInfo::eBiomol_scRNA:
        case CMolInfo::eBiomol_snoRNA:
        case CMolInfo::eBiomol_ncRNA:
        case CMolInfo::eBiomol_tmRNA:
        case CMolInfo::eBiomol_transcribed_RNA: 
            EMIT("transcribed RNA");
        case CMolInfo::eBiomol_other_genetic:
        case CMolInfo::eBiomol_other: {
            switch (inst) {
                default:
                    EMIT("other");
                case CSeq_inst::eMol_dna:
                    EMIT("other DNA");
                case CSeq_inst::eMol_rna:
                    EMIT("other RNA");
            }
        }
        case CMolInfo::eBiomol_cRNA: 
            EMIT("viral cRNA");

        case CMolInfo::eBiomol_genomic_mRNA: 
            EMIT("genomic RNA");
    }
    switch (inst) {
        default:
            EMIT("unassigned");
        case CSeq_inst::eMol_dna:
            EMIT("unassigned DNA");
        case CSeq_inst::eMol_rna:
            EMIT("unassigned RNA");
    }
    return false;
#undef EMIT
}

//  ----------------------------------------------------------------------------
string CWriteUtil::UrlEncode(
    const string& raw)
//  ----------------------------------------------------------------------------
{
    static const char s_Table[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07", "%08", "%09", 
        "%0A", "%0B", "%0C", "%0D", "%0E", "%0F", "%10", "%11", "%12", "%13", 
        "%14", "%15", "%16", "%17", "%18", "%19", "%1A", "%1B", "%1C", "%1D", 
        "%1E", "%1F", " ",   "!",   "%22", "%23", "$",   "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "-",   ".",   "%2F", "0",   "1",   
        "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",   ":",   "%3B", 
        "%3C", "%3D", "%3E", "%3F", "@",   "A",   "B",   "C",   "D",   "E",   
        "F",   "G",   "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",   "X",   "Y",   
        "Z",   "%5B", "%5C", "%5D", "^",   "_",   "%60", "a",   "b",   "c",   
        "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",   "m",   
        "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F", "%80", "%81", 
        "%82", "%83", "%84", "%85", "%86", "%87", "%88", "%89", "%8A", "%8B", 
        "%8C", "%8D", "%8E", "%8F", "%90", "%91", "%92", "%93", "%94", "%95", 
        "%96", "%97", "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7", "%A8", "%A9", 
        "%AA", "%AB", "%AC", "%AD", "%AE", "%AF", "%B0", "%B1", "%B2", "%B3", 
        "%B4", "%B5", "%B6", "%B7", "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", 
        "%BE", "%BF", "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF", "%D0", "%D1", 
        "%D2", "%D3", "%D4", "%D5", "%D6", "%D7", "%D8", "%D9", "%DA", "%DB", 
        "%DC", "%DD", "%DE", "%DF", "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", 
        "%E6", "%E7", "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7", "%F8", "%F9", 
        "%FA", "%FB", "|", "%FD", "%FE", "%FF"
    };

    string encoded;
    for ( size_t i = 0;  i < raw.size();  ++i ) {
        encoded += s_Table[static_cast<unsigned char>( raw[i] )];
    }
    return encoded;
}

END_NCBI_SCOPE
