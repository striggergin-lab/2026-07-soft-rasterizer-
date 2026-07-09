#include "AssetPath.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <filesystem>

std::string resolveAssetPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    fs::path dir = fs::path(modulePath).parent_path();

    for (int i = 0; i < 6; ++i) {
        const fs::path candidate = dir / relativePath;
        if (fs::exists(candidate)) {
            return candidate.string();
        }
        if (!dir.has_parent_path()) break;
        dir = dir.parent_path();
    }

    return relativePath;
}

std::string resolveAssetDirectory(const std::string& relativeDir) {
    namespace fs = std::filesystem;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    fs::path dir = fs::path(modulePath).parent_path();

    for (int i = 0; i < 6; ++i) {
        const fs::path candidate = dir / relativeDir;
        if (fs::exists(candidate) && fs::is_directory(candidate)) {
            return candidate.string();
        }
        if (!dir.has_parent_path()) break;
        dir = dir.parent_path();
    }

    return relativeDir;
}
