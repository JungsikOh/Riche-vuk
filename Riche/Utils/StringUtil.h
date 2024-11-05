#pragma once

static std::wstring ToWideString(std::string const& in) 
{
    std::wstring out{};
    out.reserve(in.length());
    const char* ptr = in.data();
    const char* const end = in.data() + in.length();

    mbstate_t state{};
    wchar_t wc;
    while (size_t len = mbrtowc(&wc, ptr, end - ptr, &state))
    {
        if (len == static_cast<size_t>(-1)) // bad encoding
            return std::wstring{};
        if (len == static_cast<size_t>(-2))
            break;
        out.push_back(wc);
        ptr += len;
    }
    return out;
}