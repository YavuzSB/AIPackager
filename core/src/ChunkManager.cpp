#include "core/ChunkManager.hpp"

#include <algorithm>
#include <utility>

namespace AIPackager::Core {
namespace {

[[nodiscard]] std::size_t FindSafeSplitPoint(std::string_view text, std::size_t maxBytes) noexcept {
    if (text.empty()) {
        return 0;
    }

    if (text.size() <= maxBytes) {
        return text.size();
    }

    const std::size_t minSearch = maxBytes > 32 ? (maxBytes * 3U) / 4U : 0U;

    for (std::size_t i = maxBytes; i > minSearch; --i) {
        const char ch = text[i - 1U];
        if (ch == '\n') {
            return i;
        }
    }

    for (std::size_t i = maxBytes; i > minSearch; --i) {
        const char ch = text[i - 1U];
        if (ch == ' ' || ch == '\t') {
            return i;
        }
    }

    return maxBytes == 0 ? 1U : maxBytes;
}

} // namespace

ChunkManager::ChunkManager(std::size_t maxChunkBytes) noexcept
    : maxChunkBytes_(maxChunkBytes == 0 ? kDefaultChunkBytes : maxChunkBytes) {}

void ChunkManager::Reset() noexcept {
    chunks_.clear();
}

void ChunkManager::Append(std::string_view content) {
    if (content.empty()) {
        return;
    }

    std::size_t offset = 0;
    while (offset < content.size()) {
        if (chunks_.empty()) {
            chunks_.emplace_back();
        }

        std::string& current = chunks_.back();
        const std::size_t used = current.size();
        const std::size_t freeSpace = used < maxChunkBytes_ ? (maxChunkBytes_ - used) : 0U;

        if (freeSpace == 0U) {
            chunks_.emplace_back();
            continue;
        }

        const std::string_view remaining = content.substr(offset);
        if (remaining.size() <= freeSpace) {
            current.append(remaining.data(), remaining.size());
            break;
        }

        const std::size_t take = std::max<std::size_t>(1U, FindSafeSplitPoint(remaining, freeSpace));
        current.append(remaining.data(), take);
        offset += take;

        if (offset < content.size()) {
            chunks_.emplace_back();
        }
    }
}

void ChunkManager::AppendLine(std::string_view line) {
    Append(line);
    if (line.empty() || !line.ends_with('\n')) {
        Append("\n");
    }
}

const std::vector<std::string>& ChunkManager::Chunks() const noexcept {
    return chunks_;
}

std::vector<std::string> ChunkManager::TakeChunks() noexcept {
    auto out = std::move(chunks_);
    chunks_.clear();
    return out;
}

std::size_t ChunkManager::maxChunkBytes() const noexcept {
    return maxChunkBytes_;
}

ChunkManager::Statistics ChunkManager::GetStatistics() const noexcept {
    Statistics stats {};
    stats.chunkCount = chunks_.size();

    for (const auto& chunk : chunks_) {
        stats.totalBytes += chunk.size();
        stats.maxChunkBytes = std::max(stats.maxChunkBytes, chunk.size());
    }

    return stats;
}

} // namespace AIPackager::Core
