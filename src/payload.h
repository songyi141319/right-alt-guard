#pragma once
#include <windows.h>
#include <string>
bool PreparePayload(HINSTANCE instance, const std::wstring& folder);
std::wstring DefaultPayloadFolder();
