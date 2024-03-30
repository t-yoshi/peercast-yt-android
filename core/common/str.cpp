#include "str.h"
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>
#include <cctype>
#include <stdexcept>

#include "common.h" // FormatException

namespace str
{

using namespace std;

std::string hexdump(const std::string& in)
{
    std::string res;

    for (size_t i = 0; i < in.size(); i++) {
        if (i != 0)
            res += ' ';
        char buf[3];
        snprintf(buf, 3, "%02hhX", in[i]);
        res += buf;
    }
    return res;
}

static std::string inspect(char c, bool utf8)
{
    int d = static_cast<unsigned char>(c);

    if (utf8 && d >= 0x80)
        return string() + (char) d;

    if (isprint(d)) {
        switch (d) {
        case '\'': return "\\\'";
        case '\"': return "\\\"";
        case '\?': return "\\?";
        case '\\': return "\\\\";
        default:
            return string() + (char) d;
        }
    }

    switch (d) {
    case '\a': return "\\a";
    case '\b': return "\\b";
    case '\f': return "\\f";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    default:
        return string("\\x")
            + "0123456789abcdef"[d>>4]
            + "0123456789abcdef"[d&0xf];
    }
}

std::string inspect(const std::string& str)
{
    bool utf8 = validate_utf8(str);
    std::string res = "\"";

    for (auto c : str) {
        res += inspect(c, utf8);
    }
    res += "\"";
    return res;
}

static std::string json_inspect(char c)
{
    int d = static_cast<unsigned char>(c);

    if (d >= 0x80)
        return string() + (char) d;

    if (isprint(d)) {
        switch (d) {
        case '\"': return "\\\"";
        case '\\': return "\\\\";
        default:
            return string() + (char) d;
        }
    }

    switch (d) {
    case '\b': return "\\b";
    case '\f': return "\\f";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    default:
        return string("\\u00")
            + "0123456789abcdef"[d>>4]
            + "0123456789abcdef"[d&0xf];
    }
}

std::string json_inspect(const std::string& str)
{
    bool utf8 = validate_utf8(str);

    if (!utf8)
    {
        throw std::invalid_argument("json_inspect: UTF-8 validation failed");
    }
    std::string res = "\"";

    for (auto c : str) {
        res += json_inspect(c);
    }
    res += "\"";
    return res;
}

std::string repeat(const std::string& in, int n)
{
    std::string res;

    for (int i = 0; i < n; i++) {
        res += in;
    }
    return res;
}

std::string group_digits(const std::string& in, const std::string& separator)
{
    std::string tail;

    auto end = in.find('.'); // end of integral part
    if (end != string::npos)
        tail = in.substr(end);
    else
        end = in.size();

    std::string res;
    std::string integer = in.substr(0, end);
    std::reverse(integer.begin(), integer.end());
    std::string delim = separator;
    std::reverse(delim.begin(), delim.end());
    for (size_t i = 0; i < end; i++)
    {
        if (i != 0 && i%3 == 0)
            res += delim;
        res += integer[i];
    }
    std::reverse(res.begin(), res.end());
    return res + tail;
}

std::vector<std::string> split(const std::string& in, const std::string& separator)
{
    std::vector<std::string> res;
    const char *p = in.c_str();
    const char *sep = separator.c_str();

    const char *q;

    while (true)
    {
        q = strstr(p, sep);
        if (q)
        {
            res.push_back(std::string(p, q));
            p = q + std::strlen(sep);
        }
        else
        {
            res.push_back(std::string(p));
            return res;
        }
    }
}

std::vector<std::string> split(const std::string& in, const std::string& separator, int limit)
{
    if (limit <= 0)
        throw std::domain_error("limit <= 0");

    std::vector<std::string> res;
    const char *p = in.c_str();
    const char *sep = separator.c_str();

    const char *q;

    while (true)
    {
        q = strstr(p, sep);
        if ((int)res.size() < limit-1 && q != nullptr)
        {
            res.push_back(std::string(p, q));
            p = q + std::strlen(sep);
        }
        else
        {
            res.push_back(std::string(p));
            return res;
        }
    }
}

std::string codepoint_to_utf8(uint32_t codepoint)
{
    std::string res;

    if (codepoint >= 0x110000) {
        auto message = str::format("Codepoint U+%04lX out of valid range [U+0000, U+10FFFF]", static_cast<unsigned long>(codepoint));
        throw std::out_of_range(message);
    }

    if (/* codepoint >= 0 && */ codepoint <= 0x7f) {
        res += (char) codepoint;
    } else if (codepoint >= 0x80 && codepoint <= 0x7ff) {
        res += (char) (0xb0 | (codepoint >> 6));
        res += (char) (0x80 | (codepoint & 0x3f));
    } else if (codepoint >= 0x800 && codepoint <= 0xffff) {
        res += (char) (0xe0 | (codepoint >> 12));
        res += (char) (0x80 | ((codepoint >> 6) & 0x3f));
        res += (char) (0x80 | (codepoint & 0x3f));
    } else { /* if (codepoint >= 0x10000  && codepoint <= 0x10ffff) */
        res += (char) (0xf0 | (codepoint >> 18));
        res += (char) (0x80 | ((codepoint >> 12) & 0x3f));
        res += (char) (0x80 | ((codepoint >> 6) & 0x3f));
        res += (char) (0x80 | (codepoint & 0x3f));
    }
    return res;
}

#include <stdarg.h>
std::string format(const char* fmt, ...)
{
    // 必要なバイト数の計算のために vsnprintf を呼び出し、実際に文字列
    // を作るためにもう一度 vsnprintf を呼び出す。
    // １度目の vsnprintf の呼び出しのあと ap の値は未定義になるので、
    // ２度目の呼び出しのために aq にコピーしておく。
    va_list ap, aq;
    std::string res;

    va_start(ap, fmt);
    va_copy(aq, ap);
    int size = vsnprintf(nullptr, 0, fmt, ap);
    char *data = new char[size + 1];
    vsnprintf(data, size + 1, fmt, aq);
    va_end(aq);
    va_end(ap);

    res = data;
    delete[] data;
    return res;
}

std::string vformat(const char* fmt, va_list ap)
{
    va_list aq;
    std::string res;

    va_copy(aq, ap);
    int size = vsnprintf(nullptr, 0, fmt, ap);
    char *data = new char[size + 1];
    vsnprintf(data, size + 1, fmt, aq);
    va_end(aq);

    res = data;
    delete[] data;
    return res;
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

std::string replace_prefix(const std::string& s, const std::string& prefix, const std::string& replacement)
{
    if (s.size() < prefix.size()) return s;

    if (s.substr(0, prefix.size()) == prefix)
        return replacement + s.substr(prefix.size());
    else
        return s;
}

std::string replace_suffix(const std::string& s, const std::string& suffix, const std::string& replacement)
{
    if (s.size() < suffix.size()) return s;

    if (s.substr(s.size() - suffix.size(), suffix.size()) == suffix)
        return s.substr(0, s.size() - suffix.size()) + replacement;
    else
        return s;
}

std::string upcase(const std::string& input)
{
    std::string res;
    for (auto c : input)
    {
        if (isalpha(c))
            res += toupper(c);
        else
            res += c;
    }
    return res;
}

std::string downcase(const std::string& input)
{
    std::string res;
    for (auto c : input)
    {
        if (isalpha(c))
            res += tolower(c);
        else
            res += c;
    }
    return res;
}

std::string capitalize(const std::string& input)
{
    std::string res;
    bool prevWasAlpha = false;

    for (auto c : input)
    {
        if (isalpha(c))
        {
            if (prevWasAlpha)
                res += tolower(c);
            else
            {
                res += toupper(c);
                prevWasAlpha = true;
            }
        }else
        {
            res += c;
            prevWasAlpha = false;
        }
    }
    return res;
}

bool is_prefix_of(const std::string& prefix, const std::string& string)
{
    if (string.size() < prefix.size())
        return false;

    return string.substr(0, prefix.size()) == prefix;
}

bool has_prefix(const std::string& subject, const std::string& prefix)
{
    return is_prefix_of(prefix, subject);
}

bool has_suffix(const std::string& subject, const std::string& suffix)
{
    if (subject.size() < suffix.size())
        return false;

    return subject.substr(subject.size() - suffix.size(), suffix.size()) == suffix;
}

std::string join(const std::string& delimiter, const std::vector<std::string>& vec)
{
    std::string res;

    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        if (it != vec.begin())
            res += delimiter;
        res += *it;
    }

    return res;
}

std::string ascii_dump(const std::string& in, const std::string& replacement)
{
    std::string res;

    for (auto c : in)
    {
        if (std::isprint(c))
            res += c;
        else
            res += replacement;
    }
    return res;
}

std::string extension_without_dot(const std::string& filename)
{
    auto i = filename.rfind('.');
    if (i == std::string::npos)
        return "";
    else
        return filename.substr(i + 1);
}

int count(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        throw std::domain_error("cannot count empty strings");

    size_t start = 0;
    int n = 0;

    while ((start = haystack.find(needle, start)) != std::string::npos)
    {
        n++;
        start++;
    }
    return n;
}

std::string rstrip(const std::string& str)
{
    std::string res = str;

    while (!res.empty())
    {
        auto c = res.back();
        if (c == ' ' || (c >= 0x09 && c <= 0x0d) || c == '\0')
            res.pop_back();
        else
            break;
    }

    return res;
}

std::string strip(const std::string& str)
{
    auto it = str.begin();
    while (it != str.end()) {
        auto c = *it;
        if (c == ' ' || (c >= 0x09 && c <= 0x0d) || c == '\0')
            it++;
        else
            break;
    }
    std::string res(it, str.end());

    while (!res.empty()) {
        auto c = res.back();
        if (c == ' ' || (c >= 0x09 && c <= 0x0d) || c == '\0')
            res.pop_back();
        else
            break;
    }

    return res;
}

std::string escapeshellarg_unix(const std::string& str)
{
    std::string buf = "\'";

    for (char c : str)
    {
        if (c == '\'')
            buf += "\'\"\'\"\'";
        else
            buf += c;
    }
    buf += "\'";
    return buf;
}

std::string STR()
{
    return "";
}

std::vector<std::string> to_lines(const std::string& text)
{
    std::vector<std::string> res;
    std::string line;

    for (auto it = text.begin(); it != text.end(); ++it)
    {
        if (*it == '\n')
        {
            line += *it;
            res.push_back(line);
            line.clear();
        }else
        {
            line.push_back(*it);
        }
    }
    res.push_back(line);

    if (res.back() == "")
        res.pop_back();

    return res;
}

std::string indent_tab(const std::string& text, int n)
{
    if (n < 0)
        throw std::domain_error("domain error");

    auto lines = to_lines(text);
    auto space = repeat("\t", n);

    for (size_t i = 0; i < lines.size(); ++i)
        lines[i] = space + lines[i];

    return join("", lines);
}

bool validate_utf8(const std::string& str)
{
    auto it = str.begin();
    while (it != str.end())
    {
        if ((*it & 0x80) == 0) // 0xxx xxxx
        {
            it++;
            continue;
        }else if ((*it & 0xE0) == 0xC0) // 110x xxxx
        {
            it++;
            if (it == str.end())
                return false;
            if ((*it & 0xC0) == 0x80)
                it++;
            else
                return false;
        }else if ((*it & 0xF0) == 0xE0) // 1110 xxxx
        {
            it++;
            for (int i = 0; i < 2; ++i)
            {
                if (it == str.end())
                    return false;
                if ((*it & 0xC0) == 0x80)
                    it++;
                else
                    return false;
            }
        }else if ((*it & 0xF8) == 0xF0) // 1111 0xxx
        {
            it++;
            for (int i = 0; i < 3; ++i)
            {
                if (it == str.end())
                    return false;
                if ((*it & 0xC0) == 0x80)
                    it++;
                else
                    return false;
            }
        }else
            return false;
    }
    return true;
}

std::string truncate_utf8(const std::string& str, size_t length)
{
    std::string dest;

    auto it = str.begin();
    while (it != str.end())
    {
        if ((*it & 0x80) == 0) // 0xxx xxxx
        {
            if (dest.size() + 1  > length)
                break;
            dest += *it++;
            continue;
        }else if ((*it & 0xE0) == 0xC0) // 110x xxxx
        {
            if (dest.size() + 2 > length)
                break;
            dest += *it++;
            if (it == str.end())
                goto Error;
            if ((*it & 0xC0) == 0x80)
            {
                dest += *it++;
            }else
                goto Error;
        }else if ((*it & 0xF0) == 0xE0) // 1110 xxxx
        {
            if (dest.size() + 3 > length)
                break;
            dest += *it++;
            for (int i = 0; i < 2; ++i)
            {
                if (it == str.end())
                    goto Error;
                if ((*it & 0xC0) == 0x80)
                {
                    dest += *it++;
                }else
                    goto Error;
            }
        }else if ((*it & 0xF8) == 0xF0) // 1111 0xxx
        {
            if (dest.size() + 4 > length)
                break;
            dest += *it++;
            for (int i = 0; i < 3; ++i)
            {
                if (it == str.end())
                    goto Error;
                if ((*it & 0xC0) == 0x80)
                {
                    dest += *it++;
                }else
                    goto Error;
            }
        }else
            goto Error;
    }
    return dest;

Error:
    throw std::invalid_argument("truncate_utf8: UTF-8 validation failed");
}

std::string valid_utf8(const std::string& bytes)
{
    if (str::validate_utf8(bytes))
    {
        return bytes;
    } else {
        std::string escaped;

        for (auto it = bytes.begin(); it != bytes.end(); ++it)
        {
            if (*it < 0)
                escaped += str::format("[%02X]", static_cast<unsigned char>(*it));
            else
                escaped += *it;
        }
        return escaped;
    }
}

std::vector<std::string> shellwords(const std::string& str)
{
    std::vector<std::string> words;
    std::string curr;
    bool allowempty = false; // Allow to push an empty-string word because it's been quoted.

    auto p = str.begin();
    while (p != str.end()) {
        if (*p == ' ' || *p == '\t' || *p == '\v') {
            if (allowempty || curr != "") {
                allowempty = false;
                words.push_back(curr);
                curr = "";
            }
            p++;
            continue;
        }
        
        if (*p == '\'') {
            p++;
            while (true) {
                if (p == str.end()) {
                    throw FormatException("Unterminated single-quoted string");
                }
                if (*p == '\'') {
                    allowempty = true;
                    break;
                } else {
                    curr += *p;
                }
                p++;
            }
        } else if (*p == '"') {
            p++;
            while (true) {
                if (p == str.end()) {
                    throw FormatException("Unterminated double-quoted string");
                }
                if (*p == '"') {
                    allowempty = true;
                    break;
                } else if (*p == '\\') {
                    p++;
                    if (p == str.end()) {
                        throw FormatException("Unfinished backlash escape");
                    }
                    if (*p != '\"' && *p != '\\') {
                        curr += '\\';
                    }
                    curr += *p;
                } else {
                    curr += *p;
                }
                p++;
            }
        } else if (*p == '\\') {
            p++;
            if (p == str.end()) {
                throw FormatException("Unfinished backlash escape");
            }
            curr += *p;
        } else {
            curr += *p;
        }
        p++;
    }
    
    if (allowempty || curr != "") {
        allowempty = false;
        words.push_back(curr); // push last word
    }
    return words;
}

} // namespace str
