#include "imgui/imgui.h"
#define NOMINMAX
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

enum class MessageType {
    Chat,
    System,
    Upload,
    Download,
    Error
};

struct ChatMessage {
    std::string text;
    MessageType type;
};

std::mutex chatMutex;
std::vector<ChatMessage> chatLog;
std::vector<std::string> availableFiles;
std::mutex filesMutex;
bool connected = false;
SSL* ssl = nullptr;
SOCKET clientSocket = INVALID_SOCKET;
SSL_CTX* ctx = nullptr;
std::thread recvThread;
std::string currentUsername;
bool showFileBrowser = false;
std::string connectionError = "";
bool isReceivingFile = false;

struct FileTransfer {
    std::string filename;
    size_t filesize;
    size_t transferred;
    bool active;
    std::string status;
};

std::mutex transferMutex;
FileTransfer currentTransfer;

void addChatMessage(const std::string& msg, MessageType type = MessageType::Chat) {
    std::lock_guard<std::mutex> lock(chatMutex);
    chatLog.push_back({ msg, type });
}

void updateTransferStatus(const std::string& filename, size_t transferred, size_t total) {
    std::lock_guard<std::mutex> lock(transferMutex);
    currentTransfer.filename = filename;
    currentTransfer.filesize = total;
    currentTransfer.transferred = transferred;
    currentTransfer.active = (transferred < total);

    int percent = (total > 0) ? (int)((transferred * 100) / total) : 0;
    currentTransfer.status = filename + " - " + std::to_string(percent) + "% (" +
        std::to_string(transferred) + "/" + std::to_string(total) + " bytes)";
}

ImVec4 getColorForMessageType(MessageType type) {
    switch (type) {
    case MessageType::System:   return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    case MessageType::Upload:   return ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
    case MessageType::Download: return ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
    case MessageType::Error:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    case MessageType::Chat:
    default:                    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

std::string openFileDialog() {
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

std::string saveFileDialog(const std::string& defaultName) {
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
    strcpy_s(szFile, defaultName.c_str());

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

void receiveMessages() {
    char buffer[4096];
    while (connected) {
        int bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes > 0) {
            if (isReceivingFile) {
                continue;
            }

            std::string msg(buffer, bytes);

            if (msg.find("/filelist") == 0) {
                std::lock_guard<std::mutex> lock(filesMutex);
                availableFiles.clear();

                std::istringstream iss(msg.substr(10));
                std::string filename;
                while (iss >> filename) {
                    availableFiles.push_back(filename);
                }
                addChatMessage("[SYSTEM] Received file list: " + std::to_string(availableFiles.size()) + " files", MessageType::System);
            }
            else if (msg.find("/download_ready") == 0 || msg.find("/filedata") == 0) {
                std::istringstream iss(msg);
                std::string cmd, filename;
                size_t filesize;
                iss >> cmd >> filename >> filesize;

                std::string savePath = saveFileDialog(filename);
                if (!savePath.empty()) {
                    addChatMessage("[DOWNLOAD] Saving to: " + savePath, MessageType::Download);

                    std::ofstream file(savePath, std::ios::binary);
                    if (file.is_open()) {
                        size_t totalReceived = 0;
                        updateTransferStatus(filename, 0, filesize);
                        isReceivingFile = true;

                        while (totalReceived < filesize && connected) {
                            size_t toRead = (std::min)(sizeof(buffer), filesize - totalReceived);
                            int bytes = SSL_read(ssl, buffer, (int)toRead);
                            if (bytes <= 0) break;

                            file.write(buffer, bytes);
                            totalReceived += bytes;
                            updateTransferStatus(filename, totalReceived, filesize);
                        }

                        isReceivingFile = false;
                        file.close();
                        if (totalReceived == filesize) {
                            addChatMessage("[DOWNLOAD] Complete: " + filename, MessageType::Download);
                        }
                        else {
                            addChatMessage("[ERROR] Download incomplete: " + filename, MessageType::Error);
                        }

                        std::lock_guard<std::mutex> lock(transferMutex);
                        currentTransfer.active = false;
                    }
                    else {
                        addChatMessage("[ERROR] Could not create file: " + savePath, MessageType::Error);
                    }
                }
                else {
                    addChatMessage("[DOWNLOAD] Cancelled", MessageType::System);
                }
            }
            else {
                addChatMessage(msg, MessageType::Chat);
            }
        }
        else {
            connected = false;
            break;
        }
    }
}

void uploadFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        addChatMessage("[ERROR] Could not open file: " + filepath, MessageType::Error);
        return;
    }

    size_t filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    size_t pos = filepath.find_last_of("\\/");
    std::string filename = (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);

    std::string cmd = "/upload " + filename + " " + std::to_string(filesize);
    SSL_write(ssl, cmd.c_str(), (int)cmd.length());
    addChatMessage("[UPLOAD] Sending " + filename + " (" + std::to_string(filesize) + " bytes)", MessageType::Upload);

    char buffer[4096];
    size_t totalSent = 0;
    updateTransferStatus(filename, 0, filesize);

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        int bytes = (int)file.gcount();
        SSL_write(ssl, buffer, bytes);
        totalSent += bytes;
        updateTransferStatus(filename, totalSent, filesize);
    }

    addChatMessage("[UPLOAD] Complete: " + filename, MessageType::Upload);
    file.close();

    std::lock_guard<std::mutex> lock(transferMutex);
    currentTransfer.active = false;
}

void requestFileList() {
    std::string cmd = "/list";
    SSL_write(ssl, cmd.c_str(), (int)cmd.length());
}

void downloadFile(const std::string& filename) {
    std::string cmd = "/download " + filename;
    SSL_write(ssl, cmd.c_str(), (int)cmd.length());
}

bool connectToServer(const std::string& ip, const std::string& secret, const std::string& username) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        connectionError = "Failed to create socket";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8443);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        connectionError = "Could not connect to server";
        return false;
    }

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ctx = SSL_CTX_new(TLS_client_method());
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);

    if (SSL_connect(ssl) <= 0) {
        connectionError = "SSL handshake failed";
        return false;
    }

    SSL_write(ssl, secret.c_str(), (int)secret.length());
    SSL_write(ssl, username.c_str(), (int)username.length());

    currentUsername = username;
    connected = true;
    connectionError = "";
    recvThread = std::thread(receiveMessages);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    requestFileList();

    return true;
}

void disconnect() {
    connected = false;

    // Close socket to unblock SSL_read
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }

    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    // Now we can safely join the thread
    if (recvThread.joinable()) {
        recvThread.join();
    }

    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = nullptr;
    }

    WSACleanup();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!glfwInit()) return 1;

    // Start with small login window
    GLFWwindow* window = glfwCreateWindow(400, 300, "Chatter - Login", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.FontGlobalScale = 1.3f;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    char ipBuf[64] = "127.0.0.1";
    char secretBuf[64] = "";
    char userBuf[64] = "guest";
    char msgBuf[512] = "";
    bool wasConnected = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Resize window when connection state changes
        if (connected && !wasConnected) {
            glfwSetWindowSize(window, 1000, 700);
            glfwSetWindowTitle(window, "Chatter Client");
            wasConnected = true;
        }
        else if (!connected && wasConnected) {
            glfwSetWindowSize(window, 400, 300);
            glfwSetWindowTitle(window, "Chatter - Login");
            wasConnected = false;
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        if (!connected) {
            // Centered login form
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.15f);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 300) * 0.5f);
            ImGui::BeginChild("LoginBox", ImVec2(300, 0), false);

            ImGui::Text("Connect to Server");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Server IP:");
            ImGui::InputText("##ip", ipBuf, sizeof(ipBuf));
            ImGui::Spacing();

            ImGui::Text("Username:");
            ImGui::InputText("##user", userBuf, sizeof(userBuf));
            ImGui::Spacing();

            ImGui::Text("Shared Secret:");
            bool enterPressed = ImGui::InputText("##secret", secretBuf, sizeof(secretBuf), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            ImGui::Spacing();

            if (ImGui::Button("Connect", ImVec2(290, 40)) || enterPressed) {
                if (strlen(ipBuf) > 0 && strlen(userBuf) > 0 && strlen(secretBuf) > 0) {
                    connectToServer(ipBuf, secretBuf, userBuf);
                }
                else {
                    connectionError = "Please fill in all fields";
                }
            }

            if (!connectionError.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", connectionError.c_str());
            }

            ImGui::EndChild();
        }
        else {
            // Full chat interface
            ImGui::Text("Connected as: %s", currentUsername.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 120);
            if (ImGui::Button("Disconnect", ImVec2(100, 0))) {
                disconnect();
            }
            ImGui::Separator();

            {
                std::lock_guard<std::mutex> lock(transferMutex);
                if (currentTransfer.active) {
                    float progress = (currentTransfer.filesize > 0) ?
                        (float)currentTransfer.transferred / (float)currentTransfer.filesize : 0.0f;
                    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), currentTransfer.status.c_str());
                }
            }

            // Split view: Chat on left, Files on right
            float filesPaneWidth = 300.0f;

            ImGui::BeginChild("ChatPane", ImVec2(-filesPaneWidth - 10, -140), false);
            {
                ImGui::BeginChild("Chat Log", ImVec2(0, 0), true);
                {
                    std::lock_guard<std::mutex> lock(chatMutex);
                    for (const auto& msg : chatLog) {
                        ImVec4 color = getColorForMessageType(msg.type);
                        ImGui::TextColored(color, "%s", msg.text.c_str());
                    }
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                        ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("FilesPane", ImVec2(filesPaneWidth, -140), true);
            {
                ImGui::Text("Available Files");
                ImGui::Separator();

                if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
                    requestFileList();
                }

                static int selectedFile = -1;

                ImGui::BeginChild("FilesList", ImVec2(0, -40), true);
                {
                    std::lock_guard<std::mutex> lock(filesMutex);

                    if (availableFiles.empty()) {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No files available");
                    }
                    else {
                        for (int i = 0; i < availableFiles.size(); i++) {
                            bool isSelected = (selectedFile == i);
                            if (ImGui::Selectable(availableFiles[i].c_str(), isSelected)) {
                                selectedFile = i;
                            }
                        }
                    }
                }
                ImGui::EndChild();

                {
                    std::lock_guard<std::mutex> lock(filesMutex);
                    if (selectedFile >= 0 && selectedFile < availableFiles.size()) {
                        if (ImGui::Button("Download Selected", ImVec2(-1, 0))) {
                            std::string filename = availableFiles[selectedFile];
                            std::thread([filename]() {
                                downloadFile(filename);
                                }).detach();
                        }
                    }
                    else {
                        ImGui::BeginDisabled();
                        ImGui::Button("Download Selected", ImVec2(-1, 0));
                        ImGui::EndDisabled();
                    }
                }
            }
            ImGui::EndChild();

            // Message input row
            if (ImGui::Button("Upload", ImVec2(80, 0))) {
                std::string filepath = openFileDialog();
                if (!filepath.empty()) {
                    std::thread([filepath]() {
                        uploadFile(filepath);
                        }).detach();
                }
            }

            ImGui::SameLine();
            ImGui::PushItemWidth(-90);
            if (ImGui::InputText("##msg", msgBuf, sizeof(msgBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (strlen(msgBuf) > 0) {
                    std::string fullMsg = std::string(currentUsername) + ": " + msgBuf;
                    SSL_write(ssl, fullMsg.c_str(), (int)fullMsg.length());
                    addChatMessage(fullMsg, MessageType::Chat);
                    msgBuf[0] = '\0';
                }
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Send", ImVec2(80, 0))) {
                if (strlen(msgBuf) > 0) {
                    std::string fullMsg = std::string(currentUsername) + ": " + msgBuf;
                    SSL_write(ssl, fullMsg.c_str(), (int)fullMsg.length());
                    addChatMessage(fullMsg, MessageType::Chat);
                    msgBuf[0] = '\0';
                }
            }
        }

        ImGui::End();



        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (connected) disconnect();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}