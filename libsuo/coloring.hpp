#pragma once

#include <cstdint>
#include <iostream>

#ifdef _WIN32
enum class clr : uint16_t {
    grey
    blue,
    green,
    cyan,
    red,
    magenta,
    yellow,
    white,
    on_blue,
    on_red,
    on_magenta,
    on_grey,
    on_green,
    on_cyan,
    on_yellow,
    on_white,
    reset = 0xFF
#elif __unix__
enum class clr : uint8_t {
    grey = 30,
    red = 31,
    green = 32,
    yellow = 33,
    blue = 34,
    magenta = 35,
    cyan = 36,
    white = 37,
    on_grey = 40,
    on_red = 41,
    on_green = 42,
    on_yellow = 43,
    on_blue = 44,
    on_magenta = 45,
    on_cyan = 46,
    on_white = 47,
    reset
#else
#   error unsupported
#endif
};

#ifdef _WIN32
namespace colored_cout_impl {
    uint16_t getColorCode(const clr color);
    uint16_t getConsoleTextAttr();
    void setConsoleTextAttr(const uint16_t attr);
}
#endif

template <typename type>
type& operator<<(type& ostream, const clr color) {
#ifdef _WIN32
    //static const uint16_t initial_attributes = colored_cout_impl::getConsoleTextAttr();
    static const uint16_t initial_attributes = colored_cout_impl::getColorCode(clr::grey);
    static uint16_t background = initial_attributes & 0x00F0;
    static uint16_t foreground = initial_attributes & 0x000F;
#endif
    if (color == clr::reset) {
#ifdef _WIN32
        ostream.flush();
        colored_cout_impl::setConsoleTextAttr(initial_attributes);
        background = initial_attributes & 0x00F0;
        foreground = initial_attributes & 0x000F;
#elif __unix__
        ostream << "\033[m";
#endif
    }
    else {
#ifdef _WIN32
        uint16_t set = 0;
        const uint16_t colorCode = colored_cout_impl::getColorCode(color);
        if (colorCode & 0x00F0) {
            background = colorCode;
            set = background | foreground;
        }
        else if (colorCode & 0x000F) {
            foreground = colorCode;
            set = background | foreground;
        }
        ostream.flush();
        colored_cout_impl::setConsoleTextAttr(set);
#elif __unix__
        ostream << "\033[" << static_cast<uint32_t>(color) << "m";
#endif
    }
    return ostream;
}


class IosFlagSaver {
public:
    explicit IosFlagSaver(std::ostream& _ios) :
        ios(_ios),
        f(_ios.flags()) {
    }
    ~IosFlagSaver() {
        ios.flags(f);
        ios << "\033[m";
    }

    IosFlagSaver(const IosFlagSaver& rhs) = delete;
    IosFlagSaver& operator= (const IosFlagSaver& rhs) = delete;

private:
    std::ostream& ios;
    std::ios::fmtflags f;
};
