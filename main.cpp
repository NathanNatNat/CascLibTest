#include <iostream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/gl.h>
// GLFW (include after glad)
#include <GLFW/glfw3.h>
#include <CascLib.h>

    // Add this at the top of your file, before any code that uses std::to_string
#include <sstream>

// Add this utility function near the top of your file (before usage)
template<typename T>
std::string to_string(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

static int ExtractFile(char* szStorageName, char* szStorageFile, char* szFileName);
//ExtractFile(storagePath, "Interface/FrameXML/Localization.lua", "Localization.lua");

// Function prototypes
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

// CASC-related functions
void OpenCascStorage();
void ImportCascKeys();
void ReadCascFile();
void CloseCascStorage();

// Window dimensions
const GLuint WIDTH = 1920, HEIGHT = 1080;

// Global CASC variables
HANDLE g_hStorage = nullptr;
bool g_storageOpen = false;
bool g_keysImported = false;
std::string g_statusMessage = "Ready";
std::string g_fileContent = "";

// The MAIN function, from here we start the application and run the game loop
int main()
{
    std::cout << "Starting GLFW context, OpenGL 4.6" << std::endl;
    // Init GLFW
    glfwInit();
    // Set all the required options for GLFW
    const char* glsl_version = "#version 140";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // Create a GLFWwindow object that we can use for GLFW's functions
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CascLibTest", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
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

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    bool show_casc_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Game loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("CASC Window", &show_casc_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))
                // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 4. Show CASC operations window
        if (show_casc_window)
        {
            ImGui::Begin("CASC Operations", &show_casc_window);
            
            ImGui::Text("CASC Storage Control");
            ImGui::Separator();
            
            // Status display
            ImGui::Text("Status: %s", g_statusMessage.c_str());
            ImGui::Text("Storage Open: %s", g_storageOpen ? "Yes" : "No");
            ImGui::Text("Keys Imported: %s", g_keysImported ? "Yes" : "No");
            
            ImGui::Separator();
            
            // CASC operation buttons
            if (ImGui::Button("Open CASC Storage"))
            {
                OpenCascStorage();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Import Keys") && g_storageOpen)
            {
                ImportCascKeys();
            }
            
            if (ImGui::Button("Read File") && g_storageOpen && g_keysImported)
            {
                ReadCascFile();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Close Storage") && g_storageOpen)
            {
                CloseCascStorage();
            }
            
            ImGui::Separator();
            
            // File content display
            if (!g_fileContent.empty())
            {
                ImGui::Text("File Content:");
                ImGui::TextWrapped("%s", g_fileContent.c_str());
            }
            
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                     clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup CASC if still open
    if (g_storageOpen)
    {
        CloseCascStorage();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// CASC operation functions
void OpenCascStorage()
{
    const char* storagePath = "E:/Games/BattleNet/World of Warcraft*wow";
    
    if (g_storageOpen)
    {
        g_statusMessage = "Storage already open!";
        return;
    }
    
    if (!CascOpenStorage(storagePath, 0, &g_hStorage))
    {
        g_statusMessage = "Failed to open storage: " + std::to_string(GetLastError());
        return;
    }
    
    g_storageOpen = true;
    
    // Get file count
    long dwFileCount = 0;
    if (CascGetStorageInfo(g_hStorage, CascStorageTotalFileCount, &dwFileCount, sizeof(DWORD), NULL))
    {
        g_statusMessage = "Storage opened successfully! File count: " + std::to_string(dwFileCount);
    }
    else
    {
        g_statusMessage = "Storage opened successfully!";
    }
}

void ImportCascKeys()
{
    const char* keyFilePath = "D:/Repositories/CascLibTest/EncryptionKeys.txt";
    
    if (!g_storageOpen)
    {
        g_statusMessage = "Storage not open!";
        return;
    }
    
    if (g_keysImported)
    {
        g_statusMessage = "Keys already imported!";
        return;
    }
    
    if (!CascImportKeysFromFile(g_hStorage, keyFilePath))
    {
        g_statusMessage = "Failed to import keys: " + std::to_string(GetLastError());
        return;
    }
    
    g_keysImported = true;
    g_statusMessage = "Keys imported successfully!";
}

void ReadCascFile()
{
    if (!g_storageOpen || !g_keysImported)
    {
        g_statusMessage = "Storage not open or keys not imported!";
        return;
    }
    
    HANDLE hFile;
    DWORD dwBytesRead;
    char buffer[1024];
    
    if (!CascOpenFile(g_hStorage, "interface/addons/blizzard_calendar/localization.lua", 0, 0, &hFile))
    {
        g_statusMessage = "Failed to open file: " + std::to_string(GetLastError());
        return;
    }
    
    if (CascReadFile(hFile, buffer, sizeof(buffer) - 1, &dwBytesRead))
    {
        buffer[dwBytesRead] = '\0';
        g_fileContent = std::string(buffer);
        g_statusMessage = "File read successfully! (" + std::to_string(dwBytesRead) + " bytes)";
    }
    else
    {
        g_statusMessage = "Failed to read file";
        g_fileContent = "";
    }
    
    CascCloseFile(hFile);
}

void CloseCascStorage()
{
    if (!g_storageOpen)
    {
        g_statusMessage = "Storage not open!";
        return;
    }
    
    CascCloseStorage(g_hStorage);
    g_hStorage = nullptr;
    g_storageOpen = false;
    g_keysImported = false;
    g_fileContent = "";
    g_statusMessage = "Storage closed";
}

// Is called whenever a key is pressed/released via GLFW
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
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
static int ExtractFile(char* szStorageName, char* szStorageFile, char* szFileName)
{
    HANDLE hStorage = NULL;
    HANDLE hFile = NULL; 
    HANDLE handle = NULL;
    DWORD dwErrCode = ERROR_SUCCESS;

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
        // Modern C++17: Use smart pointer for automatic memory management
        constexpr size_t BUFFER_SIZE = 0x10000;
        auto buffer = std::make_unique<char[]>(BUFFER_SIZE);
        
        DWORD dwBytes = 1;
        while (dwBytes != 0)
        {
            CascReadFile(hFile, buffer.get(), BUFFER_SIZE, &dwBytes);
            if (dwBytes == 0)
                break;

            WriteFile(handle, buffer.get(), dwBytes, &dwBytes, NULL);
            if (dwBytes == 0)
                break;
        }
        // No manual cleanup needed - unique_ptr handles it automatically
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
