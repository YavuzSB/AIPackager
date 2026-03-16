#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace AIPackager::Core {

/**
 * @brief Owns and builds bounded text chunks for downstream AI ingestion.
 */
class ChunkManager final {
public:
    static constexpr std::size_t kDefaultChunkBytes = 400U * 1024U;

    /**
     * @brief Aggregate counters for packaged chunk data.
     */
    struct Statistics {
        std::size_t chunkCount {0};
        std::size_t maxChunkBytes {0};
        std::size_t totalBytes {0};
    };

    /**
     * @brief Creates an empty chunk manager.
     * @param maxChunkBytes Hard upper bound for each chunk in bytes.
     */
    explicit ChunkManager(std::size_t maxChunkBytes = kDefaultChunkBytes) noexcept;

    /**
     * @brief Resets all buffered chunks and counters.
     */
    void Reset() noexcept;

    /**
     * @brief Appends text and auto-splits data to keep chunk size constraints.
     * @param content Arbitrary text payload to append.
     */
    void Append(std::string_view content);

    /**
     * @brief Appends a full line and enforces a trailing newline if missing.
     * @param line Input line without ownership transfer.
     */
    void AppendLine(std::string_view line);

    /**
     * @brief Returns immutable access to all generated chunks.
     * @return Chunk list where each element is <= maxChunkBytes().
     */
    [[nodiscard]] const std::vector<std::string>& Chunks() const noexcept;

    /**
     * @brief Moves out all chunks and clears internal storage.
     * @return Owned chunk container.
     */
    [[nodiscard]] std::vector<std::string> TakeChunks() noexcept;

    /**
     * @brief Reads max chunk size configured for this manager.
     * @return Byte upper bound for individual chunks.
     */
    [[nodiscard]] std::size_t maxChunkBytes() const noexcept;

    /**
     * @brief Calculates runtime statistics for current chunks.
     * @return Statistics snapshot.
     */
    [[nodiscard]] Statistics GetStatistics() const noexcept;

private:
    std::size_t maxChunkBytes_;
    std::vector<std::string> chunks_;
};

} // namespace AIPackager::Core
