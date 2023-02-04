#pragma once

#include "framework.h"
#include <sstream>

inline void DebugLog(PCWCHAR str) {
    OutputDebugStringW(str);
}

void DebugLogHresult(HRESULT hr);

class DebugLogStream : public std::wostringstream {
public:
    void Log();
    void Logl();
};
inline void DebugLogl(DebugLogStream& stream) {
    stream.Logl();
}
inline void DebugLogl(DebugLogStream&& stream) {
    stream.Logl();
}

std::wostream& operator<<(std::wostream& stream, const GUID& guid);
std::wostream& operator<<(std::wostream& stream, const SYSTEMTIME& time);