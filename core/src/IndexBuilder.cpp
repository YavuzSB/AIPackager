#include "core/IndexBuilder.hpp"

#include <algorithm>
#include <sstream>

namespace AIPackager::Core {
namespace {

[[nodiscard]] std::string GenericPath(const std::filesystem::path& path) {
    return path.generic_string();
}

[[nodiscard]] std::size_t PathDepth(const std::filesystem::path& path) {
    return static_cast<std::size_t>(std::ranges::count(path.generic_string(), '/'));
}

void AppendLine(std::string& out, const std::string& line, const std::string& lineEnding) {
    out.append(line);
    out.append(lineEnding);
}

[[nodiscard]] std::string ProfileName(LanguageProfile profile) {
    switch (profile) {
    case LanguageProfile::All:
        return "All";
    case LanguageProfile::Cpp:
        return "C++";
    case LanguageProfile::Web:
        return "Web/Node.js";
    case LanguageProfile::Rust:
        return "Rust";
    case LanguageProfile::Mobile:
        return "Mobile (Flutter/RN)";
    case LanguageProfile::Java:
        return "Java";
    default:
        return "All";
    }
}

} // namespace

IndexBuilder::IndexBuilder(IndexBuildOptions options) noexcept
    : options_(std::move(options)) {}

std::string IndexBuilder::Build(const ScanReport& report) const {
    std::vector<ScannedFile> included = report.includedFiles;
    std::vector<SkippedItem> skipped = report.skippedItems;

    if (options_.sortEntriesLexicographically) {
        std::ranges::sort(included, [](const ScannedFile& lhs, const ScannedFile& rhs) {
            return lhs.relativePath.generic_string() < rhs.relativePath.generic_string();
        });

        std::ranges::sort(skipped, [](const SkippedItem& lhs, const SkippedItem& rhs) {
            return lhs.relativePath.generic_string() < rhs.relativePath.generic_string();
        });
    }

    std::string out;
    out.reserve(4096);

    AppendLine(out, "# INDEX", options_.lineEnding);
    AppendLine(out, "", options_.lineEnding);
    AppendLine(out, "Root: " + GenericPath(report.rootPath), options_.lineEnding);
    AppendLine(out, "", options_.lineEnding);

    if (options_.includeSummarySection) {
        AppendLine(out, "## Summary", options_.lineEnding);
        AppendLine(out, "- Included files: " + std::to_string(included.size()), options_.lineEnding);
        AppendLine(out, "- Skipped items: " + std::to_string(skipped.size()), options_.lineEnding);
        AppendLine(out, "- Total included bytes: " + std::to_string(report.totalIncludedBytes), options_.lineEnding);
        AppendLine(out, "- Detected profile: " + ProfileName(report.detectedProfile), options_.lineEnding);
        AppendLine(out, "", options_.lineEnding);
    }

    if (options_.includeIncludedFilesSection) {
        AppendLine(out, "## Included Files (Tree-like)", options_.lineEnding);

        if (included.empty()) {
            AppendLine(out, "- (none)", options_.lineEnding);
        } else {
            for (const auto& item : included) {
                const std::size_t depth = PathDepth(item.relativePath);
                std::string line(depth * 2U, ' ');
                line += "- ";
                line += GenericPath(item.relativePath);
                line += " (";
                line += std::to_string(item.sizeBytes);
                line += " B)";
                AppendLine(out, line, options_.lineEnding);
            }
        }

        AppendLine(out, "", options_.lineEnding);
    }

    if (options_.includeLargeFilesSection) {
        AppendLine(out, "## Large Files (Top 10)", options_.lineEnding);

        if (included.empty()) {
            AppendLine(out, "- (none)", options_.lineEnding);
        } else {
            std::vector<ScannedFile> bySize = included;
            std::ranges::sort(bySize, [](const ScannedFile& lhs, const ScannedFile& rhs) {
                return lhs.sizeBytes > rhs.sizeBytes;
            });

            const std::size_t count = std::min<std::size_t>(10U, bySize.size());
            for (std::size_t i = 0; i < count; ++i) {
                std::string line = "- ";
                line += GenericPath(bySize[i].relativePath);
                line += " (";
                line += std::to_string(bySize[i].sizeBytes);
                line += " B)";
                AppendLine(out, line, options_.lineEnding);
            }
        }

        AppendLine(out, "", options_.lineEnding);
    }

    if (options_.includeSkippedItemsSection) {
        AppendLine(out, "## Skipped Items", options_.lineEnding);

        if (skipped.empty()) {
            AppendLine(out, "- (none)", options_.lineEnding);
        } else {
            for (const auto& item : skipped) {
                std::string line = "- ";
                line += GenericPath(item.relativePath);
                line += " | reason=";
                line += ToString(item.reason);
                if (!item.details.empty()) {
                    line += " | details=";
                    line += item.details;
                }
                AppendLine(out, line, options_.lineEnding);
            }
        }

        AppendLine(out, "", options_.lineEnding);
    }

    return out;
}

std::string IndexBuilder::ToString(SkipReason reason) {
    switch (reason) {
    case SkipReason::ExcludedDirectory:
        return "ExcludedDirectory";
    case SkipReason::ExcludedExtension:
        return "ExcludedExtension";
    case SkipReason::BlacklistedFilename:
        return "BlacklistedFilename";
    case SkipReason::TooLarge:
        return "TooLarge";
    case SkipReason::BinaryHeuristic:
        return "BinaryHeuristic";
    case SkipReason::PermissionDenied:
        return "PermissionDenied";
    case SkipReason::NotRegularFile:
        return "NotRegularFile";
    case SkipReason::SymlinkSkipped:
        return "SymlinkSkipped";
    case SkipReason::FilesystemError:
        return "FilesystemError";
    default:
        return "Unknown";
    }
}

const IndexBuildOptions& IndexBuilder::options() const noexcept {
    return options_;
}

} // namespace AIPackager::Core
