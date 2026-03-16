#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace AIPackager::Core {

/**
 * @brief Describes why a file system entry was skipped during scanning.
 */
enum class SkipReason {
    ExcludedDirectory,
    ExcludedExtension,
    BlacklistedFilename,
    TooLarge,
    BinaryHeuristic,
    PermissionDenied,
    NotRegularFile,
    SymlinkSkipped,
    FilesystemError
};

/**
 * @brief Metadata for a file that passed filtering and will be packaged.
 */
struct ScannedFile {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    std::uintmax_t sizeBytes {0};
    std::string includeCategory {"Standard"};
};

/**
 * @brief Metadata for a path that was skipped by filtering or due to an error.
 */
struct SkippedItem {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    SkipReason reason {SkipReason::FilesystemError};
    std::string details;
};

enum class LanguageProfile {
    All,
    Cpp,
    Web,
    Rust,
    Mobile,
    Java
};

/**
 * @brief Immutable scan output used by packaging and index generation.
 */
struct ScanReport {
    std::filesystem::path rootPath;
    std::vector<ScannedFile> includedFiles;
    std::vector<SkippedItem> skippedItems;
    std::map<std::string, std::size_t> extensionCounts;
    LanguageProfile detectedProfile {LanguageProfile::All};
    std::uintmax_t totalIncludedBytes {0};
};

/**
 * @brief Configuration for directory and file filtering.
 */
struct ScannerOptions {
    /**
     * @brief Directory names to skip anywhere in the tree.
     * Example: .git, build, node_modules
     */
    std::unordered_set<std::string> excludedDirectoryNames;

    /**
     * @brief File extensions (lowercase, including dot) to skip.
     * Example: .exe, .dll, .png, .mp4
     */
    std::unordered_set<std::string> excludedExtensions;

    /**
     * @brief File extensions (lowercase, including dot) allowed by default.
     * If a file extension is not in this set, it is skipped unless manually included.
     */
    std::unordered_set<std::string> allowedExtensions;

    /**
     * @brief High-level language profile used to specialize allowed extensions.
     */
    LanguageProfile languageProfile {LanguageProfile::All};

    /**
     * @brief Absolute file paths manually forced-in by UI/user override.
    * This set is checked when a file extension is not in allowedExtensions.
     */
    std::unordered_set<std::filesystem::path> manualIncludePaths;

    /**
     * @brief Maximum allowed single file size in bytes for automatic inclusion.
     * Files larger than this are skipped unless manually included.
     */
    std::uintmax_t maxSingleFileSize {500U * 1024U};

    /**
     * @brief Lowercase file names excluded regardless of extension policy.
     * Example: package-lock.json
     */
    std::unordered_set<std::string> blacklistedFilenames;

    /**
     * @brief If false, symbolic links are ignored.
     */
    bool followSymlinks {false};

    /**
        * @brief If true, unknown extensions may still be included via text heuristic.
     */
    bool allowUnknownExtensions {true};

    /**
     * @brief Number of initial bytes to probe for unknown-text detection.
     */
    std::size_t unknownTextProbeBytes {1024};

    /**
     * @brief Unknown-extension files must be smaller than this value for text heuristic inclusion.
     */
    std::uintmax_t unknownTextMaxFileSizeBytes {10U * 1024U};

    /**
     * @brief Builds production-ready defaults for common source repositories.
     * @return Default options instance.
     */
    [[nodiscard]] static ScannerOptions Default();
};

/**
 * @brief Scans a project tree and returns include/skip results with reasons.
 */
class Scanner final {
public:
    /**
     * @brief Creates a scanner with a filtering policy.
     * @param options Scanning options and exclusion sets.
     */
    explicit Scanner(ScannerOptions options = ScannerOptions::Default()) noexcept;

    /**
     * @brief Scans a root directory recursively.
     * @param rootDirectory Absolute or relative root path to scan.
     * @param errorMessage Output text for fatal validation or traversal errors.
     * @return A populated scan report on success, std::nullopt on fatal errors.
     */
    [[nodiscard]] std::optional<ScanReport> Scan(
        const std::filesystem::path& rootDirectory,
        std::string& errorMessage) const noexcept;

    /**
     * @brief Checks whether a directory name should be excluded by policy.
     * @param directoryName Directory name without parent path context.
     * @return True if the directory should be skipped.
     */
    [[nodiscard]] bool ShouldSkipDirectory(std::string_view directoryName) const noexcept;

    /**
     * @brief Checks whether a file extension is outside the whitelist policy.
     * @param extension File extension including leading dot.
    * @return True if the extension is not in allowedExtensions.
     */
    [[nodiscard]] bool ShouldSkipExtension(std::string_view extension) const noexcept;

    /**
     * @brief Returns scanner options currently in use.
     * @return Immutable options reference.
     */
    [[nodiscard]] const ScannerOptions& options() const noexcept;

private:
    ScannerOptions options_;
};

} // namespace AIPackager::Core
