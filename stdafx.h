// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

//#include <stdio.h>
//#include <tchar.h>

#define PAD16(n) (((n)+15)/16*16)
#define SAFE_RELEASE(n) if(n) { n->Release(); n = NULL; }
#define SAFE_DELETE(n) if(n) { delete n; n = NULL; }
// TODO:  在此处引用程序需要的其他头文件
