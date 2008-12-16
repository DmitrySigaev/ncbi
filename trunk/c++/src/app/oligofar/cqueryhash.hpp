#ifndef OLIGOFAR_CQUERYHASH__HPP
#define OLIGOFAR_CQUERYHASH__HPP

#include "uinth.hpp"
#include "cquery.hpp"
#include "fourplanes.hpp"
#include "cpermutator8b.hpp"
#include "array_set.hpp"
#include "cwordhash.hpp"

#include <deque>

BEGIN_OLIGOFAR_SCOPES

////////////////////////////////////////////////////////////////////////
// 0....|....1....|....2
// 111010111111110111111 18 bits
// aaaaaaaaaaa            9 bits
//           bbbbbbbbbbb 10 bits
class CHashPopulator;
class CQueryHash
{
public:
    typedef array_set<Uint1> TSkipPositions;
    typedef vector<CHashAtom> TMatchSet;

    enum EStatus {
        eStatus_empty,
        eStatus_dirty,
        eStatus_ready
    };

    ~CQueryHash();
    CQueryHash( int maxm = 2, int maxa = 4 );

    void SetWindowSize( int winSize );
    void SetWordSize( int wordSz );
    void SetStrideSize( int stride );
    void SetMaxMismatches( int mismatches );
    void SetMaxAmbiguities( int ambiguities );
    void SetAllowIndel( bool indel );
    void SetIndexBits( int bits );
    void SetStrands( int strands );
    void SetMaxSimplicity( double simpl );
    void SetMaxWindowCount( int windows );
    void SetNaHSO3mode( bool on = true );

    void SetExpectedReadCount( int count ) { m_expectedCount = count; }

    int  GetMaxWindowCount() const { return m_maxWindowCount; }
    int  GetWindowSize() const { return m_windowSize; }
    int  GetWordSize() const { return m_wordSize; }
    int  GetWordOffset() const { return m_wordOffset; }
    int  GetStrideSize() const { return m_strideSize; }
    int  GetMaxMismatches() const { return m_maxMism; }
    int  GetMaxAmbiguities() const { return m_maxAmb; }
    bool GetAllowIndel() const { return m_allowIndel; }
    bool GetNaHSO3mode() const { return m_nahso3mode; }
    int  GetIndexBits() const { return m_hashTable.GetIndexBits(); }
    int  GetStrands() const { return m_strands; }
    int  GetExpectedReadCount() const { return m_expectedCount; }
    int  GetHashedQueriesCount() const { return m_hashedCount; }
    double GetMaxSimplicity( double simpl ) { return m_maxSimplicity; }
    
    int GetAbsoluteMaxMism() const { return m_permutators.size() - 1; }
    bool CanOptimizeOffset() const { return m_skipPositions.size() == 0; }

    template<class iterator>  void SetSkipPositions( iterator begin, iterator end );
    template<class container> void SetSkipPositions( container c );
    
    const TSkipPositions& GetSkipPositions() const { return m_skipPositions; }

    void Clear() { m_status = eStatus_empty; m_hashedCount = 0; m_hashTable.Clear(); }
    void Freeze() { m_hashTable.Sort(); m_status = eStatus_ready; }

    int AddQuery( CQuery * query );

    template<class Callback> void ForEach4gnl( const UintH& ncbi4na, Callback& cbk ) const;
    template<class Callback> void ForEach4( const UintH& ncbi4na, Callback& cbk ) const;
    template<class Callback> void ForEach4( UintH hash, Callback& callback, Uint1 mask, Uint1 flags ) const;
    template<class Callback> void ForEach2gnl( const Uint8& ncbi2na, Callback& cbk ) const;
    template<class Callback> void ForEach2( const Uint8& ncbi2na, Callback& cbk ) const;
    template<class Callback> void ForEach2( Uint8 hash, Callback& callback, Uint1 mask, Uint1 flags ) const;

    template<class Callback> void ForEach4( const UintH& ncbi4na, Callback& cbk ) {
        if( m_status == eStatus_dirty ) Freeze();
        ((const CQueryHash*)this)->ForEach4gnl( ncbi4na, cbk );
    }
    template<class Callback> void ForEach2( const Uint8& ncbi2na, Callback& cbk ) {
        if( m_status == eStatus_dirty ) Freeze();
        ((const CQueryHash*)this)->ForEach2gnl( ncbi2na, cbk );
    }

    Uint2  ComputeHashPreallocSz() const;
    Uint8  ComputeEntryCountPerRead() const; // no ambiguities are allowed
    int    ComputeHasherWindowLength();
    int    ComputeScannerWindowLength();
    UintH  ComputeAlternatives( UintH w, int l ) const; // used to optimize window
    double ComputeComplexityNcbi8na( UintH w, int len, int& amb ) const;

    void Compress( UintH& window ) const; // removes skip-positions

    void SetNcbipnaToNcbi4naScore( Uint2 score ) { m_ncbipnaToNcbi4naScore = score; }
    void SetNcbiqnaToNcbi4naScore( Uint2 score ) { m_ncbiqnaToNcbi4naScore = score; }

protected:

    template<class Callback>
    class C_ScanCallback
    {
    public:
        C_ScanCallback( const CQueryHash& caller, Callback& cbk, Uint1 mask, Uint1 flags ) : 
            m_caller( caller ), m_callback( cbk ), m_mask( mask ), m_flags( flags ) {}
        void operator () ( Uint8 hash, int mism, unsigned amb ) const;
    protected:
        const CQueryHash& m_caller;
        Callback& m_callback;
        Uint1 m_mask;
        Uint1 m_flags;
    };

    class C_ListInserter
    {
    public:
        C_ListInserter( TMatchSet& ms ) : m_matchSet( ms ) {}
        void operator () ( const CHashAtom& atom ) { m_matchSet.push_back( atom ); }
    protected:
        TMatchSet& m_matchSet;
    };

    typedef UintH TCvt( const unsigned char *, int, unsigned short );
    typedef Uint2 TDecr( Uint2 );

    bool CheckWordConstraints();
    bool CheckWordConstraints() const;

    int  AddQuery( CQuery * query, int component );
    int  PopulateWindow( UintH fwindow, int offset, int ambiguities, CQuery * query, int component, CHashAtom::EConv );
    int  GetNcbi4na( UintH& window, CSeqCoding::ECoding, const unsigned char * data, unsigned length );

    int x_GetNcbi4na_ncbi8na( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_colorsp( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbipna( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbiqna( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_quality( UintH& window, const unsigned char * data, unsigned length, TCvt * fun, int incr, unsigned short score, TDecr * decr );

    static Uint2 x_UpdateNcbipnaScore( Uint2 score ) { return score /= 2; }
    static Uint2 x_UpdateNcbiqnaScore( Uint2 score ) { return score - 1; }
    static TCvt  x_Ncbipna2Ncbi4na;
    static TCvt  x_Ncbiqna2Ncbi4na;

    static Uint8 x_Convert2( Uint8 word, unsigned len, Uint8 from, Uint8 to );
    static UintH x_Convert4( UintH word, unsigned len, UintH mask, int shr );

//    template<class Callback> void ForEach( Uint8 ncbi2na, Callback& cbk, Uint1 mask, Uint1 flags );

protected:

    EStatus m_status;
    int m_windowLength;
    int m_windowSize;
    int m_strideSize;
    int m_wordSize;
    int m_maxWindowCount;
    int m_strands;
    int m_expectedCount;
    int m_hashedCount;
    int m_maxAmb;
    int m_maxMism;
    bool m_allowIndel;
    bool m_nahso3mode;
    double m_maxSimplicity;
    Uint2 m_ncbipnaToNcbi4naScore;
    Uint2 m_ncbiqnaToNcbi4naScore;
    TSkipPositions m_skipPositions;
    CWordHash m_hashTable;
    vector<CPermutator8b*> m_permutators;
    // all masks are ncbi2na, i.e. 2 bits per base
    Uint8 m_wordMask; 
    int m_wordOffset;

    mutable TMatchSet m_listA, m_listB; // to keep memory space reserved between ForEach calls
};

////////////////////////////////////////////////////////////////////////
// inline implementation follows

inline CQueryHash::~CQueryHash() 
{
    for( int i = 0; i < int( m_permutators.size() ); ++i )
        delete m_permutators[i];
}

inline CQueryHash::CQueryHash( int maxm, int maxa ) :
    m_status( eStatus_empty ),
    m_windowLength( 22 ),
    m_windowSize( 22 ),
    m_strideSize( 1 ),
    m_wordSize( 11 ),
    m_maxWindowCount( 1 ),
    m_strands( 3 ),
    m_expectedCount( 1000000 ),
    m_hashedCount( 0 ),
    m_maxAmb( maxa ),
    m_maxMism( min(1, maxm) ),
    m_allowIndel( maxm ? true : false ),
    m_maxSimplicity( 2.0 ),
    m_ncbipnaToNcbi4naScore( 0x7f ),
    m_ncbiqnaToNcbi4naScore( 3 ),
    m_hashTable( m_wordSize*2 ),
    m_wordMask( CBitHacks::WordFootprint<Uint8>( m_wordSize*2 ) ),
    m_wordOffset( m_windowSize - m_wordSize )
{
    SetMaxMismatches( maxm );
}

template<class iterator>
inline void CQueryHash::SetSkipPositions( iterator begin, iterator end ) 
{
    m_skipPositions.clear();
    for( ; begin != end ; ++begin ) {
        if( *begin <= 0 ) continue;
        m_skipPositions.insert( *begin );
    }
}
    
template<class container>
inline void CQueryHash::SetSkipPositions( container c ) 
{
    SetSkipPositions( c.begin(), c.end() );
}

inline void CQueryHash::SetWindowSize( int winSize ) 
{ 
    ASSERT( m_status == eStatus_empty );
    m_windowSize = winSize; 
    if( m_wordSize > m_windowSize ) {
        m_wordSize = m_windowSize;
        m_wordMask = CBitHacks::WordFootprint<Uint8>( 2*m_wordSize );
        m_wordOffset = 0;
    } else m_wordOffset = m_windowSize - m_wordSize;
}

inline void CQueryHash::SetMaxWindowCount( int winCnt ) 
{ 
    ASSERT( m_status == eStatus_empty );
    m_maxWindowCount = winCnt; 
    if( m_skipPositions.size() ) {
        m_skipPositions.clear();
    } else m_wordOffset = m_windowSize - m_wordSize;
}

inline void CQueryHash::SetWordSize( int wordSize ) 
{ 
    ASSERT( m_status == eStatus_empty );
    m_wordSize = wordSize; 
    m_wordMask = CBitHacks::WordFootprint<Uint8>( 2*m_wordSize );
    if( m_wordSize > m_windowSize ) m_windowSize = m_wordSize;
    m_wordOffset = m_windowSize - m_wordSize;
}

inline int CQueryHash::AddQuery( CQuery * query )
{
    if( m_status == eStatus_empty ) CheckWordConstraints();
    
    m_status = eStatus_dirty;
    int ret = AddQuery( query, 0 );
    if( query->HasComponent( 1 ) ) ret += AddQuery( query, 1 );
    if( ret ) ++m_hashedCount;
    return ret;
}

inline void CQueryHash::SetStrideSize( int stride ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_strideSize = stride; 
}
    
inline void CQueryHash::SetMaxMismatches( int mismatches ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    if( int(m_permutators.size()) <= mismatches ) {
        int oldsz = m_permutators.size();
        m_permutators.resize( mismatches + 1 );
        for( int i = oldsz; i <= mismatches; ++i ) 
            m_permutators[i] = new CPermutator8b( i, m_maxAmb );
    }
    m_maxMism = mismatches;
}

inline void CQueryHash::SetMaxAmbiguities( int ambiguities ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_maxAmb = ambiguities; 
}

inline void CQueryHash::SetAllowIndel( bool indel )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_allowIndel = indel; 
}

inline void CQueryHash::SetNaHSO3mode( bool on )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_nahso3mode = on; 
}

inline void CQueryHash::SetIndexBits( int bits )         
{ 
    ASSERT( m_status == eStatus_empty ); m_hashTable.SetIndexBits( bits ); 
}

inline void CQueryHash::SetStrands( int strands )        
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_strands = strands; 
}
    
inline void CQueryHash::SetMaxSimplicity( double simpl ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_maxSimplicity = simpl; 
}

inline Uint8 CQueryHash::ComputeEntryCountPerRead() const
{
    Uint8 ret = m_wordSize;
    if( m_maxMism > 0 ) ret += 3 * m_wordSize;
    if( m_allowIndel )  {
        Uint8 idc = (m_wordSize - 2)*4 + (m_wordSize - 1) - (m_wordSize - 3); // TODO: check last ???
        if( m_maxMism > 0 ) ret *= idc;
        else ret += idc;
    }
    if( m_maxMism > 1 ) {
        ret += 9 * m_wordSize * ( m_wordSize - 1 )/2;
        ASSERT( m_maxMism < 3 );
    }
    if( m_wordOffset ) ret *= 2; // for each word
    if( (m_strands&3) == 3 ) ret *= 2; // for each strand
    ret *= m_maxWindowCount;
    return ret;
}

inline int CQueryHash::ComputeHasherWindowLength()
{
    ASSERT( m_strideSize == 1 || m_skipPositions.size() == 0 );
    m_windowLength = m_windowSize + m_strideSize - 1 + m_allowIndel;
    ITERATE( TSkipPositions, p, m_skipPositions ) {
        if( *p <= m_windowLength ) ++m_windowLength; // m_skipPositions are sorted, unique and > 0 (1-based)
        else break;
    }
    return m_windowLength;
}

inline int CQueryHash::ComputeScannerWindowLength()
{
    ASSERT( m_strideSize == 1 || m_skipPositions.size() == 0 );
    int winlen = m_windowSize; // indel and strides are taken into account in hash
    ITERATE( TSkipPositions, p, m_skipPositions ) { // just extend window to allow skipping
        if( *p <= winlen ) ++winlen; // m_skipPositions are sorted, unique and > 0 (1-based)
        else break;
    }
    return winlen;
}

inline Uint2 CQueryHash::ComputeHashPreallocSz() const
{
    // VERIFY!!!
    if( m_maxMism == 0 && !m_allowIndel ) return 1;
    int k = m_wordSize;
    int b = (m_hashTable.GetIndexBits() + 1)/2;
    if( k <= b ) return 1;
    k -= b;
    int ret = 1;
    if( m_maxMism ) ret += 3 * k;
    if( m_allowIndel ) {
        int idc = (k - 1)*4 + k - (k - 2); // insertions and deletions may happen at the beginning of the tail!
        if( m_maxMism > 0 ) ret *= idc;
        else ret += idc;
    }
    if( m_maxMism > 1 ) {
        ret += 9 * k * (k - 1)/2;
    }
    return ret;
}

inline bool CQueryHash::CheckWordConstraints()
{
    ComputeHasherWindowLength();
    m_hashTable.SetPreallocSz( ComputeHashPreallocSz() );
    cerr << "INFO: Setting prealloc size to " << m_hashTable.GetPreallocSize() << endl;
    return ((const CQueryHash*)this)->CQueryHash::CheckWordConstraints();
}


inline bool CQueryHash::CheckWordConstraints() const
{
    ASSERT( m_skipPositions.size() == 0 || m_strideSize == 1 );
//    ASSERT( m_strideSize < m_wordSize );
//    ASSERT( m_strideSize < m_wordOffset + 1|| m_wordOffset == 0 );
    ASSERT( m_wordSize <= m_windowSize );
    ASSERT( m_wordSize * 2 >= m_windowSize );
    ASSERT( m_wordSize * 2 - m_hashTable.GetIndexBits() <= 16 ); // requirement is based on that CHashAtom may store only 16 bits
    ASSERT( m_windowLength <= 32 );
    return true;
}

template<class Callback>
void CQueryHash::ForEach4gnl( const UintH& hash, Callback& callback ) const 
{ 
    if( m_nahso3mode ) {
        ForEach4<Callback>( x_Convert4( hash, m_windowSize, UintH( 0x8888888888888888ULL, 0x8888888888888888ULL ), +2 ), callback );
        ForEach4<Callback>( x_Convert4( hash, m_windowSize, UintH( 0x1111111111111111ULL, 0x1111111111111111ULL ), -2 ), callback );
    } else {
        ForEach4<Callback>( hash, callback );
    }
}

template<class Callback>
void CQueryHash::ForEach4( const UintH& hash, Callback& callback ) const 
{ 
    /* ........................................................................
       We change strategy here: instead of putting logic on varying hash window 
       by seqscanner, we to it in hash - so that hash is a black box

       Remember, that scanner WindowLength = either 
        (a) WindowSize + 0 * (StrideSize + IndelAllowed) 
       or
        (b) WindowSize + (number of skip positions - self conjugated)
       because it is not allowed to have strides and skip positions

       That's the window scanner should use.
       ........................................................................ */
    ASSERT( m_status != eStatus_dirty );

    if( m_strands == 3 )
        ForEach4( hash, callback, 0, 0 );
    else {
        if( m_strands & 1 ) 
            ForEach4( hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandFwd );
        if( m_strands & 2 ) 
            ForEach4( hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandRev );
    }
}

template<class Callback>
void CQueryHash::ForEach2gnl( const Uint8& hash, Callback& callback ) const 
{ 
    if( m_nahso3mode ) {
        ForEach2<Callback>( x_Convert2( hash, m_windowSize, 0xffffffffffffffffULL, 0x5555555555555555ULL ), callback );
        ForEach2<Callback>( x_Convert2( hash, m_windowSize, 0x0000000000000000ULL, 0xaaaaaaaaaaaaaaaaULL ), callback );
    } else {
        ForEach2<Callback>( hash, callback );
    }
}

template<class Callback>
void CQueryHash::ForEach2( const Uint8& hash, Callback& callback ) const 
{ 
    /* ........................................................................
       We change strategy here: instead of putting logic on varying hash window 
       by seqscanner, we to it in hash - so that hash is a black box

       Remember, that scanner WindowLength = either 
        (a) WindowSize + 0 * (StrideSize + IndelAllowed) 
       or
        (b) WindowSize + (number of skip positions - self conjugated)
       because it is not allowed to have strides and skip positions

       That's the window scanner should use.
       ........................................................................ */
    ASSERT( m_status != eStatus_dirty );

    if( m_strands == 3 ) 
        ForEach2( hash, callback, 0, 0 );
    else {
        if( m_strands & 1 ) 
            ForEach2( hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandFwd );
        if( m_strands & 2 )
            ForEach2( hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandRev );
    }
}

template<class Callback>
void CQueryHash::ForEach4( UintH hash, Callback& callback, Uint1 mask, Uint1 flags ) const
{
    Compress( hash );
    C_ScanCallback<Callback> cbk( *this, callback, mask, flags );
    m_permutators[0]->ForEach( m_windowSize, hash, cbk );
}

template<class Callback>
void CQueryHash::C_ScanCallback<Callback>::operator () ( Uint8 hash, int, unsigned ) const
{
    m_caller.ForEach2( hash, m_callback, m_mask, m_flags );
}

template<class Callback>
void CQueryHash::ForEach2( Uint8 hash, Callback& callback, Uint1 mask, Uint1 flags ) const
{
    if( GetWordOffset() == 0 ) {
        m_hashTable.ForEach( hash, callback, mask, flags );
    } else {
        mask |= CHashAtom::fMask_wordId;

        Uint1 flags0 = flags | CHashAtom::fFlag_wordId0;
        Uint1 flags1 = flags | CHashAtom::fFlag_wordId1;

        TMatchSet& listA( m_listA );
        TMatchSet& listB( m_listB );

        listA.resize(0);
        listB.resize(0);

        Uint8 hashA = Uint8( hash >> ( GetWordOffset() * 2 ) );
        Uint8 hashB = Uint8( hash & CBitHacks::WordFootprint<Uint8>( 2*GetWordSize() ) ); //( ( Uint8(1) << ( 2 * GetWordSize() )) - 1 ) );

        C_ListInserter iA( listA );
        C_ListInserter iB( listB );
        
        m_hashTable.ForEach( hashA, iA, mask, flags0); 
        m_hashTable.ForEach( hashB, iB, mask, flags1); 

        sort( listA.begin(), listA.end() );
        sort( listB.begin(), listB.end() );

        TMatchSet::const_iterator a = listA.begin(), A = listA.end();
        TMatchSet::const_iterator b = listB.begin(), B = listB.end();

        while( a != A && b != B ) {

            if( *a < *b ) { ++a; continue; }
            if( *b < *a ) { ++b; continue; }

            callback( a->GetStrandId() == CHashAtom::fFlag_strandFwd ? *b : *a ); // one time is enough

            TMatchSet::const_iterator x = a; 

            do if( ! ( ++a < A ) ) return; while( *a == *x );
            do if( ! ( ++b < B ) ) return; while( *b == *x );
        }
    }
}

inline Uint8 CQueryHash::x_Convert2( Uint8 word, unsigned len, Uint8 from, Uint8 to )
{
    Uint8 mask = 3;
    for( unsigned i = 0; i < len; ++i, (mask <<= 2) ) {
        if( ( word & mask ) == ( from & mask ) ) {
            word = (word & ~mask) | ( to & mask );
        }
    }
    return word;
}

inline UintH CQueryHash::x_Convert4( UintH word, unsigned len, UintH mask, int shr )
{
    if( shr > 0 )
        word = (( word & mask ) >> (+shr) ) | ( word & ~mask );
    else
        word = (( word & mask ) << (-shr) ) | ( word & ~mask );
    return word;
}

END_OLIGOFAR_SCOPES

#endif
