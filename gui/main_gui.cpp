// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  ShadPKG GUI - Main Entry Point                                           ║
// ║                                                                           ║
// ║  ImGui + GLFW + OpenGL3 Application                                       ║
// ║                                                                           ║
// ║  Window Layout:                                                           ║
// ║  ┌──────────────┬──────────────────────────────────────────────────────┐  ║
// ║  │              │                                                      │  ║
// ║  │   NAV BAR    │                  MAIN VIEWPORT                       │  ║
// ║  │              │             (Dynamic view content)                   │  ║
// ║  │  [Extract]   │                                                      │  ║
// ║  │  [Inspect]   │                                                      │  ║
// ║  │  [RIF Tool]  ├──────────────────────────────────────────────────────┤  ║
// ║  │  [Settings]  │                  CONSOLE LOG                         │  ║
// ║  │              │          [INFO] System initialized...                │  ║
// ║  └──────────────┴──────────────────────────────────────────────────────┘  ║
// ║                                                                           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#include "common/assert.h"
#include <assert.h>
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <GL/gl.h>
#include <windows.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

// GUI Components
#include "include/ConsoleLog.h"
#include "include/GUIContext.h"
#include "include/GuiLogSink.h"
#include "include/NavBar.h"
#include "include/StyleManager.h"

// Views
#include "include/views/ExtractorView.h"
#include "include/views/InspectorView.h"
#include "include/views/RIFView.h"
#include "include/views/SettingsView.h"

// Logging
#include "common/logging/backend.h"
#include "common/logging/log.h"

using namespace ShadPKG::GUI;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  Global State                                                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
static GUIContext g_Context;
static NavBar g_NavBar;
static ConsoleLog g_ConsoleLog;
static ExtractorView g_ExtractorView;
static InspectorView g_InspectorView;
static RIFView g_RIFView;
static SettingsView g_SettingsView;

// Drag & drop file paths pending processing
static std::vector<std::string> g_DroppedFiles;
static bool g_HasDroppedFiles = false;

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  GLFW Callbacks                                                         │
// └─────────────────────────────────────────────────────────────────────────┘
static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
  GuiLogSink::Instance().Error(std::string("GLFW Error: ") + description);
}

static void glfw_drop_callback(GLFWwindow *window, int count,
                               const char **paths) {
  g_DroppedFiles.clear();
  for (int i = 0; i < count; ++i) {
    g_DroppedFiles.push_back(paths[i]);
  }
  g_HasDroppedFiles = true;
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Process Dropped Files                                                  │
// └─────────────────────────────────────────────────────────────────────────┘
static void ProcessDroppedFiles() {
  if (!g_HasDroppedFiles)
    return;

  for (const auto &path : g_DroppedFiles) {
    // Forward to extractor view
    g_ExtractorView.OnFileDrop(g_Context, path);
  }

  g_DroppedFiles.clear();
  g_HasDroppedFiles = false;
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Draw Main Application Frame                                            │
// └─────────────────────────────────────────────────────────────────────────┘
static void DrawMainFrame() {
  // Get viewport
  const ImGuiViewport *viewport = ImGui::GetMainViewport();

  // Setup window flags for fullscreen window
  ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  // Cover entire viewport
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("##MainWindow", nullptr, windowFlags);

  ImGui::PopStyleVar(3);

  // ═══════════════════════════════════════════════════════════════════════
  //  Layout: NavBar | ContentArea
  // ═══════════════════════════════════════════════════════════════════════

  // Draw NavBar on the left
  g_NavBar.Draw(g_Context);

  ImGui::SameLine();

  // Content area (view + console)
  ImGui::BeginChild("##content_area", ImVec2(0, 0), false);

  float contentHeight = ImGui::GetContentRegionAvail().y;
  float consoleHeight = g_ConsoleLog.IsExpanded() ? 180.0f : 30.0f;
  if (consoleHeight > contentHeight) {
    consoleHeight = contentHeight;
  }
  if (consoleHeight < 0.0f) {
    consoleHeight = 0.0f;
  }
  float viewHeight = contentHeight - consoleHeight;
  if (viewHeight < 0.0f) {
    viewHeight = 0.0f;
  }

  if (viewHeight > 1.0f) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("##view_area", ImVec2(0, viewHeight), true);

    switch (g_Context.GetCurrentView()) {
    case GUIContext::View::Extractor:
      g_ExtractorView.Draw(g_Context);
      break;
    case GUIContext::View::Inspector:
      g_InspectorView.Draw(g_Context);
      break;
    case GUIContext::View::RIF:
      g_RIFView.Draw(g_Context);
      break;
    case GUIContext::View::Settings:
      g_SettingsView.Draw(g_Context);
      break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  Console Log Area
  // ═══════════════════════════════════════════════════════════════════════
  if (consoleHeight > 0.0f) {
    ImGui::BeginChild("##console_area", ImVec2(0, consoleHeight), false,
                      ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    float logContentHeight = consoleHeight - 30.0f;
    if (logContentHeight < 0.0f) {
      logContentHeight = 0.0f;
    }
    g_ConsoleLog.Draw(logContentHeight);
    ImGui::EndChild();
  }

  ImGui::EndChild(); // content_area

  ImGui::End(); // MainWindow
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  Main Entry Point                                                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
int main(int argc, char *argv[]) {
  // Initialize logging
  Common::Log::Initialize("shadpkg_gui.log");
  Common::Log::SetColorConsoleBackendEnabled(true);
  LOG_INFO(Common, "[GUI] Starting ShadPKG GUI");

  // Setup GLFW
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    LOG_ERROR(Common, "[GUI] Failed to initialize GLFW");
    return 1;
  }

  // GL context hints
#if defined(__APPLE__)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  const char *glslVersion = "#version 150";
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  const char *glslVersion = "#version 130";
#endif

  // Create window
  GLFWwindow *window = glfwCreateWindow(1280, 800, "ShadPKG", nullptr, nullptr);
  if (window == nullptr) {
    LOG_ERROR(Common, "[GUI] Failed to create GLFW window");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup drag & drop
  glfwSetDropCallback(window, glfw_drop_callback);

  // Setup ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Apply custom style
  SetupImGuiStyle();

  std::filesystem::path executableDir;
  if (argv != nullptr && argc > 0 && argv[0] != nullptr) {
    std::error_code ec;
    const auto absolutePath = std::filesystem::absolute(argv[0], ec);
    if (!ec) {
      executableDir = absolutePath.parent_path();
    }
  }

  LoadFonts(io, executableDir);

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  // Window background color
  ImVec4 clearColor = Colors::WindowBg;

  GuiLogSink::Instance().Info("ShadPKG GUI initialized");

  // ═══════════════════════════════════════════════════════════════════════
  //  Main Loop
  // ═══════════════════════════════════════════════════════════════════════
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Process any dropped files
    ProcessDroppedFiles();

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Draw the application
    DrawMainFrame();

    // Render
    ImGui::Render();
    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  LOG_INFO(Common, "[GUI] ShadPKG GUI shutdown");
  Common::Log::Stop();

  return 0;
}
