# 🚀 AIPackager

![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)
![Build Status](https://img.shields.io/github/actions/workflow/status/yavuzselim/AIPackager/build.yml)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**AIPackager** is a high-performance tool designed to optimize large software projects for AI assistant context limits (e.g., NotebookLM, Gemini, Claude). [cite_start]It intelligently scans your project, filters unnecessary files, and packages the content into manageable 400 KB chunks. [cite: 9, 20]

---

## ✨ Key Features

* [cite_start]**Smart Filtering:** Automatically excludes directories like `.git`, `node_modules`, and `build` [cite: 12, 23, 153][cite_start], alongside binary/media files[cite: 154].
* [cite_start]**400 KB Chunking:** Splits project content into precise 400 KB parts [cite: 60, 406] [cite_start]while maintaining line and word integrity using safe split heuristics [cite: 111-118, 489-496].
* [cite_start]**Automatic Indexing:** Generates a comprehensive `INDEX.txt` that showcases the included file tree and lists skipped items[cite: 1, 15, 26, 79, 81].
* [cite_start]**Modern GUI:** A sleek desktop interface powered by **Dear ImGui** and **GLFW** [cite: 19, 316, 653][cite_start], featuring full Drag & Drop support and a professional dark theme [cite: 659-666, 705-728].
* [cite_start]**Powerful CLI:** A dedicated command-line interface for advanced users and automation workflows[cite: 8, 365, 366].
* [cite_start]**Native Cross-Platform:** Built to run natively on Windows, Linux, and macOS with robust path handling [cite: 139-141, 572-574].

---

## 📦 Download & Installation

No installation required. Download the pre-compiled binary for your operating system from the **[Releases](https://github.com/yavuzsb/AIPackager/releases)** page.

* **Windows:** `aipackager-gui-windows.exe`
* **Linux:** `aipackager-gui-ubuntu` (Wayland & X11 compatible)
* **macOS:** `aipackager-gui-macos`

---

## 🛠️ Usage

### Desktop (GUI)
1. Launch the application.
2. [cite_start]**Drag and drop** your project folder into the window[cite: 691, 696].
3. [cite_start]The tool automatically creates an `ai_export` folder within your target directory[cite: 696, 708].
4. [cite_start]Copy any chunk directly to your clipboard using the "Copy to Clipboard" buttons[cite: 359, 725].

### Terminal (CLI)
```bash
# Basic usage example
./aipackager-cli --dir ./my_project --out ./export_folder --max-size 400000
