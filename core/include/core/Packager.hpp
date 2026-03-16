#pragma once

#include "core/ChunkManager.hpp"
#include "core/IndexBuilder.hpp"
#include "core/Scanner.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AIPackager::Core {

/**
 * @brief Controls file boundary formatting and chunking defaults.
 */
struct PackagerOptions {
    std::size_t chunkSizeBytes {ChunkManager::kDefaultChunkBytes};
    std::string fileHeaderPrefix {"==== FILE: "};
    std::string fileHeaderSuffix {" ====\n"};
    std::string fileFooterPrefix {"==== END FILE: "};
    std::string fileFooterSuffix {" ====\n"};
    bool includeIndexAsFirstChunk {true};
};

/**
 * @brief End-to-end output of a packaging request.
 */
struct PackageResult {
    ScanReport scanReport;
    std::string indexContent;
    std::vector<std::string> chunks;
};

/**
 * @brief Orchestrates scanning, index generation, file framing, and chunking.
 */
class Packager final {
public:
    /**
     * @brief Constructs the orchestrator with explicit dependencies.
     * @param scanner Scanner instance used for traversal and filtering.
     * @param indexBuilder Index builder used to generate INDEX.txt payload.
     * @param options Packaging and framing options.
     */
    explicit Packager(
        Scanner scanner = Scanner {},
        IndexBuilder indexBuilder = IndexBuilder {},
        PackagerOptions options = {});

    /**
     * @brief Builds a full package from a project root.
     * @param rootDirectory Root directory to package.
     * @param errorMessage Output text for fatal orchestration failures.
     * @return Full package result, or std::nullopt on fatal failure.
     */
    [[nodiscard]] std::optional<PackageResult> Build(
        const std::filesystem::path& rootDirectory,
        std::string& errorMessage) const;

    /**
     * @brief Creates a file start delimiter line for a relative path.
     * @param relativePath Path relative to scan root.
     * @param options Active formatting options.
     * @return Formatted header delimiter.
     */
    [[nodiscard]] static std::string MakeFileHeader(
        std::string_view relativePath,
        const PackagerOptions& options);

    /**
     * @brief Creates a file end delimiter line for a relative path.
     * @param relativePath Path relative to scan root.
     * @param options Active formatting options.
     * @return Formatted footer delimiter.
     */
    [[nodiscard]] static std::string MakeFileFooter(
        std::string_view relativePath,
        const PackagerOptions& options);

    /**
     * @brief Reads active packager options.
     * @return Immutable options reference.
     */
    [[nodiscard]] const PackagerOptions& options() const noexcept;

private:
    Scanner scanner_;
    IndexBuilder indexBuilder_;
    PackagerOptions options_;
};

} // namespace AIPackager::Core
