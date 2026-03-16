#pragma once

#include "core/Scanner.hpp"

#include <string>

namespace AIPackager::Core {

/**
 * @brief Formatting controls for generated INDEX.txt content.
 */
struct IndexBuildOptions {
    bool includeSummarySection {true};
    bool includeIncludedFilesSection {true};
    bool includeLargeFilesSection {true};
    bool includeSkippedItemsSection {true};
    bool sortEntriesLexicographically {true};
    std::string lineEnding {"\n"};
};

/**
 * @brief Creates a formatted INDEX.txt payload from scanner output.
 */
class IndexBuilder final {
public:
    /**
     * @brief Creates an index builder with formatting options.
     * @param options Output formatting and section toggles.
     */
    explicit IndexBuilder(IndexBuildOptions options = {}) noexcept;

    /**
     * @brief Builds INDEX.txt content from a scan report.
     * @param report Included and skipped scan entries.
     * @return Fully formatted index text.
     */
    [[nodiscard]] std::string Build(const ScanReport& report) const;

    /**
     * @brief Converts a skip reason enum to human-readable text.
     * @param reason Skip reason value.
     * @return Stable reason label.
     */
    [[nodiscard]] static std::string ToString(SkipReason reason);

    /**
     * @brief Returns active formatting options.
     * @return Immutable options reference.
     */
    [[nodiscard]] const IndexBuildOptions& options() const noexcept;

private:
    IndexBuildOptions options_;
};

} // namespace AIPackager::Core
