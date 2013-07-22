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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'general.asn'.
 *
 * ---------------------------------------------------------------------------
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbistd.hpp>
#include <util/static_map.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// When adding to these lists, please take care to keep them in
// case-sensitive sorted order (lowercase entries last).

typedef SStaticPair<const char*, CDbtag::EDbtagType> TDbxrefPair;
static const TDbxrefPair kApprovedDbXrefs[] = {
    { "AFTOL", CDbtag::eDbtagType_AFTOL },
    { "APHIDBASE", CDbtag::eDbtagType_APHIDBASE },
    { "ASAP", CDbtag::eDbtagType_ASAP },
    { "ATCC", CDbtag::eDbtagType_ATCC },
    { "ATCC(dna)", CDbtag::eDbtagType_ATCC_dna },
    { "ATCC(in host)", CDbtag::eDbtagType_ATCC_in_host },
    { "AceView/WormGenes", CDbtag::eDbtagType_AceView_WormGenes },
    { "AntWeb", CDbtag::eDbtagType_AntWeb },
    { "ApiDB", CDbtag::eDbtagType_ApiDB },
    { "ApiDB_CryptoDB", CDbtag::eDbtagType_ApiDB_CryptoDB },
    { "ApiDB_PlasmoDB", CDbtag::eDbtagType_ApiDB_PlasmoDB },
    { "ApiDB_ToxoDB", CDbtag::eDbtagType_ApiDB_ToxoDB },
    { "Axeldb", CDbtag::eDbtagType_axeldb },
    { "BDGP_EST", CDbtag::eDbtagType_BDGP_EST },
    { "BDGP_INS", CDbtag::eDbtagType_BDGP_INS },
    { "BEETLEBASE", CDbtag::eDbtagType_BEETLEBASE },
    { "BGD", CDbtag::eDbtagType_BGD },
    { "BOLD", CDbtag::eDbtagType_BoLD },
    { "CDD", CDbtag::eDbtagType_CDD },
    { "CK", CDbtag::eDbtagType_CK },
    { "COG", CDbtag::eDbtagType_COG },
    { "ENSEMBL", CDbtag::eDbtagType_ENSEMBL },
    { "ESTLIB", CDbtag::eDbtagType_ESTLIB },
    { "EcoGene", CDbtag::eDbtagType_EcoGene },
    { "EnsemblGenomes", CDbtag::eDbtagType_EnsemblGenomes },
    { "FANTOM_DB", CDbtag::eDbtagType_FANTOM_DB },
    { "FBOL", CDbtag::eDbtagType_FBOL },
    { "FLYBASE", CDbtag::eDbtagType_FLYBASE },
    { "GABI", CDbtag::eDbtagType_GABI },
    { "GDB", CDbtag::eDbtagType_GDB },
    { "GI", CDbtag::eDbtagType_GI },
    { "GO", CDbtag::eDbtagType_GO },
    { "GOA", CDbtag::eDbtagType_GOA },
    { "GRIN", CDbtag::eDbtagType_GRIN },
    { "GeneDB", CDbtag::eDbtagType_GeneDB },
    { "GeneID", CDbtag::eDbtagType_GeneID },
    { "Greengenes", CDbtag::eDbtagType_Greengenes },
    { "H-InvDB", CDbtag::eDbtagType_H_InvDB },
    { "HGNC", CDbtag::eDbtagType_HGNC },
    { "HMP", CDbtag::eDbtagType_HMP },
    { "HOMD", CDbtag::eDbtagType_HOMD },
    { "HSSP", CDbtag::eDbtagType_HSSP },
    { "IKMC", CDbtag::eDbtagType_IKMC },
    { "IMGT/GENE-DB", CDbtag::eDbtagType_IMGT_GENEDB },
    { "IMGT/HLA", CDbtag::eDbtagType_IMGT_HLA },
    { "IMGT/LIGM", CDbtag::eDbtagType_IMGT_LIGM },
    { "IRD", CDbtag::eDbtagType_IRD },
    { "ISD", CDbtag::eDbtagType_ISD },
    { "ISFinder", CDbtag::eDbtagType_ISFinder },
    { "InterPro", CDbtag::eDbtagType_Interpro },
    { "InterimID", CDbtag::eDbtagType_InterimID },
    { "IntrepidBio", CDbtag::eDbtagType_IntrepidBio },
    { "JCM", CDbtag::eDbtagType_JCM },
    { "JGIDB", CDbtag::eDbtagType_JGIDB },
    { "LocusID", CDbtag::eDbtagType_LocusID },
    { "MGI", CDbtag::eDbtagType_MGI },
    { "MIM", CDbtag::eDbtagType_MIM },
    { "MaizeGDB", CDbtag::eDbtagType_MaizeGDB },
    { "MycoBank", CDbtag::eDbtagType_MycoBank },
    { "NBRC", CDbtag::eDbtagType_IFO },
    { "NMPDR", CDbtag::eDbtagType_NMPDR },
    { "NRESTdb", CDbtag::eDbtagType_NRESTdb },
    { "NextDB", CDbtag::eDbtagType_NextDB },
    { "Osa1", CDbtag::eDbtagType_Osa1 },
    { "PBmice", CDbtag::eDbtagType_PBmice },
    { "PDB", CDbtag::eDbtagType_PDB },
    { "PFAM", CDbtag::eDbtagType_PFAM },
    { "PGN", CDbtag::eDbtagType_PGN },
    { "PIR", CDbtag::eDbtagType_PIR },
    { "PSEUDO", CDbtag::eDbtagType_PSEUDO },
    { "Pathema", CDbtag::eDbtagType_Pathema },
    { "Phytozome", CDbtag::eDbtagType_Phytozome },
    { "PomBase", CDbtag::eDbtagType_PomBase },
    { "PseudoCap", CDbtag::eDbtagType_PseudoCap },
    { "RAP-DB", CDbtag::eDbtagType_RAP_DB },
    { "RATMAP", CDbtag::eDbtagType_RATMAP },
    { "RFAM", CDbtag::eDbtagType_RFAM },
    { "RGD", CDbtag::eDbtagType_RGD },
    { "RZPD", CDbtag::eDbtagType_RZPD },
    { "RiceGenes", CDbtag::eDbtagType_RiceGenes },
    { "SEED", CDbtag::eDbtagType_SEED },
    { "SGD", CDbtag::eDbtagType_SGD },
    { "SGN", CDbtag::eDbtagType_SGN },
    { "SoyBase", CDbtag::eDbtagType_SoyBase },
    { "SubtiList", CDbtag::eDbtagType_SubtiList },
    { "TAIR", CDbtag::eDbtagType_TAIR },
    { "TIGRFAM", CDbtag::eDbtagType_TIGRFAM },
    { "UNILIB", CDbtag::eDbtagType_UNILIB },
    { "UNITE", CDbtag::eDbtagType_UNITE },
    { "UniGene", CDbtag::eDbtagType_UniGene },
    { "UniProtKB/Swiss-Prot", CDbtag::eDbtagType_UniProt_SwissProt },
    { "UniProtKB/TrEMBL", CDbtag::eDbtagType_UniProt_TrEMBL },
    { "UniSTS", CDbtag::eDbtagType_UniSTS },
    { "VBASE2", CDbtag::eDbtagType_VBASE2 },
    { "VectorBase", CDbtag::eDbtagType_VectorBase },
    { "ViPR", CDbtag::eDbtagType_ViPR },
    { "WorfDB", CDbtag::eDbtagType_WorfDB },
    { "WormBase", CDbtag::eDbtagType_WormBase },
    { "Xenbase", CDbtag::eDbtagType_Xenbase },
    { "ZFIN", CDbtag::eDbtagType_ZFIN },
    { "dbClone", CDbtag::eDbtagType_dbClone },
    { "dbCloneLib", CDbtag::eDbtagType_dbCloneLib },
    { "dbEST", CDbtag::eDbtagType_dbEST },
    { "dbProbe", CDbtag::eDbtagType_dbProbe },
    { "dbSNP", CDbtag::eDbtagType_dbSNP },
    { "dbSTS", CDbtag::eDbtagType_dbSTS },
    { "dictyBase", CDbtag::eDbtagType_dictyBase },
    { "miRBase", CDbtag::eDbtagType_miRBase },
    { "niaEST", CDbtag::eDbtagType_niaEST }, 
    { "taxon", CDbtag::eDbtagType_taxon }
};

static const TDbxrefPair kApprovedRefSeqDbXrefs[] = {
    { "BEEBASE", CDbtag::eDbtagType_BEEBASE },
    { "BioProject", CDbtag::eDbtagType_BioProject },
    { "CCDS", CDbtag::eDbtagType_CCDS },
    { "CGNC", CDbtag::eDbtagType_CGNC },
    { "CloneID", CDbtag::eDbtagType_CloneID },
    { "ECOCYC", CDbtag::eDbtagType_ECOCYC },
    { "HPRD", CDbtag::eDbtagType_HPRD },
    { "LRG", CDbtag::eDbtagType_LRG },
    { "NASONIABASE", CDbtag::eDbtagType_NASONIABASE },
    { "PBR", CDbtag::eDbtagType_PBR },
    { "REBASE", CDbtag::eDbtagType_REBASE },
    { "SK-FST", CDbtag::eDbtagType_SK_FST },
    {"SRPDB", CDbtag::eDbtagType_SRPDB },
    { "VBRC", CDbtag::eDbtagType_VBRC }
};

static const TDbxrefPair kApprovedSrcDbXrefs[] = {
    { "AFTOL", CDbtag::eDbtagType_AFTOL },
    { "ATCC", CDbtag::eDbtagType_ATCC },
    { "ATCC(dna)", CDbtag::eDbtagType_ATCC_dna },
    { "ATCC(in host)", CDbtag::eDbtagType_ATCC_in_host },
    { "AntWeb", CDbtag::eDbtagType_AntWeb },
    { "BOLD", CDbtag::eDbtagType_BoLD },
    { "FANTOM_DB", CDbtag::eDbtagType_FANTOM_DB },
    { "FBOL", CDbtag::eDbtagType_FBOL },
    { "FLYBASE", CDbtag::eDbtagType_FLYBASE },
    { "GRIN", CDbtag::eDbtagType_GRIN },
    { "Greengenes", CDbtag::eDbtagType_Greengenes },
    { "HMP", CDbtag::eDbtagType_HMP },
    { "HOMD", CDbtag::eDbtagType_HOMD },
    { "IKMC", CDbtag::eDbtagType_IKMC },
    { "IMGT/HLA", CDbtag::eDbtagType_IMGT_HLA },
    { "IMGT/LIGM", CDbtag::eDbtagType_IMGT_LIGM },
    { "JCM", CDbtag::eDbtagType_JCM },
    { "MGI", CDbtag::eDbtagType_MGI },
    { "MycoBank", CDbtag::eDbtagType_MycoBank },
    { "NBRC", CDbtag::eDbtagType_IFO },
    { "RBGE_garden", CDbtag::eDbtagType_RBGE_garden },
    { "RBGE_herbarium", CDbtag::eDbtagType_RBGE_herbarium },
    { "RZPD", CDbtag::eDbtagType_RZPD },
    { "UNILIB", CDbtag::eDbtagType_UNILIB },
    { "UNITE", CDbtag::eDbtagType_UNITE }, 
    { "taxon", CDbtag::eDbtagType_taxon }
};

static const TDbxrefPair kApprovedProbeDbXrefs[] = {
    { "BB", CDbtag::eDbtagType_BB },
    { "DDBJ", CDbtag::eDbtagType_DDBJ },
    { "EMBL", CDbtag::eDbtagType_EMBL },
    { "GEO", CDbtag::eDbtagType_GEO },
    { "GenBank", CDbtag::eDbtagType_GenBank },
    { "GrainGenes", CDbtag::eDbtagType_GrainGenes },
    { "PubChem", CDbtag::eDbtagType_PubChem },
    { "RefSeq", CDbtag::eDbtagType_RefSeq },
    { "SRA", CDbtag::eDbtagType_SRA },
    { "Trace", CDbtag::eDbtagType_Trace }
};


static const char* const kSkippableDbXrefs[] = {
    "BankIt",
    "NCBIFILE",
    "TMSMART"
};

// case sensetive
typedef CStaticPairArrayMap<const char*, CDbtag::EDbtagType, PCase_CStr> TDbxrefTypeMap;
// case insensitive, per the C Toolkit
typedef CStaticArraySet<const char*, PNocase_CStr> TDbxrefSet;

DEFINE_STATIC_ARRAY_MAP(TDbxrefTypeMap, sc_ApprovedDb,       kApprovedDbXrefs);
DEFINE_STATIC_ARRAY_MAP(TDbxrefTypeMap, sc_ApprovedRefSeqDb, kApprovedRefSeqDbXrefs);
DEFINE_STATIC_ARRAY_MAP(TDbxrefTypeMap, sc_ApprovedSrcDb,    kApprovedSrcDbXrefs);
DEFINE_STATIC_ARRAY_MAP(TDbxrefTypeMap, sc_ApprovedProbeDb,  kApprovedProbeDbXrefs);
DEFINE_STATIC_ARRAY_MAP(TDbxrefSet,     sc_SkippableDbXrefs, kSkippableDbXrefs);

struct STaxidTaxname {
    STaxidTaxname( 
        const string & genus,
        const string & species,
        const string & subspecies ) :
        m_genus(genus), m_species(species), m_subspecies(subspecies) { }

    string m_genus;
    string m_species;
    string m_subspecies;
};
// Is hard-coding this here the best way to do this?
typedef SStaticPair<int, STaxidTaxname> TTaxIdTaxnamePair;
static const TTaxIdTaxnamePair sc_taxid_taxname_pair[] = {
    { 7955, STaxidTaxname("Danio", "rerio", kEmptyStr)  },
    { 8022, STaxidTaxname("Oncorhynchus", "mykiss", kEmptyStr)  },
    { 9606, STaxidTaxname("Homo", "sapiens", kEmptyStr)  },
    { 9615, STaxidTaxname("Canis", "lupus", "familiaris")  },
    { 9838, STaxidTaxname("Camelus", "dromedarius", kEmptyStr)  },
    { 9913, STaxidTaxname("Bos", "taurus", kEmptyStr)  },
    { 9986, STaxidTaxname("Oryctolagus", "cuniculus", kEmptyStr)  },
    { 10090, STaxidTaxname("Mus", "musculus", kEmptyStr)  },
    { 10093, STaxidTaxname("Mus", "pahari", kEmptyStr)  },
    { 10094, STaxidTaxname("Mus", "saxicola", kEmptyStr)  },
    { 10096, STaxidTaxname("Mus", "spretus", kEmptyStr)  },
    { 10098, STaxidTaxname("Mus", "cookii", kEmptyStr)  },
    { 10105, STaxidTaxname("Mus", "minutoides", kEmptyStr)  },
    { 10116, STaxidTaxname("Rattus", "norvegicus", kEmptyStr)  },
    { 10117, STaxidTaxname("Rattus", "rattus", kEmptyStr)  }
};

typedef CStaticPairArrayMap<int, STaxidTaxname> TTaxIdTaxnameMap;
DEFINE_STATIC_ARRAY_MAP(TTaxIdTaxnameMap, sc_TaxIdTaxnameMap, sc_taxid_taxname_pair);

// destructor
CDbtag::~CDbtag(void)
{
}

bool CDbtag::Match(const CDbtag& dbt2) const
{
    if (! PNocase().Equals(GetDb(), dbt2.GetDb()))
        return false;
    return ((GetTag()).Match((dbt2.GetTag())));
}


int CDbtag::Compare(const CDbtag& dbt2) const
{
    int ret = PNocase().Compare(GetDb(), dbt2.GetDb());
    if (ret == 0) {
        ret = GetTag().Compare(dbt2.GetTag());
    }
    return ret;
}


// Appends a label to "label" based on content of CDbtag 
void CDbtag::GetLabel(string* label) const
{
    const CObject_id& id = GetTag();
    switch (id.Which()) {
    case CObject_id::e_Str:
        *label += GetDb() + ": " + id.GetStr();
        break;
    case CObject_id::e_Id:
        *label += GetDb() + ": " + NStr::IntToString(id.GetId());
        break;
    default:
        *label += GetDb();
    }
}

// Test if CDbtag.db is in the approved databases list.
// NOTE: 'GenBank', 'EMBL', 'DDBJ' and 'REBASE' are approved only in 
//        the context of a RefSeq record.
bool CDbtag::IsApproved( EIsRefseq refseq, EIsSource is_source, EIsEstOrGss is_est_or_gss ) const
{
    if ( !CanGetDb() ) {
        return false;
    }
    const string& db = GetDb();

    if( refseq == eIsRefseq_Yes && sc_ApprovedRefSeqDb.find(db.c_str()) != sc_ApprovedRefSeqDb.end() ) {
        return true;
    }

    if( is_source == eIsSource_Yes ) {
        bool found = ( sc_ApprovedSrcDb.find(db.c_str()) != sc_ApprovedSrcDb.end() );
        if ( ! found && (is_est_or_gss == eIsEstOrGss_Yes) ) {
            // special case: for EST or GSS, source features are allowed non-src dbxrefs
            found = ( sc_ApprovedDb.find(db.c_str())       != sc_ApprovedDb.end() || 
                      sc_ApprovedRefSeqDb.find(db.c_str()) != sc_ApprovedRefSeqDb.end() );
        }
        return found;
    } else {
        return sc_ApprovedDb.find(db.c_str()) != sc_ApprovedDb.end();
    }
}


const char* CDbtag::IsApprovedNoCase(EIsRefseq refseq, EIsSource is_source ) const
{
    if ( !CanGetDb() ) {
        return NULL;
    }
    const string& db = GetDb();
    
    const char* retval = NULL;
    // This is *slow*.  Someone needs to replace this with a binary search or something
    // Since this function isn't even used right now, I'm postponing fixing this.
    ITERATE (TDbxrefTypeMap, it, sc_ApprovedDb) {
        if ( NStr::EqualNocase(db, it->first) ) {
            retval = it->first;
            break;
        }
    }
    // This is *slow*.  Someone needs to replace this with a binary search or something
    // Since this function isn't even used right now, I'm postponing fixing this.
    if ( retval == NULL  &&  (refseq == eIsRefseq_Yes) ) {
        ITERATE (TDbxrefTypeMap, it, sc_ApprovedRefSeqDb) {
            if ( NStr::EqualNocase(db, it->first) ) {
                retval = it->first;
                break;
            }
        }
    }

    return retval;
}


bool CDbtag::IsApproved(TDbtagGroup group) const
{
    if ( !CanGetDb() ) {
        return false;
    }
    const string& db = GetDb();

    if ( (group & fGenBank) != 0 && sc_ApprovedDb.find(db.c_str()) != sc_ApprovedDb.end()) {
        return true;
    }

    if ( (group & fRefSeq) != 0 && sc_ApprovedRefSeqDb.find(db.c_str()) != sc_ApprovedRefSeqDb.end()) {
        return true;
    }

    if ( (group & fSrc) != 0 && sc_ApprovedSrcDb.find(db.c_str()) != sc_ApprovedSrcDb.end()) {
        return true;
    }

    if ( (group & fProbe) != 0 && sc_ApprovedProbeDb.find(db.c_str()) != sc_ApprovedProbeDb.end()) {
        return true;
    }

    return false;
}


bool CDbtag::IsSkippable(void) const
{
    return sc_SkippableDbXrefs.find(GetDb().c_str())
        != sc_SkippableDbXrefs.end();
}


// Retrieve the enumerated type for the dbtag
CDbtag::EDbtagType CDbtag::GetType(void) const
{
    if (m_Type == eDbtagType_bad) {
        if ( !CanGetDb() ) {
            return m_Type;
        }

        const string& db = GetDb();

        TDbxrefTypeMap::const_iterator iter;

        iter = sc_ApprovedDb.find(db.c_str());
        if ( iter != sc_ApprovedDb.end() ) {
            m_Type = iter->second;
            return m_Type;
        }

        iter = sc_ApprovedRefSeqDb.find(db.c_str());
        if ( iter != sc_ApprovedRefSeqDb.end() ) {
            m_Type = iter->second;
            return m_Type;
        }

        iter = sc_ApprovedSrcDb.find(db.c_str());
        if ( iter != sc_ApprovedSrcDb.end() ) {
            m_Type = iter->second;
            return m_Type;
        }

        iter = sc_ApprovedProbeDb.find(db.c_str());
        if ( iter != sc_ApprovedProbeDb.end() ) {
            m_Type = iter->second;
            return m_Type;
        }
    }

    return m_Type;
}


CDbtag::TDbtagGroup CDbtag::GetDBFlags (string& correct_caps) const
{
    correct_caps = "";
    CDbtag::TDbtagGroup rsult = fNone;

    if ( !CanGetDb() ) {
        return fNone;
    }
    const string& db = GetDb();
    
    ITERATE (TDbxrefTypeMap, it, sc_ApprovedDb) {
        if ( NStr::EqualNocase(db, it->first) ) {
            correct_caps = it->first;
            rsult |= fGenBank;
        }
    }

    ITERATE (TDbxrefTypeMap, it, sc_ApprovedRefSeqDb) {
        if ( NStr::EqualNocase(db, it->first) ) {
            correct_caps = it->first;
            rsult |= fRefSeq;
        }
    }

    ITERATE (TDbxrefTypeMap, it, sc_ApprovedSrcDb) {
        if ( NStr::EqualNocase(db, it->first) ) {
            correct_caps = it->first;
            rsult |= fSrc;
        }
    }

    ITERATE (TDbxrefTypeMap, it, sc_ApprovedProbeDb) {
        if ( NStr::EqualNocase(db, it->first) ) {
            correct_caps = it->first;
            rsult |= fProbe;
        }
    }

    return rsult;
}


bool CDbtag::GetDBFlags (bool& is_refseq, bool& is_src, string& correct_caps) const
{
    CDbtag::TDbtagGroup group = CDbtag::GetDBFlags(correct_caps);

    is_refseq = ((group & fRefSeq) != 0);
    is_src    = ((group & fSrc)    != 0);

    return group != fNone;
}


// Force a refresh of the internal type
void CDbtag::InvalidateType(void)
{
    m_Type = eDbtagType_bad;
}


//=========================================================================//
//                              URLs                                       //
//=========================================================================//

// special case URLs
static const char kFBan[] = "http://www.fruitfly.org/cgi-bin/annot/fban?";
static const char kHInvDbHIT[] = "http://www.jbirc.aist.go.jp/hinv/hinvsys/servlet/ExecServlet?KEN_INDEX=0&KEN_TYPE=30&KEN_STR=";
static const char kHInvDbHIX[] = "http://www.jbirc.aist.go.jp/hinv/hinvsys/servlet/ExecServlet?KEN_INDEX=0&KEN_TYPE=31&KEN_STR=";
static const char kDictyPrim[] = "http://dictybase.org/db/cgi-bin/gene_page.pl?primary_id=";
static const char kMiRBaseMat[] = "http://www.mirbase.org/cgi-bin/mature.pl?mature_acc=";
static const char kMaizeGDBInt[] = "http://www.maizegdb.org/cgi-bin/displaylocusrecord.cgi?id=";
static const char kMaizeGDBStr[] = "http://www.maizegdb.org/cgi-bin/displaylocusrecord.cgi?term=";
static const char kHomdTax[] = "http://www.homd.org/taxon=";
static const char kHomdSeq[] = "http://www.homd.org/seq=";

// mapping of DB to its URL; please sort these by tag value (mostly,
// but NOT entirely, in case-sensitive ASCII-betical order as above)
typedef SStaticPair<CDbtag::EDbtagType, const char*>    TDbtUrl;
static const TDbtUrl sc_url_prefix[] = {
    { CDbtag::eDbtagType_AFTOL, "http://aftol1.biology.duke.edu/pub/displayTaxonInfo?aftol_id=" },
    { CDbtag::eDbtagType_APHIDBASE, "http://webapps1.genouest.org/grs-1.0/grs?reportID=chado_genome_report&objectID=" },
    { CDbtag::eDbtagType_ASAP, "https://asap.ahabs.wisc.edu/annotation/php/feature_info.php?FeatureID=" },
    { CDbtag::eDbtagType_ATCC, "http://www.atcc.org/Products/All/" },
    { CDbtag::eDbtagType_AceView_WormGenes, "http://www.ncbi.nlm.nih.gov/IEB/Research/Acembly/av.cgi?db=worm&c=gene&q=" },
    { CDbtag::eDbtagType_AntWeb, "http://www.antweb.org/specimen.do?name=" },
    { CDbtag::eDbtagType_ApiDB, "http://www.apidb.org/apidb/showRecord.do?name=GeneRecordClasses.ApiDBGeneRecordClass&primary_key=" },
    { CDbtag::eDbtagType_ApiDB_CryptoDB, "http://cryptodb.org/cryptodb/showRecord.do?name=GeneRecordClasses.GeneRecordClass&project_id=CryptoDB&source_id=" },
    { CDbtag::eDbtagType_ApiDB_PlasmoDB, "http://plasmodb.org/plasmo/showRecord.do?name=GeneRecordClasses.GeneRecordClass&project_id=PlasmoDB&source_id=" },
    { CDbtag::eDbtagType_ApiDB_ToxoDB, "http://toxodb.org/toxo/showRecord.do?name=GeneRecordClasses.GeneRecordClass&project_id=ToxoDB&source_id=" },
    { CDbtag::eDbtagType_BB, "http://beetlebase.org/cgi-bin/cmap/feature_search?features=" },
    { CDbtag::eDbtagType_BEETLEBASE, "http://www.beetlebase.org/cgi-bin/report.cgi?name=" },
    { CDbtag::eDbtagType_BGD, "http://bovinegenome.org/genepages/btau40/genes/" },
    { CDbtag::eDbtagType_BoLD, "http://www.boldsystems.org/connectivity/specimenlookup.php?processid=" },
    { CDbtag::eDbtagType_CCDS, "http://www.ncbi.nlm.nih.gov/CCDS/CcdsBrowse.cgi?REQUEST=CCDS&DATA=" },
    { CDbtag::eDbtagType_CDD, "http://www.ncbi.nlm.nih.gov/Structure/cdd/cddsrv.cgi?uid=" },
    { CDbtag::eDbtagType_CGNC, "http://www.agnc.msstate.edu/GeneReport.aspx?a=" },
    { CDbtag::eDbtagType_CK, "http://flybane.berkeley.edu/cgi-bin/cDNA/CK_clone.pl?db=CK&dbid=" },
    { CDbtag::eDbtagType_COG, "http://www.ncbi.nlm.nih.gov/COG/new/release/cow.cgi?cog=" },
    { CDbtag::eDbtagType_ECOCYC, "http://biocyc.org/ECOLI/new-image?type=GENE&object=" },
    { CDbtag::eDbtagType_ENSEMBL, "http://www.ensembl.org/id/" },
    { CDbtag::eDbtagType_EcoGene, "http://ecogene.org/geneInfo.php?eg_id=" },
    { CDbtag::eDbtagType_FANTOM_DB, "http://fantom.gsc.riken.jp/db/annotate/main.cgi?masterid=" },
    { CDbtag::eDbtagType_FBOL, "http://www.fungalbarcoding.org/BioloMICS.aspx?Table=Fungal%20barcodes&Fields=All&Rec=" },
    { CDbtag::eDbtagType_FLYBASE, "http://flybase.bio.indiana.edu/.bin/fbidq.html?" },
    { CDbtag::eDbtagType_GABI, "http://www.gabipd.org/database/cgi-bin/GreenCards.pl.cgi?Mode=ShowSequence&App=ncbi&SequenceId=" },
    { CDbtag::eDbtagType_GEO, "http://www.ncbi.nlm.nih.gov/geo/query/acc.cgi?acc=" },
    { CDbtag::eDbtagType_GO, "http://amigo.geneontology.org/cgi-bin/amigo/go.cgi?view=details&depth=1&query=GO:" },
    { CDbtag::eDbtagType_GOA, "http://www.ebi.ac.uk/ego/GProtein?ac=" },
    { CDbtag::eDbtagType_GRIN, "http://www.ars-grin.gov/cgi-bin/npgs/acc/display.pl?" },
    { CDbtag::eDbtagType_GeneDB, "http://old.genedb.org/genedb/Search?organism=All%3A*&name=" },
    { CDbtag::eDbtagType_GeneID, "http://www.ncbi.nlm.nih.gov/gene/" },
    { CDbtag::eDbtagType_GrainGenes, "http://wheat.pw.usda.gov/cgi-bin/graingenes/report.cgi?class=marker&name=" },
    { CDbtag::eDbtagType_Greengenes, "http://greengenes.lbl.gov/cgi-bin/show_one_record_v2.pl?prokMSA_id=" },
    { CDbtag::eDbtagType_HGNC, "http://www.genenames.org/data/hgnc_data.php?hgnc_id=" },
    { CDbtag::eDbtagType_HMP, "http://www.hmpdacc-resources.org/cgi-bin/hmp_catalog/main.cgi?section=HmpSummary&page=displayHmpProject&hmp_id=" },
    { CDbtag::eDbtagType_HOMD, "http://www.homd.org/" },
    { CDbtag::eDbtagType_HPRD, "http://www.hprd.org/protein/" },
    { CDbtag::eDbtagType_HSSP, "http://srs.ebi.ac.uk/srsbin/cgi-bin/wgetz?-newId+-e+hssp-ID:" },
    { CDbtag::eDbtagType_H_InvDB, "http://www.h-invitational.jp" },
    { CDbtag::eDbtagType_IFO, "http://www.nbrc.nite.go.jp/NBRC2/NBRCCatalogueDetailServlet?ID=NBRC&CAT=" },
    { CDbtag::eDbtagType_IMGT_GENEDB, "http://www.imgt.org/IMGT_GENE-DB/GENElect?species=Homo+sapiens&query=2+" },
    { CDbtag::eDbtagType_IMGT_LIGM, "http://www.imgt.org/cgi-bin/IMGTlect.jv?query=201+" },
    { CDbtag::eDbtagType_IRD, "http://www.fludb.org/brc/fluSegmentDetails.do?irdSubmissionId=" },
    { CDbtag::eDbtagType_ISD, "http://www.flu.lanl.gov/search/view_record.html?accession=" },
    { CDbtag::eDbtagType_ISFinder, "http://www-is.biotoul.fr/scripts/is/is_spec.idc?name=" },
    { CDbtag::eDbtagType_InterimID, "http://www.ncbi.nlm.nih.gov/gene/" },
    { CDbtag::eDbtagType_Interpro, "http://www.ebi.ac.uk/interpro/ISearch?mode=ipr&query=" },
    { CDbtag::eDbtagType_IntrepidBio, "http://server1.intrepidbio.com/FeatureBrowser/gene/browse/" },
    { CDbtag::eDbtagType_JCM, "http://www.jcm.riken.go.jp/cgi-bin/jcm/jcm_number?JCM=" },
    { CDbtag::eDbtagType_JGIDB, "http://genome.jgi-psf.org/cgi-bin/jgrs?id=" },
    { CDbtag::eDbtagType_LocusID, "http://www.ncbi.nlm.nih.gov/gene/" },
    { CDbtag::eDbtagType_MGI, "http://www.informatics.jax.org/marker/MGI:" },
    { CDbtag::eDbtagType_MIM, "http://www.ncbi.nlm.nih.gov/omim/" },
    { CDbtag::eDbtagType_MaizeGDB, "http://www.maizegdb.org/cgi-bin/displaylocusrecord.cgi?" },
    { CDbtag::eDbtagType_MycoBank, "http://www.mycobank.org/MycoTaxo.aspx?Link=T&Rec=" },
    { CDbtag::eDbtagType_NMPDR, "http://www.nmpdr.org/linkin.cgi?id=" },
    { CDbtag::eDbtagType_NRESTdb, "http://genome.ukm.my/nrestdb/db/single_view_est.php?id=" },
    { CDbtag::eDbtagType_NextDB, "http://nematode.lab.nig.ac.jp/cgi-bin/db/ShowGeneInfo.sh?celk=" },
    { CDbtag::eDbtagType_Osa1, "http://rice.plantbiology.msu.edu/cgi-bin/gbrowse/rice/?name=" },
    { CDbtag::eDbtagType_PBR, "http://www.poxvirus.org/query.asp?web_id=" },
    { CDbtag::eDbtagType_PBmice, "http://www.idmshanghai.cn/PBmice/DetailedSearch.do?type=insert&id=" },
    { CDbtag::eDbtagType_PDB, "http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=" },
    { CDbtag::eDbtagType_PFAM, "http://pfam.sanger.ac.uk/family?acc=" },
    { CDbtag::eDbtagType_PGN, "http://pgn.cornell.edu/cgi-bin/search/seq_search_result.pl?identifier=" },
    { CDbtag::eDbtagType_Pathema, "http://pathema.jcvi.org/cgi-bin/Burkholderia/shared/GenePage.cgi?all=1&locus=" },
    { CDbtag::eDbtagType_Phytozome, "http://www.phytozome.net/genePage.php?db=Phytozome&crown&method=0&search=1&detail=1&searchText=locusname:" },
    { CDbtag::eDbtagType_PomBase, "http://www.pombase.org/spombe/result/" },
    { CDbtag::eDbtagType_PseudoCap, "http://www.pseudomonas.com/getAnnotation.do?locusID=" },
    { CDbtag::eDbtagType_RAP_DB, "http://rapdb.dna.affrc.go.jp/cgi-bin/gbrowse_details/latest?name=" },
    { CDbtag::eDbtagType_RATMAP, "http://ratmap.gen.gu.se/ShowSingleLocus.htm?accno=" },
    { CDbtag::eDbtagType_RBGE_garden, "http://data.rbge.org.uk/living/" },
    { CDbtag::eDbtagType_RBGE_herbarium, "http://data.rbge.org.uk/herb/" },
    { CDbtag::eDbtagType_REBASE, "http://rebase.neb.com/rebase/enz/" },
    { CDbtag::eDbtagType_RFAM, "http://www.sanger.ac.uk/cgi-bin/Rfam/getacc?" },
    { CDbtag::eDbtagType_RGD, "http://rgd.mcw.edu/rgdweb/search/search.html?term=" },
    { CDbtag::eDbtagType_RiceGenes, "http://ars-genome.cornell.edu/cgi-bin/WebAce/webace?db=ricegenes&class=Marker&object=" },
    { CDbtag::eDbtagType_SEED, "http://www.theseed.org/linkin.cgi?id=" },
    { CDbtag::eDbtagType_SGD, "http://www.yeastgenome.org/cgi-bin/locus.fpl?sgdid=" },
    { CDbtag::eDbtagType_SGN, "http://www.sgn.cornell.edu/search/est.pl?request_type=7&request_id=" },
    { CDbtag::eDbtagType_SK_FST, "http://aafc-aac.usask.ca/fst/" },
    { CDbtag::eDbtagType_SRPDB, "http://rnp.uthscsa.edu/rnp/SRPDB/rna/sequences/fasta/" },
    { CDbtag::eDbtagType_SubtiList, "http://genolist.pasteur.fr/SubtiList/genome.cgi?external_query+" },
    { CDbtag::eDbtagType_TAIR, "http://www.arabidopsis.org/servlets/TairObject?type=locus&name=" },
    { CDbtag::eDbtagType_TIGRFAM, "http://cmr.tigr.org/tigr-scripts/CMR/HmmReport.cgi?hmm_acc=" },
    { CDbtag::eDbtagType_UNITE, "http://unite.ut.ee/bl_forw.php?nimi=" },
    { CDbtag::eDbtagType_UniGene, "http://www.ncbi.nlm.nih.gov/unigene?term=" },
    { CDbtag::eDbtagType_UniProt_SwissProt, "http://www.uniprot.org/uniprot/" },
    { CDbtag::eDbtagType_UniProt_TrEMBL, "http://www.uniprot.org/uniprot/" },
    { CDbtag::eDbtagType_UniSTS, "http://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=" },
    { CDbtag::eDbtagType_VBASE2, "http://www.dnaplot.de/vbase2/vgene.php?id=" },
    { CDbtag::eDbtagType_VBRC, "http://vbrc.org/query.asp?web_view=curation&web_id=" },
    { CDbtag::eDbtagType_VectorBase, "http://www.vectorbase.org/Genome/BRCGene/?feature=" },
    { CDbtag::eDbtagType_Vega, "http://vega.sanger.ac.uk/id/"  },
    { CDbtag::eDbtagType_WorfDB, "http://worfdb.dfci.harvard.edu/search.pl?form=1&search=" },
    { CDbtag::eDbtagType_WormBase, "http://www.wormbase.org/search/gene/" },
    { CDbtag::eDbtagType_Xenbase, "http://www.xenbase.org/gene/showgene.do?method=display&geneId=" },
    { CDbtag::eDbtagType_ZFIN, "http://zfin.org/cgi-bin/webdriver?MIval=aa-markerview.apg&OID=" },
    { CDbtag::eDbtagType_axeldb, "http://www.dkfz-heidelberg.de/tbi/services/axeldb/clone/xenopus?name=" },
    { CDbtag::eDbtagType_dbClone, "http://www.ncbi.nlm.nih.gov/sites/entrez?db=clone&cmd=Retrieve&list_uids=" },
    { CDbtag::eDbtagType_dbCloneLib, "http://www.ncbi.nlm.nih.gov/sites/entrez?db=clonelib&cmd=Retrieve&list_uids=" },
    { CDbtag::eDbtagType_dbEST, "http://www.ncbi.nlm.nih.gov/nucest/" },
    { CDbtag::eDbtagType_dbProbe, "http://www.ncbi.nlm.nih.gov/sites/entrez?db=probe&cmd=Retrieve&list_uids=" },
    { CDbtag::eDbtagType_dbSNP, "http://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?type=rs&rs=" },
    { CDbtag::eDbtagType_dbSTS, "http://www.ncbi.nlm.nih.gov/nuccore/" },
    { CDbtag::eDbtagType_dictyBase, "http://dictybase.org/db/cgi-bin/gene_page.pl?dictybaseid=" },
    { CDbtag::eDbtagType_miRBase, "http://www.mirbase.org/cgi-bin/mirna_entry.pl?acc=" },
    { CDbtag::eDbtagType_niaEST, "http://lgsun.grc.nia.nih.gov/cgi-bin/pro3?sname1=" },
    { CDbtag::eDbtagType_taxon, "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?" },
    { CDbtag::eDbtagType_BEEBASE, "http://hymenopteragenome.org/cgi-bin/gbrowse/bee_genome4/?name=" },
    { CDbtag::eDbtagType_NASONIABASE, "http://hymenopteragenome.org/cgi-bin/gbrowse/nasonia10_scaffold/?name=" },
    { CDbtag::eDbtagType_IKMC, "http://www.knockoutmouse.org/martsearch/project/" },
    { CDbtag::eDbtagType_ViPR, "http://www.viprbrc.org/brc/viprStrainDetails.do?viprSubmissionId=" },
    { CDbtag::eDbtagType_RefSeq, "http://www.ncbi.nlm.nih.gov/nuccore/" },
    { CDbtag::eDbtagType_EnsemblGenomes, "http://ensemblgenomes.org/id/" }
};

typedef CStaticPairArrayMap<CDbtag::EDbtagType, const char*> TUrlPrefixMap;
DEFINE_STATIC_ARRAY_MAP(TUrlPrefixMap, sc_UrlMap, sc_url_prefix);

string CDbtag::GetUrl(void) const
{
    return GetUrl( kEmptyStr, kEmptyStr, kEmptyStr );
}

string CDbtag::GetUrl(int taxid) const
{   
    TTaxIdTaxnameMap::const_iterator find_iter = sc_TaxIdTaxnameMap.find(taxid);
    if( find_iter == sc_TaxIdTaxnameMap.end() ) {
        return GetUrl();
    } else {
        const STaxidTaxname & taxinfo = find_iter->second;
        return GetUrl( taxinfo.m_genus, taxinfo.m_species, taxinfo.m_subspecies );
    }
}

string CDbtag::GetUrl(const string & taxname_arg ) const
{
    // The exact number doesn't matter, as long as it's long enough
    // to cover all reasonable cases
    const static SIZE_TYPE kMaxLen = 500;

    if( taxname_arg.empty() || taxname_arg.length() > kMaxLen ) {
        return GetUrl();
    }

    // make a copy because we're changing it
    string taxname = taxname_arg;

    // convert all non-alpha chars to spaces
    NON_CONST_ITERATE( string, str_iter, taxname ) {
        const char ch = *str_iter;
        if( ! isalpha(ch) ) {
            *str_iter = ' ';
        }
    }

    // remove initial and final spaces
    NStr::TruncateSpacesInPlace( taxname );

    // extract genus, species, subspeces

    vector<string> taxname_parts;
    NStr::Tokenize( taxname, " ", taxname_parts, NStr::eMergeDelims );

    if( taxname_parts.size() == 2 || taxname_parts.size() == 3 ) {
        string genus;
        string species;
        string subspecies;

        genus = taxname_parts[0];
        species = taxname_parts[1];

        if( taxname_parts.size() == 3 ) {
            subspecies = taxname_parts[2];
        }

        return GetUrl( genus, species, subspecies );
    }

    // if we couldn't figure out the taxname, use the default behavior
    return GetUrl();
}

string CDbtag::GetUrl(const string & genus,
                      const string & species,
                      const string & subspecies) const
{
    TUrlPrefixMap::const_iterator it = sc_UrlMap.find(GetType());
    if (it == sc_UrlMap.end()) {
        return kEmptyStr;
    }
    const char* prefix = it->second;

    string tag;
    if (GetTag().IsStr()) {
        tag = GetTag().GetStr();
    } else if (GetTag().IsId()) {
        tag = NStr::IntToString(GetTag().GetId());
    }
    if (NStr::IsBlank(tag)) {
        return kEmptyStr;
    }

    // URLs are constructed by catenating the URL prefix with the specific tag
    // except in a few cases handled below.
    switch (GetType()) {
        case CDbtag::eDbtagType_FLYBASE:
            if (NStr::Find(tag, "FBan") != NPOS) {
                prefix = kFBan;
            }
            break;

        case eDbtagType_MGI:
        case eDbtagType_MGD:
            if (NStr::StartsWith(tag, "MGI:", NStr::eNocase)  ||
                NStr::StartsWith(tag, "MGD:", NStr::eNocase)) {
                tag = tag.substr(4);
            }
            break;

        case eDbtagType_PID:
            if (tag[0] == 'g') {
                tag = tag.substr(1);
            }
            break;

        case eDbtagType_SRPDB:
            tag += ".fasta";
            break;

        case eDbtagType_dbSTS:
            break;

        case eDbtagType_niaEST:
            tag += "&val=1";
            break;

        case eDbtagType_MaizeGDB:
            if (GetTag().IsId()) {
                prefix = kMaizeGDBInt;
            } else if (GetTag().IsStr()) {
                prefix = kMaizeGDBStr;
            }
            break;

        case eDbtagType_GDB:
        {{
            SIZE_TYPE pos = NStr::Find(tag, "G00-");
            if (pos != NPOS) {
                tag = tag.substr(pos + 4);
                remove(tag.begin(), tag.end(), '-');
            } else if (!isdigit((unsigned char) tag[0])) {
                return kEmptyStr;
            }
            break;
        }}
        case eDbtagType_REBASE:
            tag += ".html";
            break;
        case eDbtagType_H_InvDB:
            if (NStr::Find(tag, "HIT")) {
                prefix = kHInvDbHIT;
            } else if (NStr::Find(tag, "HIX")) {
                prefix = kHInvDbHIX;
            }
            break;

        case eDbtagType_SK_FST:
            return prefix;
            break;

        case CDbtag::eDbtagType_taxon:
            if (isdigit((unsigned char) tag[0])) {
                tag.insert(0, "id=");
            } else {
                tag.insert(0, "name=");
            }
            break;

        case CDbtag::eDbtagType_dictyBase:
            if (NStr::Find(tag, "_") != NPOS) {
                prefix = kDictyPrim;
            }
            break;


        case CDbtag::eDbtagType_miRBase:
            if (NStr::Find(tag, "MIMAT") != NPOS) {
                prefix = kMiRBaseMat;
            }
            break;

        case CDbtag::eDbtagType_WormBase:
            {
                int num_alpha = 0;
                int num_digit = 0;
                int num_unscr = 0;
                if( x_LooksLikeAccession (tag, num_alpha, num_digit, num_unscr) &&
                    num_alpha == 3 && num_digit == 5 ) 
                {
                    prefix = "http://www.wormbase.org/search/protein/";
                }
            }
            break;

        case CDbtag::eDbtagType_HOMD:
            if( NStr::StartsWith(tag, "tax_") ) {
                prefix = kHomdTax;
                tag = tag.substr(4);
            } else if( NStr::StartsWith(tag, "seq_") ) {
                prefix = kHomdSeq;
                tag = tag.substr(4);
            }
            break;

        case eDbtagType_IRD:
            tag += "&decorator=influenza";
            break;

        case eDbtagType_ATCC:
            tag += ".aspx";
            break;

        case eDbtagType_ViPR:
            tag += "&decorator=vipr";
            break;

    case CDbtag::eDbtagType_IMGT_GENEDB:
        if( ! genus.empty() ) {
            string taxname_url_piece = genus + "+" + species;
            if( ! subspecies.empty() ) {
                taxname_url_piece += "+" + subspecies;
            }
            string ret = prefix;
            return NStr::Replace( ret,
                                  "species=Homo+sapiens&",
                                  "species=" + taxname_url_piece + "&" ) +
                tag;
        }
        break;

        default:
            break;
    }

    return string(prefix) + tag;
}

// static 
bool CDbtag::x_LooksLikeAccession(const string &tag, 
        int &out_num_alpha, 
        int &out_num_digit, 
        int &out_num_unscr)
{
    if ( tag.empty() ) return false;

    if ( tag.length() >= 16) return false;

    if ( ! isupper(tag[0]) ) return false;

    int     numAlpha = 0;
    int     numDigits = 0;
    int     numUndersc = 0;

    string::const_iterator tag_iter = tag.begin();
    if ( NStr::StartsWith(tag, "NZ_") ) {
        tag_iter += 3;
    }
    for ( ; tag_iter != tag.end() && isalpha(*tag_iter); ++tag_iter ) {
        numAlpha++;
    }
    for ( ; tag_iter != tag.end() && *tag_iter == '_'; ++tag_iter ) {
        numUndersc++;
    }
    for ( ; tag_iter != tag.end() && isdigit(*tag_iter) ; ++tag_iter ) {
        numDigits++;
    }
    if ( tag_iter != tag.end() && *tag_iter != ' ' && *tag_iter != '.') {
        return false;
    }

    if (numUndersc > 1) return false;

    out_num_alpha = numAlpha;
    out_num_digit = numDigits;
    out_num_unscr = numUndersc;

    if (numUndersc == 0) {
        if (numAlpha == 1 && numDigits == 5) return true;
        if (numAlpha == 2 && numDigits == 6) return true;
        if (numAlpha == 3 && numDigits == 5) return true;
        if (numAlpha == 4 && numDigits == 8) return true;
        if (numAlpha == 4 && numDigits == 9) return true;
        if (numAlpha == 5 && numDigits == 7) return true;
    } else if (numUndersc == 1) {
        if (numAlpha != 2 || (numDigits != 6 && numDigits != 8 && numDigits != 9)) return false;
        if (tag[0] == 'N' || tag[0] == 'X' || tag[0] == 'Z') {
            if (tag[1] == 'M' ||
                tag[1] == 'C' ||
                tag[1] == 'T' ||
                tag[1] == 'P' ||
                tag[1] == 'G' ||
                tag[1] == 'R' ||
                tag[1] == 'S' ||
                tag[1] == 'W' ||
                tag[1] == 'Z') {
                    return true;
            }
        }
        if (tag[0] == 'A' || tag[0] == 'Y') {
            if (tag[1] == 'P') return true;
        }
    }

    return false;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
