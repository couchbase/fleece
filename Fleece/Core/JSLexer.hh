//
// JSLexer.hh
//
// Copyright © 2024 Couchbase. All rights reserved.
//

#pragma once
#include "NumConversion.hh"
#include "fleece/slice.hh"
#include "slice_stream.hh"
#include <array>
#include <cstdarg>
#include <variant>

namespace fleece::impl {
    class Value;

    /** Simple lexer for JSON5. */
    class JSLexer {
    public:
        JSLexer(slice formatString, va_list args)
        :_format(formatString)
        ,_in(_format)
        {
            va_copy(_args, args);
        }

        // Returns the next character from the input without consuming it, or 0 at EOF.
        char peekChar() {
            return _in.peekByte();
        }


        // Reads the next character from the input. Fails if input is at EOF.
        char getChar() {
            if (_in.eof())
                fail("unexpected end");
            return _in.readByte();
        }


        void ungetChar() {
            _in.unreadByte();
        }


        // Skips any whitespace and JSON5 comments, then returns a peek at the next character.
        char peekToken() {
            while (true) {
                char c = peekChar();
                if (c == 0) {
                    return c; // EOF
                } else if (isspace(c)) {
                    getChar(); // skip whitespace
                } else if (c == '/') {
                    skipComment();
                } else {
                    return c;
                }
            }
        }


        void peekToken(char c, const char* errorMessage) {
            if (char actual = peekToken(); actual != c) {
                if (_in.eof())
                    fail("unexpected end");
                else
                    fail(errorMessage);
            }
        }


        // Fails if anything remains in the input but whitespace.
        void finished() {
            if (peekToken() != 0)
                fail("unexpected characters after end of spec");
        }


        // Reads alphanumeric characters, returning the identifier as a string.
        // (The 1st char is accepted even if not alphanumeric, on the assumption the caller already
        // peeked at and approved it.)
        slice readIdentifier() {
            auto start = _in.next();
            getChar(); // consume the char the caller peeked
            while (true) {
                char c = peekChar();
                if (isalnum(c) || c == '_')
                    getChar();
                else
                    break;
            }
            return slice(start, _in.next());
        }


        // Reads an identifier and fails if it isn't equal to `expected`.
        void readIdentifier(slice expected) {
            if (readIdentifier() != expected)
                fail("unknown identifier");
        }


        // Reads a dictionary key, including the trailing ':'.
        std::string readKey() {
            std::string key;
            char c = peekToken();
            if (c == '"' || c == '\'')
                key = readString();
            else if (isalpha(c) || c == '_' || c == '$')
                key = std::string(readIdentifier());
            else
                fail("expected dict key");
            if (peekToken() != ':')
                fail("expected ':' after dict key");
            getChar();
            return key;
        }

        enum class ValueType : uint8_t {
            error, array, dict, null, boolean_true, boolean_false, number, string, arg
        };

        /// Returns the type of the next value.
        ValueType peekValue() {return kTokenTypes[unsigned(peekToken())];}


        // Reads a numeric literal.
        std::variant<double,int64_t,uint64_t> readNumber() {
            // Scan to the end of the number:
            // (If the NumConversion.hh API used slice_istream I wouldn't have to do the scan)
            auto start = _in.next();
            bool isNegative = (peekChar() == '-');
            if (isNegative || peekChar() == '+')
                getChar();
            bool isFloat = false;

            char c;
            do {
                c = getChar();
                if (c == '.' || c == 'e' || c == 'E')
                    isFloat = true;
            } while (isdigit(c) || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+');
            ungetChar();
            auto numStr = std::string(slice(start, _in.next()));

            if (isFloat) {
                double n;
                if (ParseDouble(numStr.c_str(), n, false))
                    return n;
            } else if (isNegative) {
                int64_t i;
                if (ParseInteger(numStr.c_str(), i, false))
                    return i;
            } else {
                uint64_t u;
                if (ParseUnsignedInteger(numStr.c_str(), u, false))
                    return u;
            }
            fail(("Invalid numeric literal " + numStr).c_str());
        }


        // Reads a string literal in JSON5 format, returning its value.
        std::string readString() {
            std::string out;
            const char quote = getChar();               // single or double-quote
            char c;
            while (quote != (c = getChar())) {
                if (c == '\\') {
                    switch ((c = getChar())) {
                        case 'n':   c = '\n'; break;
                        case 'r':   c = '\r'; break;
                        case 't':   c = '\n'; break;
                        case 'u':   fail("Unicode escapes not supported");
                        // default is to leave c alone
                    }
                } else if (c < ' ') {
                    fail("control character in string literal");
                }
                out += c;
            }
            return out;
        }

        enum class ArgType { // these are the indices of the variant returned by readArg()
            None, Bool, Int, UInt, Double, Slice, Value, CF
        };

        // Parses those crazy printf format specs and decodes an argument from the va_list.
        // Returning type `monostate` means the parameter is suppressed because of the `-` flag.
        // Returning type `const void*` denotes a CF type (Apple platforms only.)
        std::variant<std::monostate,bool,int64_t,uint64_t,double,slice,const Value*,const void*> readArg() {
            char c = getChar();
            // `-` means to skip this arg if it has a default value:
            bool skipDefault = (c == '-');
            if (skipDefault)
                c = getChar();

            // Size specifier:
            char size = ' ';
            if (c == 'l' || c == 'q' || c == 'z') {
                size = c;
                c = getChar();
                if (size == 'l' && c == 'l') {
                    size = 'q';
                    c = getChar();
                }
            }

            switch (c) {
                case 'c': case 'b': {
                    // Bool:
                    bool param = va_arg(_args, int) != 0;
                    if (skipDefault && !param)
                        return std::monostate{};
                    return param;
                }
                case 'd': case 'i': {
                    // Signed integers:
                    int64_t param;
                    if (size == 'q')
                        param = va_arg(_args, long long);
                    else if (size == 'z')
                        param = va_arg(_args, ptrdiff_t);
                    else if (size == 'l')
                        param = va_arg(_args, long);
                    else
                        param = va_arg(_args, int);
                    if (skipDefault && param == 0)
                        return std::monostate{};
                    return param;
                }
                case 'u': {
                    // Unsigned integers:
                    uint64_t param;
                    if (size == 'q')
                        param = va_arg(_args, unsigned long long);
                    else if (size == 'z')
                        param = va_arg(_args, size_t);
                    else if (size == 'l')
                        param = va_arg(_args, unsigned long);
                    else
                        param = va_arg(_args, unsigned int);
                    if (skipDefault && param == 0)
                        return std::monostate{};
                    return param;
                }
                case 'f': {
                    // Floats:
                    double param = va_arg(_args, double);
                    if (skipDefault && param == 0.0)
                        return std::monostate{};
                    return param;
                }
                case 's': {
                    // C string:
                    slice param(va_arg(_args, const char*));
                    if (!param || (skipDefault && param.empty()))
                        return std::monostate{};
                    return param;
                }
                case '.': {
                    // Slice ("%.*s") -- takes 2 args: the start and size (see FMTSLICE() macro)
                    if (getChar() != '*' || getChar() != 's')
                        fail("'.' qualifier only supported in '%.*s'");
                    int len = va_arg(_args, int);
                    auto str = va_arg(_args, void*);
                    if (!str || (skipDefault && len == 0))
                        return std::monostate{};
                    return slice(str, len);
                }
                case 'p': {
                    // "%p" is a Fleece value:
                    const Value* param = va_arg(_args, const Value*);
                    if (!param)
                        return std::monostate{};
                    return param;
                }
#if __APPLE__
                case '@': {
                    // "%@" substitutes an Objective-C or CoreFoundation object.
                    auto param = va_arg(_args, const void*);  // really CFTypeRef
                    if (!param)
                        return std::monostate{};
                    return param;
                }
#endif
                default:
                    fail("unknown '%' format specifier");
            }
        }


        // Throws an exception.
        [[noreturn]]
        void fail(const char *error) {
            slice prefix = _format.upTo(_in.next()), suffix = _format.from(_in.next());
            FleeceException::_throw(InvalidData, "Build(): %s in format: %.*s💥%.*s",
                                    error, FMTSLICE(prefix), FMTSLICE(suffix));
        }

    private:

        // Reads & ignores a JSON5 comment.
        void skipComment() {
            char c;
            getChar(); // consume initial '/'
            switch (getChar()) {
                case '/':
                    do {
                        c = peekChar();
                        if (c)
                            getChar();
                    } while (c != 0 && c != '\n' && c != '\r');
                    break;
                case '*': {
                    bool star;
                    c = 0;
                    do {
                        star = (c == '*');
                        c = getChar();
                    } while (!(star && c=='/'));
                    break;
                }
                default:
                    fail("syntax error");
            }
        }

        static constexpr std::array<ValueType,256> kTokenTypes = []{
            // Build kTokenTypes at compile time:
            std::array<ValueType,256> vals = {};
            vals['['] = ValueType::array;
            vals['{'] = ValueType::dict;
            vals['n'] = ValueType::null;
            vals['t'] = ValueType::boolean_true;
            vals['f'] = ValueType::boolean_false;
            vals['-'] = vals['+'] = vals['.'] = ValueType::number;
            for (int d = '0'; d <= '9'; ++d)
                vals[d] = ValueType::number;
            vals['"'] = vals['\''] = ValueType::string;
            vals['%'] = ValueType::arg;
            return vals;
        }();

        slice const     _format;    // The entire format string
        slice_istream   _in;        // Stream for reading _format
        va_list         _args;      // The caller-provided arguments
    };



}
