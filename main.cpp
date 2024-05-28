#include <iostream>
#include <GLFW/glfw3.h>
#include <CascLib.h>

static int ExtractFile(char *szStorageName, char *szStorageFile, char *szFileName);
//ExtractFile(storagePath, "Interface/FrameXML/Localization.lua", "Localization.lua");

int main()
{
    HANDLE hStorage;
    HANDLE hFile;
    DWORD dwBytesRead;
    char buffer[1024];
    const char *storagePath = "D:/Games/BattleNet/World of Warcraft*wow";
    const char *keyFilePath = "C:/Development/Repositories/CascLibTest/EncryptionKeys.txt";

    // Open the CASC storage
    if (!CascOpenStorage(storagePath, 0, &hStorage))
    {
        std::cerr << "Failed to open storage: " << GetLastError() << std::endl;
        return 1;
    }

    long dwFileCount = 0;
    if (CascGetStorageInfo(hStorage, CascStorageTotalFileCount, &dwFileCount, sizeof(DWORD), NULL))
    {
        printf("file count: %d\n", dwFileCount);
    }

    // Import keys from file
    if (!CascImportKeysFromFile(hStorage, keyFilePath))
    {
        std::cerr << "Failed to import keys from file: " << GetLastError() << std::endl;
        CascCloseStorage(hStorage);
        return 1;
    }

    std::cout << "Keys imported successfully!" << std::endl;

    // Open a file from the storage
    if (!CascOpenFile(hStorage, "Interface/FrameXML/Localization.lua", 0, 0, &hFile))
    {
        std::cerr << "Failed to open file, casc error:" << GetLastError() << std::endl;
        CascCloseStorage(hStorage);
        return 1;
    }

    // Read from the file
    if (CascReadFile(hFile, buffer, sizeof(buffer) - 1, &dwBytesRead))
    {
        buffer[dwBytesRead] = '\0';
        std::cout << "File content: " << buffer << std::endl;
    }
    else 
    {
        std::cerr << "Failed to read file" << std::endl;
    }

    // Clean up
    CascCloseFile(hFile);
    CascCloseStorage(hStorage);

    return 0;
}

//-----------------------------------------------------------------------------
// Extracts a file from the storage and saves it to the disk.
//
// Parameters :
//
//   char * szStorageName - Name of the storage local path (optionally with product code name)
//   char * szStorageFile - Name/number of storaged file.
//   char * szFileName    - Name of the target disk file.
static int ExtractFile(char *szStorageName, char *szStorageFile, char *szFileName)
{
    HANDLE hStorage = NULL;        // Open storage handle
    HANDLE hFile = NULL;          // Storage file handle
    HANDLE handle = NULL;          // Disk file handle
    DWORD dwErrCode = ERROR_SUCCESS; // Result value

    // Open a CASC storage, e.g. "c:\Games\StarCraft II".
    // For a multi-product storage, a product code can be appended
    // after the storage local path, e.g. "C:\Games\World of Warcraft:wowt".
    if (dwErrCode == ERROR_SUCCESS)
    {
        if (!CascOpenStorage(szStorageName, 0, &hStorage))
            dwErrCode = GetLastError();
    }

    // Open a file in the storage, e.g. "mods\core.sc2mod\base.sc2assets\assets\sounds\ui_ac_allyfound.wav"
    if (dwErrCode == ERROR_SUCCESS)
    {
        if (!CascOpenFile(hStorage, szStorageFile, 0, 0, &hFile))
            dwErrCode = GetLastError();
    }

    // Create the target file
    if (dwErrCode == ERROR_SUCCESS)
    {
        handle = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (handle == INVALID_HANDLE_VALUE)
            dwErrCode = GetLastError();
    }

    // Read the data from the file
    if (dwErrCode == ERROR_SUCCESS)
    {
        char  szBuffer[0x10000];
        DWORD dwBytes = 1;

        while (dwBytes != 0)
        {
            CascReadFile(hFile, szBuffer, sizeof(szBuffer), &dwBytes);
            if (dwBytes == 0)
                break;

            WriteFile(handle, szBuffer, dwBytes, &dwBytes, NULL);
            if (dwBytes == 0)
                break;
        }
    }

    // Cleanup and exit
    if (handle != NULL)
        CloseHandle(handle);
    if (hFile != NULL)
        CascCloseFile(hFile);
    if (hStorage != NULL)
        CascCloseStorage(hStorage);

    return dwErrCode;
}
