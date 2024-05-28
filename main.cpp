#include <iostream>
#include <glad/gl.h>
// GLFW (include after glad)
#include <GLFW/glfw3.h>
#include <CascLib.h>

static int ExtractFile(char *szStorageName, char *szStorageFile, char *szFileName);
//ExtractFile(storagePath, "Interface/FrameXML/Localization.lua", "Localization.lua");

// Function prototypes
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

// Window dimensions
const GLuint WIDTH = 800, HEIGHT = 600;


// The MAIN function, from here we start the application and run the game loop
int main()
{
    std::cout << "Starting GLFW context, OpenGL 4.6" << std::endl;
    // Init GLFW
    glfwInit();
    // Set all the required options for GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // Create a GLFWwindow object that we can use for GLFW's functions
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", NULL, NULL);
    glfwMakeContextCurrent(window);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Set the required callback functions
    glfwSetKeyCallback(window, key_callback);

    // Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }

    // Successfully loaded OpenGL
    std::cout << "Loaded OpenGL " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << std::endl;

    // Define the viewport dimensions
    glViewport(0, 0, WIDTH, HEIGHT);

    // Game loop
    while (!glfwWindowShouldClose(window))
    {
        // Check if any events have been activated (key pressed, mouse moved etc.) and call corresponding response functions
        glfwPollEvents();

        // Render
        // Clear the colorbuffer
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Swap the screen buffers
        glfwSwapBuffers(window);
    }

    // Terminates GLFW, clearing any resources allocated by GLFW.
    glfwTerminate();

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

// Is called whenever a key is pressed/released via GLFW
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
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
