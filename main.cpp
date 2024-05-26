#include <iostream>
#include <CascLib.h>

int main() {
    HANDLE hStorage;
    HANDLE hFile;
    DWORD dwBytesRead;
    char buffer[1024];
    const char *storagePath = "D:/Games/BattleNet/World of Warcraft/data";

    // Open the storage
    if (!CascOpenStorage(storagePath, CASC_LOCALE_ENUS, &hStorage)) {
        std::cerr << "Failed to open storage" << std::endl;
        return 1;
    }

    // Open a file from the storage
    if (!CascOpenFile(hStorage, "Interface\\FrameXML\\Localization.lua", 0, 0, &hFile)) {
        std::cerr << "Failed to open file" << std::endl;
        CascCloseStorage(hStorage);
        return 1;
    }

    // Read from the file
    if (CascReadFile(hFile, buffer, sizeof(buffer) - 1, &dwBytesRead)) {
        buffer[dwBytesRead] = '\0';
        std::cout << "File content: " << buffer << std::endl;
    } else {
        std::cerr << "Failed to read file" << std::endl;
    }

    // Clean up
    CascCloseFile(hFile);
    CascCloseStorage(hStorage);

    return 0;
}
