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
 *   using specifications from the data definition file
 *   'seqfeat.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <algorithm>
#include <set>
#include <util/static_map.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CBioSource::~CBioSource(void)
{
}


int CBioSource::GetGenCode(void) const
{
    try {
        TGenome genome = CanGetGenome() ? GetGenome() : eGenome_unknown;

        if ( !CanGetOrg()  ||  !GetOrg().CanGetOrgname() ) {
            return 1; // assume standard genetic code
        }
        const COrgName& orn = GetOrg().GetOrgname();

        switch ( genome ) {
        case eGenome_kinetoplast:
        case eGenome_mitochondrion:
        case eGenome_hydrogenosome:
            {
                // mitochondrial code
                if (orn.IsSetMgcode()) {
                    return orn.GetMgcode();
                }
                return 1;
            }
        case eGenome_chloroplast:
        case eGenome_chromoplast:
        case eGenome_plastid:
        case eGenome_cyanelle:
        case eGenome_apicoplast:
        case eGenome_leucoplast:
        case eGenome_proplastid:
            {
                // bacteria and plant plastid code
                if (orn.IsSetPgcode()) {
                    int pgcode = orn.GetPgcode();
                    if (pgcode > 0) return pgcode;
                }
                // bacteria and plant plastids default to code 11.
                return 11;
            }
        default:
            {
                if (orn.IsSetGcode()) {
                    return orn.GetGcode();
                }
                return 1;
            }
        }
    } catch (...) {
        return 1; // was 0(!)
    }
}

typedef SStaticPair<const char *, const CBioSource::EGenome> TGenomeKey;

static const TGenomeKey genome_key_to_subtype [] = {
    {  "apicoplast",                CBioSource::eGenome_apicoplast        },
    {  "chloroplast",               CBioSource::eGenome_chloroplast       },
    {  "chromoplast",               CBioSource::eGenome_chromoplast       },
    {  "cyanelle",                  CBioSource::eGenome_cyanelle          },
    {  "endogenous_virus",          CBioSource::eGenome_endogenous_virus  },
    {  "extrachrom",                CBioSource::eGenome_extrachrom        },
    {  "genomic",                   CBioSource::eGenome_genomic           },
    {  "hydrogenosome",             CBioSource::eGenome_hydrogenosome     },
    {  "insertion_seq",             CBioSource::eGenome_insertion_seq     },
    {  "kinetoplast",               CBioSource::eGenome_kinetoplast       },
    {  "leucoplast",                CBioSource::eGenome_leucoplast        },
    {  "macronuclear",              CBioSource::eGenome_macronuclear      },
    {  "mitochondrion",             CBioSource::eGenome_mitochondrion     },
    {  "mitochondrion:kinetoplast", CBioSource::eGenome_kinetoplast       },
    {  "nucleomorph",               CBioSource::eGenome_nucleomorph       },
    {  "plasmid",                   CBioSource::eGenome_plasmid           },
    {  "plastid",                   CBioSource::eGenome_plastid           },
    {  "plastid:apicoplast",        CBioSource::eGenome_apicoplast        },
    {  "plastid:chloroplast",       CBioSource::eGenome_chloroplast       },
    {  "plastid:chromoplast",       CBioSource::eGenome_chromoplast       },
    {  "plastid:cyanelle",          CBioSource::eGenome_cyanelle          },
    {  "plastid:leucoplast",        CBioSource::eGenome_leucoplast        },
    {  "plastid:proplastid",        CBioSource::eGenome_proplastid        },
    {  "proplastid",                CBioSource::eGenome_proplastid        },
    {  "proviral",                  CBioSource::eGenome_proviral          },
    {  "transposon",                CBioSource::eGenome_transposon        },
    {  "unknown",                   CBioSource::eGenome_unknown           },
    {  "virion",                    CBioSource::eGenome_virion            }
};


typedef CStaticPairArrayMap <const char*, const CBioSource::EGenome, PNocase_CStr> TGenomeMap;
DEFINE_STATIC_ARRAY_MAP(TGenomeMap, sm_GenomeKeys, genome_key_to_subtype);

CBioSource::EGenome CBioSource::GetGenomeByOrganelle (string organelle, NStr::ECase use_case, bool starts_with)
{
    CBioSource::EGenome gtype = CBioSource::eGenome_unknown;

    if (use_case == NStr::eCase && !starts_with) {
        TGenomeMap::const_iterator g_iter = sm_GenomeKeys.find (organelle.c_str ());
        if (g_iter != sm_GenomeKeys.end ()) {
            gtype = g_iter->second;
        }
    } else {
        TGenomeMap::const_iterator g_iter = sm_GenomeKeys.begin();
        if (starts_with) {
            string match;
            while (g_iter != sm_GenomeKeys.end() && gtype == CBioSource::eGenome_unknown) {
                match = g_iter->first;
                if (NStr::StartsWith(organelle, match.c_str(), use_case)) {
                    if (organelle.length() == match.length()
                        || (match.length() < organelle.length() && isspace (organelle[match.length()]))) {
                        gtype = g_iter->second;
                    }
                }
                ++g_iter;
            }
        } else {
            while (g_iter != sm_GenomeKeys.end() && gtype == CBioSource::eGenome_unknown) {
                if (NStr::Equal(organelle, g_iter->first, use_case)) {
                    gtype = g_iter->second;
                }
                ++g_iter;
            }
        }
    }
    return gtype;
}


string CBioSource::GetOrganelleByGenome (unsigned int genome)
{
    string organelle = "";
    TGenomeMap::const_iterator g_iter = sm_GenomeKeys.begin();
    while (g_iter != sm_GenomeKeys.end() &&
           unsigned(g_iter->second) != genome) {
        ++g_iter;
    }
    if (g_iter != sm_GenomeKeys.end()) {
        organelle = g_iter->first;
    }
    return organelle;
}


bool CBioSource::IsSetTaxname(void) const
{
    return IsSetOrg () && GetOrg ().IsSetTaxname ();
}

const string& CBioSource::GetTaxname(void) const
{
    return GetOrg ().GetTaxname ();
}

bool CBioSource::IsSetCommon(void) const
{
    return IsSetOrg () && GetOrg ().IsSetCommon ();
}

const string& CBioSource::GetCommon(void) const
{
    return GetOrg ().GetCommon ();
}

bool CBioSource::IsSetLineage(void) const
{
    return IsSetOrg () && GetOrg ().IsSetLineage ();
}

const string& CBioSource::GetLineage(void) const
{
    return GetOrg ().GetLineage ();
}

bool CBioSource::IsSetGcode(void) const
{
    return IsSetOrg () && GetOrg ().IsSetGcode ();
}

int CBioSource::GetGcode(void) const
{
    return GetOrg ().GetGcode ();
}

bool CBioSource::IsSetMgcode(void) const
{
    return IsSetOrg () && GetOrg ().IsSetMgcode ();
}

int CBioSource::GetMgcode(void) const
{
    return GetOrg ().GetMgcode ();
}

bool CBioSource::IsSetPgcode(void) const
{
    return IsSetOrg () && GetOrg ().IsSetPgcode ();
}

int CBioSource::GetPgcode(void) const
{
    return GetOrg ().GetPgcode ();
}

bool CBioSource::IsSetDivision(void) const
{
    return IsSetOrg () && GetOrg ().IsSetDivision ();
}

const string& CBioSource::GetDivision(void) const
{
    return GetOrg ().GetDivision ();
}

bool CBioSource::IsSetOrgname(void) const
{
    return IsSetOrg () && GetOrg ().IsSetOrgname ();
}

const COrgName& CBioSource::GetOrgname(void) const
{
    return GetOrg ().GetOrgname ();
}

bool CBioSource::IsSetOrgMod(void) const
{
  return IsSetOrg () && GetOrg ().IsSetOrgMod ();
}


string CBioSource::GetRepliconName(void) const
{
    if (IsSetGenome() && GetGenome() == CBioSource::eGenome_mitochondrion) {
        return "MT";
    }

    ITERATE (CBioSource::TSubtype, sit, GetSubtype()) {
        if ((*sit)->IsSetSubtype() && (*sit)->GetSubtype() == CSubSource::eSubtype_plasmid_name
            && (*sit)->IsSetName()) {
            return (*sit)->GetName();
        }
    }

    if (IsSetGenome() && GetGenome() == CBioSource::eGenome_chromosome) {
        ITERATE (CBioSource::TSubtype, sit, GetSubtype()) {
            if ((*sit)->IsSetSubtype() && (*sit)->GetSubtype() == CSubSource::eSubtype_linkage_group
                && (*sit)->IsSetName()) {
                return (*sit)->GetName();
            }
        }
    }

    ITERATE (CBioSource::TSubtype, sit, GetSubtype()) {
        if ((*sit)->IsSetSubtype() && (*sit)->GetSubtype() == CSubSource::eSubtype_chromosome
            && (*sit)->IsSetName()) {
            return (*sit)->GetName();
        }
    }

    // no other name found
    if (IsSetGenome()) {
        switch (GetGenome()) {
            case CBioSource::eGenome_plasmid:
                return "unnamed";
                break;
            case CBioSource::eGenome_chromosome:
                return "ANONYMOUS";
                break;
            case CBioSource::eGenome_kinetoplast:
                return "kinetoplast";
                break;
            case CBioSource::eGenome_plastid:
            case CBioSource::eGenome_chloroplast:
            case CBioSource::eGenome_chromoplast:
            case CBioSource::eGenome_apicoplast:
            case CBioSource::eGenome_leucoplast:
            case CBioSource::eGenome_proplastid:
                return "Pltd";
        }
    }
    return "";
}


string CBioSource::GetBioprojectType (void) const
{
    if (IsSetGenome() && GetGenome() == CBioSource::eGenome_plasmid) {
        return "ePlasmid";
    }

    ITERATE (CBioSource::TSubtype, sit, GetSubtype()) {
        if ((*sit)->IsSetSubtype() && (*sit)->GetSubtype() == CSubSource::eSubtype_plasmid_name) {
            return "ePlasmid";
        }
    }

    if (IsSetGenome() && GetGenome() == CBioSource::eGenome_chromosome) {
        ITERATE (CBioSource::TSubtype, sit, GetSubtype()) {
            if ((*sit)->IsSetSubtype() && (*sit)->GetSubtype() == CSubSource::eSubtype_linkage_group) {
                return "eLinkageGroup";
            }
        }
    }

    return "eChromosome";
}


string CBioSource::GetBioprojectLocation(void) const
{
    if (IsSetGenome() && GetGenome() == CBioSource::eGenome_chromosome) {
        return "eNuclearProkaryote";
    }
    if (NStr::Equal(GetBioprojectType(), "ePlasmid")) {
        return "eNuclearProkaryote";
    }

    if (IsSetGenome()) {
        switch (GetGenome()) {
            case CBioSource::eGenome_unknown:
            case CBioSource::eGenome_genomic:
                return "eNuclearProkaryote";
                break;
            case CBioSource::eGenome_mitochondrion:
            case CBioSource::eGenome_kinetoplast:
                return "eMitochondrion";
                break;
            case CBioSource::eGenome_chromosome:
                return "eChromosome";
                break;
            case CBioSource::eGenome_chloroplast:
                return "eChloroplast";
                break;
            case CBioSource::eGenome_chromoplast:
                return "eChromoplast";
                break;
            case CBioSource::eGenome_plastid:
                return "ePlastid";
                break;
            case CBioSource::eGenome_macronuclear:
                return "eMacronuclear";
                break;
            case CBioSource::eGenome_extrachrom:
                return "eExtrachromosomal";
                break;
            case CBioSource::eGenome_cyanelle:
                return "eCyanelle";
                break;
            case CBioSource::eGenome_proviral:
                return "eProviral";
                break;
            case CBioSource::eGenome_virion:
                return "eVirion";
                break;
            case CBioSource::eGenome_nucleomorph:
                return "eNucleomorph";
                break;
            case CBioSource::eGenome_apicoplast:
                return "eApicoplast";
                break;
            case CBioSource::eGenome_leucoplast:
                return "eLeucoplast";
                break;
            case CBioSource::eGenome_proplastid:
                return "eProplastid";
                break;
            case CBioSource::eGenome_endogenous_virus:
                return "eEndogenous-virus";
                break;
            case CBioSource::eGenome_hydrogenosome:
                return "eHydrogenosome";
                break;
            case CBioSource::eGenome_chromatophore:
                return "eChromatophore";
                break;
        }
    }

    return "";
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 64, chars: 1883, CRC32: e1194deb */
