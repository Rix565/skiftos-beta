#pragma once

#include <abi/Filesystem.h>

#include <libutils/String.h>
#include <libutils/Vector.h>

#include <libio/MemoryReader.h>
#include <libio/MemoryWriter.h>
#include <libio/Scanner.h>
#include <libio/Write.h>

namespace System
{

class Path
{
private:
    bool _absolute = false;
    Vector<String> _elements{};

public:
    static constexpr int PARENT_SHORTHAND = 1; // .... -> ../../..

    bool absolute() const { return _absolute; }

    bool relative() const { return !_absolute; }

    size_t length() const { return _elements.count(); }

    static Path parse(const String &string, int flags = 0)
    {
        IO::MemoryReader reader{string.slice()};
        IO::Scanner scan{reader};

        return parse(scan, flags);
    }

    static Path parse(const char *path, int flags = 0)
    {
        IO::MemoryReader reader{path};
        IO::Scanner scan{reader};

        return parse(scan, flags);
    }

    static Path parse(IO::Scanner &scan, int flags)
    {
        bool absolute = false;

        if (scan.skip(PATH_SEPARATOR))
        {
            absolute = true;
        }

        auto parse_element = [](auto &scan) {
            IO::MemoryWriter memory{};

            while (!scan.skip(PATH_SEPARATOR) && scan.do_continue())
            {
                memory.write(scan.current());
                scan.forward();
            }

            return memory.string();
        };

        auto parse_shorthand = [](auto &scan) {
            Vector<String> elements{};

            scan.skip_word("..");
            elements.push("..");

            while (scan.skip('.'))
            {
                elements.push("..");
            }

            scan.skip('/');

            return move(elements);
        };

        Vector<String> elements{};

        while (scan.do_continue())
        {
            if ((flags & PARENT_SHORTHAND) && scan.current_is_word(".."))
            {
                elements.push_back_many(parse_shorthand(scan));
            }
            else
            {
                String el = parse_element(scan);

                if (el.length() > 0)
                {
                    elements.push_back(move(el));
                }
            }
        }

        return {absolute, move(elements)};
    }

    static Path join(String left, String right)
    {
        return join(parse(left), parse(right));
    }

    static Path join(Path &&left, String right)
    {
        return join(left, parse(right));
    }

    static Path join(String left, Path &&right)
    {
        return join(parse(left), right);
    }

    static Path join(Path &left, String right)
    {
        return join(left, parse(right));
    }

    static Path join(String left, Path &right)
    {
        return join(parse(left), right);
    }

    static Path join(Path &&left, Path &&right)
    {
        return join(left, right);
    }

    static Path join(Path &left, Path &&right)
    {
        return join(left, right);
    }

    static Path join(Path &&left, Path &right)
    {
        return join(left, right);
    }

    static Path join(Path &left, Path &right)
    {
        Vector<String> combined_elements{};

        combined_elements.push_back_many(left._elements);
        combined_elements.push_back_many(right._elements);

        return {left.absolute(), move(combined_elements)};
    }

    Path()
    {
    }

    Path(const Path &other) : _absolute{other.absolute()}, _elements{other._elements}
    {
    }

    Path(bool absolute, Vector<String> &&elements) : _absolute(absolute), _elements(elements)
    {
    }

    Path(Path &&other)
    {
        swap(_absolute, other._absolute);
        swap(_elements, other._elements);
    }

    Path &operator=(const Path &other)
    {
        if (this != &other)
        {
            _absolute = other.absolute();
            _elements = other._elements;
        }

        return *this;
    }

    Path &operator=(Path &&other)
    {
        if (this != &other)
        {
            swap(_absolute, other._absolute);
            swap(_elements, other._elements);
        }

        return *this;
    }

    String operator[](size_t index) const
    {
        return _elements[index];
    }

    bool operator!=(const Path &other) const
    {
        return !(*this == other);
    }

    bool operator==(const Path &other) const
    {
        if (this == &other)
        {
            return true;
        }

        if (_absolute != other._absolute)
        {
            return false;
        }

        return _elements == other._elements;
    }

    Path normalized()
    {
        Vector<String> stack{};

        _elements.foreach ([&](auto &element) {
            if (element == ".." && stack.count() > 0)
            {
                stack.pop_back();
            }
            else if (_absolute && element == "..")
            {
                if (stack.count() > 0)
                {
                    stack.pop_back();
                }
            }
            else if (element != ".")
            {
                stack.push_back(element);
            }

            return Iteration::CONTINUE;
        });

        return {_absolute, move(stack)};
    }

    String basename() const
    {
        if (length() > 0)
        {
            return _elements.peek_back();
        }
        else
        {
            if (_absolute)
            {
                return "/";
            }
            else
            {
                return "";
            }
        }
    }

    String basename_without_extension() const
    {
        auto basename_with_extrension = basename();

        IO::MemoryWriter memory{basename_with_extrension.length()};

        IO::MemoryReader memory_reader{basename_with_extrension};
        IO::Scanner scan{memory_reader};

        // It's not a file extention it's an hidden file.
        if (scan.current_is("."))
        {
            memory.write(scan.current());
            scan.forward();
        }

        while (!scan.current_is(".") && scan.do_continue())
        {
            memory.write(scan.current());
            scan.forward();
        }

        return memory.string();
    }

    String dirname() const
    {
        IO::MemoryWriter memory{};

        if (_absolute)
        {
            memory.write(PATH_SEPARATOR);
        }
        else if (_elements.count() <= 1)
        {
            memory.write('.');
        }

        if (_elements.count() >= 2)
        {
            for (size_t i = 0; i < _elements.count() - 1; i++)
            {
                IO::write_string(memory, _elements[i]);

                if (i != _elements.count() - 2)
                {
                    memory.write(PATH_SEPARATOR);
                }
            }
        }

        return memory.string();
    }

    Path dirpath() const
    {
        Vector<String> stack{};

        if (length() > 0)
        {
            for (size_t i = 0; i < length() - 1; i++)
            {
                stack.push_back(_elements[i]);
            }
        }

        return {_absolute, move(stack)};
    }

    String extension() const
    {
        IO::MemoryWriter memory{3};
        IO::MemoryReader filename{basename().slice()};

        IO::Scanner scan{filename};

        // It's not a file extention it's an hidden file.
        if (scan.current_is("."))
        {
            scan.forward();
        }

        while (!scan.current_is(".") && scan.do_continue())
        {
            scan.forward();
        }

        while (scan.do_continue())
        {
            IO::write_char(memory, scan.current());
            scan.forward();
        }

        return memory.string();
    }

    Path parent(size_t index) const
    {
        Vector<String> stack{};

        if (index <= length())
        {
            for (size_t i = 0; i <= index; i++)
            {
                stack.push_back(_elements[i]);
            }
        }

        return {_absolute, move(stack)};
    }

    String string() const
    {
        IO::MemoryWriter memory{};

        if (_absolute)
        {
            memory.write(PATH_SEPARATOR);
        }

        for (size_t i = 0; i < _elements.count(); i++)
        {
            IO::write_string(memory, _elements[i]);

            if (i != _elements.count() - 1)
            {
                memory.write(PATH_SEPARATOR);
            }
        }

        return memory.string();
    }
};

} // namespace System
