#ifdef _WIN32
#    error "This implementation is not meant to be used on Windows
#endif

#include "Backtrace.hh"
#include <cxxabi.h>  // abi::__cxa_demangle()
#include <cstring>

namespace fleece {
    using namespace std;
    using namespace signal_safe;

    Backtrace::frameInfo getFrame(void* addr, bool isTopFrame);

    static constexpr struct {
        const char *old, *nuu;
    } kAbbreviations[] = {
            {"(anonymous namespace)", "(anon)"},
            {"std::__1::", "std::"},
            {"std::__cxx11::", "std::"},
            {"std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string"},
    };

    // If any of these strings occur in a backtrace, suppress further frames.
    static constexpr const char* kTerminalFunctions[] = {
            "_C_A_T_C_H____T_E_S_T_",
            "Catch::TestInvokerAsFunction::invoke() const",
            "litecore::actor::Scheduler::task(unsigned)",
            "litecore::actor::GCDMailbox::safelyCall",
    };

    static void replace(std::string& str, string_view oldStr, string_view newStr) {
        string::size_type pos = 0;
        while ( string::npos != (pos = str.find(oldStr, pos)) ) {
            str.replace(pos, oldStr.size(), newStr);
            pos += newStr.size();
        }
    }

    char* unmangle(const char* function) {
        int    status;
        size_t unmangledLen;
        char*  unmangled = abi::__cxa_demangle(function, nullptr, &unmangledLen, &status);
        if ( unmangled && status == 0 ) return unmangled;
        free(unmangled);
        return (char*)function;
    }

    Backtrace::frameInfo Backtrace::getFrame(unsigned i) const {
        precondition(i < _addrs.size());
        return getFrame(_addrs[i], i == 0);
    }

    bool Backtrace::writeTo(ostream& out) const {
        for ( unsigned i = 0; i < unsigned(_addrs.size()); ++i ) {
            if ( i > 0 ) out << '\n';
            out << '\t';
            char*       cstr  = nullptr;
            auto        frame = getFrame(i);
            int         len;
            bool        stop = false;
            const char* lib  = frame.library ? frame.library : "?";
            string      name = frame.function ? Unmangle(frame.function) : string();
            if ( !name.empty() ) {
                for ( auto fn : kTerminalFunctions )
                    if ( name.find(fn) != string::npos ) stop = true;
                for ( auto& abbrev : kAbbreviations ) replace(name, abbrev.old, abbrev.nuu);
            }
#ifndef __APPLE__
            if ( frame.file ) {
                const char* fileBase = strrchr(frame.file, '/');
                fileBase             = fileBase ? fileBase + 1 : frame.file;
                len = asprintf(&cstr, "%2u  %-25s %s + %zu  (%s:%d)", i, lib, name.empty() ? "?" : name.c_str(),
                               frame.offset, fileBase, frame.line);
            } else
#endif
            {
                len = asprintf(&cstr, "%2u  %-25s %s + %zu  [0x%zx]", i, lib, name.empty() ? "?" : name.c_str(),
                               frame.offset, frame.imageOffset);
            }

            if ( len < 0 ) return false;

            if ( len > 0 ) {
                out.write(cstr, size_t(len));
                free(cstr);
            }

            if ( stop ) {
                out << "\n\t ... (" << (_addrs.size() - i - 1) << " more suppressed) ...";
                break;
            }
        }
        return true;
    }

    void Backtrace::writeTo(void** addresses, int size, int fd) {
        for ( unsigned i = 0; i < unsigned(size); ++i ) {
            if ( i > 0 ) write_to_and_stderr(fd, "\n", 1);
            write_to_and_stderr(fd, "\t", 1);
            auto        frame = getFrame(addresses[i], i == 0);
            const char* lib   = frame.library ? frame.library : "?";
            const char* name  = frame.function;

            if ( i < 10 ) write_to_and_stderr(fd, " ", 1);
            write_ulong(i, fd);
            const size_t lib_len = strlen(lib);
            write_to_and_stderr(fd, " ", 1);
            write_to_and_stderr(fd, lib, lib_len);
            for ( size_t j = lib_len; j < 26; ++j ) write_to_and_stderr(fd, " ", 1);
            if ( name ) {
                write_to_and_stderr(fd, name, strlen(name));
            } else {
                write_to_and_stderr(fd, "?", 1);
            }

            write_to_and_stderr(fd, " + ", 3);
            write_ulong(frame.offset, fd);

#ifndef __APPLE__
            if ( frame.file ) {
                const char* fileBase = strrchr(frame.file, '/');
                fileBase             = fileBase ? fileBase + 1 : frame.file;
                write_to_and_stderr(fd, " (", 2);
                write_to_and_stderr(fd, fileBase);
                write_to_and_stderr(fd, ":", 1);
                write_long(frame.line, fd);
                write_to_and_stderr(fd, ")", 1);
            } else
#endif
            {
                write_to_and_stderr(fd, " [", 2);
                write_hex_offset(frame.imageOffset, fd);
                write_to_and_stderr(fd, "]", 1);
            }
        }
    }
}  // namespace fleece

namespace signal_safe {
    void write_long(long long value, int fd) {
        // NOTE: This function assumes buffer is at least 21 bytes long
        if ( value == 0 ) {
            write_to_and_stderr(fd, "0", 1);
        } else {
            if ( value < 0 ) write_to_and_stderr(fd, "-", 1);
            char  temp[20];
            char* p = temp;
            while ( value > 0 ) {
                *p++ = static_cast<char>(value % 10 + '0');
                value /= 10;
            }

            while ( p != temp ) { write_to_and_stderr(fd, --p, 1); }
        }
    }

    void write_ulong(unsigned long long value, int fd) {
        // NOTE: This function assumes buffer is at least 21 bytes long
        if ( value == 0 ) {
            write_to_and_stderr(fd, "0", 1);
        } else {
            char  temp[20];
            char* p = temp;
            while ( value > 0 ) {
                *p++ = static_cast<char>(value % 10 + '0');
                value /= 10;
            }

            while ( p != temp ) { write_to_and_stderr(fd, --p, 1); }
        }
    }

    void write_hex_offset(size_t value, int fd) {
        static const char* hexDigits  = "0123456789abcdef";
        const int          num_digits = sizeof(size_t) * 2;
        write_to_and_stderr(fd, "0x", 2);
        if ( value == 0 ) {
            write_to_and_stderr(fd, "0", 1);
        } else {
            char  temp[num_digits];
            char* p = temp;
            while ( value > 0 ) {
                *p++ = hexDigits[value & 0xF];
                value >>= 4;
            }

            while ( p != temp ) { write_to_and_stderr(fd, --p, 1); }
        }
    }

    void write_to_and_stderr(int fd, const char* str, size_t n) {
        if ( fd != -1 ) write(fd, str, n ? n : strlen(str));
        write(STDERR_FILENO, str, n ? n : strlen(str));
    }
}  // namespace signal_safe
