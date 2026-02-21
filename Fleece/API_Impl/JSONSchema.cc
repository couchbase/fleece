//
// JSONSchema.cc
//
// Copyright 2025-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/JSONSchema.hh"
#include "fleece/Expert.hh"
#include <charconv>
#include <cmath>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "betterassert.hh"

#ifdef _MSC_VER
#include "asprintf.h"
#endif

namespace fleece {
    using namespace std;


#pragma mark - UTILITIES:


    static constexpr slice kFLTypeNames[] = {   // indexed by FLValueType
        "null", "boolean", "number", "string", "data", "array", "object"};


    // Throws an exception with a printf-formatted message.
    template <class X>
    [[noreturn]] __printflike(1, 2) static
    void fail(const char* format, ...) {
        va_list ap;
        va_start(ap, format);
        char* formatted = nullptr;
        vasprintf(&formatted, format, ap);
        va_end(ap);
        string message(formatted);
        free(formatted);
        throw X(message);
    }


    // If `value` is not of `type`, throws `invalid_schema`. (`name` is used in the message.)
    static void requireType(Value value, slice name, FLValueType type) {
        if (value.type() != type) [[unlikely]]
            fail<JSONSchema::invalid_schema>("type of \"%.*s\" must be %.*s",
                FMTSLICE(name), FMTSLICE(kFLTypeNames[type]));
    };


    // Returns true if a Value is a number with an integral value.
    static bool isIntegral(Value v) {
        if (v.isInteger())
            return true;
        if (v.type() != kFLNumber)
            return false;
        double d = v.asDouble();
        return d == floor(d);
    }


    // Compare Values, treating ints and floats with the same value as equal:
    static bool isEqual(Value a, Value b) {
        //FIXME: This doesn't handle ints vs. floats in nested values.
        if (a.isEqual(b))
            return true;
        if (a.isInteger() != b.isInteger() && a.type() == kFLNumber && b.type() == kFLNumber)
            return a.asDouble() == b.asDouble();
        return false;
    }


    // Validates the length in characters of a UTF-8 string. Avoids UTF-8 parsing if possible.
    static JSONSchema::Error checkUTF8Length(slice str, size_t minLength, size_t maxLength) {
        using enum JSONSchema::Error;
        // If we know the answer without having to scan the UTF-8, return it:
        auto mostChars = str.size, leastChars = (mostChars + 3) / 4;
        if (leastChars >= minLength && mostChars <= maxLength) [[likely]]
            return ok;
        if (mostChars < minLength)
            return tooShort;
        if (leastChars > maxLength)
            return tooLong;
        // OK, we have to scan the string to count characters:
        auto [length, valid] = str.UTF8Length();
        if (!valid) [[unlikely]]
            return invalidUTF8;
        if (length < minLength)
            return tooShort;
        if (length > maxLength)
            return tooLong;
        return ok;
    }


    // True if a URI appears to be a JSONPointer (equal to `#`, or starts with `#/`)
    static bool isJSONPointerURI(string_view uri) {
        return uri.starts_with('#') && (uri.size() == 1 || uri[1] == '/');
    }


    // True if a URI is absolute.
    static bool isAbsoluteURI(string_view uri) {
        auto colon = uri.find(':');
        return colon != string_view::npos
            && uri.substr(0, colon).find('/') == string_view::npos
            && isalpha(uri[0]);
    }


    // Returns a prefix of an absolute URI up to but not including the first '/' of the path.
    // Given "http://example.com/foo" or "http://example.com", returns "http://example.com".
    static string_view rootOfAbsoluteURI(string_view uri) {
        auto pos = uri.find("://");
        if (pos == string_view::npos)
            return "";
        auto slash = uri.find('/', pos + 3);
        if (slash == string_view::npos)
            slash = uri.size();
        return uri.substr(0, slash);
    }


    // Interprets URI `rel` relative to `base`.
    static string concatURIs(string_view base, string_view rel) {
        if (base.empty() || isAbsoluteURI(rel)) {
            return string(rel);
        } else if (rel.starts_with('/')) {
            if (!isAbsoluteURI(base))
                return string(rel);
            string_view root = rootOfAbsoluteURI(base);
            if (root.empty())
                fail<JSONSchema::invalid_schema>("can't resolve <%.*s> relative to <%.*s>",
                    FMTSLICE(slice(rel)), FMTSLICE(slice(base)));
            return string(root).append(rel);
        } else {
            string result(base);
            if (auto hash = result.find('#'); hash != string_view::npos)
                result = result.substr(0, hash);
            if (!result.ends_with('/') && !rel.starts_with('#')) {
                if (auto lastSlash = result.find_last_of('/'); lastSlash != string_view::npos)
                    result = result.substr(0, lastSlash + 1);
            }
            result += rel;
            return result;
        }
    }


    // Converts '%' escapes back into their original characters.
    static void unescapeURI(string& uri) {
        auto digittohex = [](uint8_t c) {
            // ( precondition: isxdigit(c) )
            if (c >= 'a') return c - 'a' + 10;
            else if (c >= 'A') return c - 'A' + 10;
            else return c - '0';
        };
        size_t start = 0, pos = 0;
        while (string::npos != (pos = uri.find('%', start)) && pos + 2 < uri.size()) {
            auto d1 = uint8_t(uri[pos + 1]), d2 = uint8_t(uri[pos + 2]);
            if (isxdigit(d1) && isxdigit(d2)) {         // note: not locale-sensitive
                uri[pos] = char(16 * digittohex(d1) + digittohex(d2));
                uri.erase(pos + 1, 2);
                start = pos + 1;
            } else {
                start = pos + 3;
            }
        }
    }


    // Finds a target value within a container and returns the path to it.
    // Warning: This can be ambiguous if `target` is a string, because the Fleece encoder
    // de-dups strings.
    static alloc_slice recoverPath(Value root, Value target, bool asJSONPointer = false) {
        if (target && root) {
            for (DeepIterator i(root); i; ++i) {
                if (i.value() == target)
                    return asJSONPointer ? i.JSONPointer() : i.pathString();
            }
        }
        return nullslice;
    }


    // Parses a JSON string to Fleece, optionally using SharedKeys.
    static Doc parseJSON(string_view json, FLSharedKeys sk) {
        Encoder enc;
        enc.setSharedKeys(sk);
        enc.convertJSON(json);
        FLError err;
        Doc doc = enc.finishDoc(&err);
        if (!doc)
            throw invalid_argument("invalid JSON");
        if (auto type = doc.root().type(); type != kFLDict && type != kFLBoolean)
            throw JSONSchema::invalid_schema("JSON Schema must be an object (Dict) or boolean");
        return doc;
    }


#pragma mark - SHARED KEYS:


    // All the keys in a schema that we look up.
    namespace sharedkey {
        enum {
            AdditionalProperties,
            AllOf,
            AnyOf,
            Const,
            Contains,
            Else,
            Enum,
            ExclusiveMaximum,
            ExclusiveMinimum,
            If,
            Items,
            MaxContains,
            MaxItems,
            MaxLength,
            MaxProperties,
            Maximum,
            MinContains,
            MinItems,
            MinLength,
            MinProperties,
            Minimum,
            MultipleOf,
            Not,
            OneOf,
            Pattern,
            PatternProperties,
            PrefixItems,
            Properties,
            PropertyNames,
            Ref,
            Required,
            Then,
            Type,
            UniqueItems,

            NKeys_
        };
    }

    // Names of the keys in the `sharedkey` enum.
    static constexpr const char* kSharedKeyStrings[] = {
        "additionalProperties",
        "allOf",
        "anyOf",
        "const",
        "contains",
        "else",
        "enum",
        "exclusiveMaximum",
        "exclusiveMinimum",
        "if",
        "items",
        "maxContains",
        "maxItems",
        "maxLength",
        "maxProperties",
        "maximum",
        "minContains",
        "minItems",
        "minLength",
        "minProperties",
        "minimum",
        "multipleOf",
        "not",
        "oneOf",
        "pattern",
        "patternProperties",
        "prefixItems",
        "properties",
        "propertyNames",
        "$ref",
        "required",
        "then",
        "type",
        "uniqueItems",
    };

    static_assert(std::size(kSharedKeyStrings) == sharedkey::NKeys_);

    // The singleton SharedKeys instance used for all parsed schema.
    static FLSharedKeys sSchemaSharedKeys;

    // Optimized Dict keys, indexed by `sharedkey`.
    static FLDictKey sharedkeys[sharedkey::NKeys_];

    #define SHARED_KEY(NAME) sharedkeys[sharedkey::NAME]


    static void initSharedKeys() {
        static once_flag sOnce;
        call_once(sOnce, [] {
            sSchemaSharedKeys = FLSharedKeys_New();
            for (unsigned i = 0; i < sharedkey::NKeys_; ++i) {
                FLSharedKeys_Encode(sSchemaSharedKeys, slice(kSharedKeyStrings[i]), true);
                sharedkeys[i] = FLDictKey_Init(slice(kSharedKeyStrings[i]));
            }
        });
    }


    // Parses a JSON schema, using a singleton SharedKeys instance.
    static RetainedValue parseSchema(string_view json) {
        initSharedKeys();
        Doc doc = parseJSON(json, sSchemaSharedKeys);
        if (!doc)
            throw invalid_argument("invalid JSON in schema");
        return doc.root();
    }

    // Re-encodes a Value, if necessary, so that it uses our singleton SharedKeys instance.
    static RetainedValue reencodeSchema(Value originalSchema) {
        initSharedKeys();
        precondition(originalSchema);
        if (auto doc = FLValue_FindDoc(originalSchema)) {
            if (FLDoc_GetSharedKeys(doc) == sSchemaSharedKeys)
                return originalSchema;
        }
        Encoder enc;
        enc.setSharedKeys(sSchemaSharedKeys);
        enc.writeValue(originalSchema);
        return enc.finishDoc().root();
    }


#pragma mark - JSON SCHEMA:


    struct JSONSchema::Impl {
        std::shared_mutex mutable           _mutex;         // Read/write lock
        RetainedValue const                 _schema;        // Root of main schema
        std::string                         _schemaURI;     // URI of schema, if known
        std::map<std::string,RetainedValue> _knownSchemas;  // Known schemas, by URI
        std::map<slice, std::regex>         _regexes;       // Cached regex objects

        void scanSchema(Value schema, std::string_view parentID);
        void registerSchema(Dict root, std::string id);
        Value resolveSchemaRef(std::string_view ref, Dict schemaBase) const;
        std::string schemaValueURI(Value schemaVal) const;
        void addPattern(slice);
        bool stringMatchesPattern(slice str, slice pattern) const;
    };


    JSONSchema::JSONSchema(Value root, string_view uri)
    :_impl(new Impl{._schema = reencodeSchema(root), ._schemaURI = string(uri)})
    {
        _impl->scanSchema(_impl->_schema, uri);
    }


    JSONSchema::JSONSchema(string_view json, std::string_view uri)
    :_impl(new Impl{._schema = parseSchema(json), ._schemaURI = string(uri)})
    {
        _impl->scanSchema(_impl->_schema, uri);
    }


    JSONSchema::~JSONSchema() = default;


    Value JSONSchema::schema() const {
        return _impl->_schema;
    }


    // Traverses a parsed schema, finding errors or unsupported features.
    // Also registers nested schemas and compiles regexes needed for pattern matching.
    void JSONSchema::Impl::scanSchema(Value schema, string_view parentID) {
        enum KeyType {Any, AString, ANumber, AnInteger, AnArray, Type, Pattern, PatternProperties,
                      Recurse, RecurseArray, RecurseOnValues, Unsupported};
        static const unordered_map<slice,KeyType> sKeyMap {
            // Meta stuff:
            {"$id",                 AString},
            {"$anchor",             AString},
            {"$schema",             AString},
            {"$ref",                AString},
            {"$defs",               RecurseOnValues},

            // Ignored for validation:
            {"$comment",            AString},
            {"description",         AString},
            {"default",             Any},

            // Applies to any type:
            {"type",                Type},
            {"const",               Any},
            {"allOf",               RecurseArray},
            {"anyOf",               RecurseArray},
            {"oneOf",               RecurseArray},
            {"enum",                AnArray},
            {"if",                  Recurse},
            {"then",                Recurse},
            {"else",                Recurse},
            {"not",                 Recurse},

            // Numbers:
            {"minimum",             ANumber},
            {"maximum",             ANumber},
            {"exclusiveMinimum",    ANumber},
            {"exclusiveMaximum",    ANumber},
            {"multipleOf",          ANumber},

            // Strings:
            {"minLength",           ANumber},
            {"maxLength",           ANumber},
            {"pattern",             Pattern},

            // Arrays:
            {"items",               Recurse},
            {"prefixItems",         RecurseArray},
            {"additionalItems",     Recurse},
            {"minItems",            AnInteger},
            {"maxItems",            AnInteger},
            {"uniqueItems",         Any},
            {"contains",            Recurse},
            {"minContains",         AnInteger},
            {"maxContains",         AnInteger},

            // Objects:
            {"properties",          RecurseOnValues},
            {"minProperties",       AnInteger},
            {"maxProperties",       AnInteger},
            {"propertyNames",       Recurse},
            {"patternProperties",   PatternProperties},
            {"additionalProperties",Recurse},
            {"required",            AnArray},

            // Unsupported:
            {"$dynamicAnchor",      Unsupported},
            {"$dynamicRef",         Unsupported},
            {"$vocabulary",         Unsupported},
            {"contentEncoding",     Unsupported},
            {"contentMediaType",    Unsupported},
            {"dependencies",        Unsupported},
            {"dependentRequired",   Unsupported},
            {"dependentSchemas",    Unsupported},
            {"extends",             Unsupported},
            {"format",              Unsupported},
            {"unevaluatedItems",    Unsupported},
            {"unevaluatedProperties",Unsupported},
        };

        if (Dict dict = schema.asDict()) {
            string newID;
            // "$id" and "$anchor" register new schemas; do this first before recursing:
            if (slice id = dict["$id"].asString()) {
                // Register a nested schema "$id":
                newID = concatURIs(parentID, id);
                registerSchema(dict, newID);
                parentID = newID;
            }
            if (slice anchor = dict["$anchor"].asString()) {
                if (anchor.empty() || !isalpha(anchor[0]))
                    fail<invalid_schema>("invalid $anchor \"%.*s\"", FMTSLICE(anchor));
                registerSchema(dict, concatURIs(parentID, "#"s + string(anchor)));
            }

            // Now look at each key and process it according to its type:
            for (Dict::iterator i(dict); i; ++i) {
                slice key = i.keyString();
                Value val = i.value();
                if (auto imap = sKeyMap.find(key); imap != sKeyMap.end()) {
                    switch (imap->second) {
                    case Any:
                        break;
                    case ANumber:
                        requireType(val, key, kFLNumber);
                        break;
                    case AString:
                        requireType(val, key, kFLString);
                        break;
                    case AnInteger:
                        if (!isIntegral(val))
                            fail<invalid_schema>("value of \"%.*s\" must be an integer", FMTSLICE(key));
                        break;
                    case AnArray:
                        requireType(val, key, kFLArray);
                        break;
                    case Type:
                        (void)Validation::isType(nullptr, val); // will throw if val is invalid
                        break;
                    case Pattern:
                        requireType(val, key, kFLString);
                        addPattern(val.asString());
                        break;
                    case PatternProperties:
                        requireType(val, key, kFLDict);
                        for (Dict::iterator j(val.asDict()); j; ++j)
                            addPattern(j.keyString());
                        break;
                    case Recurse:
                        if (auto type = val.type(); type != kFLDict && type != kFLBoolean)
                            fail<invalid_schema>("value of \"%.*s\" must be a schema", FMTSLICE(key));
                        scanSchema(val, parentID);
                        break;
                    case RecurseArray:
                        requireType(val, key, kFLArray);
                        for (Array::iterator j(val.asArray()); j; ++j)
                            scanSchema(j.value(), parentID);
                        break;
                    case RecurseOnValues:
                        requireType(val, key, kFLDict);
                        for (Dict::iterator j(val.asDict()); j; ++j)
                            scanSchema(j.value(), parentID);
                        break;
                    case Unsupported:
                        fail<unsupported_schema>("unsupported property \"%.*s\"", FMTSLICE(key));
                    }
                } else {
                    fail<invalid_schema>("unknown property \"%.*s\"", FMTSLICE(key));
                }
            }
        } else if (schema.type() != kFLBoolean) {
            slice name = kFLTypeNames[schema.type()];
            fail<unsupported_schema>("a %.*s cannot be a schema", FMTSLICE(name));
        }
    }


    void JSONSchema::addSchema(Dict schema, std::string_view id) {
        unique_lock lock(_impl->_mutex);
        string idstr(id);
        if (!isAbsoluteURI(id))
            fail<invalid_schema>("schema id <\"%s\"> is not an absolute URI", idstr.c_str());
        _impl->registerSchema(schema, std::move(idstr));
        _impl->scanSchema(schema, id);
    }


    void JSONSchema::Impl::registerSchema(Dict schema, string id) {
        precondition(schema);
        precondition(id.starts_with('#') || isAbsoluteURI(id));
        if (auto i = _knownSchemas.find(id); i == _knownSchemas.end()) {
            _knownSchemas.emplace(std::move(id), schema);
        } else if (i->second.isEqual(schema)) {
            fail<invalid_schema>("schema id <%s> is already registered as a different schema", id.c_str());
        }
    }


    Value JSONSchema::Impl::resolveSchemaRef(std::string_view ref, Dict schemaBase) const {
        slice originalRef = ref;
        auto failBadRef = [&](const char* msg) {
            fail<invalid_schema>("%s: %.*s", msg, FMTSLICE(originalRef));
        };

        Dict schema;
        string absRefStr;
        if (!isJSONPointerURI(ref)) {
            if (!isAbsoluteURI(ref)) {
                // Get the parent schema ID to resolve the ref:
                string_view schemaID = schemaBase["$id"].asString();
                if (schemaID.empty())
                    schemaID = _schemaURI;
                if (!schemaID.empty()) {
                    absRefStr = concatURIs(schemaID, ref);
                    ref = absRefStr;
                }
            }

            if (auto i = _knownSchemas.find(string(ref)); i != _knownSchemas.end()) {
                // Exact match:
                return i->second;
            }

            // Look at schemas to find one that's a prefix of ref:
            for (auto& [uri, sch] : _knownSchemas) {
                if (ref.starts_with(uri)) {
                    if (auto len = uri.size(); ref[len] == '#') {
                        // ref is relative to _schemaID, so make it a relative URI:
                        ref = ref.substr(len);
                        schema = sch.asDict();
                        break;
                    }
                }
            }
            if (!schema) {
                // Reference to unknown schema URI
                return nullptr;
            }
        } else {
            schema = schemaBase;
        }

        if (ref == "#") {
            return schema;
        } else if (ref.starts_with('#')) {
            if (isJSONPointerURI(ref)) {
                string ptr(ref.substr(1));
                unescapeURI(ptr);
                FLError flErr;
                Value dst = FLEvalJSONPointer(slice(ptr), schema, &flErr);
                if (!dst) {
                    if (flErr)
                        failBadRef("invalid JSON pointer");
                    else
                        failBadRef("schema reference JSON pointer doesn't resolve");
                }
                return dst;
            } else {
                failBadRef("invalid relative schema reference");
                return nullptr; //unreachable
            }
        } else {
            failBadRef("can't resolve reference");
            return nullptr; //unreachable
        }
    }


    string JSONSchema::Impl::schemaValueURI(Value schemaVal) const {
        string uri;
        alloc_slice path = recoverPath(_schema, schemaVal, true);
        if (path) {
            uri = _schemaURI;
        } else {
            shared_lock lock(_mutex);
            for (auto& [auri, aroot] : _knownSchemas) {
                path = recoverPath(aroot, schemaVal, true);
                if (path) {
                    uri = auri;
                    break;
                }
            }
            if (!path)
                return "";  // should never happen
        }
        return uri.append("#").append(path);
    }


    void JSONSchema::Impl::addPattern(slice pattern) {
        if (!_regexes.contains(pattern)) {
            try {
                _regexes.emplace(pattern, regex((const char*)pattern.buf, pattern.size));
            } catch (regex_error const&) {
                fail<invalid_schema>("invalid regular expression: %.*s", FMTSLICE(pattern));
            }
        }
    }


    bool JSONSchema::Impl::stringMatchesPattern(slice str, slice pattern) const {
        if (auto i = _regexes.find(pattern); i != _regexes.end()) [[likely]]
            return regex_search(string(str), i->second);
        throw logic_error("JSONSchema failed to pre-cache regex: " + string(pattern));
    }


#pragma mark - VALIDATION:


    JSONSchema::Validation JSONSchema::validate(Value value) const {
        assert_precondition(value);
        // Lock allows concurrent validation, but blocks mutation (addSchema).
        shared_lock lock(_impl->_mutex);
        return Validation(*this, value);
    }

    JSONSchema::Validation JSONSchema::validate(std::string_view json, SharedKeys sk) const {
        return validate(parseJSON(json, sk));
    }

    JSONSchema::Validation JSONSchema::validate(std::string_view json) const {
        return validate(parseJSON(json, SharedKeys{}));
    }


    JSONSchema::Validation::Validation(JSONSchema const& schema, Value value)
    :_schemaImpl(*schema._impl)
    {
        Result result = check(value, _schemaImpl._schema, _schemaImpl._schema.asDict());
        if (!ok(result)) {
            _result = result;
            _value = value;    // Retain it to save for later use
        }
    }


    using Result = JSONSchema::Validation::Result;

    static Result mkResult(JSONSchema::Error error, Value value, Value schema, slice schemaKey) {
        // cerr << "\tError: " << JSONSchema::errorString(error) << " for " << value.toJSONString() << " failed " << string_view(schemaKey)
        //     << ": " << schema[schemaKey].toJSONString() << endl;
        return Result{error, value, schema, schemaKey};
    }


    /// Checks a value against a schema. This is recursively called during validation.
    Result JSONSchema::Validation::check(Value value, Value schemaVal, Dict schemaBase) {
        if (auto schemaDict = schemaVal.asDict()) [[likely]] {
            // Most schema nodes are Dicts:
            if (schemaDict.empty()) [[unlikely]]
                return {};        // Empty dict matches anything

            if (schemaDict["$id"].asString()) {
                // This is a nested schema; it becomes the `schemaBase` for resolving references:
                schemaBase = schemaDict;
            }

            // First the checks that apply to any Values:
            if (Result err = checkValue(value, schemaDict, schemaBase); !ok(err))
                return err;

            // Then type-specific checks:
            switch (value.type()) {
                case kFLNumber: return checkNumber(value, schemaDict, schemaBase);
                case kFLString: return checkString(value, schemaDict, schemaBase);
                case kFLArray:  return checkArray(value.asArray(), schemaDict, schemaBase);
                case kFLDict:   return checkDict(value.asDict(), schemaDict, schemaBase);
                default:        return {};
            }
        } else if (schemaVal.type() == kFLBoolean) [[likely]] {
            // `true` matches anything, `false` matches nothing:
            return mkResult(schemaVal.asBool() ? Error::ok : Error::invalid, value, schemaVal, nullslice);
        } else {
            fail<invalid_schema>("invalid value type in schema");
        }
    }


    /// Checks the generic schema constraints of a Value.
    Result JSONSchema::Validation::checkValue(Value value, Dict schema, Dict schemaBase) {
        // "type":
        if (Value type = schema[SHARED_KEY(Type)]; type && !isType(value, type)) [[unlikely]] {
            return mkResult(Error::typeMismatch, value, schema, "type");
        }

        // "const":
        if (Value c = schema[SHARED_KEY(Const)]) {
            if (!isEqual(value, c)) [[unlikely]]
                return mkResult(Error::notEnum, value, schema, "const");
        }

        // "enum":
        if (Array e = schema[SHARED_KEY(Enum)].asArray()) {
            bool matches = false;
            for (Array::iterator i(e); i; ++i) {
                if (isEqual(value, i.value())) {
                    matches = true;
                    break;
                }
            }
            if (!matches) [[unlikely]]
                return mkResult(Error::notEnum, value, schema, "enum");
        }

        // "not":
        if (Value n = schema[SHARED_KEY(Not)]) {
            if (auto err = check(value, n, schemaBase); ok(err)) [[unlikely]]
                return mkResult(Error::notNot, value, schema, "not");
        }

        // "allOf":
        if (Array all = schema[SHARED_KEY(AllOf)].asArray()) {
            for (Array::iterator i(all); i; ++i) {
                if (auto err = check(value, i.value(), schemaBase); !ok(err)) [[unlikely]]
                    return err;
            }
        }

        // "anyOf":
        if (Array any = schema[SHARED_KEY(AnyOf)].asArray()) {
            bool matched = false;
            for (Array::iterator i(any); i; ++i) {
                if (auto err = check(value, i.value(), schemaBase); ok(err)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) [[unlikely]]
                return mkResult(Error::tooFew, value, schema, "anyOf");
        }

        // "oneOf":
        if (Array any = schema[SHARED_KEY(OneOf)].asArray()) {
            unsigned matches = 0;
            for (Array::iterator i(any); i; ++i) {
                if (auto err = check(value, i.value(), schemaBase); ok(err))
                    ++matches;
            }
            if (matches != 1) [[unlikely]]
                return mkResult(matches ? Error::tooMany : Error::tooFew, value, schema, "oneOf");
        }

        // "if", "then", "else":
        if (Value ifSchema = schema[SHARED_KEY(If)]) {
            Value thenSchema = schema[SHARED_KEY(Then)], elseSchema = schema[SHARED_KEY(Else)];
            if (thenSchema || elseSchema) {
                bool ifOK = ok(check(value, ifSchema, schemaBase));
                if (Value nextSchema = ifOK ? thenSchema : elseSchema) {
                    if (auto err = check(value, nextSchema, schemaBase); !ok(err)) [[unlikely]]
                        return err;
                }
            }
        }

        if (slice ref = schema[SHARED_KEY(Ref)].asString()) {
            Value refSchema = _schemaImpl.resolveSchemaRef(ref, schemaBase);
            if (!refSchema) [[unlikely]] {
                _unknownSchema = string(ref);
                return mkResult(Error::unknownSchemaRef, value, schema, "$ref");
            }
            if (auto err = check(value, refSchema, schemaBase); !ok(err)) [[unlikely]]
                return err;
        }

        return {};
    }


    /// Checks a number value against a schema.
    Result JSONSchema::Validation::checkNumber(Value value, Dict schema, Dict schemaBase) {
        double n = value.asDouble();
        if (Value minV = schema[SHARED_KEY(Minimum)])
            if (n < minV.asDouble()) [[unlikely]]
                return mkResult(Error::outOfRange, value, schema, "minimum");
        if (Value minV = schema[SHARED_KEY(ExclusiveMinimum)])
            if (n <= minV.asDouble()) [[unlikely]]
                return mkResult(Error::outOfRange, value, schema, "exclusiveMinimum");
        if (Value maxV = schema[SHARED_KEY(Maximum)])
            if (n > maxV.asDouble()) [[unlikely]]
                return mkResult(Error::outOfRange, value, schema, "maximum");
        if (Value maxV = schema[SHARED_KEY(ExclusiveMaximum)])
            if (n >= maxV.asDouble()) [[unlikely]]
                return mkResult(Error::outOfRange, value, schema, "exclusiveMaximum");
        if (Value mult = schema[SHARED_KEY(MultipleOf)]) {
            double d = n / mult.asDouble();
            if (d != floor(d) || isinf(d)) [[unlikely]]
                return mkResult(Error::notMultiple, value, schema, "multipleOf");
        }
        return {};
    }


    /// Checks a string value against a schema.
    Result JSONSchema::Validation::checkString(Value value, Dict schema, Dict schemaBase) {
        slice str = value.asString();
        if (Value minV = schema[SHARED_KEY(MinLength)], maxV = schema[SHARED_KEY(MaxLength)]; minV || maxV) {
            Error err = checkUTF8Length(str, minV ? minV.asUnsigned() : 0,
                                             maxV ? maxV.asUnsigned() : SIZE_MAX);
            if (err != Error::ok) [[unlikely]] {
                slice prop = (err == Error::tooShort) ? "minLength" : "maxLength";
                return mkResult(err, value, schema, prop);
            }
        }
        if (Value patV = schema[SHARED_KEY(Pattern)]) {
            if (!_schemaImpl.stringMatchesPattern(str, patV.asString())) [[unlikely]]
                return mkResult(Error::patternMismatch, value, schema, "pattern");
        }
        return {};
    }


    /// Checks an array value against a schema.
    Result JSONSchema::Validation::checkArray(Array array, Dict schema, Dict schemaBase) {
        auto count = array.count();
        if (Value minV = schema[SHARED_KEY(MinItems)])
            if (count < minV.asUnsigned()) [[unlikely]]
                return mkResult(Error::tooShort, array, schema, "minItems");
        if (Value maxV = schema[SHARED_KEY(MaxItems)])
            if (count > maxV.asUnsigned()) [[unlikely]]
                return mkResult(Error::tooLong, array, schema, "maxItems");

        // "prefixItems":
        int checkIndex = 0;
        if (Array prefixItems = schema[SHARED_KEY(PrefixItems)].asArray()) {
            for (Array::iterator i(prefixItems); i; ++i, ++checkIndex) {
                if (checkIndex >= count)
                    break;
                if (auto err = check(array[checkIndex], i.value(), schemaBase); !ok(err)) [[unlikely]]
                    return err;
            }
        }

        // "items":
        if (Value items = schema[SHARED_KEY(Items)]) {
            for (; checkIndex < count; ++checkIndex) {
                if (auto err = check(array[checkIndex], items, schemaBase); !ok(err)) [[unlikely]]
                    return err;
            }
        }

        // "contains", "minContains", "maxContains":
        if (Value contains = schema[SHARED_KEY(Contains)]) {
            uint64_t minCount = 1, maxCount = count;
            Value minV = schema[SHARED_KEY(MinContains)], maxV = schema[SHARED_KEY(MaxContains)];
            if (minV)
                minCount = minV.asUnsigned();
            if (maxV)
                maxCount = maxV.asUnsigned();
            if (count < minCount) [[unlikely]]
                return mkResult(Error::tooFew, array, schema, (minV ? "minContains" : "contains"));
            uint64_t matches = 0;
            for (Array::iterator i(array); i; ++i) {
                if (auto err = check(i.value(), contains, schemaBase); ok(err)) {
                    ++matches;
                    if (matches > maxCount) [[unlikely]]
                        return mkResult(Error::tooMany, array, schema, "maxContains");
                    if (matches >= minCount && maxCount >= count)
                        break;
                }
            }
            if (matches < minCount) [[unlikely]]
                return mkResult(Error::tooFew, array, schema, (minV ? "minContains" : "contains"));
        }

        // "uniqueItems":
        if (schema[SHARED_KEY(UniqueItems)].asBool()) {
            int index = 0;
            for (Array::iterator i(array); i; ++i, ++index) {
                Value v = i.value();
                for (int j = 0; j < index; ++j) {
                    if (isEqual(array[j], v)) [[unlikely]]
                        return mkResult(Error::notUnique, array, schema, "uniqueItems");
                }
            }
        }

        return {};
    }


    /// Checks an object value against a schema.
    Result JSONSchema::Validation::checkDict(Dict dict, Dict schema, Dict schemaBase) {
        auto count = dict.count();
        if (Value minV = schema[SHARED_KEY(MinProperties)])
            if (count < minV.asUnsigned()) [[unlikely]]
                return mkResult(Error::tooShort, dict, schema, "minProperties");
        if (Value maxV = schema[SHARED_KEY(MaxProperties)])
            if (count > maxV.asUnsigned()) [[unlikely]]
                return mkResult(Error::tooLong, dict, schema, "maxProperties");

        // Required properties: Fail if any of these are missing
        if (Array required = schema[SHARED_KEY(Required)].asArray()) {
            for (Array::iterator i(required); i; ++i) {
                if (!dict[i->asString()]) [[unlikely]]
                    return mkResult(Error::missingProperty, dict, schema, "required");
            }
        }

        // "propertyNames": Schema that all property _names_ must match
        if (Value propertyNames = schema[SHARED_KEY(PropertyNames)]) {
            for (Dict::iterator i(dict); i; ++i) {
                slice key = i.keyString();
                RetainedValue keyVal = i.key();
                if (keyVal.type() != kFLString)
                    keyVal = RetainedValue::newString(key);
                if (auto err = check(keyVal, propertyNames, schemaBase); !ok(err)) [[unlikely]]
                    return err;
            }
        }

        Dict properties = schema[SHARED_KEY(Properties)].asDict();
        Value additionalProperties = schema[SHARED_KEY(AdditionalProperties)];
        Dict patternProperties = schema[SHARED_KEY(PatternProperties)].asDict();

        // If "additionalProperties" is present and its value is not "true",
        // use a C++ set to track what properties have been matched:
        optional<unordered_set<slice>> unmatchedProperties;
        if (additionalProperties && !(additionalProperties.type() == kFLBoolean &&
                                      additionalProperties.asBool() == true)) {
            unmatchedProperties.emplace();
            unmatchedProperties->reserve(dict.count());
            for (Dict::iterator i(dict); i; ++i)
                unmatchedProperties->insert(i.keyString());
        }

        // "properties": Specific property names with their own sub-schemas
        for (Dict::iterator i(properties, sSchemaSharedKeys); i; ++i) {
            slice key = i.keyString();
            if (Value val = dict[key]) {
                if (auto err = check(val, i.value(), schemaBase); !ok(err)) [[unlikely]]
                    return err;
                if (unmatchedProperties)
                    unmatchedProperties->erase(key);
            }
        }

        // "patternProperties": Sub-schemas to apply to properties whose names match patterns
        if (patternProperties) {
            for (Dict::iterator i(patternProperties, sSchemaSharedKeys); i; ++i) {
                slice pattern = i.keyString();
                for (Dict::iterator j(dict); j; ++j) {
                    slice dictKey = j.keyString();
                    if (_schemaImpl.stringMatchesPattern(dictKey, pattern)) {
                        if (auto err = check(j.value(), i.value(), schemaBase); !ok(err)) [[unlikely]]
                            return err;
                        if (unmatchedProperties)
                            unmatchedProperties->erase(dictKey);
                    }
                }
            }
        }

        // "additionalProperties": Schema for all properties not covered by the above
        if (additionalProperties && unmatchedProperties) {
            for (slice key : *unmatchedProperties) {
                if (auto err = check(dict[key], additionalProperties, schemaBase); !ok(err)) [[unlikely]]
                    return err;
            }
        }

        return {};
    }


#pragma mark - TYPE CHECKING:


    /// Checks the type of a Value against a schema "type" property (string or array).
    bool JSONSchema::Validation::isType(Value value, Value typeVal) {
        if (slice typeStr = typeVal.asString()) {
            // String dictates the type of the value:
            return isType(value, typeStr);
        } else if (Array types = typeVal.asArray()) {
            // Array means any of the types may match:
            bool matches = false;
            for (Array::iterator i(types); i; ++i) {
                typeStr = i->asString();
                if (!typeStr) [[unlikely]]
                    fail<invalid_schema>("'type' array must contain only strings");
                if (isType(value, typeStr)) {
                    matches = true;
                    break;
                }
            }
            return matches;
        } else {
            fail<invalid_schema>("'type' must be a string or array of strings");
        }
    }


    /// Checks the type of a Value against a schema "type" string.
    bool JSONSchema::Validation::isType(Value value, slice type) {
        FLValueType valType = value.type();
        if (type == "integer") {
            return valType == kFLNumber && isIntegral(value);
        } else {
            auto i = ranges::find(kFLTypeNames, type);
            if (i == ranges::end(kFLTypeNames)) [[unlikely]]
                fail<invalid_schema>("unknown type name \"%.*s\"", FMTSLICE(type));
            return FLValueType(i - ranges::begin(kFLTypeNames)) == valType;
        }
    }


#pragma mark - ERRORS:


    std::string_view JSONSchema::errorString(Error error) noexcept {
        static constexpr string_view kErrorStrings[] = {
            "ok",
            "invalid",
            "typeMismatch",
            "outOfRange",
            "notMultiple",
            "tooShort",
            "tooLong",
            "patternMismatch",
            "missingProperty",
            "unknownProperty",
            "notEnum",
            "tooFew",
            "tooMany",
            "notNot",
            "notUnique",
            "invalidUTF8",
            "unknownSchemaRef"
        };
        assert(unsigned(error) < std::size(kErrorStrings));
        return kErrorStrings[unsigned(error)];
    }


    std::string JSONSchema::Validation::errorString() const noexcept {
        string err(JSONSchema::errorString(error()));
        if (error() == Error::unknownSchemaRef)
            err.append(": \"").append(_unknownSchema).append("\"");
        return err;
    }


    string JSONSchema::Validation::errorPath() const noexcept {
        if (alloc_slice path = recoverPath(_value, errorValue()); !path)
            return "";
        else
            return string("$").append(path);
    }


    std::pair<slice, Value> JSONSchema::Validation::errorSchema() const noexcept {
        if (Dict dict = _result.schema.asDict())
            return {_result.schemaKey, dict[_result.schemaKey]};
        else if (_result.schema)
            return {nullslice, _result.schema};
        else
            return {};
    }


    string JSONSchema::Validation::errorSchemaURI() const noexcept {
        if (!_result.schema)
            return "";
        string uri = _schemaImpl.schemaValueURI(_result.schema);
        if (!uri.ends_with('/'))
            uri.append("/");
        return uri.append(_result.schemaKey);
    }

}
