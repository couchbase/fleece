//
//  fleece_tool.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/27/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "JSONConverter.hh"
#include <stdio.h>
#include <unistd.h>
#include <sstream>

using namespace fleece;
using namespace std;

static void usage(void) {
    fprintf(stderr, "usage: fleece --encode [JSON file]\n");
    fprintf(stderr, "       fleece --decode [Fleece file]\n");
    fprintf(stderr, "       fleece --dump [Fleece file]\n");
    fprintf(stderr, "  Reads stdin unless a file is given; always writes to stdout.\n");
}

static alloc_slice readInput(FILE *in) {
    stringstream out;
    char buf[1024];
    while (true) {
        size_t n = ::fread(buf, 1, sizeof(buf), in);
        if (n == 0)
            break;
        out.write(buf, n);
    }
    if (ferror(in))
        throw "Error reading input";
    return out.str();
}

int main(int argc, const char * argv[]) {
    try {
        bool encode = false, decode = false, dump = false;

        int i;
        for (i = 1; i < argc; ++i) {
            const char *arg = argv[i];
            if (arg[0] != '-') {
                break;
            } else if (strcmp(arg, "-") == 0) {
                i++;
                break;
            } else if (strcmp(arg, "--encode") == 0) {
                encode = true;
            } else if (strcmp(arg, "--decode") == 0) {
                decode = true;
            } else if (strcmp(arg, "--dump") == 0) {
                dump = true;
            } else if (strcmp(arg, "--help") == 0) {
                usage();
                return 0;
            } else {
                fprintf(stderr, "Unknown option '%s'\n", arg);
                usage();
                return 1;
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

        if (encode && isatty(STDOUT_FILENO))
            throw "Let's not spew binary Fleece data to a terminal! Please redirect stdout.";

        auto input = readInput(in);

        if (encode) {
            Writer writer;
            Encoder e(writer);
            JSONConverter reader(e);
            if (!reader.convertJSON(input))
                throw "Couldn't parse input as JSON";
            e.end();
            auto output = writer.extractOutput();
            fwrite(output.buf, 1, output.size, stdout);
        } else if (decode) {
            auto root = Value::fromData(input);
            if (!root)
                throw "Couldn't parse input as Fleece";
            auto json = root->toJSON();
            cout.write((char*)json.buf, json.size);
        } else if (dump) {
            if (!Value::dump(input, cout))
                throw "Couldn't parse input as Fleece";
        }

        return 0;
        
    } catch (const char *err) {
        fprintf(stderr, "%s\n", err);
        return 1;
    } catch (...) {
        fprintf(stderr, "Uncaught exception!\n");
        return 1;
    }
}
