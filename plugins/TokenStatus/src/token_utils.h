#pragma once

// Dependency-free functions also used by the Linux-side unit tests.
// This is an estimate, not a model tokenizer: 4 document bytes per token.
inline unsigned long long approximate_tokens(unsigned long long document_bytes) {
    return document_bytes / 4ULL + ((document_bytes % 4ULL) >= 2ULL ? 1ULL : 0ULL);
}

inline unsigned int wlen_limit(const wchar_t* src, unsigned int cap) {
    if (!src) return 0;
    unsigned int n = 0;
    while (n < cap && src[n] != 0) ++n;
    return n;
}

inline unsigned int wcopy_bounded(wchar_t* dst, unsigned int cap, const wchar_t* src) {
    if (!cap) return 0;
    unsigned int i = 0;
    if (src) {
        for (; i + 1 < cap && src[i] != 0; ++i) dst[i] = src[i];
    }
    dst[i] = 0;
    return i;
}

inline bool wequal(const wchar_t* a, const wchar_t* b) {
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (*a != *b) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

inline unsigned int wappend_bounded(wchar_t* dst, unsigned int cap, unsigned int offset, const wchar_t* src) {
    if (!cap || offset >= cap) return offset;
    if (!src) return offset;
    while (offset + 1 < cap && *src) dst[offset++] = *src++;
    dst[offset] = 0;
    return offset;
}

// Writes commas for readability: 1234567 -> "1,234,567".
inline unsigned int wappend_grouped_number(wchar_t* dst, unsigned int cap,
                                           unsigned int offset, unsigned long long value) {
    wchar_t reversed[30];
    unsigned int n = 0;
    unsigned int digits = 0;
    do {
        if (digits != 0 && digits % 3 == 0) reversed[n++] = L',';
        reversed[n++] = (wchar_t)(L'0' + (value % 10ULL));
        value /= 10ULL;
        ++digits;
    } while (value != 0 && n < 29);
    while (n && offset + 1 < cap) dst[offset++] = reversed[--n];
    if (cap) dst[offset < cap ? offset : (cap - 1)] = 0;
    return offset;
}

inline void compose_status(const wchar_t* original,
                           unsigned long long document_bytes,
                           wchar_t* result, unsigned int capacity) {
    unsigned int n = wcopy_bounded(result, capacity, original);
    n = wappend_bounded(result, capacity, n, L"    tokens: ~");
    wappend_grouped_number(result, capacity, n, approximate_tokens(document_bytes));
}
