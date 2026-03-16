#include "core/Scanner.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <system_error>

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

[[nodiscard]] bool IsKnownTextExtension(std::string_view extLower) {
    static constexpr std::array<std::string_view, 30> kTextExtensions {
        ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ixx", ".m", ".mm",
        ".py", ".js", ".ts", ".tsx", ".jsx", ".java", ".kt", ".swift", ".rs", ".go",
        ".md", ".txt", ".json", ".yaml", ".yml", ".toml", ".xml", ".cmake", ".sh"
    };

    return std::ranges::find(kTextExtensions, extLower) != kTextExtensions.end();
}

[[nodiscard]] bool IsLikelyBinaryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::array<unsigned char, 4096> buffer {};
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize readCount = input.gcount();

    if (readCount <= 0) {
        return false;
    }

    std::size_t controlBytes = 0;
    for (std::streamsize i = 0; i < readCount; ++i) {
        const unsigned char c = buffer[static_cast<std::size_t>(i)];
        if (c == 0U) {
            return true;
        }

        const bool isAsciiControl = (c < 32U) && c != '\n' && c != '\r' && c != '\t' && c != '\f';
        if (isAsciiControl) {
            ++controlBytes;
        }
    }

    const double ratio = static_cast<double>(controlBytes) / static_cast<double>(readCount);
    return ratio > 0.30;
}

[[nodiscard]] std::string DescribeFsError(const std::error_code& ec, std::string_view fallback) {
    if (ec) {
        return ec.message();
    }
    return std::string(fallback);
}

} // namespace

ScannerOptions ScannerOptions::Default() {
    ScannerOptions options;

    options.excludedDirectoryNames = {
        ".git", ".svn", ".hg", ".idea", ".vscode", "build", "dist", "out", "bin", "obj",
        "node_modules", ".venv", "venv", "__pycache__", ".cache", ".next", ".nuxt"
    };

    options.excludedExtensions = {
        ".exe", ".dll", ".so", ".dylib", ".a", ".lib", ".obj", ".o", ".class", ".jar",
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".webp", ".svg", ".mp3", ".wav",
        ".flac", ".mp4", ".mov", ".avi", ".mkv", ".zip", ".7z", ".rar", ".tar", ".gz",
        ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".ttf", ".otf", ".woff",
        ".woff2"
    };

    options.followSymlinks = false;
    options.allowUnknownExtensions = true;
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
                if (ShouldSkipDirectory(directoryName)) {
                    report.skippedItems.push_back(SkippedItem {
                        .absolutePath = absolutePath,
                        .relativePath = relativePath,
                        .reason = SkipReason::ExcludedDirectory,
                        .details = "Directory excluded by scanner options"
                    });
                    it.disable_recursion_pending();
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

            const std::string extension = ToLowerAscii(absolutePath.extension().string());
            if (ShouldSkipExtension(extension)) {
                report.skippedItems.push_back(SkippedItem {
                    .absolutePath = absolutePath,
                    .relativePath = relativePath,
                    .reason = SkipReason::ExcludedExtension,
                    .details = "Extension excluded by scanner options"
                });

                std::error_code incEc;
                it.increment(incEc);
                if (incEc) {
                    it = end;
                }
                continue;
            }

            if (!options_.allowUnknownExtensions && !IsKnownTextExtension(extension)) {
                if (IsLikelyBinaryFile(absolutePath)) {
                    report.skippedItems.push_back(SkippedItem {
                        .absolutePath = absolutePath,
                        .relativePath = relativePath,
                        .reason = SkipReason::BinaryHeuristic,
                        .details = "Binary content detected by byte heuristic"
                    });

                    std::error_code incEc;
                    it.increment(incEc);
                    if (incEc) {
                        it = end;
                    }
                    continue;
                }
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

            report.includedFiles.push_back(ScannedFile {
                .absolutePath = absolutePath,
                .relativePath = relativePath,
                .sizeBytes = fileSize
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
    const std::string lowered = ToLowerAscii(directoryName);
    return options_.excludedDirectoryNames.contains(lowered);
}

bool Scanner::ShouldSkipExtension(std::string_view extension) const noexcept {
    const std::string lowered = ToLowerAscii(extension);
    return options_.excludedExtensions.contains(lowered);
}

const ScannerOptions& Scanner::options() const noexcept {
    return options_;
}

} // namespace AIPackager::Core
