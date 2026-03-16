#include "core/Scanner.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <vector>

namespace AIPackager::Core {
namespace {

[[nodiscard]] std::string ToLowerAscii(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());

    for (const char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return lowered;
}

[[nodiscard]] std::filesystem::path SafeRelative(
    const std::filesystem::path& root,
    const std::filesystem::path& target) {
    std::error_code ec;
    std::filesystem::path rel = std::filesystem::relative(target, root, ec);
    if (ec || rel.empty()) {
        rel = target.lexically_relative(root);
    }
    if (rel.empty()) {
        rel = target.filename();
    }
    return rel;
}

[[nodiscard]] std::filesystem::path NormalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    if (ec) {
        return path.lexically_normal();
    }

    const std::filesystem::path weaklyCanonical = std::filesystem::weakly_canonical(absolute, ec);
    if (ec) {
        return absolute.lexically_normal();
    }

    return weaklyCanonical;
}

[[nodiscard]] std::string DescribeFsError(const std::error_code& ec, std::string_view fallback) {
    if (ec) {
        return ec.message();
    }
    return std::string(fallback);
}

struct GitignorePattern {
    std::string pattern;
    std::string sourceDirectoryLower;
    bool directoryOnly {false};
    bool anchoredToRoot {false};
    bool negated {false};
};

[[nodiscard]] std::string Trim(std::string value) {
    const auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };

    while (!value.empty() && isWs(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isWs(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    return value;
}

[[nodiscard]] bool WildcardMatch(std::string_view pattern, std::string_view text) {
    std::vector<std::vector<bool>> dp(
        pattern.size() + 1U,
        std::vector<bool>(text.size() + 1U, false));

    dp[0][0] = true;
    for (std::size_t i = 1; i <= pattern.size(); ++i) {
        if (pattern[i - 1U] == '*') {
            dp[i][0] = dp[i - 1U][0];
        }
    }

    for (std::size_t i = 1; i <= pattern.size(); ++i) {
        for (std::size_t j = 1; j <= text.size(); ++j) {
            if (pattern[i - 1U] == '*') {
                dp[i][j] = dp[i - 1U][j] || dp[i][j - 1U];
            } else if (pattern[i - 1U] == '?' || pattern[i - 1U] == text[j - 1U]) {
                dp[i][j] = dp[i - 1U][j - 1U];
            }
        }
    }

    return dp[pattern.size()][text.size()];
}

[[nodiscard]] bool MatchesGitignorePattern(
    const GitignorePattern& pattern,
    std::string_view scopedRelativePathLower,
    std::string_view filenameLower,
    bool isDirectory) {
    if (pattern.directoryOnly && !isDirectory) {
        return false;
    }

    if (pattern.anchoredToRoot) {
        return WildcardMatch(pattern.pattern, scopedRelativePathLower);
    }

    if (WildcardMatch(pattern.pattern, filenameLower)) {
        return true;
    }

    if (WildcardMatch(pattern.pattern, scopedRelativePathLower)) {
        return true;
    }

    std::size_t start = 0;
    while (start < scopedRelativePathLower.size()) {
        const std::size_t slashPos = scopedRelativePathLower.find('/', start);
        if (slashPos == std::string_view::npos) {
            break;
        }
        const std::string_view suffix = scopedRelativePathLower.substr(slashPos + 1U);
        if (WildcardMatch(pattern.pattern, suffix)) {
            return true;
        }
        start = slashPos + 1U;
    }

    return false;
}

[[nodiscard]] std::string RelativeDirectoryLower(
    const std::filesystem::path& root,
    const std::filesystem::path& directory) {
    std::error_code ec;
    const std::filesystem::path rel = std::filesystem::relative(directory, root, ec);
    if (ec || rel.empty() || rel == ".") {
        return std::string {};
    }
    return ToLowerAscii(rel.generic_string());
}

void LoadGitignoreRulesForDirectory(
    const std::filesystem::path& root,
    const std::filesystem::path& directory,
    ScannerOptions& options,
    std::vector<GitignorePattern>& patterns) {
    const std::filesystem::path gitignorePath = directory / ".gitignore";
    std::error_code existsEc;
    if (!std::filesystem::exists(gitignorePath, existsEc) || existsEc) {
        return;
    }

    std::ifstream input(gitignorePath);
    if (!input.is_open()) {
        return;
    }

    std::string rawLine;
    while (std::getline(input, rawLine)) {
        std::string line = Trim(rawLine);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        bool negated = false;
        if (line[0] == '!') {
            negated = true;
            line.erase(line.begin());
            line = Trim(line);
            if (line.empty()) {
                continue;
            }
        }

        std::replace(line.begin(), line.end(), '\\', '/');

        const bool anchored = !line.empty() && line.front() == '/';
        const bool directoryOnly = !line.empty() && line.back() == '/';

        if (anchored) {
            line.erase(line.begin());
        }
        if (directoryOnly && !line.empty()) {
            line.pop_back();
        }

        line = Trim(line);
        if (line.empty()) {
            continue;
        }

        const std::string lowered = ToLowerAscii(line);

        if (directoryOnly && !negated) {
            const std::filesystem::path p(lowered);
            const std::string leaf = p.filename().string();
            if (!leaf.empty()) {
                options.excludedDirectoryNames.insert(leaf);
            }
        }

        if (!negated && lowered.rfind("*.", 0) == 0 && lowered.find('/') == std::string::npos) {
            options.excludedExtensions.insert(lowered.substr(1));
        }

        patterns.push_back(GitignorePattern {
            .pattern = lowered,
            .sourceDirectoryLower = RelativeDirectoryLower(root, directory),
            .directoryOnly = directoryOnly,
            .anchoredToRoot = anchored,
            .negated = negated
        });
    }
}

[[nodiscard]] bool EvaluateGitignore(
    const std::string& relativePathLower,
    const std::string& fileNameLower,
    bool isDirectory,
    const std::vector<GitignorePattern>& patterns) {
    bool ignored = false;

    for (const auto& pattern : patterns) {
        std::string scopedPath = relativePathLower;
        if (!pattern.sourceDirectoryLower.empty()) {
            if (relativePathLower == pattern.sourceDirectoryLower) {
                scopedPath.clear();
            } else {
                const std::string prefix = pattern.sourceDirectoryLower + "/";
                if (relativePathLower.rfind(prefix, 0) != 0) {
                    continue;
                }
                scopedPath = relativePathLower.substr(prefix.size());
            }
        }

        if (scopedPath.empty()) {
            continue;
        }

        if (MatchesGitignorePattern(pattern, scopedPath, fileNameLower, isDirectory)) {
            ignored = !pattern.negated;
        }
    }

    return ignored;
}

[[nodiscard]] bool ShouldSkipDirectoryWithOptions(
    std::string_view directoryName,
    const ScannerOptions& options) {
    const std::string lowered = ToLowerAscii(directoryName);

    if (options.excludedDirectoryNames.contains(lowered)) {
        return true;
    }

    if (lowered.find("build") != std::string::npos) {
        return true;
    }

    if (lowered.find("cmake") != std::string::npos) {
        return true;
    }

    return false;
}

[[nodiscard]] bool ShouldSkipExtensionWithOptions(
    std::string_view extension,
    const ScannerOptions& options) {
    const std::string lowered = ToLowerAscii(extension);
    if (options.allowedExtensions.empty()) {
        return false;
    }
    return !options.allowedExtensions.contains(lowered);
}

[[nodiscard]] LanguageProfile ProfileForExtension(std::string_view extension) {
    const std::string lowered = ToLowerAscii(extension);

    if (lowered == ".cpp" || lowered == ".hpp" || lowered == ".h" || lowered == ".c" ||
        lowered == ".cc" || lowered == ".cxx" || lowered == ".ixx") {
        return LanguageProfile::Cpp;
    }

    if (lowered == ".js" || lowered == ".ts" || lowered == ".jsx" || lowered == ".tsx" ||
        lowered == ".html" || lowered == ".css" || lowered == ".scss" || lowered == ".vue") {
        return LanguageProfile::Web;
    }

    if (lowered == ".rs") {
        return LanguageProfile::Rust;
    }

    if (lowered == ".dart" || lowered == ".swift" || lowered == ".kt") {
        return LanguageProfile::Mobile;
    }

    if (lowered == ".java" || lowered == ".gradle" || lowered == ".properties" || lowered == ".xml") {
        return LanguageProfile::Java;
    }

    return LanguageProfile::All;
}

[[nodiscard]] LanguageProfile DetectProfileFromExtensionCounts(
    const std::map<std::string, std::size_t>& extensionCounts) {
    auto extensionWeight = [](std::string_view extension) -> double {
        const std::string lowered = ToLowerAscii(extension);

        if (lowered == ".md" || lowered == ".txt") {
            return 0.05;
        }

        if (lowered == ".json" || lowered == ".yml" || lowered == ".yaml" || lowered == ".toml") {
            return 0.35;
        }

        if (lowered == ".env" || lowered == ".ini" || lowered == ".cfg" || lowered == ".conf") {
            return 0.25;
        }

        return 1.0;
    };

    std::map<LanguageProfile, double> weightedScores {
        {LanguageProfile::Cpp, 0.0},
        {LanguageProfile::Web, 0.0},
        {LanguageProfile::Rust, 0.0},
        {LanguageProfile::Mobile, 0.0},
        {LanguageProfile::Java, 0.0}
    };

    std::map<LanguageProfile, std::size_t> rawCounts {
        {LanguageProfile::Cpp, 0},
        {LanguageProfile::Web, 0},
        {LanguageProfile::Rust, 0},
        {LanguageProfile::Mobile, 0},
        {LanguageProfile::Java, 0}
    };

    for (const auto& [extension, count] : extensionCounts) {
        const LanguageProfile profile = ProfileForExtension(extension);
        if (profile == LanguageProfile::All) {
            continue;
        }

        weightedScores[profile] += static_cast<double>(count) * extensionWeight(extension);
        rawCounts[profile] += count;
    }

    LanguageProfile detectedProfile = LanguageProfile::All;
    double highestScore = 0.0;
    std::size_t highestRawCount = 0;

    for (const auto& [profile, score] : weightedScores) {
        const std::size_t rawCount = rawCounts[profile];
        if (score > highestScore || (score == highestScore && rawCount > highestRawCount)) {
            highestScore = score;
            highestRawCount = rawCount;
            detectedProfile = profile;
        }
    }

    return detectedProfile;
}

void ApplyLanguageProfile(ScannerOptions& options) {
    switch (options.languageProfile) {
    case LanguageProfile::Cpp:
        options.allowedExtensions = {
            ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".ixx", ".cmake", ".txt", ".md",
            ".json", ".yml", ".yaml"
        };
        break;
    case LanguageProfile::Web:
        options.allowedExtensions = {
            ".js", ".ts", ".tsx", ".jsx", ".json", ".html", ".css", ".scss", ".vue", ".md",
            ".txt", ".graphql", ".prisma", ".env"
        };
        options.excludedDirectoryNames.insert("dist");
        options.excludedDirectoryNames.insert("node_modules");
        options.excludedDirectoryNames.insert(".next");
        break;
    case LanguageProfile::Rust:
        options.allowedExtensions = {
            ".rs", ".toml", ".md", ".txt", ".json", ".yml", ".yaml"
        };
        options.excludedDirectoryNames.insert("target");
        break;
    case LanguageProfile::Mobile:
        options.allowedExtensions = {
            ".dart", ".swift", ".kt", ".js", ".ts", ".tsx", ".json", ".yml", ".yaml", ".md",
            ".txt", ".env"
        };
        options.excludedDirectoryNames.insert("pods");
        options.excludedDirectoryNames.insert(".gradle");
        break;
    case LanguageProfile::Java:
        options.allowedExtensions = {
            ".java", ".kt", ".gradle", ".properties", ".xml", ".yml", ".yaml", ".json", ".md",
            ".txt"
        };
        options.excludedDirectoryNames.insert("target");
        options.excludedDirectoryNames.insert("build");
        options.excludedDirectoryNames.insert(".gradle");
        break;
    case LanguageProfile::All:
    default:
        break;
    }
}

[[nodiscard]] std::string DetectEffectiveExtension(const std::filesystem::path& path) {
    std::string extension = ToLowerAscii(path.extension().string());
    if (!extension.empty()) {
        return extension;
    }

    const std::string fileName = ToLowerAscii(path.filename().string());
    if (fileName == ".env" || fileName.rfind(".env.", 0) == 0) {
        return ".env";
    }

    return extension;
}

[[nodiscard]] bool IsLikelyTextFile(const std::filesystem::path& path, std::size_t probeBytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::vector<unsigned char> buffer(probeBytes == 0 ? 1024U : probeBytes, 0U);
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize readCount = input.gcount();
    if (readCount <= 0) {
        return false;
    }

    std::size_t textLike = 0;
    for (std::streamsize i = 0; i < readCount; ++i) {
        const unsigned char c = buffer[static_cast<std::size_t>(i)];
        if (c == 0U) {
            return false;
        }

        const bool isWhitespaceControl = c == '\n' || c == '\r' || c == '\t' || c == '\f';
        const bool isPrintableAscii = c >= 32U && c <= 126U;
        const bool isUtf8Byte = c >= 128U;

        if (isWhitespaceControl || isPrintableAscii || isUtf8Byte) {
            ++textLike;
        }
    }

    const double ratio = static_cast<double>(textLike) / static_cast<double>(readCount);
    return ratio >= 0.85;
}

} // namespace

ScannerOptions ScannerOptions::Default() {
    ScannerOptions options;

    options.excludedDirectoryNames = {
        ".git", "build", "node_modules", "ai_export", "ai_share_plain_400kb", "venv", ".vscode",
        "target", "vendor", ".gradle", ".dart_tool", "__pycache__", ".next", ".nuxt", ".cache",
        ".terraform", "out", "dist"
    };
    options.excludedDirectoryNames.insert("build-mingw");
    options.excludedDirectoryNames.insert("_deps");

    options.excludedExtensions = {
        ".exe", ".dll", ".so", ".dylib", ".a", ".lib", ".obj", ".o", ".class", ".jar",
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".webp", ".svg", ".mp3", ".wav",
        ".flac", ".mp4", ".mov", ".avi", ".mkv", ".zip", ".7z", ".rar", ".tar", ".gz",
        ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".ttf", ".otf", ".woff",
        ".woff2"
    };

    options.allowedExtensions = {
        ".cpp", ".hpp", ".h", ".c", ".py", ".md", ".txt", ".cmake", ".yml", ".yaml",
        ".json", ".sh", ".swift", ".kt", ".dart", ".rs", ".go", ".java", ".rb", ".php", ".cs",
        ".html", ".css", ".scss", ".js", ".ts", ".tsx", ".jsx", ".vue", ".sql", ".prisma",
        ".graphql", ".toml", ".env"
    };

    options.manualIncludePaths.clear();
    options.languageProfile = LanguageProfile::All;
    options.maxSingleFileSize = 500U * 1024U;
    options.blacklistedFilenames = {
        "gamecontrollerdb.txt",
        "package-lock.json",
        "words.txt"
    };

    options.followSymlinks = false;
    options.allowUnknownExtensions = true;
    options.unknownTextProbeBytes = 1024U;
    options.unknownTextMaxFileSizeBytes = 10U * 1024U;
    return options;
}

Scanner::Scanner(ScannerOptions options) noexcept
    : options_(std::move(options)) {}

std::optional<ScanReport> Scanner::Scan(
    const std::filesystem::path& rootDirectory,
    std::string& errorMessage) const noexcept {
    errorMessage.clear();

    try {
        std::error_code ec;
        const std::filesystem::path root = std::filesystem::absolute(rootDirectory, ec);
        if (ec) {
            errorMessage = "Root path cannot be resolved: " + DescribeFsError(ec, "unknown error");
            return std::nullopt;
        }

        if (!std::filesystem::exists(root, ec)) {
            errorMessage = "Root directory does not exist.";
            return std::nullopt;
        }

        if (ec) {
            errorMessage = "Failed to inspect root directory: " + DescribeFsError(ec, "unknown error");
            return std::nullopt;
        }

        if (!std::filesystem::is_directory(root, ec) || ec) {
            errorMessage = "Root path is not a readable directory.";
            return std::nullopt;
        }

        ScanReport report;
        report.rootPath = root;

        ScannerOptions effectiveOptions = options_;
        ApplyLanguageProfile(effectiveOptions);
        std::vector<GitignorePattern> gitignorePatterns;
        LoadGitignoreRulesForDirectory(root, root, effectiveOptions, gitignorePatterns);

        auto options = std::filesystem::directory_options::skip_permission_denied;
        if (options_.followSymlinks) {
            options |= std::filesystem::directory_options::follow_directory_symlink;
        }

        std::filesystem::recursive_directory_iterator it(root, options, ec);
        const std::filesystem::recursive_directory_iterator end;
        if (ec) {
            errorMessage = "Directory traversal failed to start: " + DescribeFsError(ec, "unknown error");
            return std::nullopt;
        }

        while (it != end) {
            const std::filesystem::directory_entry entry = *it;
            const std::filesystem::path absolutePath = entry.path();
            const std::filesystem::path relativePath = SafeRelative(root, absolutePath);

            std::error_code entryEc;

            if (!options_.followSymlinks && entry.is_symlink(entryEc)) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::SymlinkSkipped,
                    .details = "Symlink skipped by policy"
                });

                std::error_code dirEc;
                if (entry.is_directory(dirEc) && !dirEc) {
                    it.disable_recursion_pending();
                }

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    report.skippedItems.push_back(SkippedItem {
                        .absolutePath = absolutePath,
                        .relativePath = relativePath,
                        .reason = SkipReason::FilesystemError,
                        .details = DescribeFsError(incEc, "iterator increment error")
                    });
                    it = end;
                }
                continue;
            }

            if (entryEc) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::FilesystemError,
                    .details = DescribeFsError(entryEc, "entry error")
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            std::error_code dirEc;
            if (entry.is_directory(dirEc)) {
                const std::string directoryName = ToLowerAscii(absolutePath.filename().string());
                const std::string relativePathLower = ToLowerAscii(relativePath.generic_string());
                const bool gitignoreExcluded = EvaluateGitignore(
                    relativePathLower,
                    directoryName,
                    true,
                    gitignorePatterns);

                if (ShouldSkipDirectoryWithOptions(directoryName, effectiveOptions) || gitignoreExcluded) {
                    report.skippedItems.push_back(SkippedItem {
                        .absolutePath = absolutePath,
                        .relativePath = relativePath,
                        .reason = SkipReason::ExcludedDirectory,
                        .details = gitignoreExcluded
                            ? "Directory excluded by .gitignore"
                            : "Directory excluded by scanner options"
                    });
                    it.disable_recursion_pending();
                } else {
                    LoadGitignoreRulesForDirectory(
                        root,
                        absolutePath,
                        effectiveOptions,
                        gitignorePatterns);
                }

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    report.skippedItems.push_back(SkippedItem {
                        .absolutePath = absolutePath,
                        .relativePath = relativePath,
                        .reason = SkipReason::PermissionDenied,
                        .details = DescribeFsError(incEc, "permission denied or traversal error")
                    });
                    it = end;
                }
                continue;
            }

            if (dirEc) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::FilesystemError,
                    .details = DescribeFsError(dirEc, "failed to inspect entry type")
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            std::error_code fileTypeEc;
            if (!entry.is_regular_file(fileTypeEc) || fileTypeEc) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::NotRegularFile,
                    .details = fileTypeEc ? DescribeFsError(fileTypeEc, "non-regular file") : "Non-regular file skipped"
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            const std::string extension = DetectEffectiveExtension(absolutePath);
            const bool manuallyIncluded =
                effectiveOptions.manualIncludePaths.contains(absolutePath) ||
                effectiveOptions.manualIncludePaths.contains(NormalizePath(absolutePath));

            if (effectiveOptions.excludedExtensions.contains(extension) && !manuallyIncluded) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::ExcludedExtension,
                    .details = "Extension excluded by scanner options/.gitignore"
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            const std::string relativePathLower = ToLowerAscii(relativePath.generic_string());
            const std::string fileNameLower = ToLowerAscii(absolutePath.filename().string());
            const bool gitignoreExcluded = EvaluateGitignore(
                relativePathLower,
                fileNameLower,
                false,
                gitignorePatterns);

            if (gitignoreExcluded && !manuallyIncluded) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::ExcludedExtension,
                    .details = "File excluded by .gitignore"
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            const std::string fileName = ToLowerAscii(absolutePath.filename().string());
            if (effectiveOptions.blacklistedFilenames.contains(fileName)) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::BlacklistedFilename,
                    .details = "Blacklisted by name"
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            if (!extension.empty()) {
                ++report.extensionCounts[extension];
            }

            std::error_code sizeEc;
            const std::uintmax_t fileSize = std::filesystem::file_size(absolutePath, sizeEc);
            if (sizeEc) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::FilesystemError,
                    .details = DescribeFsError(sizeEc, "failed to read file size")
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            if (!manuallyIncluded && fileSize > effectiveOptions.maxSingleFileSize) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::TooLarge,
                    .details = "File too large"
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            bool includedAsUnknownText = false;
            if (ShouldSkipExtensionWithOptions(extension, effectiveOptions) && !manuallyIncluded) {
                const bool allowAsUnknownText =
                    effectiveOptions.allowUnknownExtensions &&
                    fileSize < effectiveOptions.unknownTextMaxFileSizeBytes &&
                    IsLikelyTextFile(absolutePath, effectiveOptions.unknownTextProbeBytes);

                if (!allowAsUnknownText) {
                    report.skippedItems.push_back(SkippedItem {
                        .absolutePath = absolutePath,
                        .relativePath = relativePath,
                        .reason = SkipReason::ExcludedExtension,
                        .details = "Extension not in allowed extensions and text heuristic failed"
                    });

                    std::error_code incEc;
                    it.increment(incEc);
                    if (incEc) {
                        it = end;
                    }
                    continue;
                }

                includedAsUnknownText = true;
            }

            report.includedFiles.push_back(ScannedFile {
                .absolutePath = absolutePath,
                .relativePath = relativePath,
                .sizeBytes = fileSize,
                .includeCategory = includedAsUnknownText ? "Onemli Yapilandirma Dosyasi" : "Standard"
            });
            report.totalIncludedBytes += fileSize;

            std::error_code incEc;
            it.increment(incEc);
            if (incEc) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::PermissionDenied,
                    .details = DescribeFsError(incEc, "permission denied or traversal error")
                });
                it = end;
            }
        }

        report.detectedProfile = DetectProfileFromExtensionCounts(report.extensionCounts);

        return report;
    } catch (const std::filesystem::filesystem_error& ex) {
        errorMessage = std::string("Filesystem failure during scan: ") + ex.what();
        return std::nullopt;
    } catch (const std::exception& ex) {
        errorMessage = std::string("Unexpected scan failure: ") + ex.what();
        return std::nullopt;
    } catch (...) {
        errorMessage = "Unknown scan failure.";
        return std::nullopt;
    }
}

bool Scanner::ShouldSkipDirectory(std::string_view directoryName) const noexcept {
    return ShouldSkipDirectoryWithOptions(directoryName, options_);
}

bool Scanner::ShouldSkipExtension(std::string_view extension) const noexcept {
    return ShouldSkipExtensionWithOptions(extension, options_);
}

const ScannerOptions& Scanner::options() const noexcept {
    return options_;
}

} // namespace AIPackager::Core
