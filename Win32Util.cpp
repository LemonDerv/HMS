#include "Win32Util.h"

std::wstring GetWindowTextString(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(length);
    return text;
}

void SetControlFont(HWND hwnd, HFONT font) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

COLORREF BlendColor(COLORREF a, COLORREF b, double t) {
    const int r = static_cast<int>(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t);
    const int g = static_cast<int>(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t);
    const int blue = static_cast<int>(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t);
    return RGB(r, g, blue);
}

void DrawVerticalGradient(HDC hdc, const RECT& rect, COLORREF top, COLORREF bottom) {
    const int height = rect.bottom - rect.top;
    for (int i = 0; i < height; ++i) {
        const double t = height <= 1 ? 0.0 : static_cast<double>(i) / (height - 1);
        HBRUSH brush = CreateSolidBrush(BlendColor(top, bottom, t));
        RECT line = {rect.left, rect.top + i, rect.right, rect.top + i + 1};
        FillRect(hdc, &line, brush);
        DeleteObject(brush);
    }
}
