set INCLUDES=/I ..\..\src /I ..\..\src\util /I ..\..\src\platform /I ../../ext
cl /we4013 /Zi /Zc:preprocessor /std:c11 %INCLUDES% main.c /link /LIBPATH:../../bin milepost.dll.lib user32.lib /out:../../bin/example_simple_window.exe
