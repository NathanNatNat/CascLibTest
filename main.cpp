#include <iostream>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <CascLib.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <deque>
#include <cstdarg>
#include <algorithm>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// ============================================================================
// Console Log
// ============================================================================

struct AppConsole {
    std::string buf;
    std::mutex mtx;
    bool scrollToBottom = false;

    void Append(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        buf += msg;
        scrollToBottom = true;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mtx);
        buf.clear();
    }

    void Draw(const char* title, bool* p_open) {
        ImGui::Begin(title, p_open);

        if (ImGui::SmallButton("Clear")) Clear();
        ImGui::Separator();

        ImVec2 avail = ImGui::GetContentRegionAvail();

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(mtx);
            snapshot = buf;
        }

        ImGui::InputTextMultiline("##ConsoleText", snapshot.data(), snapshot.size() + 1,
            avail, ImGuiInputTextFlags_ReadOnly);

        if (scrollToBottom) {
            ImGuiID id = ImGui::GetItemID();
            ImGuiInputTextState* state = ImGui::GetInputTextState(id);
            if (state)
                state->CursorFollow = true;
            scrollToBottom = false;
        }

        ImGui::End();
    }
};

static AppConsole g_console;

// spdlog sink that writes to the in-app console
template<typename Mutex>
class imgui_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        g_console.Append(fmt::to_string(formatted));
    }
    void flush_() override {}
};

using imgui_sink_mt = imgui_sink<std::mutex>;

static void InitLogger() {
    auto console_sink = std::make_shared<imgui_sink_mt>();
    console_sink->set_pattern("[%L] %v");

    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderr_sink->set_pattern("[%H:%M:%S] [%L] %v");

    auto logger = std::make_shared<spdlog::logger>("app",
        spdlog::sinks_init_list{console_sink, stderr_sink});
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(logger);
}

// Printf-style wrapper for spdlog - keeps existing call sites simple
static void LogInfo(const char* fmt, ...) {
    char tmp[4096];
    va_list args; va_start(args, fmt); vsnprintf(tmp, sizeof(tmp), fmt, args); va_end(args);
    spdlog::info(tmp);
}
static void LogWarn(const char* fmt, ...) {
    char tmp[4096];
    va_list args; va_start(args, fmt); vsnprintf(tmp, sizeof(tmp), fmt, args); va_end(args);
    spdlog::warn(tmp);
}
static void LogError(const char* fmt, ...) {
    char tmp[4096];
    va_list args; va_start(args, fmt); vsnprintf(tmp, sizeof(tmp), fmt, args); va_end(args);
    spdlog::error(tmp);
}

// ============================================================================
// Utility
// ============================================================================

static std::string BytesToHex(const BYTE* data, size_t len) {
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", data[i]);
        result += hex;
    }
    return result;
}

// ============================================================================
// CASC State
// ============================================================================

static HANDLE g_hStorage = nullptr;
static bool g_storageOpen = false;
static bool g_keysImported = false;
static std::string g_buildInfo = "";
static std::string g_productCode = "";
static long g_fileCount = 0;
static long g_localFileCount = 0;
static bool g_isOnline = false;

// Async loading
static std::atomic<bool> g_loading{false};
static std::thread g_loadThread;

static bool WINAPI ProgressCallback(void* PtrUserParam, CASC_PROGRESS_MSG msgId, LPCSTR szObject, DWORD current, DWORD total) {
    const char* msgType = "";
    switch (msgId) {
        case CascProgressLoadingFile:                msgType = "Loading file"; break;
        case CascProgressLoadingManifest:            msgType = "Loading manifest"; break;
        case CascProgressDownloadingFile:            msgType = "Downloading"; break;
        case CascProgressLoadingIndexes:             msgType = "Loading indexes"; break;
        case CascProgressDownloadingArchiveIndexes:  msgType = "Downloading indexes"; break;
        default:                                     msgType = "Working"; break;
    }
    if (szObject && total > 0)
        LogInfo("%s: %s (%u/%u)", msgType, szObject, current, total);
    else if (szObject)
        LogInfo("%s: %s", msgType, szObject);
    else if (total > 0)
        LogInfo("%s (%u/%u)", msgType, current, total);
    else
        LogInfo("%s...", msgType);
    return false; // false = don't cancel
}

// Local storage path and detected builds
static char g_wowPath[512] = "E:/Games/BattleNet/World of Warcraft";

struct DetectedBuild {
    std::string product;
    std::string version;
    std::string region;
    std::string displayName;
};
static std::vector<DetectedBuild> g_detectedBuilds;
static int g_selectedBuild = 0;

static void ScanBuilds() {
    g_detectedBuilds.clear();
    std::string buildInfoPath = std::string(g_wowPath) + "/.build.info";
    std::ifstream f(buildInfoPath);
    if (!f.is_open()) {
        LogWarn(" Cannot open: %s", buildInfoPath.c_str());
        return;
    }

    std::string header;
    std::getline(f, header);

    int versionCol = -1, productCol = -1, branchCol = -1, activeCol = -1, col = 0;
    std::istringstream hs(header);
    std::string field;
    while (std::getline(hs, field, '|')) {
        if (field.find("Version") == 0) versionCol = col;
        if (field.find("Product") == 0) productCol = col;
        if (field.find("Branch") == 0) branchCol = col;
        if (field.find("Active") == 0) activeCol = col;
        col++;
    }

    std::string line;
    while (std::getline(f, line)) {
        std::vector<std::string> cols;
        std::istringstream ls(line);
        std::string cell;
        while (std::getline(ls, cell, '|'))
            cols.push_back(cell);

        if (activeCol >= 0 && activeCol < (int)cols.size() && cols[activeCol] != "1")
            continue;

        DetectedBuild b;
        b.region = (branchCol >= 0 && branchCol < (int)cols.size()) ? cols[branchCol] : "?";
        b.product = (productCol >= 0 && productCol < (int)cols.size()) ? cols[productCol] : "?";
        b.version = (versionCol >= 0 && versionCol < (int)cols.size()) ? cols[versionCol] : "?";
        b.displayName = b.product + " " + b.version + " (" + b.region + ")";

        // Avoid duplicates (same product can appear from multiple regions in the file)
        bool duplicate = false;
        for (auto& existing : g_detectedBuilds) {
            if (existing.product == b.product) { duplicate = true; break; }
        }
        if (!duplicate)
            g_detectedBuilds.push_back(b);
    }
    f.close();

    LogInfo(" Scanned %s: found %zu builds.", buildInfoPath.c_str(), g_detectedBuilds.size());
    for (size_t i = 0; i < g_detectedBuilds.size(); i++)
        LogInfo("  [%zu] %s", i, g_detectedBuilds[i].displayName.c_str());
}

// Online storage config
static const char* g_products[] = {
    "wow", "wowt", "wow_beta", "wowxpress",
    "wow_classic", "wow_classic_ptr", "wow_classic_beta",
    "wow_classic_era", "wow_classic_era_ptr",
    "wowz", "wowlivetest", "wowdev",
};
static const char* g_productNames[] = {
    "Retail", "Retail PTR", "Beta", "Xpress",
    "Classic", "Classic PTR", "Classic Beta",
    "Classic Era", "Classic Era PTR",
    "Submission", "Live Test", "Dev",
};
static const int g_productCount = sizeof(g_products) / sizeof(g_products[0]);
static int g_selectedProduct = 0;

static const char* g_regions[] = { "us", "eu", "cn", "kr", "tw" };
static const char* g_regionNames[] = { "US", "EU", "CN", "KR", "TW" };
static const int g_regionCount = sizeof(g_regions) / sizeof(g_regions[0]);
static int g_selectedRegion = 1;

// File existence checks
struct FileCheck {
    DWORD fileDataId;
    char name[64];
    int status; // 0 = not checked, 1 = exists, -1 = not found
    ULONGLONG fileSize;
};

static std::vector<FileCheck> g_fileChecks = {
    { 982459,  "TextureFileData", 0, 0 },
    { 1353545, "AreaTable",       0, 0 },
    { 1349477, "Map",             0, 0 },
    { 1355528, "WMOAreaTable",    0, 0 },
};
static bool g_checksRun = false;

// File search results
struct SearchResult {
    std::string fileName;
    DWORD fileDataId;
    ULONGLONG fileSize;
    bool available;
    CASC_NAME_TYPE nameType;
};
static std::vector<SearchResult> g_searchResults;
static char g_searchMask[512] = "*";
static bool g_searchRunning = false;
static int g_searchMaxResults = 1000;

// File inspector
static char g_inspectPath[512] = "";
static DWORD g_inspectFileDataId = 0;
static bool g_inspectByDataId = false;
static bool g_inspectHasResult = false;
static CASC_FILE_FULL_INFO g_inspectInfo = {};
static ULONGLONG g_inspectSize = 0;
static std::string g_inspectPreview = "";

// Encryption keys
static char g_keyName[32] = "";
static char g_keyValue[64] = "";

// ============================================================================
// CASC Operations
// ============================================================================

static void QueryBuildInfo() {
    g_buildInfo = "";
    g_productCode = "";

    CASC_STORAGE_PRODUCT productInfo = {};
    if (CascGetStorageInfo(g_hStorage, CascStorageProduct, &productInfo, sizeof(productInfo), NULL)) {
        g_productCode = productInfo.szCodeName;
    }

    // Try .build.info for full version string
    std::string buildInfoPath = std::string(g_wowPath) + "/.build.info";
    std::ifstream buildInfoFile(buildInfoPath);
    if (buildInfoFile.is_open()) {
        std::string header;
        std::getline(buildInfoFile, header);

        int versionCol = -1, productCol = -1, col = 0;
        std::istringstream headerStream(header);
        std::string field;
        while (std::getline(headerStream, field, '|')) {
            if (field.find("Version") == 0) versionCol = col;
            if (field.find("Product") == 0) productCol = col;
            col++;
        }

        std::string targetProduct = g_productCode.empty() ? "wow" : g_productCode;
        std::string line;
        while (std::getline(buildInfoFile, line)) {
            std::vector<std::string> cols;
            std::istringstream lineStream(line);
            std::string cell;
            while (std::getline(lineStream, cell, '|'))
                cols.push_back(cell);
            if (productCol >= 0 && productCol < (int)cols.size() && cols[productCol] == targetProduct) {
                if (versionCol >= 0 && versionCol < (int)cols.size())
                    g_buildInfo = cols[versionCol];
                break;
            }
        }
        buildInfoFile.close();
    }

    if (g_buildInfo.empty() && productInfo.BuildNumber > 0)
        g_buildInfo = std::to_string(productInfo.BuildNumber);
    if (g_buildInfo.empty())
        g_buildInfo = "Unknown";
}

static void QueryStorageStats() {
    g_fileCount = 0;
    g_localFileCount = 0;
    CascGetStorageInfo(g_hStorage, CascStorageTotalFileCount, &g_fileCount, sizeof(DWORD), NULL);
    CascGetStorageInfo(g_hStorage, CascStorageLocalFileCount, &g_localFileCount, sizeof(DWORD), NULL);
}

static void FinishStorageOpen(bool online) {
    g_storageOpen = true;
    g_isOnline = online;
    g_keysImported = false;
    g_checksRun = false;

    QueryBuildInfo();
    QueryStorageStats();

    LogInfo("[OK] %s storage opened.", online ? "Online" : "Local");
    LogInfo(" Product: %s | Version: %s", g_productCode.c_str(), g_buildInfo.c_str());
    LogInfo(" Total files: %ld | Local files: %ld", g_fileCount, g_localFileCount);
    g_loading = false;
}

static void JoinLoadThread() {
    if (g_loadThread.joinable())
        g_loadThread.join();
}

static void OpenLocalStorage() {
    if (g_storageOpen) { LogWarn(" Storage already open - close it first."); return; }
    if (g_loading) { LogWarn(" Already loading..."); return; }

    std::string storagePath = g_wowPath;
    if (!g_detectedBuilds.empty() && g_selectedBuild < (int)g_detectedBuilds.size())
        storagePath += "*" + g_detectedBuilds[g_selectedBuild].product;
    else
        storagePath += "*wow";

    LogInfo(" Opening local storage: %s", storagePath.c_str());
    g_loading = true;

    JoinLoadThread();
    g_loadThread = std::thread([storagePath]() {
        CASC_OPEN_STORAGE_ARGS args = {};
        args.Size = sizeof(CASC_OPEN_STORAGE_ARGS);
        args.PfnProgressCallback = ProgressCallback;

        if (!CascOpenStorageEx(storagePath.c_str(), &args, false, &g_hStorage)) {
            LogError(" CascOpenStorage failed (error %u)", GetLastError());
            g_loading = false;
            return;
        }
        FinishStorageOpen(false);
    });
}

static void OpenOnlineStorage() {
    if (g_storageOpen) { LogWarn(" Storage already open - close it first."); return; }
    if (g_loading) { LogWarn(" Already loading..."); return; }

    CreateDirectoryA("D:\\Repositories\\CascLibTest\\cache", NULL);

    std::string params = std::string("D:/Repositories/CascLibTest/cache")
        + "*" + g_products[g_selectedProduct]
        + "*" + g_regions[g_selectedRegion];

    LogInfo(" Opening online storage: %s (%s)...",
                  g_productNames[g_selectedProduct], g_regionNames[g_selectedRegion]);
    LogInfo(" Params: %s", params.c_str());
    g_loading = true;

    JoinLoadThread();
    g_loadThread = std::thread([params]() {
        CASC_OPEN_STORAGE_ARGS args = {};
        args.Size = sizeof(CASC_OPEN_STORAGE_ARGS);
        args.PfnProgressCallback = ProgressCallback;
        args.dwFlags = CASC_FEATURE_ONLINE;

        if (!CascOpenStorageEx(params.c_str(), &args, true, &g_hStorage)) {
            DWORD err = GetLastError();
            LogError(" CascOpenOnlineStorage failed (error %u)", err);
            if (err == 2) LogError(" Product may not exist on CDN or region is invalid.");
            g_loading = false;
            return;
        }
        FinishStorageOpen(true);
    });
}

static void ImportKeys() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }
    if (g_keysImported) { LogWarn(" Keys already imported."); return; }

    const char* keyFilePath = "D:/Repositories/CascLibTest/EncryptionKeys.txt";
    LogInfo(" Importing keys from: %s", keyFilePath);

    if (!CascImportKeysFromFile(g_hStorage, keyFilePath)) {
        LogError(" CascImportKeysFromFile failed (error %u)", GetLastError());
        return;
    }

    g_keysImported = true;
    LogInfo("[OK] Encryption keys imported.");
}

static void AddManualKey() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }

    ULONGLONG keyNameVal = 0;
    if (sscanf(g_keyName, "%llx", &keyNameVal) != 1) {
        LogError(" Invalid key name (expected hex, e.g. 1A2B3C4D5E6F7080)");
        return;
    }

    LogInfo(" Adding key: name=%s value=%s", g_keyName, g_keyValue);
    if (!CascAddStringEncryptionKey(g_hStorage, keyNameVal, g_keyValue)) {
        LogError(" CascAddStringEncryptionKey failed (error %u)", GetLastError());
        return;
    }
    LogInfo("[OK] Key added successfully.");
}

static void CheckMissingKey() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }

    ULONGLONG missingKey = 0;
    if (CascGetNotFoundEncryptionKey(g_hStorage, &missingKey)) {
        LogInfo(" Missing encryption key: %016llX", missingKey);
    } else {
        LogInfo(" No missing encryption keys reported.");
    }
}

static void CloseStorage() {
    if (!g_storageOpen) { LogWarn(" No storage open."); return; }

    CascCloseStorage(g_hStorage);
    g_hStorage = nullptr;
    g_storageOpen = false;
    g_keysImported = false;
    g_isOnline = false;
    g_buildInfo = "";
    g_productCode = "";
    g_fileCount = 0;
    g_localFileCount = 0;
    g_checksRun = false;
    g_searchResults.clear();
    g_inspectHasResult = false;
    LogInfo("[OK] Storage closed.");
}

static void CheckFileExistence() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }

    LogInfo(" Checking %zu files by FileDataID...", g_fileChecks.size());

    int found = 0;
    for (auto& fc : g_fileChecks) {
        HANDLE hFile = nullptr;
        if (CascOpenFile(g_hStorage, CASC_FILE_DATA_ID(fc.fileDataId), 0, CASC_OPEN_BY_FILEID, &hFile)) {
            fc.status = 1;
            found++;
            ULONGLONG size = 0;
            CascGetFileSize64(hFile, &size);
            fc.fileSize = size;
            LogInfo("  [FOUND]     %7u  %-20s  (%llu bytes)", fc.fileDataId, fc.name, size);
            CascCloseFile(hFile);
        } else {
            DWORD err = GetLastError();
            fc.status = -1;
            fc.fileSize = 0;
            LogInfo("  [NOT FOUND] %7u  %-20s  (error %u)", fc.fileDataId, fc.name, err);
        }
    }

    g_checksRun = true;
    LogInfo(" Check complete: %d/%zu found.", found, g_fileChecks.size());
}

static void RunFileSearch() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }

    g_searchResults.clear();
    LogInfo(" Searching: \"%s\" (max %d results)...", g_searchMask, g_searchMaxResults);

    CASC_FIND_DATA findData;
    HANDLE hFind = CascFindFirstFile(g_hStorage, g_searchMask, &findData, NULL);
    if (!hFind) {
        LogWarn(" No files found matching \"%s\".", g_searchMask);
        return;
    }

    int count = 0;
    do {
        SearchResult r;
        r.fileName = findData.szFileName;
        r.fileDataId = findData.dwFileDataId;
        r.fileSize = findData.FileSize;
        r.available = findData.bFileAvailable != 0;
        r.nameType = findData.NameType;
        g_searchResults.push_back(r);
        count++;
    } while (count < g_searchMaxResults && CascFindNextFile(hFind, &findData));

    CascFindClose(hFind);
    LogInfo("[OK] Found %d files.", count);
}

static void InspectFile() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }

    g_inspectHasResult = false;
    g_inspectPreview = "";
    memset(&g_inspectInfo, 0, sizeof(g_inspectInfo));

    HANDLE hFile = nullptr;
    bool opened = false;

    if (g_inspectByDataId) {
        LogInfo(" Inspecting FileDataID: %u", g_inspectFileDataId);
        opened = CascOpenFile(g_hStorage, CASC_FILE_DATA_ID(g_inspectFileDataId), 0, CASC_OPEN_BY_FILEID, &hFile);
    } else {
        LogInfo(" Inspecting path: %s", g_inspectPath);
        opened = CascOpenFile(g_hStorage, g_inspectPath, 0, 0, &hFile);
    }

    if (!opened) {
        LogError(" Failed to open file (error %u)", GetLastError());
        return;
    }

    CascGetFileSize64(hFile, &g_inspectSize);
    CascGetFileInfo(hFile, CascFileFullInfo, &g_inspectInfo, sizeof(g_inspectInfo), NULL);

    LogInfo("[OK] File opened successfully.");
    LogInfo("  CKey:         %s", BytesToHex(g_inspectInfo.CKey, MD5_HASH_SIZE).c_str());
    LogInfo("  EKey:         %s", BytesToHex(g_inspectInfo.EKey, MD5_HASH_SIZE).c_str());
    LogInfo("  FileDataId:   %u", g_inspectInfo.FileDataId);
    LogInfo("  ContentSize:  %llu", g_inspectInfo.ContentSize);
    LogInfo("  EncodedSize:  %llu", g_inspectInfo.EncodedSize);
    LogInfo("  SpanCount:    %u", g_inspectInfo.SpanCount);
    LogInfo("  DataFile:     %s", g_inspectInfo.DataFileName);
    LogInfo("  LocaleFlags:  0x%08X", g_inspectInfo.LocaleFlags);
    LogInfo("  ContentFlags: 0x%08X", g_inspectInfo.ContentFlags);

    // Read preview (first 1KB for text preview)
    char previewBuf[1025];
    DWORD bytesRead = 0;
    if (CascReadFile(hFile, previewBuf, 1024, &bytesRead)) {
        previewBuf[bytesRead] = '\0';
        // Check if it looks like text
        bool isText = true;
        for (DWORD i = 0; i < bytesRead && i < 256; i++) {
            unsigned char c = (unsigned char)previewBuf[i];
            if (c == 0 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
                isText = false;
                break;
            }
        }
        if (isText && bytesRead > 0) {
            g_inspectPreview = std::string(previewBuf, bytesRead);
        } else {
            // Show hex dump
            std::ostringstream hex;
            for (DWORD i = 0; i < bytesRead && i < 256; i++) {
                if (i > 0 && i % 16 == 0) hex << "\n";
                char h[4];
                snprintf(h, sizeof(h), "%02X ", (unsigned char)previewBuf[i]);
                hex << h;
            }
            if (bytesRead > 256) hex << "\n...";
            g_inspectPreview = hex.str();
        }
    }

    g_inspectHasResult = true;
    CascCloseFile(hFile);
}

static void ExtractFile() {
    if (!g_storageOpen) { LogWarn(" Open storage first."); return; }

    HANDLE hFile = nullptr;
    bool opened = false;

    if (g_inspectByDataId) {
        opened = CascOpenFile(g_hStorage, CASC_FILE_DATA_ID(g_inspectFileDataId), 0, CASC_OPEN_BY_FILEID, &hFile);
    } else {
        opened = CascOpenFile(g_hStorage, g_inspectPath, 0, 0, &hFile);
    }

    if (!opened) {
        LogError(" Failed to open file for extraction (error %u)", GetLastError());
        return;
    }

    // Determine output filename
    std::string outName = "extracted_";
    if (g_inspectByDataId) {
        outName += std::to_string(g_inspectFileDataId);
    } else {
        std::string path = g_inspectPath;
        size_t lastSlash = path.find_last_of("/\\");
        outName += (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    }

    std::ofstream outFile(outName, std::ios::binary);
    if (!outFile.is_open()) {
        LogError(" Cannot create output file: %s", outName.c_str());
        CascCloseFile(hFile);
        return;
    }

    char buffer[65536];
    DWORD bytesRead = 0;
    ULONGLONG totalRead = 0;
    while (CascReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        totalRead += bytesRead;
    }

    outFile.close();
    CascCloseFile(hFile);
    LogInfo("[OK] Extracted %llu bytes to: %s", totalRead, outName.c_str());
}

// ============================================================================
// UI Drawing
// ============================================================================

static bool g_firstFrame = true;

static void SetupDefaultLayout(ImGuiID dockspaceId) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dockMain = dockspaceId;
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.35f, nullptr, &dockMain);
    ImGuiID dockRight = dockMain;

    ImGui::DockBuilderDockWindow("Storage", dockLeft);
    ImGui::DockBuilderDockWindow("File Tests", dockRight);
    ImGui::DockBuilderDockWindow("File Search", dockRight);
    ImGui::DockBuilderDockWindow("File Inspector", dockRight);
    ImGui::DockBuilderDockWindow("Console", dockBottom);

    ImGui::DockBuilderFinish(dockspaceId);
}

static void DrawStoragePanel() {
    ImGui::Begin("Storage");

    // Connection status
    if (g_loading) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "LOADING...");
    } else if (g_storageOpen) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "CONNECTED");
        ImGui::SameLine();
        ImGui::Text("(%s)", g_isOnline ? "Online" : "Local");
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DISCONNECTED");
    }

    if (g_storageOpen) {
        ImGui::Text("Product: %s", g_productCode.c_str());
        ImGui::Text("Version: %s", g_buildInfo.c_str());
        ImGui::Text("Total Files: %ld", g_fileCount);
        ImGui::Text("Local Files: %ld", g_localFileCount);
        ImGui::Text("Keys: %s", g_keysImported ? "Imported" : "Not loaded");
    }

    ImGui::Separator();

    // Local storage
    ImGui::Text("Local Storage");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##WowPath", g_wowPath, sizeof(g_wowPath));
    if (ImGui::Button("Scan Builds")) ScanBuilds();
    ImGui::SameLine();

    if (!g_detectedBuilds.empty()) {
        ImGui::SetNextItemWidth(-1);
        auto getter = [](void* data, int idx) -> const char* {
            auto* builds = (std::vector<DetectedBuild>*)data;
            return (*builds)[idx].displayName.c_str();
        };
        ImGui::Combo("##BuildSelect", &g_selectedBuild, getter, &g_detectedBuilds, (int)g_detectedBuilds.size());
    } else {
        ImGui::TextDisabled("(click Scan Builds)");
    }

    if (ImGui::Button("Open Local")) OpenLocalStorage();
    ImGui::SameLine();
    if (ImGui::Button("Close##local")) CloseStorage();

    ImGui::Separator();

    // Online storage
    ImGui::Text("Online");
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##Product", &g_selectedProduct, g_productNames, g_productCount);
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("##Region", &g_selectedRegion, g_regionNames, g_regionCount);
    ImGui::SameLine();
    if (ImGui::Button("Open Online")) OpenOnlineStorage();

    ImGui::Separator();

    // Encryption keys
    ImGui::Text("Encryption Keys");
    if (ImGui::Button("Import from File")) ImportKeys();
    ImGui::SameLine();
    if (ImGui::Button("Check Missing")) CheckMissingKey();

    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##KeyName", "Key Name (hex)", g_keyName, sizeof(g_keyName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##KeyValue", "Key Value (hex)", g_keyValue, sizeof(g_keyValue));
    if (ImGui::Button("Add Key Manually")) AddManualKey();

    ImGui::End();
}

static void DrawFileTestsPanel() {
    ImGui::Begin("File Tests");

    if (ImGui::CollapsingHeader("FileDataID Existence Check", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Run Check")) CheckFileExistence();

        if (g_checksRun && ImGui::BeginTable("FileChecks", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("FileDataID", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            for (auto& fc : g_fileChecks) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%u", fc.fileDataId);
                ImGui::TableNextColumn(); ImGui::Text("%s", fc.name);
                ImGui::TableNextColumn();
                if (fc.status == 1) ImGui::Text("%llu", fc.fileSize);
                else ImGui::TextDisabled("-");
                ImGui::TableNextColumn();
                if (fc.status == 1) ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "FOUND");
                else if (fc.status == -1) ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "NOT FOUND");
                else ImGui::TextDisabled("---");
            }
            ImGui::EndTable();
        }

        // Add new entry
        static DWORD newId = 0;
        static char newName[64] = "";
        ImGui::SetNextItemWidth(100);
        ImGui::InputScalar("##NewID", ImGuiDataType_U32, &newId);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##NewName", newName, sizeof(newName));
        ImGui::SameLine();
        if (ImGui::Button("Add##filecheck")) {
            if (newId > 0) {
                FileCheck fc = {};
                fc.fileDataId = newId;
                strncpy(fc.name, newName[0] ? newName : "Unnamed", sizeof(fc.name) - 1);
                g_fileChecks.push_back(fc);
                LogInfo(" Added check: %u (%s)", newId, fc.name);
                newId = 0; newName[0] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All##filecheck")) {
            g_fileChecks.clear();
            g_checksRun = false;
        }
    }

    ImGui::End();
}

static void DrawFileSearchPanel() {
    ImGui::Begin("File Search");

    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint("##SearchMask", "Search mask (e.g. *.db2, interface/*)", g_searchMask, sizeof(g_searchMask));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Max##search", &g_searchMaxResults);
    if (g_searchMaxResults < 1) g_searchMaxResults = 1;
    if (g_searchMaxResults > 50000) g_searchMaxResults = 50000;
    ImGui::SameLine();
    if (ImGui::Button("Search")) RunFileSearch();

    ImGui::Text("%zu results", g_searchResults.size());
    ImGui::Separator();

    if (!g_searchResults.empty() && ImGui::BeginTable("SearchResults", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable, ImVec2(0, -1))) {

        ImGui::TableSetupColumn("FileDataID", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("File Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Available", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)g_searchResults.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto& r = g_searchResults[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (r.fileDataId != 0xFFFFFFFF) ImGui::Text("%u", r.fileDataId);
                else ImGui::TextDisabled("-");
                ImGui::TableNextColumn(); ImGui::TextUnformatted(r.fileName.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%llu", r.fileSize);
                ImGui::TableNextColumn();
                if (r.available) ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Yes");
                else ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No");
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                if (ImGui::SmallButton("Inspect")) {
                    if (r.fileDataId != 0xFFFFFFFF) {
                        g_inspectFileDataId = r.fileDataId;
                        g_inspectByDataId = true;
                    } else {
                        strncpy(g_inspectPath, r.fileName.c_str(), sizeof(g_inspectPath) - 1);
                        g_inspectByDataId = false;
                    }
                    InspectFile();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

static void DrawFileInspectorPanel() {
    ImGui::Begin("File Inspector");

    ImGui::Checkbox("By FileDataID##inspect", &g_inspectByDataId);
    if (g_inspectByDataId) {
        ImGui::SetNextItemWidth(150);
        ImGui::InputScalar("##InspectFDID", ImGuiDataType_U32, &g_inspectFileDataId);
    } else {
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##InspectPath", g_inspectPath, sizeof(g_inspectPath));
    }
    ImGui::SameLine();
    if (ImGui::Button("Inspect")) InspectFile();
    ImGui::SameLine();
    if (ImGui::Button("Extract")) ExtractFile();

    if (g_inspectHasResult) {
        ImGui::Separator();

        if (ImGui::BeginTable("InspectInfo", 2, ImGuiTableFlags_Borders)) {
            auto row = [](const char* label, const char* fmt, ...) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                char buf[256];
                va_list args; va_start(args, fmt); vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);
                ImGui::TextUnformatted(buf);
            };

            row("FileDataID", "%u", g_inspectInfo.FileDataId);
            row("Content Size", "%llu bytes", g_inspectInfo.ContentSize);
            row("Encoded Size", "%llu bytes", g_inspectInfo.EncodedSize);
            row("CKey", "%s", BytesToHex(g_inspectInfo.CKey, MD5_HASH_SIZE).c_str());
            row("EKey", "%s", BytesToHex(g_inspectInfo.EKey, MD5_HASH_SIZE).c_str());
            row("Data File", "%s", g_inspectInfo.DataFileName);
            row("Span Count", "%u", g_inspectInfo.SpanCount);
            row("Segment Index", "%u", g_inspectInfo.SegmentIndex);
            row("Locale Flags", "0x%08X", g_inspectInfo.LocaleFlags);
            row("Content Flags", "0x%08X", g_inspectInfo.ContentFlags);
            row("Tag Bitmask", "0x%016llX", g_inspectInfo.TagBitMask);

            ImGui::EndTable();
        }

        if (!g_inspectPreview.empty()) {
            ImGui::Separator();
            ImGui::Text("Preview (first 1KB):");
            ImGui::BeginChild("Preview", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
            ImGui::TextUnformatted(g_inspectPreview.c_str());
            ImGui::PopStyleColor();
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return -1;
    }

    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(2560, 1440, "CascLib Test Suite", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to init OpenGL" << std::endl;
        return -1;
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // DPI scaling
    float xscale = 1.0f, yscale = 1.0f;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    float dpiScale = xscale > yscale ? xscale : yscale;
    if (dpiScale < 1.5f) dpiScale = 2.0f;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale);

    float fontSize = 16.0f * dpiScale;
    io.Fonts->AddFontFromFileTTF("external/imgui/misc/fonts/Roboto-Medium.ttf", fontSize);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    InitLogger();

    LogInfo("=== CascLib Test Suite ===");
    LogInfo("Default path: %s", g_wowPath);
    LogInfo("Key file: D:/Repositories/CascLibTest/EncryptionKeys.txt");
    LogInfo("Cache dir: D:/Repositories/CascLibTest/cache");
    LogInfo("");
    ScanBuilds();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (g_firstFrame) {
            SetupDefaultLayout(dockspaceId);
            g_firstFrame = false;
        }

        DrawStoragePanel();
        DrawFileTestsPanel();
        DrawFileSearchPanel();
        DrawFileInspectorPanel();

        bool consoleOpen = true;
        g_console.Draw("Console", &consoleOpen);

        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    JoinLoadThread();
    if (g_storageOpen) CascCloseStorage(g_hStorage);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
