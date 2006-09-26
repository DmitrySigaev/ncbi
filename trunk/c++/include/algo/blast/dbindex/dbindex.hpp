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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Header file for CDbIndex and some related classes.
 *
 */

#ifndef C_DB_INDEX_HPP
#define C_DB_INDEX_HPP

#include <corelib/ncbiobj.hpp>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_extend.h>

#include "sequence_istream.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

// Template length for discontiguous words. 
//      0 - use contiguous words.
const unsigned long DISC_CONT   = 0UL;  /**< Contiguous words. */
const unsigned long DISC_16     = 1UL;  /**< 16 bp pattern.    */
const unsigned long DISC_18     = 2UL;  /**< 18 bp pattern.    */
const unsigned long DISC_21     = 3UL;  /**< 21 bp pattern.    */

// Template types.
const unsigned long DISC_CODING  = 0UL;  /**< Use coding templates.  */
const unsigned long DISC_OPTIMAL = 1UL;  /**< Use optimal templates. */

// Compression types.
const unsigned long UNCOMPRESSED = 0UL; /**< No compression. */

// Encoding of entries in offset lists.
const unsigned long OFFSET_RAW      = 0UL;      /**< Raw index-wide offset. */
const unsigned long OFFSET_COMBINED = 1UL;      /**< Combination of chunk 
                                                     number and chunk-based 
                                                     offset. */

// Index bit width.
const unsigned long WIDTH_32    = 0UL;  /**< 32-bit index. */
const unsigned long WIDTH_64    = 1UL;  /**< 64-bit index. */

// Sequence input type for index creation.
const unsigned long FASTA       = 0UL;  /**< FASTA formatted input with
                                             masking information encoded
                                             in lower case letters. */

// Is it possible to break sequences between indices?
const unsigned long WHOLE_SEQ   = 0UL;  /**< Index should contain whole sequences.    */
const unsigned long CHUNKS      = 1UL;  /**< Index can be broken on a chunk boundary. */

// Level of progress reporting.
const unsigned long REPORT_QUIET   = 0UL;       /**< No progress reporting. */
const unsigned long REPORT_NORMAL  = 1UL;       /**< Normal reporting. */
const unsigned long REPORT_VERBOSE = 2UL;       /**< Verbose reporting. */

/** Types of exception the indexing library can throw.
  */
class CDbIndex_Exception : public CException
{
    public:

        /** Numerical error codes. */
        enum EErrCode
        {
            eBadOption,         /**< Bad index creation/search option. */
            eBadSequence,       /**< Bad input sequence data. */
            eBadVersion,        /**< Wrong index version. */
            eBadData,           /**< Bad index data. */
            eIO                 /**< I/O error. */
        };

        /** Get a human readable description of the exception type.
            @return string describing the exception type
          */
        virtual const char * GetErrCodeString() const;

        NCBI_EXCEPTION_DEFAULT( CDbIndex_Exception, CException );
};

/** Base class providing high level interface to index objects.
  */
class CDbIndex : public CObject
{
    public:

        /** Letters per byte in the sequence store.
            Sequence data is stored in the index packed 4 bases per byte.
        */
        static const unsigned long CR = 4;

        /** Only process every STRIDEth nmer.
            STRIDE value of 5 allows for search of contiguous seeds of
            length >= 16.
        */
        static const unsigned long STRIDE = 5;          

        /** Offsets below this are reserved for special purposes.
            Bits 0-2 of such an offset represent the distance from
            the start of the Nmer to the next invalid base to the left of
            the Nmer. Bits 3-5 represent the distance from the end of the 
            Nmer to the next invalid base to the right of the Nmer.
        */
        static const unsigned long MIN_OFFSET = 64;     

        /** How many bits are used for special codes for first/last nmers.
            See comment to MIN_OFFSET.
        */
        static const unsigned long CODE_BITS = 3;       

        /** Index version that this library handles. */
        static const unsigned char VERSION = (unsigned char)1;

        /** Simple record type used to specify index creation parameters.

            The following template types are defined:

            <PRE>
            0   - contiguous megablast
            1   - 16 bit pattern, 11 bit hash, coding   pattern: 110,110,110,110,110,1
            2   - 16 bit pattern, 11 bit hash, optimal  pattern: 1,110,010,110,110,111
            3   - 16 bit pattern, 12 bit hash, coding   pattern: 111,110,110,110,110,1
            4   - 16 bit pattern, 12 bit hash, optimal  pattern: 1,110,110,110,110,111
            5   - 18 bit pattern, 11 bit hash, coding   pattern: 10,110,110,010,110,110,1
            6   - 18 bit pattern, 11 bit hash, optimal  pattern: 111,010,010,110,010,111
            7   - 18 bit pattern, 12 bit hash, coding   pattern: 10,110,110,110,110,110,1
            8   - 18 bit pattern, 12 bit hash, optimal  pattern: 111,010,110,010,110,111
            9   - 21 bit pattern, 11 bit hash, coding   pattern: 10,010,110,010,110,010,110,1
            10  - 21 bit pattern, 11 bit hash, optimal  pattern: 111,010,010,100,010,010,111
            11  - 21 bit pattern, 12 bit hash, coding   pattern: 10,010,110,110,110,010,110,1
            12  - 21 bit pattern, 12 bit hash, optimal  pattern: 111,010,010,110,010,010,111
            </PRE>
          */
        struct SOptions
        {
            unsigned long template_type;        /**< Type of the discontiguous pattern. */
            unsigned long compression;          /**< Type of compression to use. */
            unsigned long bit_width;            /**< Creating 32 or 64 bit wide index. */
            unsigned long input_type;           /**< Input format for index creation. */
            unsigned long whole_seq;            /**< Whether the index can be broken on 
                                                  chunk boundaries. */
            unsigned long hkey_width;           /**< Width of the hash key in bits. */
            unsigned long chunk_size;           /**< Long sequences are split into chunks
                                                  of this size. */
            unsigned long chunk_overlap;        /**< Amount by which individual chunks overlap. */
            unsigned long report_level;         /**< Verbose index creation. */
            unsigned long max_index_size;       /**< Maximum index size in megabytes. */
            unsigned long version;              /**< Index format version. */
        };

        /** Type used to enumerate sequences in the index. */
        typedef CSequenceIStream::TStreamPos TSeqNum;

        /** This class represents a set of seeds obtained by searching
            all subjects represented by the index.
        */
        class CSearchResults : public CObject
        {
            /** Each vector item points to results for a particular 
                logical subject. 
            */
            typedef vector< BlastInitHitList * > TResults;

            public:

                /** Object constructor.
                    @param start        [I]     logical subject corresponding to the
                                                first element of the result set
                    @param size         [I]     number of logical subjects covered by
                                                this result set
                */
                CSearchResults( TSeqNum start, TSeqNum size )
                    : start_( start ), results_( size, 0 )
                {}

                /** Get the result set for a particular logical subject.
                    @param seq  [I]     logical subject number
                    @return pointer to a C structure describing the set of seeds
                */
                BlastInitHitList * GetResults( TSeqNum seq ) const
                {
                    if( seq == 0 ) return 0;
                    else if( seq - start_ - 1 >= results_.size() ) return 0;
                    else return results_[seq - start_ - 1];
                }

                /** Set the result set for a given logical subject.
                    @param seq  [I]     logical subject number
                    @param res  [I]     pointer to the C structure describing
                                        the set of seeds
                */
                void SetResults( TSeqNum seq, BlastInitHitList * res )
                {
                    if( seq > 0 && seq - start_ - 1 < results_.size() ) {
                        results_[seq - start_ - 1] = res;
                    }
                }

                /** Object destructor. */
                ~CSearchResults()
                {
                    for( TResults::iterator it = results_.begin();
                            it != results_.end(); ++it ) {
                        if( *it ) {
                            BLAST_InitHitListFree( *it );
                        }
                    }
                }

                /** Get the number of logical sequences in the results set.
                    @return number of sequences in the result set
                */
                TSeqNum NumSeq() const { return results_.size(); }

            private:

                TSeqNum start_;                 /**< Starting logical subject number. */
                TResults results_;              /**< The combined result set. */
        };

        /** Creates an SOptions instance initialized with default values.

          @return instance of SOptions filled with default option values
          */
        static SOptions DefaultSOptions();

        /** Simple record type used to specify index search parameters.
            For description of template types see documentation for
            CDbIndex::SOptions.
          */
        struct SSearchOptions
        {
            unsigned long word_size;            /**< Target seed length. */
            unsigned long template_type;        /**< Type of the discontiguous pattern. */
            unsigned long two_hits;             /**< Use two-hit method (see megablast docs). */
        };

        /** Create an index object.

          Creates an instance of CDbIndex using the named resource as input.
          The name of the resource is given by the <TT>fname</TT> parameter. 
          CDbIndex::SOptions::input_type controls the type of resource used
          as input.

          <TT>stop_chunk</TT> is needed only if <TT>options.whole_seq</TT> is
          set to <TT>CHUNKS</TT>.

          @param fname          [I]     input file name
          @param oname          [I]     output file name
          @param start          [I]     number of the first sequence in the index
          @param start_chunk    [I]     number of the first chunk at which the starting
                                        sequence should be processed
          @param stop           [I/O]   number of the last sequence in the index;
                                        returns the number of the actual last sequece
                                        stored
          @param stop_chunk     [I/O]   number of the last chunk of the last sequence
                                        in the index
          @param options        [I]     index creation parameters
          */
        static void MakeIndex(
                const std::string & fname, 
                const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( fname, start, start_chunk, stop, stop_chunk, options )
          with start_chunk set to 0.
          */
        static void MakeIndex(
                const std::string & fname, 
                const std::string & oname,
                TSeqNum start, 
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options )
        { MakeIndex( fname, oname, start, 0, stop, stop_chunk, options ); }

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( fname, start, stop, stop_chunk, options )
          except that it does not need <TT>stop_chunk</TT> parameter and
          can only be used to create indices containing whole sequences.
          */
        static void MakeIndex(
                const std::string & fname, 
                const std::string & oname,
                TSeqNum start, TSeqNum & stop,
                const SOptions & options
        );

        /** Create an index object.

          Creates an instance of CDbIndex using a given stream as input.
          CDbIndex::Soptions::input_type has no effect in this case.

          <TT>stop_chunk</TT> is needed only if <TT>options.whole_seq</TT> is
          set to <TT>CHUNKS</TT>.

          @param input          [I]     stream for reading sequence and mask information
          @param oname          [I]     output file name
          @param start          [I]     number of the first sequence in the index
          @param start_chunk    [I]     number of the first chunk at which the starting
                                        sequence should be processed
          @param stop           [I/O]   number of the last sequence in the index;
                                        returns the number of the actual last sequece
                                        stored
          @param stop_chunk     [I/O]   number of the last chunk of the last sequence
                                        in the index
          @param options        [I]     index creation parameters
          */
        static void MakeIndex(
                CSequenceIStream & input,
                const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( input, start, start_chunk, stop, stop_chunk, options )
          with start_chunk set to 0.
          */
        static void MakeIndex(
                CSequenceIStream & input, 
                const std::string & oname,
                TSeqNum start, 
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options )
        { return MakeIndex( input, oname, start, 0, stop, stop_chunk, options ); }

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( input, start, stop, stop_chunk, options )
          except that it does not need <TT>stop_chunk</TT> parameter and
          can only be used to create indices containing whole sequences.
          */
        static void MakeIndex(
                CSequenceIStream & input,
                const std::string & oname,
                TSeqNum start, TSeqNum & stop, 
                const SOptions & options
        );

        /** Load index.

          @param fname  [I]     file containing index data

          @return CRef to the loaded index
          */
        static CRef< CDbIndex > Load( const std::string & fname );

        /** Search the index.

          @param query          [I]     the query sequence in BLASTNA format
          @param locs           [I]     which parts of the query to search
          @param search_options [I]     search parameters
          */
        CConstRef< CSearchResults > Search( 
                const BLAST_SequenceBlk * query, 
                const BlastSeqLoc * locs,
                const SSearchOptions & search_options
        );

        /** Return internal subject id by subject id and chunk number.
            @param all_results combined result set for all subjects
            @param subject real subject id
            @param chunk   chunk number
            @return internal if of the sequence corresponding to the
                    given chunk of the given subject sequence
        */
        BlastInitHitList * ExtractResults( 
                CConstRef< CSearchResults > all_results, 
                TSeqNum subject, TSeqNum chunk ) const
        { return DoExtractResults( all_results, subject, chunk ); }

        /** Check if the there are results available for the subject sequence.
            @param all_results  [I]     combined result set for all subjects
            @param oid          [I]     real subject id
            @return     true if there are results for the given oid; 
                        false otherwise
        */
        bool CheckResults( 
                CConstRef< CSearchResults > all_results, TSeqNum oid ) const
        { return DoCheckResults( all_results, oid ); }

        /** Index object destructor. */
        virtual ~CDbIndex() {}

        /** Get the OID of the first sequence in the index.
            @return OID of the first sequence in the index
        */
        TSeqNum StartSeq() const { return start_; }

        /** Get the number of the first chunk of the first sequence 
            in the index.
            @return the number of the first sequence chunk in the index
        */
        TSeqNum StartChunk() const { return start_chunk_; }

        /** Get the OID of the last sequence in the index.
            @return OID of the last sequence in the index
        */
        TSeqNum StopSeq() const { return stop_; }

        /** Get the number of the last chunk of the last sequence 
            in the index.
            @return the number of the last sequence chunk in the index
        */
        TSeqNum StopChunk() const { return stop_chunk_; }

    protected:

        /** Version specific index loading function.
            @param is           [I/O]   IO stream with index data
            @param version      [I]     index format version
            @return object containing loaded index data
        */
        static CRef< CDbIndex > LoadByVersion( 
                CNcbiIstream & is, unsigned long version );

    private:

        /** Load index from an open stream.
            @param VERSION      index format version
            @param is   [I]     stream containing index data
            @return object containing loaded index data
        */
        template< unsigned long VERSION >
        static CRef< CDbIndex > LoadIndex( CNcbiIstream & is );

        /** Load index from a named file.
            Usually this is used to memmap() the file data into
            the index structure.
            @param VERSION      index format version
            @param fname        [I]     index file name
            @return object containing loaded index data
        */
        template< unsigned long VERSION >
        static CRef< CDbIndex > LoadIndex( const std::string & fname );

        /** Actual implementation of seed searching.
            Must be implemented by child classes.
            @sa Search
        */
        virtual CConstRef< CSearchResults > DoSearch(
                const BLAST_SequenceBlk *, 
                const BlastSeqLoc *,
                const SSearchOptions & )
        { return CConstRef< CSearchResults >( null ); }

        /** Actual result extraction procedure.
            Must be implemented by child classes.
            @sa ExtractResults
        */
        virtual BlastInitHitList * DoExtractResults( 
                CConstRef< CSearchResults >, 
                TSeqNum, TSeqNum ) const
        { return 0; }

        /** Actual result checking procedure.
            Must be implemented by child classes.
            @sa CheckResults
        */
        virtual bool DoCheckResults(
                CConstRef< CSearchResults >, TSeqNum ) const
        { return false; }

        TSeqNum start_;         /**< OID of the first sequence in the index. */
        TSeqNum start_chunk_;   /**< Number of the first chunk of the first sequence. */
        TSeqNum stop_;          /**< OID of the last sequence in the inex. */
        TSeqNum stop_chunk_;    /**< Number of the last chunk of the last sequence. */
};

//-------------------------------------------------------------------------
/** Metafunction that computes the base word type from the index width. */
template< unsigned long WIDTH > struct SWord {};
template<> struct SWord< WIDTH_32 > { typedef Uint4 type; };
template<> struct SWord< WIDTH_64 > { typedef Uint8 type; };

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

#endif

