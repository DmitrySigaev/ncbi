/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   Temporary code - will be part of the hit filtering library
*
*/

#include "hf_hit.hpp"
#include "splign_app_exception.hpp"

#include <algorithm>
#include <sstream>
#include <math.h>


BEGIN_NCBI_SCOPE

CHit::CHit()
{
    x_InitDefaults();
}

void CHit::x_InitDefaults()
{
    m_Score = m_Value = 0;
    m_GroupID = -1;
    m_an[0] = m_an[1] = m_an[2] = m_an[3] = 0;
    m_ai[0] = m_ai[1] = m_ai[2] = m_ai[3] = 0;
}

// constructs new hit by merging two existing
CHit::CHit(const CHit& a, const CHit& b)
{
    x_InitDefaults();

    bool bsa = a.IsStraight();
    bool bsb = b.IsStraight();
    bool bBothStraight = (bsa && bsb);
    bool bBothInverse = (!bsa && !bsb);
    if(!bBothStraight && !bBothInverse) {

        NCBI_THROW(
                   CHitFilterException,
                   eInternal,
                   "Cannot merge hits with diferent strands");
    }
  
    m_an[0] = min(a.m_an[0],min(a.m_an[1],min(b.m_an[0],b.m_an[1])));
    m_an[1] = max(a.m_an[0],max(a.m_an[1],max(b.m_an[0],b.m_an[1])));
    m_an[2] = min(a.m_an[2],min(a.m_an[3],min(b.m_an[2],b.m_an[3])));
    m_an[3] = max(a.m_an[2],max(a.m_an[3],max(b.m_an[2],b.m_an[3])));
    
    // make size the same
    int n1 = m_an[1]-m_an[0];
    int n2 = m_an[3]-m_an[2];
    if(n1 != n2)
    {
        int n = min(n1,n2);
        m_an[1] = m_an[0] + n;
        m_an[3] = m_an[2] + n;
    }

    if(bBothInverse)
    {
        int k = m_an[2];
        m_an[2] = m_an[3];
        m_an[3] = k;
    }
    
    // adjust parameters
    m_Query = a.m_Query;
    m_Subj = a.m_Subj;

    m_GroupID = a.m_GroupID;

    m_Score = a.m_Score + b.m_Score;

    m_anLength[0] = abs(m_an[1]-m_an[0]) + 1;
    m_anLength[1] = abs(m_an[3]-m_an[2]) + 1;
    m_Length = max(m_anLength[0], m_anLength[1]);

    m_Value = (double(a.m_Length)*a.m_Value + double(b.m_Length)*b.m_Value) /
              (a.m_Length + b.m_Length);
    m_Idnty = (a.m_Length*a.m_Idnty + b.m_Length*b.m_Idnty) / 
             (a.m_Length + b.m_Length);
    
    m_Gaps = a.m_Gaps + b.m_Gaps;
    m_MM = a.m_MM + b.m_MM;
    
    SetEnvelop();
}


CHit::CHit(const string& strTemplate)
{
    x_InitDefaults();
    
    int nCount = 0;
    string::const_iterator ii = strTemplate.begin(),
        ii_end = strTemplate.end(), jj = ii;

    while( jj < ii_end ) {

        ii = find(jj, ii_end, '\t');
        string s (jj, ii);

        if(s == "-") {
            s = "-1";
        }
        stringstream ss (s);

        switch(nCount++)   {
        
        case 0:    m_Query = s;  break;
        case 1:    m_Subj = s;   break;
        case 2:    ss >> m_Idnty;  break;
        case 3:    ss >> m_Length; break;
        case 4:    ss >> m_MM;     break;
        case 5:    ss >> m_Gaps;   break;
        case 6:    ss >> m_an[0];   break;
        case 7:    ss >> m_an[1];   break;
        case 8:    ss >> m_an[2];   break;
        case 9:    ss >> m_an[3];   break;
        case 10:   ss >> m_Value;  break;
        case 11:   ss >> m_Score;  break;
        default: {
            NCBI_THROW(
                       CHitException,
                       eFormat,
                       "Too many fields found");
            }
        }
        jj = ii + 1;
    }

    if (nCount < 12) {
        NCBI_THROW(
                   CHitException,
                   eFormat,
                   "At least one field missing");
    }
    
    // u-turn the hit if necessary
    if(m_an[0] > m_an[1] && m_an[2] > m_an[3]) {
        int n = m_an[0]; m_an[0] = m_an[1]; m_an[1] = n;
        n = m_an[2]; m_an[2] = m_an[3]; m_an[3] = n;
    }
    
    SetEnvelop();

    m_anLength[0] = m_ai[1] - m_ai[0] + 1;
    m_anLength[1] = m_ai[3] - m_ai[2] + 1;
    m_Length = max(m_anLength[0], m_anLength[1]);
}


bool CHit::IsConsistent() const
{
    return m_ai[0] <= m_ai[1] && m_ai[2] <= m_ai[3];
}


void CHit::SetEnvelop()
{
    m_ai[0] = min(m_an[0],m_an[1]);    m_ai[1] = max(m_an[0],m_an[1]);
    m_ai[2] = min(m_an[2],m_an[3]);    m_ai[3] = max(m_an[2],m_an[3]);
}


void CHit::Move(unsigned char i, int pos_new, bool prot2nucl)
{
    bool strand_plus = IsStraight();
    // for minus strand, flip the hit if necessary
    if(!strand_plus && m_an[0] > m_an[1]) {
        int n = m_an[0]; m_an[0] = m_an[1]; m_an[1] = n;
        n = m_an[2]; m_an[2] = m_an[3]; m_an[3] = n;
    }
    
    // i --> m_ai, n --> m_an
    unsigned char n = 0;
    switch(i) {
        case 0: n = 0; break; 
        case 1: n = 1; break; 
        case 2: n = strand_plus? 2: 3; break; 
        case 3: n = strand_plus? 3: 2; break; 
    }
    
    int shift = pos_new - m_an[n], shiftQ, shiftS;
    if(shift == 0) return;

    // figure out shifts on query and subject
    if(prot2nucl) {
        if(n < 2) {
            shiftQ = shift;
            shiftS = 3*shiftQ;
        }
        else {
            if(shift % 3) {
                shiftQ = (shift > 0)? (shift/3 + 1): (shift/3 - 1);
                shiftS = 3*shiftQ;
            }
            else {
                shiftQ = shift / 3;
                shiftS = shift;
            }
        }
    }
    else {
        shiftQ = shiftS = shift;
    }

    // make sure hit ratio doesn't change (todo: prot2nucl)
    if(!prot2nucl) {
        bool plus = shift >= 0;
        unsigned shift_abs = plus? shift: -shift;
        if(n < 2) {
            double D = double(shift_abs) / (m_ai[1] - m_ai[0] + 1);
            shiftQ = shift;
            shiftS = int(ceil(D*(m_ai[3] - m_ai[2] + 1)));
            if(!plus) shiftS = -shiftS;
        }
        else {
            double D = double(shift_abs) / (m_ai[3] - m_ai[2] + 1);
            shiftS = shift;
            shiftQ = int(ceil(D*(m_ai[1] - m_ai[0] + 1)));
            if(!plus) shiftQ = -shiftQ;
        }
    }


    if(strand_plus) {
        m_an[n % 2] += shiftQ; m_an[n % 2 + 2] += shiftS;
        m_ai[i % 2] += shiftQ; m_ai[i % 2 + 2] += shiftS;
    }
    else {
        switch(i) {
            case 0: m_an[0] = m_ai[0] += shiftQ;
                    m_an[2] = m_ai[3] -= shiftS;
                    break;
            case 1: m_an[1] = m_ai[1] += shiftQ;
                    m_an[3] = m_ai[2] -= shiftS;
                    break;
            case 2: m_an[3] = m_ai[2] += shiftS;
                    m_an[1] = m_ai[1] -= shiftQ;
                    break;
            case 3: m_an[2] = m_ai[3] += shiftS;
                    m_an[0] = m_ai[0] -= shiftQ;
                    break;
        }
    }
}


bool CHit::Inside(const CHit& b) const
{
    bool b0 = b.m_ai[0] <= m_ai[0] && m_ai[1] <= b.m_ai[1];
    if(!b0) return false;
    bool b1 = b.m_ai[2] <= m_ai[2] && m_ai[3] <= b.m_ai[3];
    return b1;
}

// This function must be called once after a series of Move() calls.
// Changes are made based on previous values of the parameters.
void CHit::UpdateAttributes()
{
    int nLength0 = m_Length;
    m_anLength[0] = m_ai[1] - m_ai[0] + 1;
    m_anLength[1] = m_ai[3] - m_ai[2] + 1;
    m_Length = max(m_anLength[0], m_anLength[1]);
    double dk = nLength0? double(m_Length) / nLength0: 1.0;
    m_Score *= dk;
    m_Gaps = int(m_Gaps*dk);
    m_MM = int(m_MM*dk);
    // do not change m_Value, m_Idnty
}

ostream& operator << (ostream& os, const CHit& hit) {
    const int nLen = min(hit.m_anLength[0], hit.m_anLength[1]);
    return os
       << hit.m_Query << '\t' << hit.m_Subj << '\t'
       << hit.m_Idnty   << '\t' << nLen          << '\t'
       << hit.m_MM      << '\t' << hit.m_Gaps   << '\t'
       << hit.m_an[0]    << '\t' << hit.m_an[1]   << '\t'
       << hit.m_an[2]    << '\t' << hit.m_an[3]   << '\t'
       << hit.m_Value   << '\t' << hit.m_Score;
}


bool hit_same_order::operator() (const CHit& hm, const CHit& h0) const
{
    bool bStraightM = hm.IsStraight();
    bool bStraight0 = h0.IsStraight();
    bool b0 = bStraightM && bStraight0;
    bool b1 = !bStraightM && !bStraight0;
    if(b0 || b1)
    {
        int vm [4]; copy(hm.m_an, hm.m_an + 4, vm);
        int v0 [4]; copy(h0.m_an, h0.m_an + 4, v0);
        if(b1)
        {
                // we need to change direction for one axis
            double dmin1 = 1e38, dmax1 = -dmin1;
            int i = 0;
            for(i=2; i<4; i++)
            {
                if(dmin1 > vm[i]) dmin1 = vm[i];
                   if(dmin1 > v0[i]) dmin1 = v0[i];
                if(dmax1 < vm[i]) dmax1 = vm[i];
                   if(dmax1 < v0[i]) dmax1 = v0[i];
            }
            for(i=2; i<4; i++)
            {
                vm[i] = int(-vm[i] + dmin1 + dmax1);
                v0[i] = int(-v0[i] + dmin1 + dmax1);
            }
                // now u-turn hits if necessary
            if(vm[0] > vm[1] && vm[2] > vm[3])
            {
                int n = vm[0]; vm[0] = vm[1]; vm[1] = n;
                n = vm[2]; vm[2] = vm[3]; vm[3] = n;
            }
            if(v0[0] > v0[1] && v0[2] > v0[3])
            {
                int n = v0[0]; v0[0] = v0[1]; v0[1] = n;
                n = v0[2]; v0[2] = v0[3]; v0[3] = n;
            }
        }
        double dx = .1*0.5*(abs(vm[1] - vm[0]) + abs(v0[1] - v0[0]));
        if(vm[0] > v0[1] - dx) // left bottom
            return vm[2] > v0[3] - dx? true: false;
        else
        if(vm[2] > v0[3] - dx) // left top
            return vm[0] > v0[1] - dx? true: false;
        else
        if(vm[1] < v0[0] + dx) // right bottom
            return vm[3] < v0[2] + dx? true: false;
        else
        if(vm[3] < v0[2] + dx) // right top
            return vm[1] < v0[0] + dx? true: false;
        else return false;
    }
    else
        return false;   // different strands and same order are incompatible
}

bool hit_not_same_order::operator()(const CHit& hm, const CHit& h0) const
{
    return !hit_same_order::operator() (hm,h0);
}

// Calculates distance btw. two hits on query or subject
int CHit::GetHitDistance(const CHit& h1, const CHit& h2, int nWhere)
{
    int i0 = -1, i1 = -1;
    switch(nWhere)
    {
        case 0: // query
            i0 = 0; i1 = 1;
            break;
        case 1: // subject
            i0 = 2; i1 = 3;
            break;
        default:
            return -2; // wrong parameter
    }
    if(h1.m_ai[i1] < h2.m_ai[i0])  // no intersection
        return h2.m_ai[i0] - h1.m_ai[i1];
    else
    if(h2.m_ai[i1] < h1.m_ai[i0])  // no intersection
        return h1.m_ai[i0] - h2.m_ai[i1];
    else // overlapping
        return 0;
}

END_NCBI_SCOPE
