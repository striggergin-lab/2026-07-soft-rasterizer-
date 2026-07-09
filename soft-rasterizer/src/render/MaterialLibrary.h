#pragma once

#include "render/Texture.h"

#include <string>
#include <vector>

// 一对材质：{前缀}_color.* + {前缀}_normal.*
struct MaterialSet {
    std::string name;
    std::string colorPath;
    std::string normalPath;
};

// 扫描 image 文件夹，按前缀配对颜色/法线贴图
struct MaterialLibrary {
    std::vector<MaterialSet> entries;
    int currentIndex = 0;

    bool scan(const std::string& relativeImageDir = "image");
    bool empty() const { return entries.empty(); }
    size_t count() const { return entries.size(); }

    const MaterialSet& current() const;
    const std::string& currentName() const;

    bool next();
    bool prev();
    bool applyCurrent(Texture& albedo, Texture& normal) const;
};
