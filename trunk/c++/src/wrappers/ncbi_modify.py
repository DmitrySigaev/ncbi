# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Authors:  Josh Cherry
#
# File Description:  Performs various special-purpose modifications of
#                    SWIG preprocessor output
#

# As a last resort for dealing with some problems wrapping with SWIG,
# we modify the SWIG preprocessor output before feeding it back to SWIG.

import os, sys, re


def Replace(s, begin, end, replacement):
    '''
    Replace string from first occurence of begin to first
    subsequent occurence of end with replacement
    '''
    begin_index = s.index(begin)
    end_index   = s.index(end, begin_index + len(begin)) + len(end)
    s = s[:begin_index] + replacement + s[end_index:]
    return s


def Modify(s):
    trouble = '''    template<typename T>
    class SOptional {
    public:
        /// Constructor.
        SOptional()
            : m_IsSet(false), m_Value(T())
        {
        }
        
        /// Check whether the value has been set.
        bool Have()
        {
            return m_IsSet;
        }
        
        /// Get the value.
        T Get()
        {
            return m_Value;
        }

        /// Get the reference to the stored value.
        T& GetRef()
        {
            return m_Value;
        }
        
        /// Assign the field from another optional field.
        SOptional<T> & operator=(const T & x)
        {
            m_IsSet = true;
            m_Value = x;
            return *this;
        }
        
    private:
        /// True if the value has been specified.
        bool m_IsSet;
        
        /// The value itself, valid only if m_IsSet is true.
        T    m_Value;
    };
'''
    s = Replace(s, trouble, '', '// template class SOptimal deleted\n')

    # Part of hit_filter.hpp causes SWIG parse error
    begin_trouble = \
        '    // merging of abutting hits sharing same ids and subject strand'
    end_trouble = '\n    }\n'
    s = Replace(s, begin_trouble, end_trouble, '// sx_TestAndMerge deleted\n')
    
    # In biotree.hpp, nested class inheriting from template causes trouble.
    # Hide the inheritance from SWIG.
    s = re.sub(re.escape('class CBioNode : public CTreeNode<TBioNode>'),
               'class CBioNode', s)

    # CSplicedAligner contains a nested templace struct, SAllocator
    begin_trouble = \
        '    // a trivial but helpful memory allocator for core dynprog'
    end_trouble = '\n    };\n'
    s = Replace(s, begin_trouble, end_trouble,
                '// template struct SAllocator deleted\n')

    # template class CNetSchedulekeys in connect/services/netschedule_key.hpp
    # contains nested class
    begin_trouble = 'template <typename TBVector = bm::bvector<> >\n' \
                    'class CNetScheduleKeys'
    end_trouble = '\n};'
    s = Replace(s, begin_trouble, end_trouble,
                '// template class CNetScheduleKeys deleted\n')
    
    # Bad nested/template interaction in util/align_range_coll.hpp
    begin_trouble = 'template <class TColl>\n' \
                    '    class CAlignRangeCollExtender'
    end_trouble = '\n};'
    s = Replace(s, begin_trouble, end_trouble,
                '// template class CAlignRangeCollExtender deleted\n')
    
    # Some %template declarations for base classes must be inserted
    # into bdb_types.hpp (after templates, before subclasses)
    begin_subclasses = '///  Int8 field type'
    replacement = \
        '%template(CBDB_FieldSimple_Int4) ncbi::CBDB_FieldSimple<Int4>;\n' \
        '%template(CBDB_FieldSimple_float) ncbi::CBDB_FieldSimple<float>;\n' \
        '%template(CBDB_FieldSimple_Uint8) ncbi::CBDB_FieldSimple<Uint8>;\n' \
        '%template(CBDB_FieldSimple_unsigned_char) ncbi::CBDB_FieldSimple<unsigned char>;\n' \
        '%template(CBDB_FieldSimple_double) ncbi::CBDB_FieldSimple<double>;\n' \
        '%template(CBDB_FieldSimple_char) ncbi::CBDB_FieldSimple<char>;\n' \
        '%template(CBDB_FieldSimple_Uint2) ncbi::CBDB_FieldSimple<Uint2>;\n' \
        '%template(CBDB_FieldSimple_Int8) ncbi::CBDB_FieldSimple<Int8>;\n' \
        '%template(CBDB_FieldSimple_Uint4) ncbi::CBDB_FieldSimple<Uint4>;\n' \
        '%template(CBDB_FieldSimple_Int2) ncbi::CBDB_FieldSimple<Int2>;\n' \
        '\n' \
        '%template(CBDB_FieldSimpleInt_Uint8) ncbi::CBDB_FieldSimpleInt<Uint8>;\n' \
        '%template(CBDB_FieldSimpleInt_Int4) ncbi::CBDB_FieldSimpleInt<Int4>;\n' \
        '%template(CBDB_FieldSimpleInt_unsigned_char) ncbi::CBDB_FieldSimpleInt<unsigned char>;\n' \
        '%template(CBDB_FieldSimpleInt_Int2) ncbi::CBDB_FieldSimpleInt<Int2>;\n' \
        '%template(CBDB_FieldSimpleInt_char) ncbi::CBDB_FieldSimpleInt<char>;\n' \
        '%template(CBDB_FieldSimpleInt_Uint4) ncbi::CBDB_FieldSimpleInt<Uint4>;\n' \
        '%template(CBDB_FieldSimpleInt_Int8) ncbi::CBDB_FieldSimpleInt<Int8>;\n' \
        '%template(CBDB_FieldSimpleInt_Uint2) ncbi::CBDB_FieldSimpleInt<Uint2>;\n' \
        '\n' \
        '%template(CBDB_FieldSimpleFloat_float) ncbi::CBDB_FieldSimpleFloat<float>;\n' \
        '%template(CBDB_FieldSimpleFloat_double) ncbi::CBDB_FieldSimpleFloat<double>;\n' \
        '\n' \
        + begin_subclasses
    s = Replace(s, begin_subclasses, '', replacement)

    return s
