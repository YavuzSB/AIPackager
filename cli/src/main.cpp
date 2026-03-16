#include "core/Packager.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;

struct CliOptions {
    fs::path inputDir{"."};
    fs::path outputDir{"ai_export"};
    std::size_t maxChunkBytes{400U * 1024U};
    bool showHelp{false};
};

void PrintUsage(std::string_view programName) {
    std::cout
        << "Usage: " << programName << " [--dir <path>] [--out <path>] [--max-size <bytes>]\n"
        << "\n"
        << "Options:\n"
        << "  --dir       Target directory to scan (default: .)\n"
        << "  --out       Output directory for generated files (default: ai_export)\n"
        << "  --max-size  Max chunk size in bytes (default: 409600)\n"
        << "  --help      Show this help message\n";
}

std::optional<std::size_t> ParsePositiveSize(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }

    std::size_t parsedValue = 0;
    try {
        const auto pos = static_cast<std::size_t>(std::stoull(std::string(value)));
        if (pos == 0U) {
            return std::nullopt;
        }
        parsedValue = pos;
    } catch (...) {
        return std::nullopt;
    }

    return parsedValue;
}

bool ParseArgs(int argc, char** argv, CliOptions& options, std::string& errorMessage) {
    errorMessage.clear();

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help") {
            options.showHelp = true;
            continue;
        }

        if (arg == "--dir" || arg == "--out" || arg == "--max-size") {
            if (i + 1 >= argc) {
                errorMessage = "Missing value for argument: " + std::string(arg);
                return false;
            }

            const std::string_view value = argv[++i];
            if (arg == "--dir") {
                options.inputDir = fs::path(value);
            } else if (arg == "--out") {
                options.outputDir = fs::path(value);
            } else {
                const auto parsed = ParsePositiveSize(value);
                if (!parsed.has_value()) {
                    errorMessage = "Invalid --max-size value: " + std::string(value);
                    return false;
                }
                options.maxChunkBytes = *parsed;
            }

            continue;
        }

        errorMessage = "Unknown argument: " + std::string(arg);
        return false;
    }

    return true;
}

bool WriteTextFile(const fs::path& path, std::string_view content, std::string& errorMessage) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        errorMessage = "Unable to open file for writing: " + path.string();
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output.good()) {
        errorMessage = "Failed to write file: " + path.string();
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char** argv) {
    CliOptions options;
    std::string parseError;

    if (!ParseArgs(argc, argv, options, parseError)) {
        std::cerr << "Argument error: " << parseError << "\n\n";
        PrintUsage(argc > 0 ? argv[0] : "aipackager-cli");
        return 1;
    }

    if (options.showHelp) {
        PrintUsage(argc > 0 ? argv[0] : "aipackager-cli");
        return 0;
    }

    AIPackager::Core::PackagerOptions packagerOptions;
    packagerOptions.chunkSizeBytes = options.maxChunkBytes;

    AIPackager::Core::Packager packager {
        AIPackager::Core::Scanner {},
        AIPackager::Core::IndexBuilder {},
        packagerOptions};

    std::string buildError;
    auto packageResult = packager.Build(options.inputDir, buildError);
    if (!packageResult.has_value()) {
        std::cerr << "Packaging failed: " << buildError << "\n";
        return 2;
    }

    std::error_code mkdirError;
    fs::create_directories(options.outputDir, mkdirError);
    if (mkdirError) {
        std::cerr << "Failed to create output directory: " << options.outputDir << "\n";
        std::cerr << "Reason: " << mkdirError.message() << "\n";
        return 3;
    }

    std::string ioError;
    const fs::path indexPath = options.outputDir / "INDEX.txt";
    if (!WriteTextFile(indexPath, packageResult->indexContent, ioError)) {
        std::cerr << ioError << "\n";
        return 4;
    }

    const std::size_t chunkCount = packageResult->chunks.size();
    const std::size_t width = std::max<std::size_t>(2U, std::to_string(chunkCount).size());

    for (std::size_t i = 0; i < chunkCount; ++i) {
        std::ostringstream fileName;
        fileName << "SOURCE_part" << std::setfill('0') << std::setw(static_cast<int>(width)) << (i + 1) << ".txt";

        const fs::path chunkPath = options.outputDir / fileName.str();
        if (!WriteTextFile(chunkPath, packageResult->chunks[i], ioError)) {
            std::cerr << ioError << "\n";
            return 5;
        }
    }

    std::cout << "Packaging completed successfully.\n";
    std::cout << "Output directory: " << options.outputDir << "\n";
    std::cout << "INDEX file: " << indexPath << "\n";
    std::cout << "Chunk files: " << chunkCount << "\n";

    return 0;
}
