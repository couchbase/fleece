//
// JSONSchema.hh
//
// Copyright 2025-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#ifndef _FLEECE_JSONSCHEMA_HH
#define _FLEECE_JSONSCHEMA_HH
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

FL_ASSUME_NONNULL_BEGIN

namespace fleece {

    /** Validates Values against a JSON Schema. (See https://json-schema.org )
     *
     *  Unsupported features (will throw an `unsupported_schema` exception if detected):
     *  - Path-relative `$ref`s (URIs that start with `/`)
     *  - `$dynamicRef`, `$dynamicAnchor`, `$vocabulary`
     *  - `format`, `contentEncoding`, `contentMediaType`
     *  - `dependencies`, `dependentRequired`, `dependentSchemas`, `extends`
     *  - `unevaluatedItems`, `unevaluatedProperties`
     *
     *  Known bugs:
     *  - JSON Schema's equality comparisons do not distinguish between integers and floats,
     *    so `7` is equal to `7.0`. However, Fleece considers ints and floats distinct types.
     *    This implementation conforms to JSON Schema equality when making direct comparisons
     *    between numeric Values, bbut _not_ when the numbers are nested in collections.
     *    So for example `[7]` will not match `[7.0]`.
     *
     *  @note  This class does not download schemas on demand; it does no I/O at all.
     *         See the docs of \ref unknownSchemaID to see how to handle external schema refs.
     *  @note  This class is thread-safe.
     */
    class JSONSchema {
    public:

        /** Thrown if errors are discovered in a schema. */
        class invalid_schema : public std::runtime_error { using runtime_error::runtime_error; };
        /** Thrown if a schema is found to use unsupported/unimplemented features. */
        class unsupported_schema : public std::runtime_error { using runtime_error::runtime_error; };

        class Validation;


        /// Constructor that takes a parsed JSON schema object.
        /// @note  The Value will be retained, so the caller doesn't need to keep a reference.
        /// @param schemaRoot  The parsed schema.
        /// @param id_uri  The absolute URI identifying this schema. Optional.
        /// @throws invalid_schema if the schema is invalid.
        /// @throws unsupported_schema if the schema uses unsupported features.
        explicit JSONSchema(Value schemaRoot, std::string_view id_uri = "");

        /// Convenience constructor that takes a JSON schema string and parses it.
        /// @param json  The schema as JSON data.
        /// @param id_uri  The absolute URI identifying this schema. Optional.
        /// @throws invalid_schema if the schema is invalid.
        /// @throws unsupported_schema if the schema uses unsupported features.
        explicit JSONSchema(std::string_view json, std::string_view id_uri = "");

        ~JSONSchema();

        /// The root of the parsed schema. (Almost always a Dict.)
        Value schema() const;

        /// Registers an external schema that the main schema may refer to.
        /// @note  The Dict will be retained, so the caller doesn't need to keep a reference.
        /// @param schemaRoot  The parsed schema.
        /// @param id_uri  The absolute URI identifying this schema.
        /// @throws invalid_schema if the schema is invalid.
        /// @throws unsupported_schema if the schema uses unsupported features.
        void addSchema(Dict schemaRoot, std::string_view id_uri);

        /// Validates a parsed Fleece value against the schema.
        /// @returns A \ref Validation object describing the result.
        /// @throws invalid_schema if the schema itself is invalid.
        /// @throws unsupported_schema if the schema uses unsupported features.
        Validation validate(Value value) const LIFETIMEBOUND;

        /// Convenience method that parses JSON and then validates it against the schema.
        /// @returns A \ref Validation object describing the result.
        /// @throws std::invalid_argument if the JSON fails to parse.
        /// @throws invalid_schema if the schema itself is invalid.
        /// @throws unsupported_schema if the schema uses unsupported features.
        Validation validate(std::string_view json) const LIFETIMEBOUND;
        Validation validate(std::string_view json, SharedKeys) const LIFETIMEBOUND;


        /** Errors that can occur during validation. */
        enum class Error : unsigned {
            ok = 0,
            invalid,            // value matched against a "false" in the schema
            typeMismatch,       // value doesn't match "type" property in schema
            outOfRange,         // Number is out of range of "minimum", etc.
            notMultiple,        // Number is not a multiple of the "multipleOf"
            tooShort,           // String is too short or collection has too few items
            tooLong,            // String is too long or collection has too many items
            patternMismatch,    // String doesn't match regex pattern
            missingProperty,    // Dict is missing a required property
            unknownProperty,    // Dict has an invalid property
            notEnum,            // Value doesn't match any "enum" or "const" value
            tooFew,             // Value doesn't match anything in an "anyOf" or "oneOf" array
            tooMany,            // "oneOf" or "maxContains" failed
            notNot,             // Value matched a "not" schema
            notUnique,          // Array items are not unique
            invalidUTF8,        // A string's length could not be checked because of invalid UTF-8
            unknownSchemaRef,   // Reference to a schema URI that's not registered
        };

        static bool ok(Error e) noexcept {return e == Error::ok;}
        static std::string_view errorString(Error) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };


    /** The result of validating against a JSONSchema. */
    class JSONSchema::Validation {
    public:
        /// True if validation succeeded.
        bool ok() const noexcept                    {return _result.error == Error::ok;}
        explicit operator bool() const              {return ok();}

        /// The specific error. (Will be `Error::ok` if there was no error.)
        Error error() const noexcept                { return _result.error; }

        /// The specific error, as a string.
        std::string errorString() const noexcept;

        /// The detected invalid Value; either the one passed to \ref validate
        /// or something nested in it. (Will be nullptr if there was no error.)
        Value errorValue() const noexcept           {return _result.value;}

        /// On error, this is the path to the detected invalid Value, in \ref KeyPath syntax.
        std::string errorPath() const noexcept;

        /// The key and value of the item in the schema that caused the failure;
        /// e.g. `{"maxLength", 5}`.
        std::pair<slice,Value> errorSchema() const noexcept;

        /// A URI pointing to the item in the schema that caused the failure.
        std::string errorSchemaURI() const noexcept;

        /// If the error is `Error::unknownSchemaRef`, this is the URI of the unknown schema.
        /// If you can download or otherwise look up the schema, you can call \ref addSchema
        /// to register it, then call \ref validate again to retry.
        std::string const& unknownSchemaID() const noexcept {return _unknownSchema;}

        struct Result {Error error; Value value; Value schema; slice schemaKey;};
        static bool ok(Result const& e) noexcept {return e.error == Error::ok;}
    private:
        friend class JSONSchema;

        Validation(JSONSchema const& schema, Value value);
        Result check(Value value, Value schema, Dict schemaBase);
        Result checkValue(Value value, Dict schema, Dict schemaBase);
        Result checkNumber(Value value, Dict schema, Dict schemaBase);
        Result checkString(Value value, Dict schema, Dict schemaBase);
        Result checkArray(Array, Dict schema, Dict schemaBase);
        Result checkDict(Dict, Dict schema, Dict schemaBase);

        static bool isType(Value value, Value typeVal);
        static bool isType(Value value, slice schemaType);

        Impl const&     _schemaImpl;    // The guts of the owning JSONSchema
        RetainedValue   _value;         // The root Value being validated (only after failure)
        Result          _result {};     // Details of validation error
        std::string     _unknownSchema; // Unknown schema ID found during validation
    };

}

FL_ASSUME_NONNULL_END

#endif // _FLEECE_JSONSCHEMA_HH
