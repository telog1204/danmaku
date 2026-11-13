#pragma once

// プリコンパイル済みヘッダー。
// DxLib.h は大きなヘッダー（Windowsでは中で windows.h も読み込む）なので、Visual Studio ではここで一度だけコンパイルして各ファイルで使い回し、ビルドを速くする。
// Android（CMake）ではプリコンパイルせず、普通のヘッダーとして読み込む。
// 各 .cpp ファイルは、最初の #include でこのファイルを読み込むこと。
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <DxLib.h>
