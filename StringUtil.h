#pragma once

#include <string>

std::string Trim(std::string value);
std::string ToLower(std::string value);
std::wstring ToWide(const std::string& text);
std::string ToUtf8(const std::wstring& text);
