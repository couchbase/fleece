//
// JSON5.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "JSON5.hh"
#include <sstream>

using namespace std;


namespace fleece {

    static inline bool isnewline(int c) {return (c == '\n' || c == '\r');}


    class json5converter {
    public:
        json5converter(istream &in, ostream &out)
        :_in(in)
        ,_out(out)
        { }

        // Parses a complete JSON5 string.
        void parse() {
            parseValue();
            if (peekToken() != 0)
                fail("Unexpected characters after end of value");
        }

    private:

        // Parses a JSON5 value, writing JSON to the output.
        void parseValue() {
            switch(peekToken()) {
                case 'n':
                    parseConstant("null");
                    break;
                case 't':
                    parseConstant("true");
                    break;
                case 'f':
                    parseConstant("false");
                    break;
                case '-':
                case '+':
                case '.':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    parseNumber();
                    break;
                case '"':
                case '\'':
                    parseString();
                    break;
                case '[':
                    parseSequence(false);
                    break;
                case '{':
                    parseSequence(true);
                    break;
                default:
                    fail("invalid start of JSON5 value");
            }
        }

        // Reads (and writes) a specific sequence of characters, failing if it doesn't match
        // or if the next character is alphanumeric.
        void parseConstant(const char *ident) {
            auto cp = ident;
            while (*cp && get() == *cp)
                ++cp;
            char c = peek();
            if (*cp || isalnum(c) || c == '$' || c == '_')
                fail("unknown identifier");
            _out << ident;
        }

        // Reads a number, writing JSON to the output.
        void parseNumber() {
            // TODO: Handle hex numbers
            // TODO: Handle Infinity and NaN
            char c = get();
            if (c == '.')
                _out << "0.";
            else if (c != '+')
                _out << c;

            while (true) {
                // Remember, we don't have to validate that this is a correct JSON number;
                // we just have to pass valid numbers through.
                c = peek();
                if (isdigit(c) || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+')
                    _out << get();
                else
                    break;
            }
        }
        
        // Reads a string, writing JSON to the output.
        void parseString() {
            _out << '"';
            const char quote = get();
            char c;
            while (quote != (c = get())) {
                if (c == '"') {
                    _out << "\\\"";                 // Escape double-quote in single-quoted string
                } else if (c == '\\') {
                    char esc = get();
                    if (!isnewline(esc)) {          // ignore backslash + newline
                        if (esc != '\'')
                            _out << '\\';           // Don't write a single-quote as an escape
                        _out << esc;
                    }
                    // We don't need to detect Unicode escapes; just pass them through.
                } else {
                    _out << c;
                }
            }
            _out << '"';
        }

        // Reads an array or object, writing JSON to the output.
        void parseSequence(bool isObject) {
            _out << get();  // open bracket/brace
            const char closeBracket = (isObject ? '}' : ']');
            bool first = true;
            char c;
            while (closeBracket != (c = peekToken())) {
                if (first)
                    first = false;
                else
                    _out << ",";

                if (isObject) {
                    // Key:
                    if (c == '"' || c == '\'') {
                        parseString();
                    } else if (isalpha(c) || c == '_' || c == '$') {
                        _out << '"' << get();
                        while (true) {
                            c = peek();
                            if (isalnum(c) || c == '_')
                                _out << get();
                            else
                                break;
                        }
                        _out << '"';
                    } else {
                        fail("Invalid key");
                    }
                    if (peekToken() != ':')
                        fail("Expected ':' after key");
                    _out << get();
                }

                // Value, or array item:
                parseValue();

                if (peekToken() == ',')
                    get();
                else if (peekToken() != closeBracket)
                    fail("unexpected token after array/object item");
            }
            _out << get(); // copy close bracket/brace
        }

        // Returns the next non-whitespace, non-comment character from the input.
        // Consumes whitespace and comments, but not the character it returns.
        char peekToken() {
            while (true) {
                char c = peek();
                if (c == 0) {
                    return c; // EOF
                } else if (isspace(c)) {
                    get(); // skip whitespace
                } else if (c == '/') {
                    skipComment();
                } else {
                    return c;
                }
            }
        }

        // Reads a comment from the input. Writes nothing to the output.
        void skipComment() {
            char c;
            get(); // consume initial '/'
            switch (get()) {
                case '/':
                    do {
                        c = peek();
                        if (c)
                            get();
                    } while (c != 0 && !isnewline(c));
                    break;
                case '*': {
                    bool star;
                    c = 0;
                    do {
                        star = (c == '*');
                        c = get();
                    } while (!(star && c=='/'));
                    break;
                }
                default:
                    fail("Syntax error after '/'");
            }
        }

        // Returns the next character from the input without consuming it, or 0 at EOF.
        char peek() {
            int c = _in.peek();
            return (c < 0) ? 0 : (char)c;
        }

        // Reads the next character from the input. Fails if input is at EOF.
        char get() {
            int c = _in.get();
            if (_in.eof())
                fail("Unexpected end of JSON5");
            ++_pos;
            return (char)c;
        }

        // Throws an exception.
        [[noreturn]] void fail(const char *error) {
            stringstream message;
            message << error << " (at :" << _pos << ")";
            throw json5_error(message.str(), _pos);
        }

        istream &_in;
        ostream &_out;
        string::size_type _pos {0};
    };


    json5_error::json5_error(const std::string &what, std::string::size_type inputPos_)
    :std::runtime_error(what)
    ,inputPos(inputPos_)
    { }


    void ConvertJSON5(istream &in, ostream &out) {
        json5converter(in, out).parse();
    }

    std::string ConvertJSON5(const std::string &json5) {
        stringstream in(json5), out;
        ConvertJSON5(in, out);
        return out.str();
    }

}
