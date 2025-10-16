#include "pch.h"
#include "StringUtil.h"
#include <Windows.h>

namespace StringUtil {
    std::wstring Utf8ToWString(const std::string& utf8)
    {
        if (utf8.empty()) return std::wstring();
        int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (needed <= 0) return std::wstring();
        std::wstring wide;
        wide.resize(needed - 1);
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), needed);
        return wide;
    }

    std::string WStringToUtf8(const std::wstring& wide)
    {
        if (wide.empty()) return std::string();
        int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return std::string();
        std::string utf8;
        utf8.resize(needed - 1);
        ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
        return utf8;
    }
}
