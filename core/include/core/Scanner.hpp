#pragma once

#include <cstdint>
#include <filesystem>
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

/**
 * @brief Immutable scan output used by packaging and index generation.
 */
struct ScanReport {
    std::filesystem::path rootPath;
    std::vector<ScannedFile> includedFiles;
    std::vector<SkippedItem> skippedItems;
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
     * @brief If false, symbolic links are ignored.
     */
    bool followSymlinks {false};

    /**
     * @brief If true, unknown extensions are treated as potential text files.
     * If false, simple binary-byte heuristics may skip suspicious files.
     */
    bool allowUnknownExtensions {true};

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
     * @brief Checks whether a file extension should be excluded by policy.
     * @param extension File extension including leading dot.
     * @return True if the extension is excluded.
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
