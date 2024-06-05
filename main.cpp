#include <iostream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/gl.h>
// GLFW (include after glad)
#include <GLFW/glfw3.h>
#include <CascLib.h>

static int ExtractFile(char* szStorageName, char* szStorageFile, char* szFileName);
//ExtractFile(storagePath, "Interface/FrameXML/Localization.lua", "Localization.lua");

// Function prototypes
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

// Window dimensions
const GLuint WIDTH = 1920, HEIGHT = 1080;


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

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    HANDLE hStorage;
    HANDLE hFile;
    DWORD dwBytesRead;
    char buffer[1024];
    const char* storagePath = "D:/Games/BattleNet/World of Warcraft*wow";
    const char* keyFilePath = "C:/Development/Repositories/CascLibTest/EncryptionKeys.txt";

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
    HANDLE hStorage = NULL; // Open storage handle
    HANDLE hFile = NULL; // Storage file handle
    HANDLE handle = NULL; // Disk file handle
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
        char szBuffer[0x10000];
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
