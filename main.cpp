#include <iostream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/gl.h>
// GLFW (include after glad)
#include <GLFW/glfw3.h>
#include <CascLib.h>
#include <cmath>
#include <cstring> 
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
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// CASC-related functions
void OpenCascStorage();
void ImportCascKeys();
void ReadCascFile();
void CloseCascStorage();

// 3D Viewport functions
void InitViewport();
void UpdateViewport(int width, int height);
void RenderViewport();
void CleanupViewport();

// Window dimensions
const GLuint WIDTH = 2560, HEIGHT = 1440;

// Global CASC variables
HANDLE g_hStorage = nullptr;
bool g_storageOpen = false;
bool g_keysImported = false;
std::string g_statusMessage = "Ready";
std::string g_fileContent = "";

// 3D Viewport variables
struct ViewportData {
    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthBuffer = 0;
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    int width = 512;
    int height = 512;
    float rotation = 0.0f;
} g_viewport;

// Simple vertex shader source
const char* vertexShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vertexColor;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vertexColor = aColor;
}
)";

// Simple fragment shader source
const char* fragmentShaderSource = R"(
#version 460 core
in vec3 vertexColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vertexColor, 1.0);
}
)";

// Cube vertices with colors
float cubeVertices[] = {
    // positions          // colors
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,

    -0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f,

     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,

    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,

    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f
};

// Simple triangle vertices in clip space coordinates
float triangleVertices[] = {
    // positions (clip space)  // colors
    -0.5f, -0.5f, 0.0f,        1.0f, 0.0f, 0.0f,  // bottom left - red
     0.5f, -0.5f, 0.0f,        0.0f, 1.0f, 0.0f,  // bottom right - green  
     0.0f,  0.5f, 0.0f,        0.0f, 0.0f, 1.0f   // top - blue
};

// Simple matrix multiplication functions
void multiplyMatrix4x4(float* result, const float* a, const float* b) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
}

void createPerspectiveMatrix(float* matrix, float fov, float aspect, float nearPlane, float farPlane) {
    memset(matrix, 0, 16 * sizeof(float));
    float tanHalfFov = tan(fov / 2.0f);
    matrix[0] = 1.0f / (aspect * tanHalfFov);
    matrix[5] = 1.0f / tanHalfFov;
    matrix[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    matrix[11] = -1.0f;
    matrix[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
}

void createViewMatrix(float* matrix, float eyeX, float eyeY, float eyeZ) {
    memset(matrix, 0, 16 * sizeof(float));
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    matrix[12] = -eyeX;
    matrix[13] = -eyeY;
    matrix[14] = -eyeZ;
}

void createRotationMatrix(float* matrix, float angle, float x, float y, float z) {
    memset(matrix, 0, 16 * sizeof(float));
    float c = cos(angle);
    float s = sin(angle);
    float oneMinusC = 1.0f - c;
    
    matrix[0] = c + x * x * oneMinusC;
    matrix[1] = x * y * oneMinusC - z * s;
    matrix[2] = x * z * oneMinusC + y * s;
    matrix[4] = y * x * oneMinusC + z * s;
    matrix[5] = c + y * y * oneMinusC;
    matrix[6] = y * z * oneMinusC - x * s;
    matrix[8] = z * x * oneMinusC - y * s;
    matrix[9] = z * y * oneMinusC + x * s;
    matrix[10] = c + z * z * oneMinusC;
    matrix[15] = 1.0f;
}

// The MAIN function, from here we start the application and run the game loop
int main()
{
    std::cout << "Starting GLFW context, OpenGL 4.6" << std::endl;
    // Init GLFW
    glfwInit();
    // Set all the required options for GLFW
    const char* glsl_version = "#version 460 core";  // Changed from "#version 140"
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

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
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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

    // Initialize 3D viewport
    InitViewport();

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

    // Setup scaling for high-DPI displays
    float scale = 2.0f; // Adjust this value based on your display (1.5f, 2.0f, 2.5f, etc.)
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);        // Scale all UI elements
    style.FontScaleDpi = scale;        // Scale font rendering
    io.ConfigDpiScaleFonts = true;     // Enable automatic DPI font scaling
    io.ConfigDpiScaleViewports = true; // Enable automatic DPI viewport scaling

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
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
    bool show_viewport_window = true;
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

        // Create main dockspace
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // Optional: Add a menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                ImGui::MenuItem("Demo Window", NULL, &show_demo_window);
                ImGui::MenuItem("Another Window", NULL, &show_another_window);
                ImGui::MenuItem("CASC Window", NULL, &show_casc_window);
                ImGui::MenuItem("3D Viewport", NULL, &show_viewport_window);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End(); // End DockSpace

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
            ImGui::Checkbox("3D Viewport", &show_viewport_window);

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

        // 5. Show 3D Viewport window
        if (show_viewport_window)
        {
            ImGui::Begin("3D Viewport", &show_viewport_window);
            
            // Get available content region
            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
            
            // Update viewport size if needed
            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0 && 
                (g_viewport.width != (int)viewportPanelSize.x || g_viewport.height != (int)viewportPanelSize.y))
            {
                UpdateViewport((int)viewportPanelSize.x, (int)viewportPanelSize.y);
            }
            
            // Render 3D scene to framebuffer
            RenderViewport();
            
            // Display the rendered texture
            if (g_viewport.colorTexture)
            {
                ImGui::Image((ImTextureID)(intptr_t)g_viewport.colorTexture, 
                           ImVec2((float)g_viewport.width, (float)g_viewport.height),
                           ImVec2(0, 1), ImVec2(1, 0)); // Flip Y coordinate
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

    // Cleanup 3D viewport
    CleanupViewport();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// 3D Viewport implementation
void InitViewport()
{
    std::cout << "Initializing viewport..." << std::endl;
    
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    // Check vertex shader compilation
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return;
    }
    else {
        std::cout << "Vertex shader compiled successfully" << std::endl;
    }
    
    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    // Check fragment shader compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return;
    }
    else {
        std::cout << "Fragment shader compiled successfully" << std::endl;
    }
    
    // Link shaders
    g_viewport.shaderProgram = glCreateProgram();
    glAttachShader(g_viewport.shaderProgram, vertexShader);
    glAttachShader(g_viewport.shaderProgram, fragmentShader);
    glLinkProgram(g_viewport.shaderProgram);
    
    // Check shader program linking
    glGetProgramiv(g_viewport.shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(g_viewport.shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return;
    }
    else {
        std::cout << "Shader program linked successfully" << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Set up vertex data and buffers
    glGenVertexArrays(1, &g_viewport.VAO);
    glGenBuffers(1, &g_viewport.VBO);
    
    std::cout << "Generated VAO: " << g_viewport.VAO << ", VBO: " << g_viewport.VBO << std::endl;
    
    glBindVertexArray(g_viewport.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, g_viewport.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL error during VAO setup: " << error << std::endl;
    }
    
    // Create framebuffer
    UpdateViewport(g_viewport.width, g_viewport.height);
    
    std::cout << "Viewport initialization complete" << std::endl;
}

void UpdateViewport(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    
    g_viewport.width = width;
    g_viewport.height = height;
    
    // Delete existing framebuffer objects if they exist
    if (g_viewport.framebuffer != 0)
    {
        glDeleteFramebuffers(1, &g_viewport.framebuffer);
        glDeleteTextures(1, &g_viewport.colorTexture);
        glDeleteRenderbuffers(1, &g_viewport.depthBuffer);
    }
    
    // Create framebuffer
    glGenFramebuffers(1, &g_viewport.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, g_viewport.framebuffer);
    
    // Create color texture
    glGenTextures(1, &g_viewport.colorTexture);
    glBindTexture(GL_TEXTURE_2D, g_viewport.colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_viewport.colorTexture, 0);
    
    // Create depth buffer
    glGenRenderbuffers(1, &g_viewport.depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, g_viewport.depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_viewport.depthBuffer);
    
    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderViewport()
{
    // Update rotation
    g_viewport.rotation += 0.01f;

    // Validate required OpenGL objects first
    if (g_viewport.shaderProgram == 0 || g_viewport.VAO == 0 || g_viewport.framebuffer == 0) {
        std::cout << "Invalid OpenGL objects - shader: " << g_viewport.shaderProgram 
                  << ", VAO: " << g_viewport.VAO << ", framebuffer: " << g_viewport.framebuffer << std::endl;
        return;
    }

    // Save current OpenGL states
    GLint previousFramebuffer, previousViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    // Bind our offscreen framebuffer and set the viewport size
    glBindFramebuffer(GL_FRAMEBUFFER, g_viewport.framebuffer);
    glViewport(0, 0, g_viewport.width, g_viewport.height);

    // Enable depth testing and clear the buffers
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use our shader program
    glUseProgram(g_viewport.shaderProgram);

    // Get uniform locations
    GLint modelLoc = glGetUniformLocation(g_viewport.shaderProgram, "model");
    GLint viewLoc  = glGetUniformLocation(g_viewport.shaderProgram, "view");
    GLint projLoc  = glGetUniformLocation(g_viewport.shaderProgram, "projection");

    // Set up our transformation matrices
    float model[16], view[16], projection[16];
    // Rotate the cube about the Y-axis
    createRotationMatrix(model, g_viewport.rotation, 0.0f, 1.0f, 0.0f);
    // Place the camera at (0,0,3) so that the view matrix translates the scene by (0,0,-3)
    createViewMatrix(view, 0.0f, 0.0f, 3.0f);
    float aspect = (float)g_viewport.width / (float)g_viewport.height;
    createPerspectiveMatrix(projection, 45.0f * 3.14159f / 180.0f, aspect, 0.1f, 100.0f);

    // Pass transformation matrices to the shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
    glUniformMatrix4fv(viewLoc,  1, GL_FALSE, view);
    glUniformMatrix4fv(projLoc,  1, GL_FALSE, projection);

    // Check for any OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL error: " << error << std::endl;
    }

    // Draw the cube (36 vertices -> 12 triangles)
    glBindVertexArray(g_viewport.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Check again for errors after drawing
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL error after drawing: " << error << std::endl;
    }

    // Restore previous OpenGL state
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

void CleanupViewport()
{
    if (g_viewport.framebuffer != 0)
    {
        glDeleteFramebuffers(1, &g_viewport.framebuffer);
        glDeleteTextures(1, &g_viewport.colorTexture);
        glDeleteRenderbuffers(1, &g_viewport.depthBuffer);
    }
    
    if (g_viewport.VAO != 0)
    {
        glDeleteVertexArrays(1, &g_viewport.VAO);
        glDeleteBuffers(1, &g_viewport.VBO);
    }
    
    if (g_viewport.shaderProgram != 0)
    {
        glDeleteProgram(g_viewport.shaderProgram);
    }
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

// Add this function somewhere near your other callback functions:
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // Update the main viewport:
    glViewport(0, 0, width, height);
    
    // Optionally update the offscreen viewport dimensions as well:
    UpdateViewport(width, height);
}

void window_iconify_callback(GLFWwindow* window, int iconified)
{
    if (iconified)
        std::cout << "Window minimized" << std::endl;
    else
    {

    }
        std::cout << "Window restored" << std::endl;
}

void window_maximize_callback(GLFWwindow* window, int maximized)
{
    if (maximized)
        std::cout << "Window maximized" << std::endl;
    else
        std::cout << "Window restored from maximized state" << std::endl;
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
