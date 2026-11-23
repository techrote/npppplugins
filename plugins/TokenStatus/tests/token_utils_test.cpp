#include "../src/token_utils.h"
#include <cassert>
#include <cwchar>
#include <climits>

int main() {
    assert(approximate_tokens(0) == 0);
    assert(approximate_tokens(1) == 0);
    assert(approximate_tokens(2) == 1);
    assert(approximate_tokens(3) == 1);
    assert(approximate_tokens(4) == 1);
    assert(approximate_tokens(5) == 1);
    assert(approximate_tokens(6) == 2);
    assert(approximate_tokens(4000) == 1000);
    assert(approximate_tokens(ULLONG_MAX) == ULLONG_MAX / 4 + 1);
    wchar_t buf[128] = {};
    compose_status(L"length: 12345    lines: 200", 12345, buf, 128);
    assert(std::wcscmp(buf, L"length: 12345    lines: 200    tokens: ~3,086") == 0);
    compose_status(L"length: 0    lines: 1", 0, buf, 128);
    assert(std::wcscmp(buf, L"length: 0    lines: 1    tokens: ~0") == 0);
    wchar_t tiny[12] = {};
    compose_status(L"some ridiculously long text", 123, tiny, 12);
    assert(tiny[11] == 0);
    assert(std::wcslen(tiny) <= 11);
    assert(wequal(L"x", L"x"));
    assert(!wequal(L"x", L"y"));
    return 0;
}
