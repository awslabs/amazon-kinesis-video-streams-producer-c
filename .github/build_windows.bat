call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat x64 8.1"
mkdir build
cd build
cmd.exe /c cmake -G "NMake Makefiles" ..
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE ..
nmake
