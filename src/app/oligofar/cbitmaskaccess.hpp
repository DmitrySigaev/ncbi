#ifndef OLIGOFAR__CBITMASKACCESS__HPP
#define OLIGOFAR__CBITMASKACCESS__HPP

#include "cbitmaskbase.hpp"

BEGIN_OLIGOFAR_SCOPES

class CBitmaskAccess : public CBitmaskBase
{
public:
    ~CBitmaskAccess() { Close(); }
    CBitmaskAccess() : CBitmaskBase( 0, 0, 0 ), m_rqWordSize( 0 ) {}
    CBitmaskAccess( const string& name ) : CBitmaskBase( 0, 0, 0 ), m_rqWordSize( 0 ) { Open( name ); }
    
    Uint4 GetRqWordSize() const { return m_rqWordSize; }

    void SetRqWordSize( int ws ) { m_rqWordSize = ws; }
    bool Open( const string& name );
    void Close();

    bool HasExactWord( Uint8 word ) const { word &= m_wMask; return m_data[word/32] & (1<<(word%32)); }
    bool HasWord( Uint8 word ) const;

protected:
    Uint4 m_rqWordSize;
    Uint8 m_wMask;
};

inline bool CBitmaskAccess::HasWord( Uint8 word ) const
{
    if( m_rqWordSize >= m_wSize ) {
        for( Uint4 i = m_wSize; i <= m_rqWordSize; ++i, (word >>= 2) ) 
            if( !HasExactWord( word ) ) return false;
        return true; // all subwords exist
    } else {
        int k = m_wSize - m_rqWordSize;
        Uint8 wstart = word << (2*k);
        Uint8 wend = (word + 1) << (2*k);
        for( Uint8 w = wstart; w != wend; ++w ) {
            if( HasExactWord( w ) ) return true;
        }
        return false;
    }
}

END_OLIGOFAR_SCOPES

#endif
