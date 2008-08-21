#ifndef OLIGOFAR_CQUERY__HPP
#define OLIGOFAR_CQUERY__HPP

#include "iupac-util.hpp"
#include "cseqcoding.hpp"
#include "debug.hpp"
#include "chit.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHit;
class CScoreTbl;
class CQuery
{
public:
    friend class CFilter;
    friend class CGuideFile;
    
	enum EFlags {
		fCoding_ncbi8na = 0x01,
		fCoding_ncbipna = 0x02,
		fCoding_BITS = fCoding_ncbi8na|fCoding_ncbipna,
		fNONE = 0
	};
    static Uint4 GetCount() { return s_count; }

	CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& data2 = "" );
	~CQuery() { delete m_topHit; delete [] m_data; --s_count; }

	bool IsPairedRead() const { return m_length[1] != 0; }
	bool HasComponent( int i ) const { return GetLength( i ) != 0; }

	const char* GetId() const { return (const char*)m_data; }
	const char* GetData( int i ) const { return (const char*)m_data + m_offset[i]; }

	unsigned GetLength( int i ) const { return m_length[i]; }
	unsigned GetDataSize( int i ) const { return ( GetCoding() == CSeqCoding::eCoding_ncbipna ? 5 : 1 ) * m_length[i]; }

	CSeqCoding::ECoding GetCoding() const { return m_flags & fCoding_ncbi8na ? CSeqCoding::eCoding_ncbi8na : CSeqCoding::eCoding_ncbipna; }
    CHit * GetTopHit() const { return m_topHit; }
	void ClearHits() { delete m_topHit; m_topHit = 0; }

	double ComputeBestScore( const CScoreTbl& scoring, int component ) const;

private:
    explicit CQuery( const CQuery& q );
    
protected:
	void x_InitNcbi8na( const string& id, const string& data1, const string& data2 );
	void x_InitNcbiqna( const string& id, const string& data1, const string& data2 );
	void x_InitNcbipna( const string& id, const string& data1, const string& data2 );
    unsigned x_ComputeInformativeLength( const string& seq ) const;

protected:
	unsigned char * m_data; // contains id\0 data1 data2
	unsigned short  m_offset[2]; // m_offset[1] should always point to next byte after first seq data
	unsigned char   m_length[2];
	unsigned short  m_flags;
    CHit * m_topHit;
    static Uint4 s_count;
};

////////////////////////////////////////////////////////////////////////
// Implementation

inline CQuery::CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& data2 ) : m_data(0), m_flags(0), m_topHit(0)
{
	switch( coding ) {
	case CSeqCoding::eCoding_ncbi8na: x_InitNcbi8na( id, data1, data2 ); break;
	case CSeqCoding::eCoding_ncbiqna: x_InitNcbiqna( id, data1, data2 ); break;
	case CSeqCoding::eCoding_ncbipna: x_InitNcbipna( id, data1, data2 ); break;
	default: THROW( logic_error, "Invalid coding " << coding );
	}
    ++s_count;
}

inline unsigned CQuery::x_ComputeInformativeLength( const string& seq ) const
{
    unsigned l = seq.length();
    while( l && strchr("N. \t",seq[l - 1]) ) --l;
    return l;
}

inline void CQuery::x_InitNcbi8na( const string& id, const string& data1, const string& data2 )
{
	m_flags &= ~fCoding_BITS;
	m_flags |= fCoding_ncbi8na;
	ASSERT( data1.length() < 255 );
	ASSERT( data2.length() < 255 );
	m_length[0] = x_ComputeInformativeLength( data1 );
	m_length[1] = x_ComputeInformativeLength( data2 );
    unsigned sz = id.length() + m_length[0] + m_length[1] + 1;
    m_data = new unsigned char[sz];
    memset( m_data, 0, sz );
	strcpy( (char*)m_data, id.c_str() );
	m_offset[0] = id.length() + 1;
	m_offset[1] = m_offset[0] + m_length[0];
	for( unsigned i = 0, j = m_offset[0]; i < m_length[0]; )
		m_data[j++] = CNcbi8naBase( CIupacnaBase( data1[i++] ) );
	for( unsigned i = 0, j = m_offset[1]; i < m_length[1]; )
		m_data[j++] = CNcbi8naBase( CIupacnaBase( data2[i++] ) );
}

inline void CQuery::x_InitNcbiqna( const string& id, const string& data1, const string& data2 )
{
	m_flags &= ~fCoding_BITS;
	m_flags |= fCoding_ncbipna;
	ASSERT( data1.length() < 255 );
	ASSERT( data2.length() < 255 );
	m_length[0] = x_ComputeInformativeLength( data1.substr( 0, data1.length()/2 ) );
	m_length[1] = x_ComputeInformativeLength( data2.substr( 0, data2.length()/2 ) );
    unsigned sz = id.length() + 5*m_length[0] + 5*m_length[1];
    m_data = new unsigned char[sz];
    memset( m_data, 0, sz );
	strcpy( (char*)m_data, id.c_str() );
	m_offset[0] = id.length() + 1;
	m_offset[1] = m_offset[0] + 4*m_length[0];
	Iupacnaq2Ncbapna( m_data + m_offset[0], data1.substr(0, m_length[0]), data1.substr( data1.length()/2, m_length[0] ) );
	Iupacnaq2Ncbapna( m_data + m_offset[1], data2.substr(0, m_length[1]), data2.substr( data2.length()/2, m_length[1] ) );
}

inline void CQuery::x_InitNcbipna( const string& id, const string& data1, const string& data2 )
{
	m_flags &= ~fCoding_BITS;
	m_flags |= fCoding_ncbipna;
	vector<unsigned char> buff1, buff2;
    Solexa2Ncbipna( back_inserter( buff1 ), data1 );
    if( data2.length() ) Solexa2Ncbipna( back_inserter( buff2 ), data2 );
	m_data = new unsigned char[id.length() + 1 + buff1.size() + buff2.size()];
    ASSERT( buff1.size()%5 == 0 );
    ASSERT( buff2.size()%5 == 0 );
    ASSERT( buff1.size()/5 < 255 );
    ASSERT( buff2.size()/5 < 255 );
	m_length[0] = buff1.size()/5;
	m_length[1] = buff2.size()/5;
	strcpy( (char*)m_data, id.c_str());
	memcpy( m_data + ( m_offset[0] = id.length() + 1 ), &buff1[0], buff1.size() );
	memcpy( m_data + ( m_offset[1] = m_offset[0] + buff1.size() ), &buff2[0], buff2.size() );
}

END_OLIGOFAR_SCOPES

#endif
