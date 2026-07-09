#include "MaterialLibrary.h"

#include "core/AssetPath.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>

namespace {

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool isImageExtension(const std::string& ext) {
    const std::string e = toLower(ext);
    return e == ".png" || e == ".jpg" || e == ".jpeg";
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string stripSuffix(const std::string& s, const std::string& suffix) {
    if (endsWith(s, suffix)) {
        return s.substr(0, s.size() - suffix.size());
    }
    return {};
}

}  // namespace

bool MaterialLibrary::scan(const std::string& relativeImageDir) {
    entries.clear();
    currentIndex = 0;

    namespace fs = std::filesystem;
    const std::string dirPath = resolveAssetDirectory(relativeImageDir);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return false;
    }

    std::unordered_map<std::string, std::string> colorByPrefix;
    std::unordered_map<std::string, std::string> normalByPrefix;

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;

        const fs::path path = entry.path();
        if (!isImageExtension(path.extension().string())) continue;

        const std::string stem = path.stem().string();
        if (endsWith(stem, "_color")) {
            const std::string prefix = stripSuffix(stem, "_color");
            if (!prefix.empty()) {
                colorByPrefix[prefix] = path.string();
            }
        } else if (endsWith(stem, "_normal")) {
            const std::string prefix = stripSuffix(stem, "_normal");
            if (!prefix.empty()) {
                normalByPrefix[prefix] = path.string();
            }
        }
    }

    for (const auto& [prefix, colorPath] : colorByPrefix) {
        const auto it = normalByPrefix.find(prefix);
        if (it == normalByPrefix.end()) continue;

        MaterialSet set;
        set.name = prefix;
        set.colorPath = colorPath;
        set.normalPath = it->second;
        entries.push_back(std::move(set));
    }

    std::sort(entries.begin(), entries.end(),
              [](const MaterialSet& a, const MaterialSet& b) { return a.name < b.name; });

    return !entries.empty();
}

const MaterialSet& MaterialLibrary::current() const {
    static const MaterialSet kEmpty{};
    if (entries.empty()) return kEmpty;
    return entries[static_cast<size_t>(currentIndex)];
}

const std::string& MaterialLibrary::currentName() const { return current().name; }

bool MaterialLibrary::next() {
    if (entries.empty()) return false;
    currentIndex = (currentIndex + 1) % static_cast<int>(entries.size());
    return true;
}

bool MaterialLibrary::prev() {
    if (entries.empty()) return false;
    currentIndex = (currentIndex - 1 + static_cast<int>(entries.size())) %
                   static_cast<int>(entries.size());
    return true;
}

bool MaterialLibrary::applyCurrent(Texture& albedo, Texture& normal) const {
    if (entries.empty()) return false;

    const MaterialSet& set = current();
    const bool colorOk = albedo.loadFromFile(set.colorPath);
    const bool normalOk = normal.loadFromFile(set.normalPath);

    if (!colorOk) {
        albedo = Texture::createCheckerboard();
    }
    if (!normalOk) {
        normal = Texture::createFlatNormal();
    }

    return colorOk;
}
