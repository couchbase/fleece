//
//  JSON5.cc
//  Fleece
//
//  Created by Jens Alfke on 12/13/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "JSON5.hh"
#include <sstream>
#include <stdexcept>

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
        void fail(const char *error) {
            stringstream message;
            message << error << " (at :" << _pos << ")";
            throw runtime_error(message.str());
        }

        istream &_in;
        ostream &_out;
        size_t _pos {0};
    };


    void ConvertJSON5(istream &in, ostream &out) {
        json5converter(in, out).parse();
    }

    std::string ConvertJSON5(const std::string &json5) {
        stringstream in(json5), out;
        ConvertJSON5(in, out);
        return out.str();
    }

}
