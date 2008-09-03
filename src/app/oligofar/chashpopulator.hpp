#ifndef OLIGOFAR_CHASHPOPULATOR__HPP
#define OLIGOFAR_CHASHPOPULATOR__HPP

#include "cpermutator4b.hpp"
#include "dust.hpp"

BEGIN_OLIGOFAR_SCOPES

class CQuery;

template<class THashTable>
class CHashInserterCbk
{
public:
    void operator() ( Uint4 hash, int mism, Uint8 alt ) { 
        double c = ComputeComplexity( hash, m_windowSize );
        if( c <= m_maxSimplicity ) {
            m_count ++;
            m_table.Insert( hash, CQueryHash::SHashAtom( m_query, m_strand, m_pairmate, mism, m_offset ) ); 
        }
    }
    CHashInserterCbk( THashTable& table, unsigned wsize, CQuery * query, char strand, int offset, bool pairmate, double msimpl ) 
        : m_table( table ), m_maxSimplicity( msimpl ), m_windowSize( wsize ), m_query( query ), m_strand( strand ), m_offset( offset ), m_pairmate( pairmate ), m_count( 0 ) {}
    unsigned GetCount() const { return m_count; }
    
protected:
    THashTable& m_table;
    double m_maxSimplicity;
    unsigned m_windowSize;
    CQuery * m_query;
    char m_strand;
    char m_offset;
    bool m_pairmate;
    unsigned m_count;
};

class CHashPopulator 
{
public:
	CHashPopulator( const CPermutator4b& permutator, int windowLength, CQuery * query, 
			int strands, int offset, int component, double maxSimplicity, Uint8 fwindow, CSeqCoding::ECoding coding ) : 
		m_permutator( permutator ), m_windowLength( windowLength ), m_query( query ), 
		m_strands( strands ), m_offset( offset ), m_component( component ),
		m_maxSimplicity( maxSimplicity ), m_fwindow( fwindow ), m_coding( coding ) {}
	
	template<class THashTable>
	int PopulateHash( THashTable& hashTable ) const {
		int ret = 0;
        if( (1 << m_component) & m_strands ) {
			CHashInserterCbk<THashTable> cbkf( hashTable, m_windowLength, m_query, '+', m_offset, m_component != 0, m_maxSimplicity );
			m_permutator.ForEach( m_windowLength, m_fwindow, cbkf );
			ret += cbkf.GetCount();
		}
	    if( (1 << (1-m_component)) & m_strands ) {
			CHashInserterCbk<THashTable> cbkr( hashTable, m_windowLength, m_query, '-', m_offset, m_component != 0, m_maxSimplicity );
    		Uint8 rwindow = m_coding == CSeqCoding::eCoding_colorsp ? 
                Ncbi4naReverse( m_fwindow, m_windowLength ) : 
                Ncbi4naRevCompl( m_fwindow, m_windowLength );
			m_permutator.ForEach( m_windowLength, rwindow, cbkr );
			ret += cbkr.GetCount();
		}
		return ret;
	}
protected:
	const CPermutator4b& m_permutator;
	int m_windowLength;
	CQuery * m_query;
	int m_strands;
	int m_offset;
	int m_component;
	double m_maxSimplicity;
	Uint8 m_fwindow;
    CSeqCoding::ECoding m_coding;
};
    
END_OLIGOFAR_SCOPES

#endif
