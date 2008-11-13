#include <ncbi_pch.hpp>
#include "coligofarapp.hpp"
#include "cprogressindicator.hpp"
#include "cseqscanner.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "cfilter.hpp"
#include "cbatch.hpp"
#include "csnpdb.hpp"

#include "string-util.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <sys/resource.h>
#else
#define strtoll( a, b, c ) strtoui64( a, b, c )
#endif

USING_OLIGOFAR_SCOPES;

#ifndef OLIGOFAR_VERSION
#define OLIGOFAR_VERSION "3.29" 
#endif

COligoFarApp::COligoFarApp( int argc, char ** argv ) :
    CApp( argc, argv ),
    m_hashPass( 0 ),
    m_maxHashAmb( 4 ),
    m_maxFastaAmb( 4 ),
    m_strands( 0x03 ),
    m_readsPerRun( 250000 ),
    m_phrapSensitivity( 4 ),
    m_xdropoff( 2 ),
    m_topCnt( 10 ),
    m_topPct( 99 ),
    m_minPctid( 40 ),
    m_maxSimplicity( 2.0 ),
    m_identityScore(  1.0 ),
    m_mismatchScore( -1.0 ),
    m_gapOpeningScore( -3.0 ),
    m_gapExtentionScore( -1.5 ),
    m_minPair( 100 ),
    m_maxPair( 200 ),
    m_pairMargin( 20 ),
    m_qualityChannels( 0 ),
    m_qualityBase( 33 ),
    m_minBlockLength( 1000 ),
    m_guideFilemaxMismatch( 0 ),
    m_memoryLimit( Uint8( sizeof(void*) == 4 ? 3 : 8 ) * int(kGigaByte) ),
    m_performTests( false ),
    m_colorSpace( false ),
    m_sodiumBisulfiteCuration( false ),
    m_alignmentAlgo( eAlignment_fast ),
#ifdef _WIN32
    //m_guideFile( "nul:" ),
    m_outputFile( "con:" ),
#else
    m_guideFile( "/dev/null" ),
    m_outputFile( "/dev/stdout" ),
#endif
    m_outputFlags( "m" ),
    m_geometry( "p" )
{
    m_hashParam.push_back( CHashParam() );
#ifndef _WIN32
    ifstream meminfo( "/proc/meminfo" );
    string buff;
    if( !meminfo.fail() ) {
        m_memoryLimit = 0;

        while( getline( meminfo, buff ) ) {
            istringstream line(buff);
            string name, units;
            Uint8 value;
            line >> name >> value >> units;
            if( units.length() ) {
                switch( tolower( units[0] ) ) {
                case 'g': value *= kKiloByte;
                case 'm': value *= kKiloByte;
                case 'k': value *= kKiloByte;
                }
            }
            if( name == "MemFree:" || name == "Buffers:" || name == "Cached:" || name == "SwapCached:" ) {
                m_memoryLimit += value;
            }
        }
    }
#endif
}

int COligoFarApp::RevNo()
{
    return strtol( "$Rev$"+6, 0, 10 );
}

void COligoFarApp::Version( const char * )
{
    cout << GetProgramBasename() << " ver " OLIGOFAR_VERSION " (Rev:" << RevNo() << ") " NCBI_SIGNATURE << endl;
}

// letter   lower       letter  upper
// a        ambCnt      A       ambCnt
// b        snpdb       B       batchSz
// c        colorsp     C       /config/
// d        database    D       pairRange
// e        maxGaps     E
// f                    F       dustScore
// g        guidefile   G       gapScore
// h        help        H       hashBits
// i        input       I       idScore
// j                    J       
// k        skipPos     K       
// l        gilist      L       memLimit
// m        margin      M       mismScore
// n        maxMism     N       maxWindows
// o        output      O       outputFmt
// p        pctCutoff   P       phrapScore
// q        qualChn     Q       gapExtScore
// r        alignAlgo   R       geometry
// s        strands     S       stride
// t        topPct      T       runTests
// u        topCnt      U       assertVer
// v                    V       version
// w        win/word    W       POSIX 
// x        guideMism   X       xDropoff
// y        onlySeqid   Y
// z                    Z
// 0        baseQual    5
// 1        solexa1     6
// 2        solexa2     7
// 3                    8
// 4                    9

void COligoFarApp::Help( const char * arg )
{
    enum EFlags {
        fSynopsis = 0x01,
        fDetails  = 0x02,
        fExtended = 0x04
    };
    int flags = fSynopsis;
    if( arg ) {
        switch( *arg ) {
        case 'b': case 'B': break;
        case 'e': case 'E': flags = fExtended; break;
        case 'f': case 'F': flags = fDetails; break;
        }
    }
    if( flags & fSynopsis ) 
        cout << "usage: [-hV] [--help[=full|brief|extended]] [-U version]\n"
             << "  [-i inputfile] [-d genomedb] [-b snpdb] [-g guidefile] [-l gilist|-y seqID]\n"
             << "  [-1 solexa1] [-2 solexa2] [-q 0|1] [-0 qbase] [-c +|-] [-o output]\n"
             << "  [-O -eumxtadh] [-B batchsz] [-x 0|1|2] [-s 1|2|3] [-k skipPos]\n"
             << "  [--pass0] [-w win[/word]] [-N wcnt] [-n mism] [-e gaps] [-S stride] [-H bits]\n"
             << "  [--pass1  [-w win[/word]] [-N wcnt] [-n mism] [-e gaps] [-S stride] [-H bits] ]\n"
             << "  [-r f|s] [-a maxamb] [-A maxamb] [-P phrap] [-F dust]\n"
             << "  [-X xdropoff] [-I idscore] [-M mismscore] [-G gapcore] [-Q gapextscore]\n"
             << "  [-D minPair[-maxPair]] [-m margin] [-R geometry]\n"
             << "  [-p cutoff] [-u topcnt] [-t toppct] [-L memlimit] [-T +|-]\n";
    if( flags & fDetails ) {
        cout 
            << "\nFile options:\n" 
            << "   --input-file=file         -i file       short reads input file [" << m_readFile << "]\n"
            << "   --fasta-file=file         -d file       database (fasta or basename of blastdb) file [" << m_fastaFile << "]\n"
            << "   --snpdb-file=file         -b file       snp database subject file [" << m_snpdbFile << "]\n"
            << "   --guide-file=file         -g file       guide file (output of sr-search [" << m_guideFile << "]\n"
            << "   --gi-list=file            -l file       gi list to use for the blast db [" << m_gilistFile << "]\n"
            << "   --qual-1-file=file        -1 file       read 1 4-channel quality file [" << m_read1qualityFile << "]\n"
            << "   --qual-2-file=file        -2 file       read 2 4-channel quality file [" << m_read2qualityFile << "]\n"
            << "   --output-file=output      -o output     set output file [" << m_outputFile << "]\n"
            << "   --only-seqid=seqId        -y seqId      make database scan only seqIds indicated here [" << Join( ", ", m_seqIds ) << "]\n"
            << "   --guide-max-mism=count    -x count      set maximal number of mismatches for hits in guide file [" << m_guideFilemaxMismatch << "]\n"
            << "   --colorspace=+|-          -c +|-       *reads are set in dibase colorspace [" << (m_colorSpace?"yes":"no") << "]\n"
            << "   --quality-channels=cnt    -q 0|1        number of channels in input file quality columns [" << m_qualityChannels << "]\n"
            << "   --quality-base=value      -0 value      base quality number (ASCII value for character representing phrap score 0) [" << m_qualityBase << "]\n"
            << "   --quality-base=+char      -0 +char      base quality char (character representing phrap score 0) [+" << char(m_qualityBase) << "]\n"
            << "   --output-flags=flags      -O flags      add output flags (-huxmtdae) [" << m_outputFlags << "]\n"
            << "   --batch-size=count        -B count      how many short seqs to map at once [" << m_readsPerRun << "]\n"
            << "   --NaHSO3=+|-                            subject sequences sodium bisulfite curation [" << (m_sodiumBisulfiteCuration?"yes":"no") << "]\n"
//            << "  -C config     --config-file=file         take parameters from config file section `oligofar' and continue parsing commandline\n"
            << "\nGeneral hashing and scanning options:\n"
            << "   --window-skip=pos[,...]   -k pos[,...]  skip read positions when hashing (1-based) [" << Join( ",", m_skipPositions ) << "]\n"
            << "   --input-max-amb=amb       -a amb        maximal number of ambiguities in hash window [" << m_maxHashAmb << "]\n"
            << "   --fasta-max-amb=amb       -A amb        maximal number of ambiguities in fasta window [" << m_maxFastaAmb << "]\n"
            << "   --phrap-cutoff=score      -P score      set maximal phrap score to consider base as ambiguous [" << m_phrapSensitivity << "]\n"
            << "   --max-simplicity=val      -F simpl      low complexity filter cutoff for hash window [" << m_maxSimplicity << "]\n"
            << "   --strands=1|2|3           -s 1|2|3      hash and lookup for strands (bitmask: 1 for +, 2 for -, 3 for both) [" << m_strands << "]\n"
            ;
    
        cout
            << "\nPass-specific hashing and scanning options:\n";
        for( unsigned i = 0; i < max( size_t(2), m_hashParam.size() ); ++i ) {
            cout 
                << "                --pass" << i << "                    following options will be used for pass " << i;
            if( i >= m_hashParam.size() ) cout << " [off]\n";
            else {
                cout 
                    << ":\n"
                    << "   --window-size=win[/word] -w win[/word]  hash using window size and word size [" << m_hashParam[i].GetWindowSize() << "/" << m_hashParam[i].GetWordSize() << "]\n"
                    << "   --stride-size=stride     -S stride      hash with given stride size [" << m_hashParam[i].GetStrideSize() << "]\n"
                    << "   --max-windows=count      -N count       hash using maximum number of consecutive windows [" << m_hashParam[i].GetWindowCount() << "]\n"
                    << "   --max-mism=mismatch      -n mismatch    hash allowing up to given number of mismatches (0-2) [" << m_hashParam[i].GetHashMismatches() << "]\n"
                    << "   --max-indels=indel       -e indel       hash allowing up to given number of indels (0-1) [" << m_hashParam[i].GetHashIndels() << "]\n"
                    << "   --index-bits=bits        -H bits        set number of bits for index part of hash table [" << m_hashParam[i].GetHashBits() << "]\n"   
                    ;
            }
        }
        cout
            << "\nAlignment and scoring options:\n"
            << "   --algorithm=f|s           -r f|q|s      use alignment algoRithm (Fast or Smith-waterman) [" << char(m_alignmentAlgo) << "]\n"
            << "   --x-dropoff=value         -X value      set half band width for alignment [" << m_xdropoff << "]\n"
            << "   --identity-score=score    -I score      set identity score [" << m_identityScore << "]\n"
            << "   --mismatch-score=score    -M score      set mismatch score [" << m_mismatchScore << "]\n"
            << "   --gap-opening-score=score -G score      set gap opening score [" << m_gapOpeningScore << "]\n"
            << "   --gap-extention-score=val -Q score      set gap extention score [" << m_gapExtentionScore << "]\n"
            << "\nFiltering and ranking options:\n"
            << "   --min-pctid=pctid         -p pctid      set global percent identity cutoff [" << m_minPctid << "]\n"
            << "   --top-count=val           -u topcnt     maximal number of top hits per read [" << m_topCnt << "]\n"
            << "   --top-percent=val         -t toppct     maximal score of hit (in % to best) to be reported [" << m_topPct << "]\n"
            << "   --pair-distance=min[-max] -D min[-max]  pair distance [" << m_minPair << "-" << m_maxPair << "]\n"
            << "   --pair-margin=len         -m dist       pair distance margin [" << m_pairMargin << "]\n"
            << "   --geometry=value          -R value      restrictions on relative hit orientation and order for paired hits [" << (m_geometry) << "]\n"
            << "\nOther options:\n"
            << "   --help=[brief|full|ext]   -h            print help with current parameter values and exit after parsing cmdline\n"
            << "   --version                 -V            print version and exit after parsing cmdline\n"
            << "   --assert-version=version  -U version    make sure that the oligofar version is what expected [" OLIGOFAR_VERSION "]\n"
            << "   --memory-limit=value      -L value      set rlimit for the program (k|M|G suffix is allowed) [" << m_memoryLimit << "]\n"
            << "   --test-suite=+|-          -T +|-        turn test suite on/off [" << (m_performTests?"on":"off") << "]\n"
            << "\nRelative orientation flags recognized:\n"
            << "     p|centripetal|inside|pcr|solexa       reads are oriented so that vectors 5'->3' pointing to each other\n"
            << "     f|centrifugal|outside                 reads are oriented so that vectors 5'->3' are pointing outside\n"
            << "     i|incr|incremental|solid              reads are on same strand, first preceeds second on this strand\n"
            << "     d|decr|decremental                    reads are on same strand, first succeeds second on this strand\n"
            << "\nOutput flags (for -O):\n"
            << "     -   reset all flags\n"
            << "     h   report all hits before ranking\n"
            << "     u   report unmapped reads\n"
            << "     x   indicate that there are more reads of this rank\n"
            << "     m   indicate that there are more reads of lower ranks\n"
            << "     t   indicate that there were no more hits\n"
            << "     d   report differences between query and subject\n"
            << "     a   report alignment details\n"
            << "     e   print empty line after all hits of the read are reported\n"
            << "\nNB: although -L flag is optional, it is strongly recommended to use it!\n"
            ;
    }
    if( flags & fExtended ) 
        cout << "\nExtended options:\n"
             << "   --min-block-length=bases   Length for subject sequence to be scanned at once [" << m_minBlockLength << "]\n"
             ;
}

const option * COligoFarApp::GetLongOptions() const
{
    static struct option opt[] = {
        {"help", 2, 0, 'h'},
        {"version", 0, 0, 'V'},
        {"assert-version", 1, 0, 'U'},
        {"window-size", 1, 0, 'w'},
        {"window-skip",1,0,'k'},
        {"max-windows",1,0,'N'},
        {"input-mism", 1, 0, 'n'},
        {"input-gaps", 1, 0, 'e'},
        {"input-max-amb", 1, 0, 'a'},
        {"fasta-max-amb", 1, 0, 'A'},
        {"colorspace", 1, 0, 'c'},
        {"NaHSO3", 1, 0, kLongOpt_NaHSO3},
        {"input-file", 1, 0, 'i'},
        {"fasta-file", 1, 0, 'd'},
        {"snpdb-file", 1, 0, 'b'},
        {"vardb-file", 1, 0, 'v'},
        {"guide-file", 1, 0, 'g'},
        {"output-file", 1, 0, 'o'},
        {"output-flags", 1, 0, 'O'},
//        {"config-file", 1, 0, 'C'},
        {"only-seqid", 1, 0, 'y'},
        {"gi-list", 1, 0, 'l'},
        {"strands", 1, 0, 's'},
        {"batch-size", 1, 0, 'B'},
        {"guide-max-mism", 1, 0, 'x'},
        {"min-pctid", 1, 0, 'p'},
        {"top-count", 1, 0, 'u'},
        {"top-percent", 1, 0, 't'},
        {"qual-1-file", 1, 0, '1'},
        {"qual-2-file", 1, 0, '2'},
        {"quality-channels", 1, 0, 'q'},
        {"quality-base", 1, 0, '0'},
        {"phrap-score", 1, 0, 'P'},
        {"pair-distance", 1, 0, 'D'},
        {"pair-margin", 1, 0, 'm'},
        {"geometry", 1, 0, 'R'},
        {"max-simplicity", 1, 0, 'F'},
        {"algorithm", 1, 0, 'r'},
        {"identity-score", 1, 0, 'I'},
        {"mismatch-score", 1, 0, 'M'},
        {"gap-opening-score", 1, 0, 'G'},
        {"gap-extention-score", 1, 0, 'Q'},
        {"x-dropoff", 1, 0, 'X'},
        {"memory-limit", 1, 0, 'L'},
        {"test-suite", 1, 0, 'T'},
        {"index-bits", 1, 0, 'H'},
        {"pass0", 0, 0, kLongOpt_pass0},
        {"pass1", 0, 0, kLongOpt_pass1},
        {"min-block-length", 1, 0, kLongOpt_min_block_length },
        {0,0,0,0}
    };
    return opt;
}

const char * COligoFarApp::GetOptString() const
{
    return "U:H:S:w:N:k:n:e:a:A:c:i:d:b:v:g:o:O:l:y:s:B:x:p:u:t:1:2:q:0:P:m:D:R:F:r:I:M:G:Q:X:L:T:";
}

int COligoFarApp::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case kLongOpt_min_block_length: m_minBlockLength = NStr::StringToInt( arg ); break;
    case kLongOpt_NaHSO3: m_sodiumBisulfiteCuration = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case kLongOpt_pass0: m_hashPass = 0; break;
    case kLongOpt_pass1: if( m_hashParam.size() < 2 ) m_hashParam.push_back( m_hashParam.back() ); m_hashPass = 1; break;
    case 'U': if( strcmp( arg, OLIGOFAR_VERSION ) ) THROW( runtime_error, "Expected oligofar version " << arg << ", called " OLIGOFAR_VERSION ); break;
//    case 'C': ParseConfig( arg ); break;
    case 'k': 
        do {
            list<string> x;
            Split( arg, ",", back_inserter( x ) );
            ITERATE( list<string>, t, x ) m_skipPositions.insert( m_skipPositions.end(), NStr::StringToInt( *t ) );
        } while(0);
        break;
    case 'w': 
        do {
            int win, word;
            ParseRange( win, word, arg, "/" ); 
            m_hashParam[m_hashPass].SetWindowSize( win );
            m_hashParam[m_hashPass].SetWordSize( word );
        } while(0);
        break;
    case 'N': m_hashParam[m_hashPass].SetWindowCount( NStr::StringToInt( arg ) ); break;
    case 'H': m_hashParam[m_hashPass].SetHashBits( NStr::StringToInt( arg ) ); break;
    case 'S': m_hashParam[m_hashPass].SetStrideSize( NStr::StringToInt( arg ) ); break;
    case 'n': m_hashParam[m_hashPass].SetHashMismatches( NStr::StringToInt( arg ) ); break;
    case 'e': m_hashParam[m_hashPass].SetHashIndels( NStr::StringToInt( arg ) > 0 ); break;
    case 'a': m_maxHashAmb = strtol( arg, 0, 10 ); break;
    case 'A': m_maxFastaAmb = strtol( arg, 0, 10 ); break;
    case 'c': m_colorSpace = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case 'i': m_readFile = arg; break;
    case 'd': m_fastaFile = arg; break;
    case 'b': m_snpdbFile = arg; break;
    case 'v': m_vardbFile = arg; break;
    case 'g': m_guideFile = arg; break;
    case 'o': m_outputFile = arg; break;
    case 'O': m_outputFlags += arg; if( const char * m = strrchr( m_outputFlags.c_str(), '-' ) ) m_outputFlags = m + 1; break;
    case 'l': m_gilistFile = arg; break;
    case 'y': m_seqIds.push_back( CSeq_id( arg ).AsFastaString() ); break;
    case 's': m_strands = strtol( arg, 0, 10 ); break;
    case 'B': m_readsPerRun = strtol( arg, 0, 10 ); break;
    case 'x': m_guideFilemaxMismatch = strtol( arg, 0, 10 ); break;
    case 'p': m_minPctid = NStr::StringToDouble( arg ); break;
    case 'u': m_topCnt = strtol( arg, 0, 10 ); break;
    case 't': m_topPct = NStr::StringToDouble( arg ); break;
    case '1': m_read1qualityFile = arg; break;
    case '2': m_read2qualityFile = arg; break;
    case 'q': m_qualityChannels = NStr::StringToInt( arg ); break;
    case '0': m_qualityBase = ( arg[0] && arg[0] == '+' ) ? arg[1] : NStr::StringToInt( arg ); break;
    case 'P': m_phrapSensitivity = strtol( arg, 0, 10 ); break;
    case 'D': ParseRange( m_minPair, m_maxPair, arg ); break;
    case 'm': m_pairMargin = strtol( arg, 0, 10 ); break;
    case 'R': m_geometry = arg; break;
    case 'F': m_maxSimplicity = NStr::StringToDouble( arg ); break;
    case 'r': 
        switch( *arg ) {
        case 'f': case 'F': m_alignmentAlgo = eAlignment_fast ; break;
        case 's': case 'S': m_alignmentAlgo = eAlignment_SW ; break;
        case 'g': case 'Q': THROW( runtime_error, "Quick algorithm is not supported in the current version" ); //m_alignmentAlgo = eAlignment_quick ; break;
        default: THROW( runtime_error, "Bad alignment algorithm option " << arg );
        }
        break;
    case 'I': m_identityScore = fabs( NStr::StringToDouble( arg ) ); break;
    case 'M': m_mismatchScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'G': m_gapOpeningScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'Q': m_gapExtentionScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'X': m_xdropoff = abs( strtol( arg, 0, 10 ) ); break;
    case 'L':
#ifndef _WIN32
        do {
            char * t = 0;
            m_memoryLimit = strtoll( arg, &t, 10 );
            if( t ) {
                switch( tolower(*t) ) {
                case 'g': m_memoryLimit *= 1024;
                case 'm': m_memoryLimit *= 1024;
                case 'k': m_memoryLimit *= 1024;
                }
            }
        } while(0);
#else
        cerr << "[" << GetProgramBasename() << "] Warning: -L is ignored in win32\n";
#endif
        break;
    case 'T': m_performTests = (*arg == '+') ? true : (*arg == '-') ? false : NStr::StringToBool( arg ); break;
    default: return CApp::ParseArg( opt, arg, longindex );
    }
    return 0;
}

void COligoFarApp::ParseConfig( const string& cfg ) 
{
    ifstream in( cfg.c_str() );
    if( !in.good() ) THROW( runtime_error, "Failed to read config file " << cfg );
    CNcbiRegistry reg( in );
    ParseConfig( &reg );
}

void COligoFarApp::ParseConfig( IRegistry * reg )
{
    const option * opts = GetLongOptions();
    for( int index = 0 ; opts && opts->name != 0 ; ++opts, ++index ) {
        if( reg->HasEntry( "oligofar", opts->name ) ) {
            ParseArg( opts->val, reg->Get( "oligofar", opts->name ).c_str(), index );
        }
    }
}

int COligoFarApp::RunTestSuite()
{
    if( m_performTests ) {
        if( int rc = TestSuite() ) {
            cerr << "[" << GetProgramBasename() << "] internal tests failed: " << rc << endl;
            return rc;
        } else {
            cerr << "[" << GetProgramBasename() << "] internal tests succeeded!\n";
        }
    }
    return 0;
}

int COligoFarApp::SetLimits()
{
#ifndef _WIN32
    struct rlimit rl;
    rl.rlim_cur = m_memoryLimit;
    rl.rlim_max = RLIM_INFINITY;
    cerr << "[" << GetProgramBasename() << "] Setting memory limit to " << m_memoryLimit << ": ";
    errno = 0;
    int rc = setrlimit( RLIMIT_AS, &rl );
    cerr << strerror( errno ) << endl;
    return rc;
#else
    cerr << "[" << GetProgramBasename() << "] Setting memory limit is not implemented for win32 yet, ignored.\n";
    return 0;
#endif
}

int COligoFarApp::Execute()
{
    if( int rc = RunTestSuite() ) return rc;
    if( int rc = SetLimits() ) return rc;
    return ProcessData();
}

int COligoFarApp::GetOutputFlags() const
{
    int oflags = 0;
    for( const char * f = m_outputFlags.c_str(); *f; ++f ) {
        switch( tolower(*f) ) {
        case '-': oflags = 0; break;
        case 'e': oflags |= COutputFormatter::fReportEmptyLines; break;
        case 'u': oflags |= COutputFormatter::fReportUnmapped; break;
        case 'm': oflags |= COutputFormatter::fReportMany; break;
        case 'x': oflags |= COutputFormatter::fReportMore; break;
        case 't': oflags |= COutputFormatter::fReportTerminator; break;
        case 'a': oflags |= COutputFormatter::fReportAlignment; break;
        case 'd': oflags |= COutputFormatter::fReportDifferences; break;
        case 'h': oflags |= COutputFormatter::fReportAllHits; break;
        default: cerr << "[" << GetProgramBasename() << "] Warning: unknown format flag `" << *f << "'\n"; break;
        }
    }
    return oflags;
}

void COligoFarApp::SetupGeometries( map<string,int>& geometries )
{
    geometries.insert( make_pair("p",CFilter::eCentripetal) );
    geometries.insert( make_pair("centripetal",CFilter::eCentripetal) );
    geometries.insert( make_pair("inside",CFilter::eCentripetal) );
    geometries.insert( make_pair("pcr",CFilter::eCentripetal) );
    geometries.insert( make_pair("solexa",CFilter::eCentripetal) );
    geometries.insert( make_pair("f",CFilter::eCentrifugal) );
    geometries.insert( make_pair("centrifugal",CFilter::eCentrifugal) );
    geometries.insert( make_pair("outside",CFilter::eCentrifugal) );
    geometries.insert( make_pair("i",CFilter::eIncremental) );
    geometries.insert( make_pair("incr",CFilter::eIncremental) );
    geometries.insert( make_pair("incremental",CFilter::eIncremental) );
    geometries.insert( make_pair("solid",CFilter::eIncremental) );
    geometries.insert( make_pair("d",CFilter::eDecremental) );
    geometries.insert( make_pair("decr",CFilter::eDecremental) );
    geometries.insert( make_pair("decremental",CFilter::eDecremental) );
}

int COligoFarApp::ProcessData()
{
    switch( m_qualityChannels ) {
        case 0: case 1: break;
        default: THROW( runtime_error, "Quality channels supported in main input file columns are 0 or 1" );
    }

    for( unsigned p = 0; p < m_hashParam.size(); ++p ) {
        string msg;
        if( ! m_hashParam[p].ValidateParam( m_skipPositions, msg ) ) {
            THROW( runtime_error, "Incompatible set of hash parameters for pass" << p << ( m_skipPositions.size() ? " with skip ppositions:" : ":" ) << msg );
        }
    }
    
    ofstream o( m_outputFile.c_str() );
    ifstream reads( m_readFile.c_str() ); // to format output

    CSeqCoding::ECoding sbjCoding = m_colorSpace ? CSeqCoding::eCoding_colorsp : CSeqCoding::eCoding_ncbi8na;
    CSeqCoding::ECoding qryCoding = sbjCoding;

    auto_ptr<ifstream> i1(0), i2(0);
    if( m_read1qualityFile.length() ) {
        if( m_colorSpace ) THROW( runtime_error, "Have no idea how to use 4-channel quality files and colorspace at the same time" ); 
        qryCoding = CSeqCoding::eCoding_ncbipna;
        i1.reset( new ifstream( m_read1qualityFile.c_str() ) );
        if( i1->fail() )
            THROW( runtime_error, "Failed to open file [" << m_read1qualityFile << "]: " << strerror(errno) );
    } else if( m_qualityChannels == 1 ) {
        if( m_colorSpace ) THROW( runtime_error, "Have no idea how to use 1-channel quality scores and colorspace at the same time" ); 
        qryCoding = CSeqCoding::eCoding_ncbiqna;
    }
    if( m_read2qualityFile.length() ) {
        if( ! m_read1qualityFile.length() )
            THROW( runtime_error, "if -2 switch is used, -1 should be present, too" );
        i2.reset( new ifstream( m_read2qualityFile.c_str() ) );
        if( i2->fail() )
            THROW( runtime_error, "Failed to open file [" << m_read2qualityFile << "]: " << strerror(errno) );
    }
    map<string,int> geometries;
    SetupGeometries( geometries );

    if( geometries.find( m_geometry ) == geometries.end() ) {
        THROW( runtime_error, "Unknown geometry `" << m_geometry << "'" );
    }

    for( vector<CHashParam>::iterator p = m_hashParam.begin(); p != m_hashParam.end(); ++p ) {
        if( p->GetWindowSize() + p->GetStrideSize() - 1 + ( p->GetHashIndels() ? 0 : 1 ) > 32 ) {
            THROW( runtime_error, "Sorry, constraint ($w + $S + $e - 1) <= 32 is violated, can't proceed ($w - winsz, $S - strides, $e - indels)" );
        }
        if( p->GetWordSize() * 2 < p->GetWindowSize() ) {
            p->SetWordSize( ( p->GetWindowSize() + 1 ) / 2 );
            cerr << "[" << GetProgramBasename() << "] WARNING: Increasing word size to " << p->GetWordSize() << " bases to cover window\n";
        }
        if( p->GetWordSize()*2 - 16 >= 32 ) {
            p->SetWordSize( 23 );
            cerr << "[" << GetProgramBasename() << "] WARNING: Decreasing word size to " << p->GetWordSize() << " as required for hashing algorithm\n";
        }
        if( p->GetWordSize()*2 - p->GetHashBits() > 16 ) {
            p->SetHashBits( p->GetWordSize()*2 - 16 );
            cerr << "[" << GetProgramBasename() << "] WARNING: Increasing hash bits to " << p->GetHashBits() << " to fit " << p->GetWordSize() << "-base word\n";
        }
    }

    CSeqIds seqIds;
    CFilter filter;
    CSeqVecProcessor seqVecProcessor;
    CSeqScanner seqScanner;
    COutputFormatter formatter( o, seqIds );
    CQueryHash queryHash( 0, 0 ); // TODO: fix this
    CScoreTbl scoreTbl( m_identityScore, m_mismatchScore, m_gapOpeningScore, m_gapExtentionScore );
    CGuideFile guideFile( m_guideFile, filter, seqIds );
    CBatch batch( m_readsPerRun, m_fastaFile, queryHash, seqVecProcessor, filter, formatter, scoreTbl );

    CSnpDb snpDb( CSnpDb::eSeqId_integer );
    if( m_snpdbFile.length() ) {
        snpDb.Open( m_snpdbFile, CBDB_File::eReadOnly );
        seqScanner.SetSnpDb( &snpDb );
    }

    batch.SetAligner( CBatch::eAligner_noIndel, CreateAligner( eAlignment_HSP, &scoreTbl ), true );
    batch.SetAligner( CBatch::eAligner_regular, CreateAligner( m_xdropoff ? m_alignmentAlgo : eAlignment_HSP, &scoreTbl ), true );
    batch.SetHashParam( m_hashParam );
    
    guideFile.SetMismatchPenalty( scoreTbl );
    guideFile.SetMaxMismatch( m_guideFilemaxMismatch );

    formatter.AssignFlags( GetOutputFlags() );
    formatter.SetGuideFile( guideFile );
    formatter.SetAligner( batch.GetAligner( CBatch::eAligner_regular ) );

    filter.SetGeometry( geometries[m_geometry] );
    filter.SetSeqIds( &seqIds );
    filter.SetMaxDist( m_maxPair + m_pairMargin );
    filter.SetMinDist( m_minPair - m_pairMargin );
    filter.SetTopPct( m_topPct );
    filter.SetTopCnt( m_topCnt );
    filter.SetScorePctCutoff( m_minPctid );
    filter.SetOutputFormatter( &formatter );
    filter.SetAligner( batch.GetAligner( CBatch::eAligner_regular ) );

    if( m_minPctid > m_topPct ) {
        cerr << "[" << GetProgramBasename() << "] Warning: top% is greater then %cutoff ("
             << m_topPct << " < " << m_minPctid << ")\n";
    }

//    for( TSkipPositions::iterator i = m_skipPositions.begin(); i != m_skipPositions.end(); ++i ) --*i; // 1-based to 0-based

    queryHash.SetStrands( m_strands );
    queryHash.SetSkipPositions( m_skipPositions );
    queryHash.SetMaxSimplicity( m_maxSimplicity );
    queryHash.SetMaxAmbiguities( m_maxHashAmb );
    queryHash.SetNcbipnaToNcbi4naScore( Uint2( 255 * pow( 10.0, double(m_phrapSensitivity)/10) ) );
    queryHash.SetNcbiqnaToNcbi4naScore( m_phrapSensitivity );

    seqScanner.SetFilter( &filter );
    seqScanner.SetQueryHash( &queryHash );
    seqScanner.SetSeqIds( &seqIds );
    seqScanner.SetMaxAmbiguities( m_maxFastaAmb );
    seqScanner.SetMaxSimplicity( m_maxSimplicity );
    seqScanner.SetMinBlockLength( m_minBlockLength );
    seqScanner.SetInputChunk( batch.GetInputChunk() );
    seqScanner.SetSodiumBisulfateCuration( m_sodiumBisulfiteCuration );

    seqVecProcessor.SetTargetCoding( sbjCoding );
    seqVecProcessor.AddCallback( 1, &filter );
    seqVecProcessor.AddCallback( 0, &seqScanner );

    if( m_seqIds.size() ) seqVecProcessor.SetSeqIdList( m_seqIds );
    if( m_gilistFile.length() ) seqVecProcessor.SetGiListFile( m_gilistFile );

    string buff;
    Uint8 queriesTotal = 0;
    Uint8 entriesTotal = 0;

    CProgressIndicator p( "Reading " + m_readFile, "lines" );
    batch.SetReadProgressIndicator( &p );
    batch.Start();

    for( int count = 0; getline( reads, buff ); ++count ) {
        if( buff.length() == 0 ) continue;
        if( buff[0] == '#' ) continue; // ignore comments
        istringstream iline( buff );
        string id, fwd, rev;
        iline >> id >> fwd;
        if( !iline.eof() ) iline >> rev;
        if( rev == "-" ) rev = "";
        if( i1.get() ) {
            if( !getline( *i1, fwd ) )
                THROW( runtime_error, "File [" << m_read1qualityFile << "] is too short" );
            if( i2.get() ) {
                if( !getline( *i2, rev ) )
                    THROW( runtime_error, "File [" << m_read2qualityFile << "] is too short" );
            } else if ( rev.length() )
                THROW( runtime_error, "Paired read on input, but single quality file is present" );
        } else if( m_qualityChannels == 1 ) {
            string qf, qr;
            iline >> qf;
            if( rev.length() ) iline >> qr;
            if( qr == "-" ) qr = "";
            if( qf.length() != fwd.length() ) {
                THROW( runtime_error, "Fwd read quality column length (" << qf << " - " << qf.length() 
                        << ") does not equal to IUPAC column length (" << fwd << " - " << fwd.length() << ") for read " << id );
            }
            if( qr.length() != rev.length() ) {
                THROW( runtime_error, "Rev read quality column length (" << qr << " - " << qr.length() 
                        << ") does not equal to IUPAC column length (" << rev << " - " << rev.length() << ") for read " << id );
            }
            fwd += qf;
            rev += qr;
        }
        if( iline.fail() ) THROW( runtime_error, "Failed to parse line [" << buff << "]" );
        CQuery * query = new CQuery( qryCoding, id, fwd, rev, m_qualityBase );
        /*
        ITERATE( TSkipPositions, k, m_skipPositions ) {
            query->MarkPositionAmbiguous( 0, *k - 1 );
            query->MarkPositionAmbiguous( 1, *k - 1 );
        }
        */
        query->ComputeBestScore( scoreTbl, 0 );
        query->ComputeBestScore( scoreTbl, 1 );
        while( guideFile.NextHit( queriesTotal, query ) ); // add guided hits
        entriesTotal += batch.AddQuery( query );
        queriesTotal ++;
        p.Increment();
    }
    batch.Purge();
    batch.SetReadProgressIndicator( 0 );
    p.Summary();
    cerr << "Queries processed: " << queriesTotal << " (" << entriesTotal << " hash entries)\n";

    return 0;
}

