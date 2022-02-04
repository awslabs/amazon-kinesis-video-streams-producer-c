call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" amd64
mkdir build
cd build
cmd.exe /c cmake -G "NMake Makefiles" ..
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE ..
nmake
"D:\a\amazon-kinesis-video-streams-producer-c\amazon-kinesis-video-streams-producer-c\build\tst\producer_test.exe" --gtest_break_on_failure