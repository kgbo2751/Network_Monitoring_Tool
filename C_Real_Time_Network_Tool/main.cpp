#define _WIN32_WINNT 0x0600
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <netioapi.h>
#include <psapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma warning(disable:4996)

void print_boxed_title(const char* title) {
    int len = strlen(title);
    int width = 32;
    for (int i = 0; i < width; i++) printf("■");
    printf("\n■ %s", title);
    for (int i = 0; i < width - len - 3; i++) printf(" ");
    printf("■\n");
    for (int i = 0; i < width; i++) printf("■");
    printf("\n");
}

void get_network_speed(char* downloadStr, char* uploadStr) {
    PMIB_IFTABLE pIfTable = NULL;
    DWORD dwSize = 0, dwRetVal = 0;
    pIfTable = (MIB_IFTABLE*)malloc(sizeof(MIB_IFTABLE));
    if (!pIfTable) { strcpy(downloadStr, "N/A"); strcpy(uploadStr, "N/A"); return; }

    dwSize = sizeof(MIB_IFTABLE);
    if (GetIfTable(pIfTable, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
        free(pIfTable);
        pIfTable = (MIB_IFTABLE*)malloc(dwSize);
        if (!pIfTable) { strcpy(downloadStr, "N/A"); strcpy(uploadStr, "N/A"); return; }
    }

    dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
            MIB_IFROW row = pIfTable->table[i];
            if (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL &&
                (strstr((char*)row.bDescr, "Wi-Fi") || strstr((char*)row.bDescr, "Ethernet"))) {

                DWORD rxOld = row.dwInOctets;
                DWORD txOld = row.dwOutOctets;
                Sleep(3000);
                dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
                if (dwRetVal != NO_ERROR) break;

                row = pIfTable->table[i];
                DWORD rxNew = row.dwInOctets;
                DWORD txNew = row.dwOutOctets;

                DWORD downloadKBps = (rxNew > rxOld) ? (rxNew - rxOld) / 1024 : 0;
                DWORD uploadKBps = (txNew > txOld) ? (txNew - txOld) / 1024 : 0;

                sprintf(downloadStr, "%lu KB/s", downloadKBps);
                sprintf(uploadStr, "%lu KB/s", uploadKBps);

                free(pIfTable);
                return;
            }
        }
        strcpy(downloadStr, "0 KB/s");
        strcpy(uploadStr, "0 KB/s");
    }
    else {
        strcpy(downloadStr, "N/A");
        strcpy(uploadStr, "N/A");
    }
    free(pIfTable);
}

void get_ping_result(char* resultStr) {
    FILE* fp;
    char buffer[256];
    int total = 0, min = 9999, max = 0;

    for (int i = 0; i < 3; i++) {
        fp = _popen("ping -n 1 8.8.8.8", "r");
        if (!fp) { strcpy(resultStr, "Ping 실패"); return; }

        int found = 0;
        while (fgets(buffer, sizeof(buffer), fp)) {
            char* timePtr = strstr(buffer, "time=");
            if (!timePtr) timePtr = strstr(buffer, "시간=");
            if (timePtr) {
                int time;
                if (sscanf(timePtr, "time=%d", &time) == 1 || sscanf(timePtr, "시간=%d", &time) == 1) {
                    total += time;
                    if (time < min) min = time;
                    if (time > max) max = time;
                    found = 1;
                }
            }
        }
        _pclose(fp);
        if (!found) { strcpy(resultStr, "Ping 실패"); return; }
        Sleep(300);
    }
    sprintf(resultStr, "최소 = %dms, 최대 = %dms, 평균 = %dms", min, max, total / 3);
}

void get_packet_stats(ULONG* inPackets, ULONG* outPackets, ULONG* inErrors, ULONG* outErrors, double* lossRate) {
    PMIB_IFTABLE pIfTable = NULL;
    DWORD dwSize = sizeof(MIB_IFTABLE);
    pIfTable = (MIB_IFTABLE*)malloc(dwSize);
    if (!pIfTable) return;

    if (GetIfTable(pIfTable, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
        free(pIfTable);
        pIfTable = (MIB_IFTABLE*)malloc(dwSize);
        if (!pIfTable) return;
    }

    if (GetIfTable(pIfTable, &dwSize, FALSE) == NO_ERROR) {
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
            MIB_IFROW row = pIfTable->table[i];
            if (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL &&
                (strstr((char*)row.bDescr, "Wi-Fi") || strstr((char*)row.bDescr, "Ethernet"))) {
                *inPackets = row.dwInUcastPkts;
                *outPackets = row.dwOutUcastPkts;
                *inErrors = row.dwInErrors;
                *outErrors = row.dwOutErrors;

                ULONG totalPkts = row.dwInUcastPkts + row.dwOutUcastPkts;
                ULONG totalErrors = row.dwInErrors + row.dwOutErrors;
                if (totalPkts > 0) {
                    *lossRate = ((double)totalErrors / totalPkts) * 100.0;
                }
                else {
                    *lossRate = 0.0;
                }
                break;
            }
        }
    }
    free(pIfTable);
}

DWORD get_tcp_connection_count() {
    PMIB_TCPTABLE tcpTable;
    DWORD size = 0;
    GetTcpTable(NULL, &size, TRUE);
    tcpTable = (PMIB_TCPTABLE)malloc(size);
    if (!tcpTable) return 0;
    if (GetTcpTable(tcpTable, &size, TRUE) == NO_ERROR) {
        DWORD count = tcpTable->dwNumEntries;
        free(tcpTable);
        return count;
    }
    free(tcpTable);
    return 0;
}

DWORD get_udp_connection_count() {
    PMIB_UDPTABLE udpTable;
    DWORD size = 0;
    GetUdpTable(NULL, &size, TRUE);
    udpTable = (PMIB_UDPTABLE)malloc(size);
    if (!udpTable) return 0;
    if (GetUdpTable(udpTable, &size, TRUE) == NO_ERROR) {
        DWORD count = udpTable->dwNumEntries;
        free(udpTable);
        return count;
    }
    free(udpTable);
    return 0;
}

void print_interface_info() {
    PMIB_IFTABLE pIfTable;
    DWORD dwSize = 0;
    GetIfTable(NULL, &dwSize, FALSE);
    pIfTable = (MIB_IFTABLE*)malloc(dwSize);
    if (!pIfTable) return;
    if (GetIfTable(pIfTable, &dwSize, FALSE) == NO_ERROR) {
        printf("■네트워크 인터페이스■\n");
        printf("인터페이스 수: %lu\n", pIfTable->dwNumEntries);
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
            MIB_IFROW row = pIfTable->table[i];
            printf("[%lu] %s\n", i + 1, row.bDescr);
            printf("  상태: %s\n",
                row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL ? "연결됨" :
                row.dwOperStatus == IF_OPER_STATUS_DISCONNECTED ? "연결 안 됨" : "기타");
            printf("  속도: %.2f Mbps\n", row.dwSpeed / 1000000.0);
        }
        printf("\n");
    }
    free(pIfTable);
}

void print_process_network_usage() {
    printf("■프로세스별 네트워크 사용량■\n");

    DWORD size = 0;
    GetExtendedTcpTable(NULL, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    PMIB_TCPTABLE_OWNER_PID tableOld = (PMIB_TCPTABLE_OWNER_PID)malloc(size);
    GetExtendedTcpTable(tableOld, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    Sleep(3000);

    PMIB_TCPTABLE_OWNER_PID tableNew = (PMIB_TCPTABLE_OWNER_PID)malloc(size);
    GetExtendedTcpTable(tableNew, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    for (DWORD i = 0; i < tableNew->dwNumEntries; i++) {
        DWORD pid = tableNew->table[i].dwOwningPid;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        char procName[MAX_PATH] = "<알 수 없음>";
        if (hProc) {
            GetModuleFileNameExA(hProc, NULL, procName, MAX_PATH);
            CloseHandle(hProc);
        }

        // 알 수 없음은 출력하지 않음
        if (strcmp(procName, "<알 수 없음>") == 0)
            continue;

        // 실제 바이트 계산은 TCP 통계 API 필요 (여기서는 연결 개수만 표시)
        printf("PID: %lu, 프로세스: %s, TCP 연결: %lu\n", pid, procName, 1UL);
    }

    free(tableOld);
    free(tableNew);
}

void print_network_info() {
    print_boxed_title("실시간 네트워크 모니터링 도구");
    printf("종료하려면 0번을 입력하세요.\n\n");

    print_interface_info();

    char ipv4[16] = "", subnet[16] = "", gateway[16] = "", mac[32] = "", dns[16] = "";
    DWORD bufLen = sizeof(IP_ADAPTER_INFO);
    IP_ADAPTER_INFO* adapterInfo = (IP_ADAPTER_INFO*)malloc(bufLen);
    if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO*)malloc(bufLen);
    }
    if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
        IP_ADAPTER_INFO* adapter = adapterInfo;
        strcpy(ipv4, adapter->IpAddressList.IpAddress.String);
        strcpy(subnet, adapter->IpAddressList.IpMask.String);
        strcpy(gateway, adapter->GatewayList.IpAddress.String);
        sprintf(mac, "%02X-%02X-%02X-%02X-%02X-%02X",
            adapter->Address[0], adapter->Address[1], adapter->Address[2],
            adapter->Address[3], adapter->Address[4], adapter->Address[5]);
    }
    free(adapterInfo);

    FIXED_INFO* fixedInfo = (FIXED_INFO*)malloc(sizeof(FIXED_INFO));
    ULONG fixedInfoLen = sizeof(FIXED_INFO);
    if (GetNetworkParams(fixedInfo, &fixedInfoLen) == ERROR_BUFFER_OVERFLOW) {
        free(fixedInfo);
        fixedInfo = (FIXED_INFO*)malloc(fixedInfoLen);
    }
    if (GetNetworkParams(fixedInfo, &fixedInfoLen) == NO_ERROR) {
        strcpy(dns, fixedInfo->DnsServerList.IpAddress.String);
    }
    free(fixedInfo);

    char down[32], up[32], ping[64];
    get_network_speed(down, up);
    get_ping_result(ping);

    ULONG inPackets, outPackets, inErrors, outErrors;
    double lossRate;
    get_packet_stats(&inPackets, &outPackets, &inErrors, &outErrors, &lossRate);

    DWORD tcpCount = get_tcp_connection_count();
    DWORD udpCount = get_udp_connection_count();

    printf("■네트워크 주소■\n");
    printf("IPv4      → %s\n", ipv4);
    printf("서브넷    → %s\n", subnet);
    printf("게이트웨이→ %s\n", gateway);
    printf("MAC       → %s\n", mac);
    printf("DNS       → %s\n\n", dns);

    printf("■네트워크 성능■\n");
    printf("다운로드: %s, 업로드: %s\n", down, up);
    printf("핑: %s\n\n", ping);

    printf("■패킷■\n");
    printf("수신 패킷 수: %lu\n", inPackets);
    printf("송신 패킷 수: %lu\n", outPackets);
    printf("수신 오류 수: %lu\n", inErrors);
    printf("송신 오류 수: %lu\n", outErrors);
    printf("패킷 손실률: %.2f%%\n\n", lossRate);

    printf("■연결 상태■\n");
    printf("TCP 연결 수: %lu\n", tcpCount);
    printf("UDP 연결 수: %lu\n\n", udpCount);

    print_process_network_usage();
}

int main(void) {
    print_network_info();
    int choice;
    while (1) {
        scanf("%d", &choice);
        if (choice == 0) {
            printf("프로그램을 종료합니다.\n");
            break;
        }
        else {
            printf("잘못된 입력입니다. 0번만 입력 가능합니다.\n");
        }
    }
    return 0;
}