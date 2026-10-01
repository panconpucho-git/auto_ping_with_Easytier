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
#define TIMEOUT_MS   500
#define PING_DATA    "PingTest"

#define CONFIG_FILE_NAME "config.txt"
#define CONFIG_LINE_MAX  512

typedef struct {
    int language;
    int waitSeconds;
} Config;

static Config g_config = { 0, 8 };

typedef struct {
    const char *key;
    int        *pValue;
    int         defaultValue;
} ConfigField;

static ConfigField g_configFields[] = {
    { "Languages", &g_config.language,    0 },
    { "Wait",      &g_config.waitSeconds, 8 },
};

#define CONFIG_FIELD_COUNT (sizeof(g_configFields) / sizeof(g_configFields[0]))

static void config_trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') s[--len] = '\0';
        else break;
    }
}

static int config_find_field(const char *key) {
    for (size_t i = 0; i < CONFIG_FIELD_COUNT; i++) {
        if (_stricmp(key, g_configFields[i].key) == 0)
            return (int)i;
    }
    return -1;
}

static void config_set_defaults(void) {
    for (size_t i = 0; i < CONFIG_FIELD_COUNT; i++)
        *g_configFields[i].pValue = g_configFields[i].defaultValue;
}

static int config_parse_int(const char *val, int defVal) {
    if (!val || !*val) return defVal;

    const char *p = val;
    if (*p == '+' || *p == '-') p++;
    if (!*p) return defVal;

    for (const char *q = p; *q; q++) {
        if (*q < '0' || *q > '9')
            return defVal;
    }
    return atoi(val);
}

static void config_write(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    for (size_t i = 0; i < CONFIG_FIELD_COUNT; i++) {
        fprintf(fp, "%s=%d\n", g_configFields[i].key, *g_configFields[i].pValue);
    }
    fclose(fp);
}

static void config_load(const char *path) {
    config_set_defaults();

    FILE *fp = fopen(path, "r");
    if (!fp) {
        config_write(path);
        return;
    }

    char line[CONFIG_LINE_MAX];
    while (fgets(line, sizeof(line), fp)) {
        char buf[CONFIG_LINE_MAX];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        config_trim(buf);

        if (buf[0] == '\0' || buf[0] == '#' || buf[0] == ';')
            continue;

        char *eq = strchr(buf, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = buf;
        char *val = eq + 1;
        config_trim(key);
        config_trim(val);

        int idx = config_find_field(key);
        if (idx < 0) continue;

        *g_configFields[idx].pValue =
            config_parse_int(val, g_configFields[idx].defaultValue);
    }
    fclose(fp);

    if (g_config.language != 0 && g_config.language != 1)
        g_config.language = 0;

    if (g_config.waitSeconds < 0)
        g_config.waitSeconds = 8;

    config_write(path);
}

typedef struct {
    const char *wsa_fail;
    const char *srv_open_fail;
    const char *srv_hint;
    const char *press_any_key;
    const char *pinging;
    const char *config_file;
    const char *delay_ok;
    const char *timeout;
    const char *resolve_fail;
    const char *detail_header;
    const char *detail_ok;
    const char *detail_timeout;
    const char *detail_error;
    const char *local_header;
    const char *local_ok;
    const char *local_timeout;
    const char *local_error;
    const char *sep;
    const char *valid_count;
    const char *best_delay;
    const char *copied;
    const char *no_response;
    const char *auto_exit;
    const char *config_created;
    const char *server_created;
    const char *lang_hint;
} LangText;

static const LangText g_langCN = {
    "WSAStartup 失败\n",
    "无法打开配置文件: %s\n",
    "请确保 server.txt 文件在程序同目录下。\n",
    "\n按任意键退出...",
    "正在 Ping 列表中的主机...\n",
    "配置文件: %s\n\n",
    "%-20s 延迟 = %d ms  (IP: %s)\n",
    "%-20s 请求超时\n",
    "%-20s 解析失败或错误\n",
    "\n========== 延迟明细 ==========\n",
    "%-20s : %3d ms\n",
    "%-20s : 超时\n",
    "%-20s : 错误\n",
    "\n========== 本地测试 ==========\n",
    "%s 延迟 = %d ms\n",
    "%s 请求超时\n",
    "%s 解析失败或错误\n",
    "\n================================\n",
    "有效回复数: %d\n",
    "最低延迟: %d ms  (地址: %s)\n",
    "已将最低延迟服务地址 (%s) 复制到剪贴板。\n",
    "没有任何主机响应。\n",
    "\n程序将在%d秒后自动退出...",
    "已生成 config.txt，默认语言为中文。\n",
    "已生成 server.txt，请编辑该文件添加要 Ping 的地址（每行一个）。\n",
    "[提示] 修改 config.txt 中的 Languages 可切换语言：0 = 中文，1 = English\n\n"
};

static const LangText g_langEN = {
    "WSAStartup failed\n",
    "Cannot open config file: %s\n",
    "Please make sure server.txt is in the same directory as the program.\n",
    "\nPress any key to exit...",
    "Pinging hosts in list...\n",
    "Config file: %s\n\n",
    "%-20s delay = %d ms  (IP: %s)\n",
    "%-20s request timeout\n",
    "%-20s resolve failed or error\n",
    "\n========== Latency Details ==========\n",
    "%-20s : %3d ms\n",
    "%-20s : timeout\n",
    "%-20s : error\n",
    "\n========== Local Test ==========\n",
    "%s delay = %d ms\n",
    "%s request timeout\n",
    "%s resolve failed or error\n",
    "\n================================\n",
    "Valid replies: %d\n",
    "Lowest latency: %d ms  (address: %s)\n",
    "Copied the lowest-latency address (%s) to clipboard.\n",
    "No host responded.\n",
    "\nThe program will exit in %d seconds...",
    "config.txt has been created. Default language is Chinese.\n",
    "server.txt has been created. Please edit it to add addresses to ping (one per line).\n",
    "[Tip] Edit Languages in config.txt to switch language: 0 = Chinese, 1 = English\n\n"
};

static const LangText *g_L = &g_langCN;

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

int main(void) {
    SetConsoleOutputCP(936);
    SetConsoleCP(936);

    char exePath[MAX_PATH];
    get_exe_path(exePath, sizeof(exePath));

    char configPath[MAX_PATH];
    snprintf(configPath, sizeof(configPath), "%s%s", exePath, CONFIG_FILE_NAME);

    char serverPath[MAX_PATH];
    snprintf(serverPath, sizeof(serverPath), "%sserver.txt", exePath);

    int configExisted = (GetFileAttributesA(configPath) != INVALID_FILE_ATTRIBUTES);
    int serverExisted = (GetFileAttributesA(serverPath) != INVALID_FILE_ATTRIBUTES);

    if (!serverExisted) {
        FILE *f = fopen(serverPath, "w");
        if (f) {
            fprintf(f, "127.0.0.1\n");
            fclose(f);
        }
    }

    config_load(configPath);

    g_L = (g_config.language == 1) ? &g_langEN : &g_langCN;

    printf("%s", g_L->lang_hint);

    if (!configExisted) {
        printf("%s", g_L->config_created);
    }
    if (!serverExisted) {
        printf("%s", g_L->server_created);
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("%s", g_L->wsa_fail);
        system("pause");
        return 1;
    }

    FILE *fp = fopen(serverPath, "r");
    if (!fp) {
        printf(g_L->srv_open_fail, serverPath);
        printf("%s", g_L->srv_hint);
        printf("%s", g_L->press_any_key);
        getchar();
        WSACleanup();
        return 1;
    }

    char line[MAX_ADDR_LEN];
    int valid = 0;
    int minDelay = -1;
    char bestLine[MAX_ADDR_LEN] = {0};
    char bestHostPart[MAX_ADDR_LEN] = {0};

    #define MAX_HOSTS 100
    char hosts[MAX_HOSTS][MAX_ADDR_LEN];
    char hostParts[MAX_HOSTS][MAX_ADDR_LEN];
    int delays[MAX_HOSTS];
    int total = 0;

    printf("%s", g_L->pinging);
    printf(g_L->config_file, serverPath);

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
            printf(g_L->delay_ok, hostPart, delay, ipStr);
            valid++;
            if (minDelay < 0 || delay < minDelay) {
                minDelay = delay;
                strcpy(bestLine, line);
                strcpy(bestHostPart, hostPart);
            }
        } else if (delay == -1) {
            printf(g_L->timeout, hostPart);
        } else {
            printf(g_L->resolve_fail, hostPart);
        }
        total++;
    }
    fclose(fp);

    printf("%s", g_L->detail_header);
    for (int i = 0; i < total; i++) {
        if (delays[i] >= 0)
            printf(g_L->detail_ok, hostParts[i], delays[i]);
        else if (delays[i] == -1)
            printf(g_L->detail_timeout, hostParts[i]);
        else
            printf(g_L->detail_error, hostParts[i]);
    }

    printf("%s", g_L->local_header);
    {
        const char *localAddrs[2] = { "127.0.0.1", "192.168.0.1" };
        for (int i = 0; i < 2; i++) {
            char ipLocal[16];
            int d = ping_host(localAddrs[i], ipLocal);
            if (d >= 0)        printf(g_L->local_ok, localAddrs[i], d);
            else if (d == -1)  printf(g_L->local_timeout, localAddrs[i]);
            else               printf(g_L->local_error, localAddrs[i]);
        }
    }

    printf("%s", g_L->sep);
    if (valid > 0) {
        printf(g_L->valid_count, valid);
        printf(g_L->best_delay, minDelay, bestHostPart);
        copy_to_clipboard(bestLine);
        printf(g_L->copied, bestLine);
    } else {
        printf("%s", g_L->no_response);
    }
    printf("%s", g_L->sep);

    printf(g_L->auto_exit, g_config.waitSeconds);
    Sleep((DWORD)g_config.waitSeconds * 1000);

    WSACleanup();
    return 0;
}