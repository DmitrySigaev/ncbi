#ifndef ALGO_BLAST_BLASTINPUT___BLAST_SCOPE_SRC__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_SCOPE_SRC__HPP

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
 * ===========================================================================
 */

/** @file blast_scope_src.hpp
 * Declares CBlastScopeSource class to create properly configured CScope
 * objects to invoke the BLAST database data loader first and fall back on to
 * the Genbank data loader if the former fails.
 */

#include <algo/blast/core/blast_export.h>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
END_SCOPE(objects)
BEGIN_SCOPE(blast)

/// Configuration structure for the CBlastScopeSource
struct NCBI_XBLAST_EXPORT SDataLoaderConfig {

    /// Default protein BLAST database to use for the BLAST DB data loader
    static const char* kDefaultProteinBlastDb;
    /// Default nucleotide BLAST database to use for the BLAST DB data loader
    static const char* kDefaultNucleotideBlastDb;

    /// Configuration options for the BlastScopeSource
    enum EConfigOpts {
        eUseBlastDbDataLoader = (0x1 << 0),
        eUseGenbankDataLoader = (0x1 << 1),
        eUseNoDataLoaders     = (0x1 << 2),
        eDefault = (eUseBlastDbDataLoader | eUseGenbankDataLoader)
    };

    /// Constructor
    /// @param load_proteins is this object going to load/read proteins only
    /// [in]
    /// @param options configuration options [in]
    SDataLoaderConfig(bool load_proteins, EConfigOpts options = eDefault)
    {
        const string db_name(load_proteins 
                             ? kDefaultProteinBlastDb 
                             : kDefaultNucleotideBlastDb);
        x_Init(options, db_name, load_proteins);
    }

    /// Constructor
    /// @param dbname name of BLAST database [in]
    /// @param protein_data is this object going to load/read proteins only
    /// [in]
    /// @param options configuration options [in]
    SDataLoaderConfig(const string& dbname, bool protein_data,
                      EConfigOpts options = eDefault)
    {
        x_Init(options, dbname, protein_data);
    }

    /// Determine whether either of the data loaders should be used
    bool UseDataLoaders() const { return m_UseBlastDbs || m_UseGenbank; }

    /// Use the BLAST database data loader
    bool m_UseBlastDbs;
    /// Name of the BLAST database to use (only valid if m_UseBlastDbs is true)
    string m_BlastDbName;

    /// Is this intended to load protein sequences
    bool m_IsLoadingProteins;

    /// Use the Genbank data loader
    bool m_UseGenbank;

private:
    /// Initialization method
    /// @param options configuration options [in]
    /// @param dbname name of BLAST database [in]
    /// @param load_proteins is this object going to load/read proteins only
    /// [in]
    void x_Init(EConfigOpts options, const string& dbname, bool load_proteins) 
    {
        m_UseBlastDbs = (options & eUseBlastDbDataLoader) ? true : false;
        m_UseGenbank = (options & eUseGenbankDataLoader) ? true : false;
        m_BlastDbName.assign(dbname);
        m_IsLoadingProteins = load_proteins;
    }
};


/// Class whose purpose is to create CScope objects which have data loaders
/// added with different priorities, so that the BLAST database data loader is
/// used first, then the Genbank data loader.
/// @note all data loaders are registered as non-default data loaders
class NCBI_XBLAST_EXPORT CBlastScopeSource : public CObject 
{
public:
    /// Convenience typedef
    typedef CBlastDbDataLoader::EDbType EDbType;

    /// Constructor which only registers the Genbank data loader
    CBlastScopeSource(CObjectManager* objmgr = NULL);

    /// Constructor with explicit data loader configuration object
    CBlastScopeSource(const SDataLoaderConfig& config,
                      CObjectManager* objmgr = NULL);

    /// Constructor which registers the specified BLAST database handle and 
    /// Genbank data loaders
    CBlastScopeSource(CRef<CSeqDB> db_handle,
                      CObjectManager* objmgr = NULL);

    /// Create a new, properly configured CScope
    CRef<objects::CScope> NewScope();

private:
    /// Our reference to the object manager
    CRef<objects::CObjectManager> m_ObjMgr;
    /// Name of the BLAST database data loader
    string m_BlastDbLoaderName;
    /// Name of the Genbank data loader
    string m_GbLoaderName;

    /// Initializes the BLAST database data loader
    /// @param dbname name of the BLAST database [in]
    /// @param dbtype molecule type of the database above [in]
    void x_InitBlastDatabaseDataLoader(const string& dbname, EDbType dbtype);
    /// Initializes the BLAST database data loader
    /// @param db_handle Handle to a BLAST database [in]
    void x_InitBlastDatabaseDataLoader(CRef<CSeqDB> db_handle);
    /// Initialize the Genbank data loader
    void x_InitGenbankDataLoader();

    /// Data loader priority for BLAST database data loader
    static const int kBlastDbLoaderPriority = 80;
    /// Data loader priority for Genbank data loader
    static const int kGenbankLoaderPriority = 99;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_BLASTINPUT___BLAST_SCOPE_SRC__HPP */
