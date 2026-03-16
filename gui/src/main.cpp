#include "core/Packager.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <array>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if __has_include("IconsFontAwesome6.h")
#include "IconsFontAwesome6.h"
#define AIPACKAGER_HAS_FA_ICONS 1
#else
#define AIPACKAGER_HAS_FA_ICONS 0
#define ICON_FA_FOLDER_OPEN "[DIR]"
#define ICON_FA_COPY "[COPY]"
#define ICON_FA_BOX_ARCHIVE "[PACK]"
#define ICON_FA_DOWNLOAD "[DROP]"
#define ICON_FA_GEAR "[CFG]"
#define ICON_FA_CIRCLE_CHECK "[OK]"
#define ICON_FA_TRIANGLE_EXCLAMATION "[WARN]"
#endif

namespace {

namespace fs = std::filesystem;

struct ClipboardItem {
    std::string name;
    std::string content;
};

struct AppState {
    std::string droppedPath;
    std::string outputDirectory {"ai_export"};
    std::array<char, 2048> outputDirectoryBuffer {};
    std::string statusMessage {"Drop a folder to start packaging."};
    bool lastRunSucceeded {false};
    bool isProcessing {false};
    bool exportedToDisk {false};
    std::size_t maxChunkSizeBytes {AIPackager::Core::ChunkManager::kDefaultChunkBytes};
    std::vector<ClipboardItem> generatedItems;
};

void SyncOutputDirectoryBuffer(AppState& state) {
    state.outputDirectoryBuffer.fill('\0');
    const std::size_t maxCopy = state.outputDirectoryBuffer.size() - 1U;
    const std::size_t length = state.outputDirectory.size() < maxCopy ? state.outputDirectory.size() : maxCopy;
    std::memcpy(state.outputDirectoryBuffer.data(), state.outputDirectory.data(), length);
    state.outputDirectoryBuffer[length] = '\0';
}

std::optional<std::filesystem::path> FindExistingPath(const std::vector<std::filesystem::path>& candidates) {
    for (const auto& path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && !ec) {
            return path;
        }
    }
    return std::nullopt;
}

void SetupFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig textCfg;
    textCfg.OversampleH = 3;
    textCfg.OversampleV = 2;
    textCfg.PixelSnapH = false;
    textCfg.RasterizerMultiply = 1.05F;

    const std::vector<std::filesystem::path> textFontCandidates {
        "C:/Windows/Fonts/Inter-Regular.ttf",
        "C:/Windows/Fonts/Roboto-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttc",
        "/usr/share/fonts/truetype/inter/Inter-Regular.ttf",
        "/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };

    ImFont* baseFont = nullptr;
    if (const auto fontPath = FindExistingPath(textFontCandidates); fontPath.has_value()) {
        baseFont = io.Fonts->AddFontFromFileTTF(fontPath->string().c_str(), 18.0F, &textCfg);
    }

    if (baseFont == nullptr) {
        baseFont = io.Fonts->AddFontDefault();
    }

    io.FontDefault = baseFont;

#if AIPACKAGER_HAS_FA_ICONS
    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.OversampleH = 2;
    iconCfg.OversampleV = 2;
    iconCfg.GlyphMinAdvanceX = 16.0F;

    const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    const std::vector<std::filesystem::path> iconFontCandidates {
        "C:/Windows/Fonts/fa-solid-900.ttf",
        "/usr/share/fonts/truetype/font-awesome/fa-solid-900.ttf",
        "/usr/share/fonts/opentype/font-awesome/Font Awesome 6 Free-Solid-900.otf",
        "/Library/Fonts/Font Awesome 6 Free-Solid-900.otf"
    };

    if (const auto iconPath = FindExistingPath(iconFontCandidates); iconPath.has_value()) {
        io.Fonts->AddFontFromFileTTF(iconPath->string().c_str(), 16.0F, &iconCfg, iconRanges);
    }
#endif
}

void ApplyDarkTheme() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 14.0F;
    style.FrameRounding = 10.0F;
    style.GrabRounding = 10.0F;
    style.PopupRounding = 10.0F;
    style.ScrollbarRounding = 12.0F;
    style.TabRounding = 10.0F;
    style.ChildRounding = 12.0F;
    style.PopupRounding = 12.0F;
    style.ItemSpacing = ImVec2(10.0F, 8.0F);
    style.FramePadding = ImVec2(10.0F, 7.0F);
    style.WindowBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.05F, 0.08F, 0.12F, 1.00F);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08F, 0.11F, 0.16F, 1.00F);
    colors[ImGuiCol_PopupBg] = ImVec4(0.09F, 0.12F, 0.17F, 0.98F);
    colors[ImGuiCol_Border] = ImVec4(0.22F, 0.30F, 0.39F, 0.92F);
    colors[ImGuiCol_Text] = ImVec4(0.88F, 0.93F, 0.98F, 1.00F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.54F, 0.62F, 0.71F, 1.00F);
    colors[ImGuiCol_Header] = ImVec4(0.09F, 0.43F, 0.84F, 0.72F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.13F, 0.56F, 0.98F, 0.85F);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.11F, 0.70F, 0.96F, 0.95F);
    colors[ImGuiCol_Button] = ImVec4(0.10F, 0.50F, 0.86F, 0.70F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.16F, 0.67F, 1.00F, 0.92F);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00F, 0.86F, 0.64F, 0.95F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.09F, 0.14F, 0.21F, 1.00F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12F, 0.22F, 0.33F, 1.00F);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.10F, 0.31F, 0.47F, 1.00F);
    colors[ImGuiCol_Tab] = ImVec4(0.08F, 0.24F, 0.39F, 1.00F);
    colors[ImGuiCol_TabHovered] = ImVec4(0.16F, 0.49F, 0.78F, 1.00F);
    colors[ImGuiCol_TabActive] = ImVec4(0.03F, 0.76F, 0.58F, 1.00F);
}

std::string BuildSuccessMessage(std::size_t chunkCount) {
    std::ostringstream oss;
    oss << "Packaging completed successfully. Generated " << chunkCount << " chunk(s).";
    return oss.str();
}

bool WriteTextFile(const fs::path& filePath, std::string_view content, std::string& errorMessage) {
    std::ofstream output(filePath, std::ios::binary);
    if (!output.is_open()) {
        errorMessage = "Unable to open file for writing: " + filePath.string();
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output.good()) {
        errorMessage = "Write failure: " + filePath.string();
        return false;
    }

    return true;
}

bool ExportToDisk(const fs::path& outputDir, const std::vector<ClipboardItem>& generatedItems, std::string& errorMessage) {
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        errorMessage = "Failed to create output directory: " + outputDir.string() + " (" + ec.message() + ")";
        return false;
    }

    for (const auto& item : generatedItems) {
        if (!WriteTextFile(outputDir / item.name, item.content, errorMessage)) {
            return false;
        }
    }

    return true;
}

std::string EscapeSingleQuotes(std::string text) {
    std::string out;
    out.reserve(text.size() + 16);
    for (char ch : text) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

bool OpenInFileManager(const fs::path& pathToOpen, std::string& errorMessage) {
    const std::string p = pathToOpen.string();

#if defined(_WIN32)
    std::string escaped;
    escaped.reserve(p.size() + 8);
    for (char ch : p) {
        if (ch == '"') {
            escaped += '\\';
        }
        escaped.push_back(ch);
    }
    const std::string cmd = "explorer \"" + escaped + "\"";
#elif defined(__APPLE__)
    const std::string cmd = "open '" + EscapeSingleQuotes(p) + "'";
#else
    const std::string cmd = "xdg-open '" + EscapeSingleQuotes(p) + "'";
#endif

    const int result = std::system(cmd.c_str());
    if (result != 0) {
        errorMessage = "Unable to open file manager for: " + p;
        return false;
    }

    return true;
}

void RunPackaging(AppState& state, const fs::path& folderPath) {
    state.isProcessing = true;
    state.lastRunSucceeded = false;
    state.exportedToDisk = false;
    state.statusMessage = "Packaging in progress...";
    state.generatedItems.clear();

    AIPackager::Core::PackagerOptions options;
    options.chunkSizeBytes = state.maxChunkSizeBytes;

    AIPackager::Core::Packager packager {
        AIPackager::Core::Scanner {},
        AIPackager::Core::IndexBuilder {},
        options};

    std::string errorMessage;
    auto result = packager.Build(folderPath, errorMessage);
    if (!result.has_value()) {
        state.statusMessage = "Error: " + errorMessage;
        state.isProcessing = false;
        return;
    }

    state.generatedItems.push_back(ClipboardItem {
        .name = "INDEX.txt",
        .content = result->indexContent
    });

    const std::size_t width = result->chunks.size() > 99U
        ? std::to_string(result->chunks.size()).size()
        : 2U;

    for (std::size_t i = 0; i < result->chunks.size(); ++i) {
        std::ostringstream name;
        name << "SOURCE_part";
        name << std::setfill('0') << std::setw(static_cast<int>(width)) << (i + 1U) << ".txt";

        state.generatedItems.push_back(ClipboardItem {
            .name = name.str(),
            .content = result->chunks[i]
        });
    }

    std::string exportError;
    if (!ExportToDisk(fs::path(state.outputDirectory), state.generatedItems, exportError)) {
        state.statusMessage = "Packaging succeeded, but export failed: " + exportError;
        state.lastRunSucceeded = false;
        state.exportedToDisk = false;
        state.isProcessing = false;
        return;
    }

    state.statusMessage = BuildSuccessMessage(result->chunks.size());
    state.lastRunSucceeded = true;
    state.exportedToDisk = true;
    state.isProcessing = false;
}

void DropCallback(GLFWwindow* window, int pathCount, const char** paths) {
    if (window == nullptr || pathCount <= 0 || paths == nullptr) {
        return;
    }

    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }

    fs::path dropped = fs::path(paths[0]);
    state->droppedPath = dropped.string();

    std::error_code ec;
    if (!fs::exists(dropped, ec) || ec) {
        state->statusMessage = "Error: dropped path does not exist.";
        state->lastRunSucceeded = false;
        state->generatedItems.clear();
        return;
    }

    if (!fs::is_directory(dropped, ec) || ec) {
        state->statusMessage = "Error: please drop a directory, not a file.";
        state->lastRunSucceeded = false;
        state->generatedItems.clear();
        return;
    }

    state->outputDirectory = (dropped / "ai_export").string();
    SyncOutputDirectoryBuffer(*state);

    RunPackaging(*state, dropped);
}

} // namespace

int main() {
    if (!glfwInit()) {
        return 1;
    }

    const char* glslVersion = "#version 130";
#if defined(__APPLE__)
    glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 800, "AI Packager - Desktop", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    SetupFonts();
    ApplyDarkTheme();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
    }

    if (!ImGui_ImplOpenGL3_Init(glslVersion)) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    AppState appState;
    SyncOutputDirectoryBuffer(appState);
    glfwSetWindowUserPointer(window, &appState);
    glfwSetDropCallback(window, DropCallback);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("AIPackager Pro", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize   |
            ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoScrollbar);

        // ── SOL SIDEBAR: Ayarlar + Durum ─────────────────────────────────
        if (ImGui::BeginChild("sidebar", ImVec2(310.0F, 0.0F), true)) {
            ImGui::TextColored(ImVec4(0.20F, 0.70F, 1.00F, 1.00F),
                "%s  SYSTEM CONFIG", ICON_FA_GEAR);
            ImGui::Separator();
            ImGui::Spacing();

            // Bırakılan klasör
            const std::string displayName = appState.droppedPath.empty()
                ? "None"
                : fs::path(appState.droppedPath).filename().string();
            ImGui::Text("Target: %s", displayName.c_str());
            if (!appState.droppedPath.empty()) {
                ImGui::TextDisabled("%s", appState.droppedPath.c_str());
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Output Directory");
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputText("##output_dir",
                    appState.outputDirectoryBuffer.data(),
                    appState.outputDirectoryBuffer.size())) {
                appState.outputDirectory = appState.outputDirectoryBuffer.data();
            }

            ImGui::Spacing();
            if (!appState.droppedPath.empty()) {
                if (ImGui::Button("RE-PROCESS", ImVec2(-1.0F, 36.0F))) {
                    RunPackaging(appState, fs::path(appState.droppedPath));
                }
                if (ImGui::Button("Reset to ai_export", ImVec2(-1.0F, 28.0F))) {
                    appState.outputDirectory =
                        (fs::path(appState.droppedPath) / "ai_export").string();
                    SyncOutputDirectoryBuffer(appState);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Drop Zone
            ImGui::TextColored(ImVec4(0.20F, 0.70F, 1.00F, 1.00F),
                "%s  DROP ZONE", ICON_FA_DOWNLOAD);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39F, 0.80F, 1.00F, 1.00F));
            ImGui::SetWindowFontScale(1.80F);
            ImGui::TextUnformatted(ICON_FA_BOX_ARCHIVE);
            ImGui::SetWindowFontScale(1.00F);
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Drag & drop a project folder here to start packaging instantly.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Durum mesajı
            if (appState.isProcessing) {
                ImGui::TextColored(ImVec4(0.46F, 0.75F, 1.00F, 1.00F), "Processing...");
            } else {
                const ImVec4 statusColor = appState.lastRunSucceeded
                    ? ImVec4(0.34F, 0.82F, 0.46F, 1.00F)
                    : ImVec4(0.94F, 0.49F, 0.42F, 1.00F);
                ImGui::TextColored(statusColor, "%s", appState.statusMessage.c_str());
            }

            if (appState.lastRunSucceeded && appState.exportedToDisk) {
                ImGui::Spacing();
                ImGui::TextWrapped("Export: %s", appState.outputDirectory.c_str());
                if (ImGui::Button(
                        (std::string(ICON_FA_FOLDER_OPEN) + " Open Export Folder").c_str(),
                        ImVec2(-1.0F, 32.0F))) {
                    std::string openError;
                    if (!OpenInFileManager(fs::path(appState.outputDirectory), openError)) {
                        appState.statusMessage = "Error: " + openError;
                        appState.lastRunSucceeded = false;
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // ── SAĞ PANEL: Üretilen dosyalar ─────────────────────────────────
        if (ImGui::BeginChild("main_view", ImVec2(0.0F, 0.0F), false)) {
            ImGui::TextColored(ImVec4(0.20F, 0.70F, 1.00F, 1.00F),
                "%s  GENERATED CHUNKS (%zu)",
                ICON_FA_CIRCLE_CHECK,
                appState.generatedItems.size());
            ImGui::Separator();

            const ImGuiTableFlags tableFlags =
                ImGuiTableFlags_RowBg        |
                ImGuiTableFlags_Borders      |
                ImGuiTableFlags_Resizable    |
                ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("files_table", 3, tableFlags)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Part Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Size",      ImGuiTableColumnFlags_WidthFixed, 90.0F);
                ImGui::TableSetupColumn("Action",    ImGuiTableColumnFlags_WidthFixed, 110.0F);
                ImGui::TableHeadersRow();

                for (std::size_t i = 0; i < appState.generatedItems.size(); ++i) {
                    const ClipboardItem& item = appState.generatedItems[i];
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(item.name.c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%zu KB", item.content.size() / 1024U);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::SmallButton(
                            (std::string(ICON_FA_COPY) + " Copy##" + item.name).c_str())) {
                        ImGui::SetClipboardText(item.content.c_str());
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        ImGui::End();

        ImGui::Render();
        int displayWidth = 0;
        int displayHeight = 0;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.04F, 0.07F, 0.11F, 1.00F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
