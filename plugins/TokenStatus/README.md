# TokenStatus for Notepad++

A small native Notepad++ plugin that adds a **ballpark token count** to the existing length / lines field on the lower status bar.

    length: 12345    lines: 200    tokens: ~3,086

## Install (Windows x64)

Download the **TokenStatus-windows-x64** artifact from a successful GitHub Actions build, then:

1. Exit Notepad++.
2. Extract the built DLL to \`plugins\TokenStatus\TokenStatus.dll\` inside the directory containing \`notepad++.exe\`. The usual installation is \`C:\Program Files\Notepad++\plugins\TokenStatus\TokenStatus.dll\`.
3. Restart Notepad++. The extra count should appear automatically.
4. Toggle it with **Plugins > TokenStatus > Show approximate tokens**.

To uninstall, close Notepad++, remove the TokenStatus plugin folder, and restart. If the status bar is hidden, enable it in Notepad++ preferences. Narrow windows can clip status text.

## Estimate / limitations

- The number is **rounded document byte length / 4**. This is not an actual GPT, Claude or other model tokenizer, and can diverge substantially for code, non-Latin text or unusual documents.
- The estimate is constant-time: it queries Scintilla's current document byte length and does not scan the document.
- Local-only: no network activity and no transmission of document contents.
- Expands the document-size panel at the expense of the language panel (by at most 135 DPI-scaled pixels). Restores the original widths when disabled.
- The plugin subclasses Notepad++'s Win32 status bar; UI changes in newer versions could affect compatibility.

## Build (MSVC, x64)

With Visual Studio C++/Windows SDK and CMake installed:

\`\`\`powershell
cmake -S plugins/TokenStatus -B build/tokenstatus -A x64
cmake --build build/tokenstatus --config Release
\`\`\`

The DLL is at \`build/tokenstatus/Release/TokenStatus.dll\`. Alternatively, run \`plugins/TokenStatus/build-windows.ps1\`.

## Tests

The repository's CI runs two portable tests on Linux:

\`\`\`sh
g++ -std=c++17 -Wall -Wextra -Werror plugins/TokenStatus/tests/token_utils_test.cpp -o /tmp/token-utils-test
/tmp/token-utils-test
g++ -std=c++17 -Wall -Wextra -Werror plugins/TokenStatus/tests/status_emulation_test.cpp -o /tmp/status-emulation-test
/tmp/status-emulation-test
\`\`\`

The emulation test exercises the real plugin code against simulated Win32 messages; it is **not** an end-to-end Windows Notepad++ test. CI also builds a native x64 DLL using Windows/MSVC and uploads it as a workflow artifact.

**Manual acceptance still needed:** Launch the CI-built DLL in a real 64-bit Notepad++ session, check typing, tab changes, status-bar resize, toggle off/on and clean shutdown. Do not treat compilation or simulated tests as proof of live UI compatibility.

## Provenance

Source implementation originates from the TokenStatus v1.0 standalone prototype. The original 64-bit build was compiled with clang-cl/lld-link and verified as PE32+ with all six Notepad++ exports, but it has not been interactively smoke-tested on Windows.

Notepad++ plugin API reference: https://npp-user-manual.org/docs/plugin-communication/
