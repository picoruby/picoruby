#include "../include/net.h"
#include "lwipopts.h"
#include "lwip/dns.h"

//static void
//dns_found(const char *name, const ip_addr_t *ip, void *arg)
//{
//  ip_addr_t *result = (ip_addr_t *)arg;
//  if (ip) {
//    ip4_addr_copy(*result, *ip);
//  } else {
//    ip4_addr_set_loopback(result);
//  }
//}
//
//static err_t
//get_ip_impl(const char *name, ip_addr_t *ip)
//{
//  lwip_begin();
//  err_t err = dns_gethostbyname(name, ip, dns_found, ip);
//  lwip_end();
//  return err;
//}
// net.c
#include "lwip/timeouts.h"
#include "lwip/dns.h"
#include <unistd.h>

static volatile enum {
    DNS_PENDING,
    DNS_RESOLVED,  
    DNS_FAILED
} dns_result = DNS_PENDING;

static void dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    ip4_addr_t *result_ip = (ip4_addr_t*)callback_arg;
    
    if (ipaddr != NULL) {
        // 解決成功
        ip4_addr_copy(*result_ip, *ip_2_ip4(ipaddr));
        dns_result = DNS_RESOLVED;
        printf("DNS resolved: %s -> %s\n", name, ip4addr_ntoa(result_ip));
    } else {
        // 解決失敗
        dns_result = DNS_FAILED;
        printf("DNS resolution failed for: %s\n", name);
    }
}

static int get_ip_impl(const char *name, ip4_addr_t *ip) {
    err_t err;
    int timeout_count = 0;
    const int MAX_TIMEOUT = 50; // 5秒タイムアウト (50 * 100ms)
    
    lwip_begin();
    
    // DNS結果をリセット
    dns_result = DNS_PENDING;
    
    printf("Starting DNS resolution for: %s\n", name);
    
    // DNSクエリ開始
    err = dns_gethostbyname(name, ip, dns_found, ip);
    
    if (err == ERR_OK) {
        // すでにキャッシュされていて即座に解決
        printf("DNS resolved immediately from cache\n");
        return 0;
    } else if (err == ERR_INPROGRESS) {
        // 解決処理中 - ポーリングで待機
        printf("DNS query in progress, waiting for response...\n");
        
        while (dns_result == DNS_PENDING && timeout_count < MAX_TIMEOUT) {
            // LwIPのタイマー処理を実行（DNS含む）
            sys_check_timeouts();
            
            // 少し待機
            usleep(100000); // 100ms
            timeout_count++;
            
            if (timeout_count % 10 == 0) {
                printf("DNS waiting... (%d/5 seconds)\n", timeout_count / 10);
            }
        }
        
        if (dns_result == DNS_RESOLVED) {
            printf("DNS resolution successful\n");
            return 0;
        } else if (dns_result == DNS_FAILED) {
            printf("DNS resolution failed\n");
            return -1;
        } else {
            printf("DNS resolution timed out\n");
            return -1;
        }
    } else {
        printf("DNS query failed immediately with error: %d\n", err);
        return -1;
    }
}

err_t
Net_get_ip(const char *name, ip_addr_t *ip)
{
  err_t err = get_ip_impl(name, ip);
  if (err != ERR_OK) {
    return err;
  }
  while (!ip_addr_get_ip4_u32(ip)) {
    Net_sleep_ms(50);
  }
  return ERR_OK;
}


#if defined(PICORB_VM_MRUBY)

#include "mruby/net.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/net.c"

#endif
