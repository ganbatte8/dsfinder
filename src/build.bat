@echo off
set CompilerFlags= -nologo -MTd -Gm- -GR- -EHa- -O2 -Oi -FC -Z7 -WX -W4 -DDEBUG=0 -wd4201 -wd4244 -wd4456 -wd4505 -wd4100 -wd4189 -wd4701 -DCOMPILER_MSVC=1 -std:c++17
if not exist ..\build mkdir ..\build
pushd ..\build
del *.pdb
REM cl %CompilerFlags% ../src/dsfinder_asset_packer.cpp /link -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib
cl %CompilerFlags% ../src/dsfinder.cpp -LD /link -incremental:no -opt:ref -PDB:dll%random%.pdb -EXPORT:GameUpdate
cl %CompilerFlags% ../src/win32_dsfinder.cpp /link -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib
popd