#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// 기본 설정
#define NO_SYS                      1
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    16384
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

// 타이머 설정 추가 (PANIC 에러 해결 핵심)
#define MEMP_NUM_SYS_TIMEOUT        10 

#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            (4 * (TCP_SND_BUF / TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0
#define LWIP_STATS                  0
#define SYS_LIGHTWEIGHT_PROT        0

// Wi-Fi (cyw43-driver) 관련 설정
#define LWIP_CHKSUM_ALGORITHM       3
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_IGMP                   1

// MQTT 관련 설정 (필수)
#define LWIP_ALTCP                  1
#define LWIP_ALTCP_TLS              0
#define LWIP_CALLBACK_API           1


#define MQTT_OUTPUT_RINGBUF_SIZE    1024
#define MQTT_VAR_HEADER_BUFFER_LEN  128

#endif