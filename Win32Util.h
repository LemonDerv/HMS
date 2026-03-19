#pragma once

#include <windows.h>

#include <string>

std::wstring GetWindowTextString(HWND hwnd);
void SetControlFont(HWND hwnd, HFONT font);
COLORREF BlendColor(COLORREF a, COLORREF b, double t);
void DrawVerticalGradient(HDC hdc, const RECT& rect, COLORREF top, COLORREF bottom);
