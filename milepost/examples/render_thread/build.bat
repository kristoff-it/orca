
set INCLUDES=/I ..\..\src /I ..\..\src\util /I ..\..\src\platform /I ../../ext /I ../../ext/angle_headers

cl /we4013 /Zi /Zc:preprocessor /std:c11 /experimental:c11atomics %INCLUDES% main.c /link /MANIFEST:EMBED /MANIFESTINPUT:../../src/win32_manifest.xml /LIBPATH:../../bin milepost.dll.lib /out:../../bin/example_render_thread.exe
