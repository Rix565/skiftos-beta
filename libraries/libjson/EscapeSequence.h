#pragma once

#include <libutils/Strings.h>

#include <libio/MemoryReader.h>
#include <libio/NumberScanner.h>
#include <libio/Scanner.h>

namespace Json
{

inline const char *escape_sequence(IO::Scanner &scan)
{
    scan.skip('\\');

    if (scan.ended())
    {
        return "\\";
    }

    char chr = scan.current();
    scan.foreward();

    switch (chr)
    {
    case '"':
        return "\"";

    case '\\':
        return "\\";

    case '/':
        return "/";

    case 'b':
        return "\b";

    case 'f':
        return "\f";

    case 'n':
        return "\n";

    case 'r':
        return "\r";

    case 't':
        return "\t";

    case 'u':
    {
        auto read_4hex = [&]() {
            char buffer[5];

            for (size_t i = 0; i < 4 && scan.current_is(Strings::LOWERCASE_XDIGITS); i++)
            {
                buffer[i] = scan.current();
                scan.foreward();
            }

            IO::MemoryReader buffer_reader{buffer, 5};
            IO::Scanner buffer_scanner{buffer_reader};
            auto value_or_result = IO::NumberScanner::hexadecimal().scan_uint(buffer_scanner);

            return *value_or_result;
        };

        uint32_t first_surrogate = read_4hex();

        if (first_surrogate >= 0xDC00 && first_surrogate <= 0xDFFF)
        {
            // Invalid first half of the surrogate pair.
            return "�";
        }

        if (!(first_surrogate >= 0xD800 && first_surrogate <= 0xDBFF))
        {
            // Not an UTF16 surrogate pair.
            static uint8_t utf8[5] = {};
            codepoint_to_utf8((Codepoint)first_surrogate, utf8);
            return (char *)utf8;
        }

        if (!scan.skip_word("\\u"))
        {
            return "�";
        }

        uint32_t second_surrogate = read_4hex();

        if ((second_surrogate < 0xDC00) || (second_surrogate > 0xDFFF))
        {
            // Invalid second half of the surrogate pair.
            return "�";
        }

        Codepoint codepoint = 0x10000 + (((first_surrogate & 0x3FF) << 10) | (second_surrogate & 0x3FF));

        static uint8_t utf8[5] = {};
        codepoint_to_utf8((Codepoint)codepoint, utf8);
        return (char *)utf8;
    }

    default:
    {
        static char buffer[3] = "\\x";
        buffer[1] = chr;

        return buffer;
    }
    }
}

} // namespace Json
