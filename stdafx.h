// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

//#include <stdio.h>
//#include <tchar.h>

#define PAD16(n) (((n)+15)/16*16)
#define SAFE_RELEASE(n) if(n) { n->Release(); n = NULL; }
#define SAFE_DELETE(n) if(n) { delete n; n = NULL; }
// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�
