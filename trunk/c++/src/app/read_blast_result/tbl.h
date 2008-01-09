

using namespace std;

#include <istream>

class Ctbl;
class Ctbl_feat;
class Ctbl_seg;

#include <corelib/ncbiobj.hpp>


class Ctbl_seg : public CObject
{
public:
  typedef map < string, string > Ttbl_qual;

  bool Read(istream& in);

  const bool& GetFuzzyFrom(void) const  { return m_fuzzy_from; } ;
  bool&       SetFuzzyFrom(void)        { return m_fuzzy_from; } ;
  void        SetFuzzyFrom(bool& value) { m_fuzzy_from = value; } ;
  
  const bool& GetFuzzyTo(void) const  { return m_fuzzy_to; } ;
  bool&       SetFuzzyTo(void)        { return m_fuzzy_to; } ;
  void        SetFuzzyTo(bool& value) { m_fuzzy_to = value; } ;

  const int&  GetFrom(void) const   { return m_from; } ;
  int&        SetFrom(void)         { return m_from; } ;
  void        SetFrom(int& value)   { m_from = value; } ;

  const int&  GetTo(void) const   { return m_to; } ;
  int&        SetTo(void)         { return m_to; } ;
  void        SetTo(int& value)   { m_to = value; } ;

  const string& GetKey(void) const    { return m_key; } ;
  string&       SetKey(void)          { return m_key; } ;
  void          SetKey(string& value) { m_key = value; } ;

  const Ttbl_qual& GetQual(void) const    { return m_qual; } ;
  Ttbl_qual&       SetQual(void)          { return m_qual; } ;
  void             SetQual(Ttbl_qual& value) { m_qual = value; } ; 
  
private:
  bool m_fuzzy_from;
  bool m_fuzzy_to;
  int  m_from;
  int  m_to; 
  string m_key;
  Ttbl_qual m_qual;
};

class Ctbl_feat : public CObject
{
public:
  typedef list < CRef < Ctbl_seg > > Ttbl_seg;
  bool Read(istream& in);

  const Ttbl_seg& GetSeg(void) const    { return m_seg; } ;
  Ttbl_seg&       SetSeg(void)          { return m_seg; } ;
  void            SetSeg(Ttbl_seg& value) { m_seg = value; } ;

  const string&   GetSeqid(void) const  { return m_seqid; } ;
  string&         SetSeqid(void)        { return m_seqid; } ;
  void            SetSeqid(string& value) { m_seqid = value; } ;

  const string&   GetTableName(void) const  { return m_table_name; } ;
  string&         SetTableName(void)        { return m_table_name; } ;
  void            SetTableName(string& value) { m_table_name = value; } ;

private:
  Ttbl_seg m_seg;
  string   m_seqid;
  string   m_table_name;
};

class Ctbl
{
public:
  typedef list < CRef < Ctbl_feat > > Ttbl_feat;
  bool Read(istream& in);

  const Ttbl_feat&    GetFeat(void) const    { return m_feat; } ;
  Ttbl_feat&          SetFeat(void)          { return m_feat; } ;
  void                SetFeat(Ttbl_feat& value) { m_feat = value; } ;
private:
  Ttbl_feat m_feat;
};

