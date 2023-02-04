#include "debuglog.h"

#include <iomanip>

void DebugLogHresult(HRESULT hr) {
    switch (hr) {
    case S_OK:
        return;
    default:
        DebugLogl(DebugLogStream{} << L"Unknown error: " << hr);
        return;
    }
}

void DebugLogStream::Log() {
    OutputDebugStringW(this->str().c_str());
}

void DebugLogStream::Logl() {
    *this << std::endl;
    Log();
}

std::wostream& operator<<(std::wostream& stream, const GUID& guid) {
    std::wostream::fmtflags base = stream.flags() & std::wostream::basefield;
    WCHAR fill = stream.fill();

    stream << std::hex << std::setfill(L'0')
        << std::setw(8) << guid.Data1 << L'-'
        << std::setw(4) << guid.Data2 << L'-'
        << std::setw(4) << guid.Data3 << L'-'
        << std::setw(2) << guid.Data4[0] << std::setw(2) << guid.Data4[1] << L'-';
    for (int i = 2; i < 8; ++i)
        stream << std::setw(2) << guid.Data4[i];

    stream.setf(base, std::wostream::basefield);
    stream.fill(fill);
    return stream;
}

std::wostream& operator<<(std::wostream& stream, const SYSTEMTIME& time) {
    stream << time.wYear << L'-' << time.wMonth << L'-' << time.wDay << L' ' << time.wHour << L':' << time.wMinute << L':' << time.wSecond << L'.' << time.wMilliseconds;
    return stream;
}
