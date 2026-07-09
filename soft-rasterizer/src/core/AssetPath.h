#pragma once

#include <string>

// 从 exe 所在目录向上查找资源（兼容 x64/Release 运行）
std::string resolveAssetPath(const std::string& relativePath);
std::string resolveAssetDirectory(const std::string& relativeDir);
