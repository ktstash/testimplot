// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui\imgui.h"
#include "imgui\backends\imgui_impl_glfw.h"
#include "imgui\backends\imgui_impl_opengl3.h"
#include "imgui\implot.h"
#include <stdio.h>
#include <deque>
#include <string>
#include <vector>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void Demo_BarPlots() {
    std::deque<uint64_t> times1 = { 0,5,11,18,26,35,45,56,68,81 };
    std::deque<bool> data1 = { true,false,true,false,true,false,true,false,true,false };

    ImVec4 color_true = ImVec4(0, 0, 1, 1);
    ImVec4 color_false = ImVec4(1, 0, 0, 1);
    
    std::vector<uint64_t> x_data;
    x_data.reserve(times1.size());
    std::vector<uint64_t> y_data;
    y_data.reserve(times1.size());

    for (size_t i = 0; i < times1.size() - 1; ++i)
    {
        x_data.push_back(times1[i]);
        y_data.push_back(times1[i + 1] - times1[i]);
        ImPlot::PushStyleColor(ImPlotCol_Fill, data1[i] ? color_true :color_false);
    }


    std::deque<uint64_t> times2 = { 0,5,10,30 };
    std::deque<bool> data2 = { false,true,true,false };

    if (ImPlot::BeginPlot("Bar Plot")) {
        ImPlot::PlotBars("State Bars",y_data.data(), y_data.size(), 0.4, 1, ImPlotBarsFlags_Horizontal);
        //ImPlot::PlotBars("Horizontal2", times2, data2, 10, 0.4, 1, ImPlotBarsFlags_Horizontal);
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("Bar Plot"))
    {
        for (size_t i = 0; i < times1.size() - 1; ++i)
        {
            ImVec4 color = data1[i] ? color_true : color_false;
            ImPlot::PushStyleColor(ImPlotCol_Fill, color);
            //uint64_t x_data[2] = { times1[i] ,times1[i]};
            uint64_t y_data[2] = { times1[i], times1[i + 1] - times1[i] };
            //ImPlot::PlotBars_Continous("State Bars", y_data, 1, 0.4, ImPlotBarsFlags_Horizontal);
            //ImPlot::PlotBars("Horizontal2", times2, data2, 10, 0.4, 1, ImPlotBarsFlags_Horizontal);
            const char* text = data1[i] ? "TRUE" : "FALSE";
            ImPlot::PlotText(text, times1[i], 0.5f);
            ImPlot::PopStyleColor();
        }
        ImPlot::EndPlot();
    }

}

//-----------------------------------------------------------------------------

void Demo_BarGroups() {
    static std::deque<uint64_t>  times = { 0,10,15,16,19,21,23,25,29,30 };

    std::vector<uint64_t> datas(times.size() - 1);

    for (size_t i = 0; i < times.size() - 1; ++i)
    {
        datas[i] = times[i + 1] - times[i];
    }

    static std::deque<bool> values = { true,false,true,true,false,true,true,false,true,false };
    static const double positions[] = { 0,1,2,3,4,5,6,7,8,9 };
    static const char* ilabels[] = { "a","a","a","a","a","a","a","a","a","a" };
    //static std::vector<const char> ilabels = { "Midterm Exam","Final Exam","Course Grade" };
    static int items = times.size();
    static int groups = 1;
    static float size = 0.67f;

    static ImPlotBarGroupsFlags flags = 0;

    ImGui::CheckboxFlags("Stacked", (unsigned int*)&flags, ImPlotBarGroupsFlags_Stacked);
    ImGui::SameLine();


    if (ImPlot::BeginPlot("Bar Group")) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);

        ImPlot::SetupAxes("Topics", "Time", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        //ImPlot::SetupAxisTicks(ImAxis_Y1, positions, groups, glabels);
        ImPlot::PlotBarGroups(ilabels, datas.data(), datas.size(), groups, size, 0, flags | ImPlotBarGroupsFlags_Horizontal);


        ImPlot::EndPlot();
    }
}

//-----------------------------------------------------------------------------

void Demo_BarStacks()
{
    ImVec4 color_true = ImVec4(0, 0, 1, 1);
    ImVec4 color_false = ImVec4(1, 0, 0, 1);
    std::deque<uint64_t> times1 = { 0,10,15,16,19,21,23,25,29,30 };
    std::deque<bool> data1 = { true,false,true,true,false,true,true,false,true,false };

    static ImPlotColormap bools_color_map = -1;
    std::vector<ImVec4> colors;
    colors.reserve(data1.size());
    std::vector<const char*> labels;
    labels.push_back("TESTTOPIC");

    if (bools_color_map == -1)
    {
        for (int i = 0; i < data1.size(); i++)
        {
            ImVec4 color = data1[i] ? color_true : color_false;
            colors.push_back(color);
        }
        bools_color_map = ImPlot::AddColormap("testcolor1u", colors.data(), colors.size());
    }

    std::vector<uint64_t> data_plot;
    data_plot.reserve(times1.size() - 1);
    for (size_t i = 0; i < times1.size() - 1; ++i)
    {
        data_plot.push_back(times1[i + 1] - times1[i]);
    }


    std::vector<const char*> topics;
    topics.reserve(1);

    ImPlot::PushColormap(bools_color_map);
    if (ImPlot::BeginPlot("Test", ImVec2(-1, 400), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_AutoFit);
        //ImPlot::SetupAxisTicks(ImAxis_Y1, 0, 0, 1, labels.data(), false);

        ImPlot::PlotBarGroups(labels.data(), data_plot.data(), labels.size(), data_plot.size(), 0.75, 0, ImPlotBarGroupsFlags_Stacked | ImPlotBarGroupsFlags_Horizontal);
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}


// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
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

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 4. Test bar plots
        Demo_BarPlots();


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

