/* Host info */
#undef HOST
#undef HOST_CPU
#undef HOST_VENDOR
#undef HOST_OS

/* Define if C++ namespaces are supported */
#undef HAVE_NAMESPACE

/* Microsoft C++ specific */
#undef SIZEOF___INT64

/* Does not give enough support to the in-class template functions */
#undef NO_INCLASS_TMPL

/* Can use exception specifications("throw(...)" after func. proto) */
#undef NCBI_USE_THROW_SPEC

/* Non-standard basic_string::compare() -- most probably, from <bastring> */
#undef NCBI_OBSOLETE_STR_COMPARE

/* "auto_ptr" template class is not implemented in <memory> */
#undef HAVE_NO_AUTO_PTR

/* Fast-CGI library is available */
#undef HAVE_LIBFASTCGI
