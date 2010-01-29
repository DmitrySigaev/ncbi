/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements.
 */

/** @file
 * This file contains the definition of the xml::event_parser class.
**/

#ifndef _xmlwrapp_event_parser_h_
#define _xmlwrapp_event_parser_h_

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>

// standard includes
#include <cstddef>
#include <string>
#include <iosfwd>
#include <map>
#include <vector>


// Forward declaration of a raw libxml2 structure
struct _xmlElementContent;

namespace xml {

namespace impl {
struct epimpl; // forward declaration of private implementation
}

/**
 * The current event_parser class implementation is not the fastest and is
 * not the most flexible. The idea behind the current implementation is to
 * keep the same approach as it was used in the original xmlwrapp 0.6.0.
 * The event_parser can be improved at least in the following areas:
 * - Currently handlers are called regardless whether the deriving class
 *   implements them or not. This leads to the fact that each element from
 *   the XML document is converted from xmlChar* to std::string. This leads
 *   to the performace drawbacks. It would be nicer if the handlers are called
 *   only if they are implemented in the deriving class. Unfortunately it
 *   is impossible to detect automatically in the base class whether a
 *   deriving class overrides a virtual function (at least using just portable
 *   ISO C++).
 * - Currently all the handlers arguments are converted from C types to
 *   their C++ counterparts even if just a part of them are used. It is
 *   possible to convert handler arguments to structures (classes) with ability
 *   to provide access to raw libxml2 data and to have lazy conversions to
 *   std::string. This will improve the parser performance.
**/

/**
 * The xml::event_parser is used to parse an XML document by calling member
 * functions when certain things in the XML document are parsed. In order to
 * use this class you derive a sub-class from it and override the protected
 * virtual functions.
**/
class event_parser {
public:
    typedef std::map<std::string, std::string> attrs_type;  ///< a type for holding XML node attributes
    typedef std::size_t size_type;                          ///< size type
    typedef std::vector<std::string> values_type;           ///< a type for holding attribute declaration values

    //####################################################################
    /**
     * xml::event_parser class constructor.
     *
     * @author Peter Jones
    **/
    //####################################################################
    event_parser (void);

    //####################################################################
    /**
     * xml::event_parser class destructor.
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~event_parser (void);

    //####################################################################
    /**
     * Call this member function to parse the given file.
     *
     * @param filename The name of the file to parse.
     * @return True if the file was successfully parsed; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool parse_file (const char *filename);

    //####################################################################
    /**
     * Parse what ever data that can be read from the given stream.
     *
     * @param stream The stream to read data from.
     * @return True if the stream was successfully parsed; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool parse_stream (std::istream &stream);

    //####################################################################
    /**
     * Call this function to parse a chunk of xml data. When you are done
     * feeding the parser chucks of data you need to call the parse_finish
     * member function.
     *
     * @param chunk The xml data chuck to parse.
     * @param length The size of the given data chunk
     * @return True if the chunk was parsed sucessfully; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool parse_chunk (const char *chunk, size_type length);

    //####################################################################
    /**
     * Finish parsing chunked data. You only need to call this member
     * function is you were parsing chunked xml data via the parse_chunk
     * member function.
     *
     * @return True if all parsing was successful; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool parse_finish (void);

    //####################################################################
    /**
     * If there was an error parsing the XML data, (indicated by one of the
     * parsing functions returning false), you can call this function to get
     * a message describing the error.
     *
     * @return A description of the XML parsing error.
     * @author Peter Jones
    **/
    //####################################################################
    const std::string& get_error_message (void) const;

protected:
    /// enum for different types of XML entities
    enum entity_type {
        type_internal_general_entity,
        type_external_general_parsed_entity,
        type_external_general_unparsed_entity,
        type_internal_parameter_entity,
        type_external_parameter_entity,
        type_internal_predefined_entity
    };

    /// enum for different types of XML attributes
    enum attribute_type {
        type_attribute_cdata,
        type_attribute_id,
        type_attribute_idref,
        type_attribute_idrefs,
        type_attribute_entity,
        type_attribute_entities,
        type_attribute_nmtoken,
        type_attribute_nmtokens,
        type_attribute_enumeration,
        type_attribute_notation
    };

    /// enum for different default attribute definition
    enum attribute_default_type {
        type_attribute_none,
        type_attribute_required,
        type_attribute_implied,
        type_attribute_fixed
    };

    /// enum for element content types
    enum element_content_type {
        type_undefined,
        type_empty,
        type_any,
        type_mixed,
        type_element
    };

    //####################################################################
    /**
     * Override this member function to receive the start_document message.
     * This member function is called when the document start is being
     * processed.
     *
     * @return You should return true to continue parsing; false to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool start_document ();
    //####################################################################
    /**
     * Override this member function to receive the start_document message.
     * This member function is called when the document end has been
     * detected.
     *
     * @return You should return true to continue parsing; false to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool end_document ();
    //####################################################################
    /**
     * Override this member function to receive the start_element message.
     * This member function is called when the parser encounters an xml
     * element.
     *
     * @param name The name of the element
     * @param attrs The element's attributes
     * @return You should return true to continue parsing; false to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool start_element (const std::string &name, const attrs_type &attrs) = 0;

    //####################################################################
    /**
     * Override this member function to receive the end_element message.
     * This member function is called when the parser encounters the closing
     * of an element.
     *
     * @param name The name of the element that was closed.
     * @return You should return true to continue parsing; false to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool end_element (const std::string &name) = 0;

    //####################################################################
    /** 
     * Override this member function to receive the text message. This
     * member function is called when the parser encounters text nodes.
     *
     * @param contents The contents of the text node.
     * @return You should return true to continue parsing; false to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool text (const std::string &contents) = 0;

    //####################################################################
    /**
     * Override this member function to receive the cdata mesage. This
     * member function is called when the parser encounters a <![CDATA[]]>
     * section in the XML data.
     *
     * The default implementation just calls the text() member function to
     * handle the text inside the CDATA section.
     *
     * @param contents The contents of the CDATA section.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool cdata (const std::string &contents);

    //####################################################################
    /**
     * Override this member function to receive the procesing_instruction
     * message. This member function will be called when the XML parser
     * encounters a processing instruction <?target data?>.
     *
     * The default implementation will ignore processing instructions and
     * return true.
     *
     * @param target The target of the processing instruction
     * @param data The data of the processing instruction.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool processing_instruction (const std::string &target, const std::string &data);

    //####################################################################
    /**
     * Override this member function to receive the comment message. This
     * member function will be called when the XML parser encounters a
     * comment <!-- contents -->.
     *
     * The default implementation will ignore XML comments and return true.
     *
     * @param contents The contents of the XML comment.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool comment (const std::string &contents);

    //####################################################################
    /**
     * Override this memeber function to receive parser warnings. The
     * default behaviour is to ignore warnings.
     *
     * @param message The warning message from the compiler.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Peter Jones
    **/
    //####################################################################
    virtual bool warning (const std::string &message);

    //####################################################################
    /**
     * Override this memeber function to receive the notation declaration
     * message. This member function will be called when the XML parser
     * encounters <!NOTATION ...> declaration.
     *
     * @param name The name of the notation.
     * @param public_id The public ID of the entity.
     * @param system_id The system ID of the entity.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool notation_declaration (const std::string &name,
                                       const std::string &public_id,
                                       const std::string &system_id);

    //####################################################################
    /**
     * Override this memeber function to receive the entity declaration
     * message. This member function will be called when the XML parser
     * encounters <!ENTITY ...> declaration.
     *
     * @param name The name of the entity.
     * @param type The type of the entity.
     * @param public_id The public ID of the entity.
     * @param system_id The system ID of the entity.
     * @param content The entity value.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool entity_declaration (const std::string &name,
                                     entity_type        type,
                                     const std::string &public_id,
                                     const std::string &system_id,
                                     const std::string &content);

    //####################################################################
    /**
     * Override this memeber function to receive the unparsed entity
     * declaration message.
     *
     * @param name The name of the entity.
     * @param public_id The public ID of the entity.
     * @param system_id The system ID of the entity.
     * @param notation_name The notation name.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool unparsed_entity_declaration (const std::string &name,
                                              const std::string &public_id,
                                              const std::string &system_id,
                                              const std::string &notation_name);

    //####################################################################
    /**
     * Override this memeber function to receive the external subset
     * declaration message.
     *
     * @param name The root element name.
     * @param external_id The external ID.
     * @param public_id The public ID.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool external_subset_declaration (const std::string &name,
                                              const std::string &external_id,
                                              const std::string &system_id);

    //####################################################################
    /**
     * Override this memeber function to receive the internal subset
     * declaration message.
     *
     * @param name The root element name.
     * @param external_id The external ID.
     * @param public_id The public ID.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool internal_subset_declaration (const std::string &name,
                                              const std::string &external_id,
                                              const std::string &system_id);

    //####################################################################
    /**
     * Override this memeber function to receive the attribute
     * declaration message.
     *
     * @param element_name The element name.
     * @param attribute_name The attribute full name.
     * @param attr_type The attribute type.
     * @param default_type The attribute default value type.
     * @param default_value The attribute default value.
     * @param default_values The attribute possible default values.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool attribute_declaration (const std::string &element_name,
                                        const std::string &attribute_name,
                                        attribute_type attr_type,
                                        attribute_default_type default_type,
                                        const std::string &default_value,
                                        const values_type &default_values);

    //####################################################################
    /**
     * Override this memeber function to receive the element
     * declaration message.
     *
     * @param name The element name.
     * @param type The element content type.
     * @param content The raw libxml2 structure pointer.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool element_declaration (const std::string &name,
                                      element_content_type type,
                                      _xmlElementContent *content);

    //####################################################################
    /**
     * Override this memeber function to receive the entity
     * reference message.
     *
     * @param name The ientity reference name.
     * @return You should return true to continue parsing.
     * @return Return false if you want to stop.
     * @note see http://mail.gnome.org/archives/xml/2009-May/msg00006.html
     *       and http://xmlsoft.org/entities.html for more details when you
     *       get errors on entity reference parsing
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    virtual bool entity_reference (const std::string &name);

    //####################################################################
    /**
     * Set the error message that will be returned from the
     * get_error_message() member function. If one of your callback
     * functions returns false and does not first call this memeber
     * function, "Unknown Error" will be returned from get_error_message().
     *
     * @param message The message to return from get_error_message().
     * @author Peter Jones
    **/
    //####################################################################
    void set_error_message (const char *message);
private:
    friend struct impl::epimpl;
    impl::epimpl *pimpl_; // private implementation

    entity_type get_entity_type (int);
    attribute_type get_attribute_type (int);
    attribute_default_type get_attribute_default_type (int);
    element_content_type get_element_content_type (int);

    /*
     * Don't allow anyone to copy construct an event_parser or to call the
     * assignment operator. It does not make sense to copy a parser if it is
     * half way done parsing. Plus, it would be a pain!
     */
    event_parser(const event_parser&);
    event_parser& operator= (const event_parser&);
}; // end xml::event_parser class

} // end xml namespace
#endif
