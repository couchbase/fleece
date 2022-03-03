//
// fleece_tool.cc
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/Fleece.hh"
#include "fleece/FLExpert.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#ifndef _MSC_VER
#include <unistd.h>
#define _isatty isatty
#else
#include <io.h>
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#endif

using namespace fleece;
using namespace std;


static void usage(void) {
    fprintf(stderr, "usage: fleece [--hex] encode [JSON file]\n");
    fprintf(stderr, "       fleece [--hex] decode [Fleece file]\n");
    fprintf(stderr, "       fleece dump [Fleece file]\n");
    fprintf(stderr, "  Reads stdin unless a file is given; always writes to stdout.\n");
}


#if defined(__ANDROID__) || defined(__GLIBC__) || defined(_MSC_VER) || defined(__EMSCRIPTEN__)
// digittoint is a BSD function, not available on Android, Linux, etc.
static int digittoint(char ch) {
    int d = ch - '0';
    if ((unsigned) d < 10) {
        return d;
    }
    d = ch - 'a';
    if ((unsigned) d < 6) {
        return d + 10;
    }
    d = ch - 'A';
    if ((unsigned) d < 6) {
        return d + 10;
    }
    return 0;
}
#endif // defined(__ANDROID__) || defined(__GLIBC__)


static size_t decodeHex(uint8_t buf[], size_t n) {
    if (n & 1)
        return 0;
    size_t size = 0;
    const uint8_t *cp = buf, *end = buf + n;
    unsigned byte = 0;
    bool partial = false;
    while (cp < end) {
        uint8_t c = *cp++;
        if (isspace(c))
            continue;
        if (!isxdigit(c))
            return 0;
        auto nybble = digittoint(c);
        partial = !partial;
        if (partial)
            byte = nybble << 4;
        else
            buf[size++] = uint8_t(byte | nybble);
    }
    if (partial)
        return 0;
    return size;
}


static alloc_slice readInput(FILE *in, bool asHex) {
    stringstream out;
    uint8_t buf[1024];
    while (true) {
        size_t n = ::fread(buf, 1, sizeof(buf), in);
        if (n == 0)
            break;
        if (asHex) {
            n = decodeHex(buf, n);
            if (n == 0)
                throw "Invalid hex input";
        }
        out.write((char*)buf, n);
    }
    if (ferror(in))
        throw "Error reading input";
    return alloc_slice(out.str());
}


static void writeOutput(slice output, bool asHex =false) {
    if (asHex) {
        string hex = output.hexString();
        fputs(hex.c_str(), stdout);
    } else {
        fwrite(output.buf, 1, output.size, stdout);
    }
}


int main(int argc, const char * argv[]) {
    try {
        bool encode = false, decode = false, dump = false, hex = false;

        int i;
        for (i = 1; i < argc; ++i) {
            const char *arg = argv[i];
            if (arg[0] == '-') {
                if (strcmp(arg, "-") == 0) {
                    i++;
                    break;
                } else if (strcmp(arg, "--encode") == 0) {
                    encode = true;
                } else if (strcmp(arg, "--decode") == 0) {
                    decode = true;
                } else if (strcmp(arg, "--dump") == 0) {
                    dump = true;
                } else if (strcmp(arg, "--hex") == 0) {
                    hex = true;
                } else if (strcmp(arg, "--help") == 0) {
                    usage();
                    return 0;
                } else {
                    fprintf(stderr, "Unknown option '%s'\n", arg);
                    usage();
                    return 1;
                }
            } else if (encode+decode+dump == 0) {
                // Also allow mode without '--' prefix, if none was chosen yet:
                if (strcmp(arg, "encode") == 0) {
                    encode = true;
                } else if (strcmp(arg, "decode") == 0) {
                    decode = true;
                } else if (strcmp(arg, "dump") == 0) {
                    dump = true;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        if (encode + decode + dump != 1) {
            fprintf(stderr, "Choose one of --encode, --decode, or --dump\n");
            usage();
            return 1;
        }

        FILE *in = stdin;
        if (i < argc) {
            in = fopen(argv[i], "r");
            if (!in) {
                fprintf(stderr, "Couldn't open file %s\n", argv[i]);
                return 1;
            }
            ++i;
        }

        if (i < argc) {
            fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
            usage();
            return 1;
        }

        if (encode && !hex && _isatty(STDOUT_FILENO))
            throw "Let's not spew binary Fleece data to a terminal! Please redirect stdout.";

        auto input = readInput(in, (decode && hex));

        if (encode) {
            Doc doc = Doc::fromJSON(input);
            if (!doc)
                throw "Invalid JSON input";
            slice output = doc.data();
            writeOutput(output, hex);
        } else if (decode) {
            Doc doc(input);
            if (!doc)
                throw "Couldn't parse input as Fleece";
            auto json = doc.root().toJSON();
            writeOutput(json);
            fprintf(stdout, "\n");
        } else if (dump) {
            alloc_slice output = FLData_Dump(input);
            if (!output)
                throw "Couldn't parse input as Fleece";
            writeOutput(output);
        }

        return 0;

    } catch (const char *err) {
        fprintf(stderr, "%s\n", err);
        return 1;
    } catch (const std::exception &x) {
        fprintf(stderr, "%s\n", x.what());
    } catch (...) {
        fprintf(stderr, "Uncaught exception!\n");
        return 1;
    }
}
