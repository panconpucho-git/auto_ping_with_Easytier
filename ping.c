#define _WIN32_WINNT 0x0600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define MAX_ADDR_LEN 256
#define TIMEOUT_MS   3000
#define PING_DATA    "PingTest"

IPAddr resolve_host(const char *host) {
    IPAddr addr = inet_addr(host);
    if (addr != INADDR_NONE) return addr;

    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &result) != 0) {
        return INADDR_NONE;
    }
    struct sockaddr_in *sin = (struct sockaddr_in *)result->ai_addr;
    addr = sin->sin_addr.s_addr;
    freeaddrinfo(result);
    return addr;
}

int ping_host(const char *host, char *ip_out) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        return -2;
    }

    IPAddr dest = resolve_host(host);
    if (dest == INADDR_NONE) {
        IcmpCloseHandle(hIcmp);
        return -2;
    }

    if (ip_out) {
        struct in_addr addr;
        addr.s_addr = dest;
        strcpy(ip_out, inet_ntoa(addr));
    }

    char replyBuffer[sizeof(ICMP_ECHO_REPLY) + 256] = {0};
    DWORD replySize = sizeof(replyBuffer);
    DWORD ret = IcmpSendEcho(
        hIcmp,
        dest,
        (LPVOID)PING_DATA,
        (DWORD)strlen(PING_DATA),
        NULL,
        replyBuffer,
        replySize,
        TIMEOUT_MS
    );

    IcmpCloseHandle(hIcmp);

    if (ret == 0) {
        return -1;
    }

    PICMP_ECHO_REPLY pReply = (PICMP_ECHO_REPLY)replyBuffer;
    return (int)pReply->RoundTripTime;
}

void copy_to_clipboard(const char *text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        size_t len = strlen(text) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            memcpy(GlobalLock(hMem), text, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

void get_exe_path(char *buffer, DWORD size) {
    GetModuleFileNameA(NULL, buffer, size);
    char *last = strrchr(buffer, '\\');
    if (last) *(last + 1) = '\0';
}

// 从 "协议://主机:端口" 中提取主机名（IP或域名）
void extract_host(const char *url, char *host) {
    const char *p = strstr(url, "://");
    if (!p) {
        strcpy(host, url);
        return;
    }
    p += 3;
    const char *colon = strrchr(p, ':');
    if (colon) {
        size_t len = colon - p;
        strncpy(host, p, len);
        host[len] = '\0';
    } else {
        strcpy(host, p);
    }
}

int main() {
    SetConsoleOutputCP(936);
    SetConsoleCP(936);

    char exePath[MAX_PATH];
    get_exe_path(exePath, sizeof(exePath));

    char configPath[MAX_PATH];
    snprintf(configPath, sizeof(configPath), "%sserver.txt", exePath);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup 失败\n");
        system("pause");
        return 1;
    }

    FILE *fp = fopen(configPath, "r");
    if (!fp) {
        printf("无法打开配置文件: %s\n", configPath);
        printf("请确保 server.txt 文件在程序同目录下。\n");
        printf("\n按任意键退出...");
        getchar();
        WSACleanup();
        return 1;
    }

    char line[MAX_ADDR_LEN];
    int valid = 0;
    int minDelay = -1;
    char bestLine[MAX_ADDR_LEN] = {0};      // 完整行（用于剪贴板）
    char bestHostPart[MAX_ADDR_LEN] = {0};  // 仅主机名（用于显示）

    #define MAX_HOSTS 100
    char hosts[MAX_HOSTS][MAX_ADDR_LEN];       // 原始行
    char hostParts[MAX_HOSTS][MAX_ADDR_LEN];   // 提取的主机名
    int delays[MAX_HOSTS];
    int total = 0;

    printf("正在 Ping 列表中的主机...\n");
    printf("配置文件: %s\n\n", configPath);

    while (fgets(line, sizeof(line), fp) && total < MAX_HOSTS) {
        char *p = strchr(line, '\n');
        if (p) *p = '\0';
        p = strchr(line, '\r');
        if (p) *p = '\0';
        if (strlen(line) == 0) continue;

        strcpy(hosts[total], line);

        char hostPart[MAX_ADDR_LEN];
        extract_host(line, hostPart);
        strcpy(hostParts[total], hostPart);

        char ipStr[16];
        int delay = ping_host(hostPart, ipStr);
        delays[total] = delay;

        if (delay >= 0) {
            printf("%-20s 延迟 = %d ms  (IP: %s)\n", hostPart, delay, ipStr);
            valid++;
            if (minDelay < 0 || delay < minDelay) {
                minDelay = delay;
                strcpy(bestLine, line);
                strcpy(bestHostPart, hostPart);
            }
        } else if (delay == -1) {
            printf("%-20s 请求超时\n", hostPart);
        } else {
            printf("%-20s 解析失败或错误\n", hostPart);
        }
        total++;
    }
    fclose(fp);

    printf("\n========== 延迟明细 ==========\n");
    for (int i = 0; i < total; i++) {
        if (delays[i] >= 0)
            printf("%-20s : %3d ms\n", hostParts[i], delays[i]);
        else if (delays[i] == -1)
            printf("%-20s : 超时\n", hostParts[i]);
        else
            printf("%-20s : 错误\n", hostParts[i]);
    }

    // ---------- 新增：本地测试 ----------
    printf("\n========== 本地测试 ==========\n");
    char ipLocal[16];
    int delayLocal = ping_host("127.0.0.1", ipLocal);
    if (delayLocal >= 0) {
        printf("127.0.0.1 延迟 = %d ms\n", delayLocal);
    } else if (delayLocal == -1) {
        printf("127.0.0.1 请求超时\n");
    } else {
        printf("127.0.0.1 解析失败或错误\n");
    }

    int delayLocal2 = ping_host("192.168.0.1", ipLocal);
    if (delayLocal2 >= 0) {
        printf("192.168.0.1 延迟 = %d ms\n", delayLocal2);
    } else if (delayLocal2 == -1) {
        printf("192.168.0.1 请求超时\n");
    } else {
        printf("192.168.0.1 解析失败或错误\n");
    }

    printf("\n================================\n");
    if (valid > 0) {
        printf("有效回复数: %d\n", valid);
        printf("最低延迟: %d ms  (地址: %s)\n", minDelay, bestHostPart);
        copy_to_clipboard(bestLine);
        printf("已将最低延迟服务地址 (%s) 复制到剪贴板。\n", bestLine);
    } else {
        printf("没有任何主机响应。\n");
    }
    printf("================================\n");

    printf("\n按任意键退出...");
    getchar();

    WSACleanup();
    return 0;
}