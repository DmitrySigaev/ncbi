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
 * Authors:  Josh Cherry
 *
 * File Description:  Support for SWIG wrapping of std::string
 *
 */


%include ncbi_std_helpers.i

CATCH_OUT_OF_RANGE(std::string::__getitem__)

%{
#include <string>
%}

namespace std {

    %typecheck(SWIG_TYPECHECK_STRING) string {
        if (CheckPtrForbidNull($input, $&1_descriptor)) {
            $1 = 1;
        } else if (PyString_Check($input)) {
            $1 = 1;
        } else {
            $1 = 0;
        }
    }

    %typecheck(SWIG_TYPECHECK_STRING) const string & {
        if (CheckPtrForbidNull($input, $1_descriptor)) {
            $1 = 1;
        } else if (PyString_Check($input)) {
            $1 = 1;
        } else {
            $1 = 0;
        }
    }

    %typemap(in) string (std::string *s) {
        if (PyString_Check($input)) {
            $1 = std::string(PyString_AsString($input),
                             PyString_Size($input));
        } else if (SWIG_ConvertPtr($input, (void **) &s,
                                   $&1_descriptor, 0) != -1) {
            $1 = *s;
        } else {
            SWIG_exception(SWIG_TypeError, "string expected");
        }
    }

    %typemap(in) const string & (std::string temp, std::string *s) {
        if (PyString_Check($input)) {
            temp = std::string(PyString_AsString($input),
                               PyString_Size($input));
            $1 = &temp;
        } else if (SWIG_ConvertPtr($input, (void **) &s,
                                   $1_descriptor,0) != -1) {
            $1 = s;
        } else {
            SWIG_exception(SWIG_TypeError, "string expected");
        }
    }

    %typemap(out) string {
        $result = PyString_FromStringAndSize($1.data(), $1.size());
    }

    %typemap(out) const string& {
        $result = PyString_FromStringAndSize($1->data(), $1->size());
    }


    class string {
    public:
        // Used by some functions
        // System-dependent, but hopefully unsigned long will suffice
        typedef size_t size_type;  
                    
        string(void);
        string(const string& rhs);
        string(const string& rhs, size_type idx);
        string(const string& rhs, size_type idx, size_type num);
        string(size_type num, char ch);
        string(const char* cstr);
        string(const char* cstr, size_type len);
        ~string();

        size_type size(void) const;
        %extend {
            size_t __len__(void) {
                return self->size();
            }
        }
        void erase(void);
        string substr() const;
        string substr(size_type start) const;
        string substr(size_type start, size_type length) const;

        %extend {
            char __getitem__(ssize_t i) {
                std::string::size_type size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    return (*self)[i];
                }
            }

            std::string __getslice__(ssize_t i, ssize_t j) const {
                ssize_t size = self->size();
                AdjustSlice(i, j, size);
                std::string rv;
                rv.reserve(j - i);
                rv.insert(rv.begin(), self->begin() + i, self->begin() + j);
                return rv;
            }

            string __str__(void) {
                return *self;
            }

            bool operator==(const string& rhs) {
                return *self == rhs;
            }
            bool operator<(const string& rhs) {
                return *self < rhs;
            }
            bool operator>(const string& rhs) {
                return *self > rhs;
            }
            bool operator<=(const string& rhs) {
                return *self <= rhs;
            }
            bool operator>=(const string& rhs) {
                return *self >= rhs;
            }
            bool operator!=(const string& rhs) {
                return *self != rhs;
            }
        }

        // We want these functions to return a raw pointer,
        // i.e., not to have it converted to a scripting
        // language string.
        %typemap(out) orig_const_char_ptr = const char *;
        %typemap(out) const char * = SWIGTYPE *;
        const char* c_str(void);
        const char* data(void);
        %typemap(out) const char * = orig_const_char_ptr;

        %extend {
            string __add__(const string& rhs) {
                return *self + rhs;
            }
            string __radd__(const string& lhs) {
                return lhs + *self;
            }
        }
        string& operator+=(const string& rhs);
    };
}
