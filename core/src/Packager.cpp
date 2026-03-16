#include "core/Packager.hpp"

#include <algorithm>
#include <array>
#include <fstream>

namespace AIPackager::Core {
namespace {

[[nodiscard]] std::string GenericPath(const std::filesystem::path& path) {
    return path.generic_string();
}

[[nodiscard]] bool StreamFileToChunks(const std::filesystem::path& filePath, ChunkManager& chunkManager) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::array<char, 16 * 1024> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize readCount = input.gcount();
        if (readCount > 0) {
            chunkManager.Append(std::string_view(buffer.data(), static_cast<std::size_t>(readCount)));
        }
    }

    if (!input.eof() && input.fail()) {
        return false;
    }

    return true;
}

} // namespace

Packager::Packager(Scanner scanner, IndexBuilder indexBuilder, PackagerOptions options)
    : scanner_(std::move(scanner)),
      indexBuilder_(std::move(indexBuilder)),
      options_(std::move(options)) {}

std::optional<PackageResult> Packager::Build(
    const std::filesystem::path& rootDirectory,
    std::string& errorMessage) const {
    errorMessage.clear();

    auto scanOpt = scanner_.Scan(rootDirectory, errorMessage);
    if (!scanOpt.has_value()) {
        return std::nullopt;
    }

    PackageResult result;
    result.scanReport = std::move(*scanOpt);
    result.indexContent = indexBuilder_.Build(result.scanReport);

    ChunkManager chunkManager(options_.chunkSizeBytes);

    if (options_.includeIndexAsFirstChunk) {
        chunkManager.Append(MakeFileHeader("INDEX.txt", options_));
        chunkManager.Append(result.indexContent);
        if (!result.indexContent.empty() && !result.indexContent.ends_with('\n')) {
            chunkManager.Append("\n");
        }
        chunkManager.Append(MakeFileFooter("INDEX.txt", options_));
        chunkManager.Append("\n");
    }

    std::vector<ScannedFile> files = result.scanReport.includedFiles;
    std::ranges::sort(files, [](const ScannedFile& lhs, const ScannedFile& rhs) {
        return lhs.relativePath.generic_string() < rhs.relativePath.generic_string();
    });

    for (const auto& file : files) {
        if (!std::filesystem::exists(file.absolutePath)) {
            result.scanReport.skippedItems.push_back(SkippedItem {
                .absolutePath = file.absolutePath,
                .relativePath = file.relativePath,
                .reason = SkipReason::FilesystemError,
                .details = "File disappeared before packaging"
            });
            continue;
        }

        chunkManager.Append(MakeFileHeader(GenericPath(file.relativePath), options_));

        if (!StreamFileToChunks(file.absolutePath, chunkManager)) {
            result.scanReport.skippedItems.push_back(SkippedItem {
                .absolutePath = file.absolutePath,
                .relativePath = file.relativePath,
                .reason = SkipReason::FilesystemError,
                .details = "Unable to read file content during packaging"
            });
            chunkManager.Append("\n[READ_ERROR]\n");
        }

        chunkManager.Append("\n");
        chunkManager.Append(MakeFileFooter(GenericPath(file.relativePath), options_));
        chunkManager.Append("\n");
    }

    result.chunks = chunkManager.TakeChunks();
    return result;
}

std::string Packager::MakeFileHeader(std::string_view relativePath, const PackagerOptions& options) {
    std::string out;
    out.reserve(options.fileHeaderPrefix.size() + relativePath.size() + options.fileHeaderSuffix.size());
    out.append(options.fileHeaderPrefix);
    out.append(relativePath.data(), relativePath.size());
    out.append(options.fileHeaderSuffix);
    return out;
}

std::string Packager::MakeFileFooter(std::string_view relativePath, const PackagerOptions& options) {
    std::string out;
    out.reserve(options.fileFooterPrefix.size() + relativePath.size() + options.fileFooterSuffix.size());
    out.append(options.fileFooterPrefix);
    out.append(relativePath.data(), relativePath.size());
    out.append(options.fileFooterSuffix);
    return out;
}

const PackagerOptions& Packager::options() const noexcept {
    return options_;
}

} // namespace AIPackager::Core
