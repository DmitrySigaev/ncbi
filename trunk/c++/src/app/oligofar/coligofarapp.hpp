#ifndef OLIGOFAR_COLIGOFARAPP__HPP
#define OLIGOFAR_COLIGOFARAPP__HPP

#include "capp.hpp"
#include "util.hpp"
#include "array_set.hpp"
#include "cqueryhash.hpp"
#include <corelib/ncbireg.hpp>
#include <string>
#include <set>

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CScoreTbl;
class COligoFarApp : public CApp
{
public:
    COligoFarApp( int argc, char ** argv );
	static int RevNo();
    enum {
        kKiloByte = 1024,
        kMegaByte = 1024*kKiloByte,
        kGigaByte = 1024*kMegaByte
    };
    enum EAlignmentAlgo {
        eAlignment_HSP   = 'h',
        eAlignment_SW    = 's',
        eAlignment_fast  = 'f',
        eAlignment_quick = 'q'
    };
    enum ELongOpt { 
        kLongOptBase = 0x100,
        kLongOpt_old = kLongOptBase + 0x01,
        kLongOpt_min_block_length = kLongOptBase + 0x02,
        kLongOpt_ambiguifyPositions = kLongOptBase + 0x03
    };
    typedef array_set<int> TSkipPositions;

protected:
    virtual void Help( const char * );
    virtual void Version( const char * );
    virtual int  Execute();
    virtual int  TestSuite();
    
    int RunTestSuite();
    int SetLimits();
    int ProcessData();
	void ParseConfig( const string& cfg );
	void ParseConfig( IRegistry * );

    int GetOutputFlags() const;

	IAligner * CreateAligner( EAlignmentAlgo, CScoreTbl * tbl ) const;

    virtual const option * GetLongOptions() const;
    virtual const char * GetOptString() const;
    virtual int ParseArg( int, const char *, int );

    void SetupGeometries( map<string,int>& );

    char HashTypeChar( int  = 0 ) const;
    void SetHashType( int, char );

    static void ParseRange( int& a, int& b, const string& str, const string& delim = "-," ) {
        pair<int,int> val = NUtil::ParseRange( str, delim );
        a = val.first;
        b = val.second;
    }
    static void ParseRange( unsigned& a, unsigned& b, const string& str, const string& delim = "-," ) {
        pair<int,int> val = NUtil::ParseRange( str, delim );
        a = val.first;
        b = val.second;
    }

protected:
    unsigned m_windowLength[2];
    unsigned m_minHashMism;
    unsigned m_maxHashMism;
    unsigned m_maxHashAlt;
    unsigned m_maxFastaAlt;
    unsigned m_strands;
    unsigned m_readsPerRun;
	unsigned m_phrapSensitivity;
    unsigned m_xdropoff;
    unsigned m_topCnt;
    double   m_topPct;
    double   m_minPctid;
    double   m_maxSimplicity;
    double   m_identityScore;
    double   m_mismatchScore;
    double   m_gapOpeningScore;
    double   m_gapExtentionScore;
    double   m_rehashFactor;
    int      m_minPair;
    int      m_maxPair;
    int      m_pairMargin;
	int      m_qualityChannels;
	int      m_qualityBase;
    int      m_minBlockLength;
    int      m_guideFilemaxMismatch;
    Uint8    m_memoryLimit;
    bool     m_performTests;
	bool     m_colorSpace;
    bool     m_run_old_scanning_code;
    bool     m_ambiguifyPositions;
	EAlignmentAlgo m_alignmentAlgo;
	CQueryHash::EHashType m_hashType[2];
    TSkipPositions m_skipPositions;
    string m_readFile;
    string m_gilistFile;
    string m_fastaFile;
    string m_snpdbFile;
    string m_vardbFile;
	string m_guideFile;
    string m_outputFile;
    string m_outputFlags;
	string m_read1qualityFile;
	string m_read2qualityFile;
	string m_geometry;
};

END_OLIGOFAR_SCOPES

#endif
