/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "mdns.h"
#include "mdns_private.h"
#include "inttypes.h"
#include "mdns_mem_caps.h"

static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};

static void mdns_print_results(mdns_result_t *results)
{
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1;
    while (r) {
        if (r->esp_netif) {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", i++, esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], r->ttl);
        }
        if (r->instance_name) {
            printf("  PTR : %s\n", r->instance_name);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%u] ", (int)r->txt_count);
            for (size_t t = 0; t < r->txt_count; t++) {
                printf("%s=%s; ", r->txt[t].key, r->txt[t].value);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }
}

static struct {
    struct arg_str *hostname;
    struct arg_int *timeout;
    struct arg_end *end;
} mdns_query_a_args;

#ifdef CONFIG_LWIP_IPV4
static int cmd_mdns_query_a(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_a_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_a_args.end, argv[0]);
        return 1;
    }

    const char *hostname = mdns_query_a_args.hostname->sval[0];
    int timeout = mdns_query_a_args.timeout->ival[0];

    if (!hostname || !hostname[0]) {
        printf("ERROR: Hostname not supplied\n");
        return 1;
    }

    if (timeout <= 0) {
        timeout = 1000;
    }

    printf("Query A: %s.local, Timeout: %d\n", hostname, timeout);

    struct esp_ip4_addr addr;
    addr.addr = 0;

    esp_err_t err = mdns_query_a(hostname, timeout,  &addr);
    if (err) {
        if (err == ESP_ERR_NOT_FOUND) {
            printf("ERROR: Host was not found!\n");
            return 0;
        }
        printf("ERROR: Query Failed\n");
        return 1;
    }

    printf(IPSTR "\n", IP2STR(&addr));

    return 0;
}

static void register_mdns_query_a(void)
{
    mdns_query_a_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Hostname that is searched for");
    mdns_query_a_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_a_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_a",
        .help = "Query MDNS for IPv4",
        .hint = NULL,
        .func = &cmd_mdns_query_a,
        .argtable = &mdns_query_a_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}
#endif /* CONFIG_LWIP_IPV4 */

#ifdef CONFIG_LWIP_IPV6
static int cmd_mdns_query_aaaa(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_a_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_a_args.end, argv[0]);
        return 1;
    }

    const char *hostname = mdns_query_a_args.hostname->sval[0];
    int timeout = mdns_query_a_args.timeout->ival[0];

    if (!hostname || !hostname[0]) {
        printf("ERROR: Hostname not supplied\n");
        return 1;
    }

    if (timeout <= 0) {
        timeout = 1000;
    }

    printf("Query AAAA: %s.local, Timeout: %d\n", hostname, timeout);

    struct esp_ip6_addr addr;
    memset(addr.addr, 0, 16);

    esp_err_t err = mdns_query_aaaa(hostname, timeout,  &addr);
    if (err) {
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Host was not found!\n");
            return 0;
        }
        printf("ERROR: Query Failed\n");
        return 1;
    }

    printf(IPV6STR "\n", IPV62STR(addr));

    return 0;
}

static void register_mdns_query_aaaa(void)
{
    mdns_query_a_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Hostname that is searched for");
    mdns_query_a_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_a_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_aaaa",
        .help = "Query MDNS for IPv6",
        .hint = NULL,
        .func = &cmd_mdns_query_aaaa,
        .argtable = &mdns_query_a_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}
#endif /* CONFIG_LWIP_IPV6 */

static struct {
    struct arg_str *instance;
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_int *timeout;
    struct arg_end *end;
} mdns_query_srv_args;

static int cmd_mdns_query_srv(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_srv_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_srv_args.end, argv[0]);
        return 1;
    }

    const char *instance = mdns_query_srv_args.instance->sval[0];
    const char *service = mdns_query_srv_args.service->sval[0];
    const char *proto = mdns_query_srv_args.proto->sval[0];
    int timeout = mdns_query_srv_args.timeout->ival[0];

    if (timeout <= 0) {
        timeout = 1000;
    }

    printf("Query SRV: %s.%s.%s.local, Timeout: %d\n", instance, service, proto, timeout);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_srv(instance, service, proto, timeout,  &results);
    if (err) {
        printf("ERROR: Query Failed\n");
        return 1;
    }
    if (!results) {
        printf("No results found!\n");
        return 0;
    }
    mdns_print_results(results);
    mdns_query_results_free(results);
    return 0;
}

static void register_mdns_query_srv(void)
{
    mdns_query_srv_args.instance = arg_str1(NULL, NULL, "<instance>", "Instance to search for");
    mdns_query_srv_args.service = arg_str1(NULL, NULL, "<service>", "Service to search for (ex. _http, _smb, etc.)");
    mdns_query_srv_args.proto = arg_str1(NULL, NULL, "<proto>", "Protocol to search for (_tcp, _udp, etc.)");
    mdns_query_srv_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_srv_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_srv",
        .help = "Query MDNS for Service SRV",
        .hint = NULL,
        .func = &cmd_mdns_query_srv,
        .argtable = &mdns_query_srv_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}

static struct {
    struct arg_str *instance;
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_int *timeout;
    struct arg_end *end;
} mdns_query_txt_args;

static int cmd_mdns_query_txt(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_txt_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_txt_args.end, argv[0]);
        return 1;
    }

    const char *instance = mdns_query_txt_args.instance->sval[0];
    const char *service = mdns_query_txt_args.service->sval[0];
    const char *proto = mdns_query_txt_args.proto->sval[0];
    int timeout = mdns_query_txt_args.timeout->ival[0];

    printf("Query TXT: %s.%s.%s.local, Timeout: %d\n", instance, service, proto, timeout);

    if (timeout <= 0) {
        timeout = 5000;
    }

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_txt(instance, service, proto, timeout,  &results);
    if (err) {
        printf("ERROR: Query Failed\n");
        return 1;
    }
    if (!results) {
        printf("No results found!\n");
        return 0;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
    return 0;
}

static void register_mdns_query_txt(void)
{
    mdns_query_txt_args.instance = arg_str1(NULL, NULL, "<instance>", "Instance to search for");
    mdns_query_txt_args.service = arg_str1(NULL, NULL, "<service>", "Service to search for (ex. _http, _smb, etc.)");
    mdns_query_txt_args.proto = arg_str1(NULL, NULL, "<proto>", "Protocol to search for (_tcp, _udp, etc.)");
    mdns_query_txt_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_txt_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_txt",
        .help = "Query MDNS for Service TXT",
        .hint = NULL,
        .func = &cmd_mdns_query_txt,
        .argtable = &mdns_query_txt_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_int *timeout;
    struct arg_int *max_results;
    struct arg_end *end;
} mdns_query_ptr_args;

static int cmd_mdns_query_ptr(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_ptr_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_ptr_args.end, argv[0]);
        return 1;
    }

    const char *service = mdns_query_ptr_args.service->sval[0];
    const char *proto = mdns_query_ptr_args.proto->sval[0];
    int timeout = mdns_query_ptr_args.timeout->ival[0];
    int max_results = mdns_query_ptr_args.max_results->ival[0];

    if (timeout <= 0) {
        timeout = 5000;
    }

    if (max_results <= 0 || max_results > 255) {
        max_results = 255;
    }

    printf("Query PTR: %s.%s.local, Timeout: %d, Max Results: %d\n", service, proto, timeout, max_results);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service, proto, timeout, max_results,  &results);
    if (err) {
        printf("ERROR: Query Failed\n");
        return 1;
    }
    if (!results) {
        printf("No results found!\n");
        return 0;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
    return 0;
}

static void register_mdns_query_ptr(void)
{
    mdns_query_ptr_args.service = arg_str1(NULL, NULL, "<service>", "Service to search for (ex. _http, _smb, etc.)");
    mdns_query_ptr_args.proto = arg_str1(NULL, NULL, "<proto>", "Protocol to search for (_tcp, _udp, etc.)");
    mdns_query_ptr_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_ptr_args.max_results = arg_int0("m", "max_results", "<max_results>", "Maximum results returned");
    mdns_query_ptr_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_ptr",
        .help = "Query MDNS for Service",
        .hint = NULL,
        .func = &cmd_mdns_query_ptr,
        .argtable = &mdns_query_ptr_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}

static struct {
    struct arg_str *hostname;
    struct arg_int *timeout;
    struct arg_int *max_results;
    struct arg_end *end;
} mdns_query_ip_args;

static int cmd_mdns_query_ip(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_ip_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_ip_args.end, argv[0]);
        return 1;
    }

    const char *hostname = mdns_query_ip_args.hostname->sval[0];
    int timeout = mdns_query_ip_args.timeout->ival[0];
    int max_results = mdns_query_ip_args.max_results->ival[0];

    if (!hostname || !hostname[0]) {
        printf("ERROR: Hostname not supplied\n");
        return 1;
    }

    if (timeout <= 0) {
        timeout = 1000;
    }

    if (max_results < 0 || max_results > 255) {
        max_results = 255;
    }

    printf("Query IP: %s.local, Timeout: %d, Max Results: %d\n", hostname, timeout, max_results);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query(hostname, NULL, NULL, MDNS_TYPE_ANY, timeout, max_results, &results);
    if (err) {
        printf("ERROR: Query Failed\n");
        return 1;
    }
    if (!results) {
        printf("No results found!\n");
        return 0;
    }
    mdns_print_results(results);
    mdns_query_results_free(results);

    return 0;
}

static void register_mdns_query_ip(void)
{
    mdns_query_ip_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Hostname that is searched for");
    mdns_query_ip_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_ip_args.max_results = arg_int0("m", "max_results", "<max_results>", "Maximum results returned");
    mdns_query_ip_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_ip",
        .help = "Query MDNS for IP",
        .hint = NULL,
        .func = &cmd_mdns_query_ip,
        .argtable = &mdns_query_ip_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}

static struct {
    struct arg_str *instance;
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_int *timeout;
    struct arg_int *max_results;
    struct arg_end *end;
} mdns_query_svc_args;

static int cmd_mdns_query_svc(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_query_svc_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_query_svc_args.end, argv[0]);
        return 1;
    }

    const char *instance = mdns_query_svc_args.instance->sval[0];
    const char *service = mdns_query_svc_args.service->sval[0];
    const char *proto = mdns_query_svc_args.proto->sval[0];
    int timeout = mdns_query_svc_args.timeout->ival[0];
    int max_results = mdns_query_svc_args.max_results->ival[0];

    if (timeout <= 0) {
        timeout = 5000;
    }

    if (max_results < 0 || max_results > 255) {
        max_results = 255;
    }

    printf("Query SVC: %s.%s.%s.local, Timeout: %d, Max Results: %d\n", instance, service, proto, timeout, max_results);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query(instance, service, proto, MDNS_TYPE_ANY, timeout, max_results,  &results);
    if (err) {
        printf("ERROR: Query Failed\n");
        return 1;
    }
    if (!results) {
        printf("No results found!\n");
        return 0;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
    return 0;
}

static void register_mdns_query_svc(void)
{
    mdns_query_svc_args.instance = arg_str1(NULL, NULL, "<instance>", "Instance to search for");
    mdns_query_svc_args.service = arg_str1(NULL, NULL, "<service>", "Service to search for (ex. _http, _smb, etc.)");
    mdns_query_svc_args.proto = arg_str1(NULL, NULL, "<proto>", "Protocol to search for (_tcp, _udp, etc.)");
    mdns_query_svc_args.timeout = arg_int0("t", "timeout", "<timeout>", "Timeout for this query");
    mdns_query_svc_args.max_results = arg_int0("m", "max_results", "<max_results>", "Maximum results returned");
    mdns_query_svc_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_query_svc",
        .help = "Query MDNS for Service TXT & SRV",
        .hint = NULL,
        .func = &cmd_mdns_query_svc,
        .argtable = &mdns_query_svc_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}

static struct {
    struct arg_str *hostname;
    struct arg_str *instance;
    struct arg_end *end;
} mdns_init_args;

static int cmd_mdns_init(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_init_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_init_args.end, argv[0]);
        return 1;
    }

    ESP_ERROR_CHECK(mdns_init());

    if (mdns_init_args.hostname->sval[0]) {
        ESP_ERROR_CHECK(mdns_hostname_set(mdns_init_args.hostname->sval[0]));
        printf("MDNS: Hostname: %s\n", mdns_init_args.hostname->sval[0]);
    }

    if (mdns_init_args.instance->count) {
        ESP_ERROR_CHECK(mdns_instance_name_set(mdns_init_args.instance->sval[0]));
        printf("MDNS: Instance: %s\n", mdns_init_args.instance->sval[0]);
    }

    return 0;
}

static void register_mdns_init(void)
{
    mdns_init_args.hostname = arg_str0("h", "hostname", "<hostname>", "Hostname that the server will advertise");
    mdns_init_args.instance = arg_str0("i", "instance", "<instance>", "Default instance name for services");
    mdns_init_args.end = arg_end(2);

    const esp_console_cmd_t cmd_init = {
        .command = "mdns_init",
        .help = "Start MDNS Server",
        .hint = NULL,
        .func = &cmd_mdns_init,
        .argtable = &mdns_init_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_init));
}

static int cmd_mdns_free(int argc, char **argv)
{
    mdns_free();
    return 0;
}

static void register_mdns_free(void)
{
    const esp_console_cmd_t cmd_free = {
        .command = "mdns_free",
        .help = "Stop MDNS Server",
        .hint = NULL,
        .func = &cmd_mdns_free,
        .argtable = NULL
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_free));
}

static struct {
    struct arg_str *hostname;
    struct arg_end *end;
} mdns_set_hostname_args;

static int cmd_mdns_set_hostname(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_set_hostname_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_set_hostname_args.end, argv[0]);
        return 1;
    }

    if (mdns_set_hostname_args.hostname->sval[0] == NULL) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }

    ESP_ERROR_CHECK(mdns_hostname_set(mdns_set_hostname_args.hostname->sval[0]));
    return 0;
}

static void register_mdns_set_hostname(void)
{
    mdns_set_hostname_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Hostname that the server will advertise");
    mdns_set_hostname_args.end = arg_end(2);

    const esp_console_cmd_t cmd_set_hostname = {
        .command = "mdns_set_hostname",
        .help = "Set MDNS Server hostname",
        .hint = NULL,
        .func = &cmd_mdns_set_hostname,
        .argtable = &mdns_set_hostname_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_set_hostname));
}

static struct {
    struct arg_str *instance;
    struct arg_end *end;
} mdns_set_instance_args;

static int cmd_mdns_set_instance(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_set_instance_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_set_instance_args.end, argv[0]);
        return 1;
    }

    if (mdns_set_instance_args.instance->sval[0] == NULL) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }

    ESP_ERROR_CHECK(mdns_instance_name_set(mdns_set_instance_args.instance->sval[0]));
    return 0;
}

static void register_mdns_set_instance(void)
{
    mdns_set_instance_args.instance = arg_str1(NULL, NULL, "<instance>", "Default instance name for services");
    mdns_set_instance_args.end = arg_end(2);

    const esp_console_cmd_t cmd_set_instance = {
        .command = "mdns_set_instance",
        .help = "Set MDNS Server Istance Name",
        .hint = NULL,
        .func = &cmd_mdns_set_instance,
        .argtable = &mdns_set_instance_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_set_instance));
}

static mdns_txt_item_t *_convert_items(const char **values, int count)
{
    int i = 0, e;
    const char *value = NULL;
    mdns_txt_item_t *items = (mdns_txt_item_t *) mdns_mem_malloc(sizeof(mdns_txt_item_t) * count);
    if (!items) {
        printf("ERROR: No Memory!\n");
        goto fail;

    }
    memset(items, 0, sizeof(mdns_txt_item_t) * count);

    for (i = 0; i < count; i++) {
        value = values[i];
        char *esign = strchr(value, '=');
        if (!esign) {
            printf("ERROR: Equal sign not found in '%s'!\n", value);
            goto fail;
        }
        int var_len = esign - value;
        int val_len = strlen(value) - var_len - 1;
        char *var = (char *)mdns_mem_malloc(var_len + 1);
        if (var == NULL) {
            printf("ERROR: No Memory!\n");
            goto fail;
        }
        char *val = (char *)mdns_mem_malloc(val_len + 1);
        if (val == NULL) {
            printf("ERROR: No Memory!\n");
            mdns_mem_free(var);
            goto fail;
        }
        memcpy(var, value, var_len);
        var[var_len] = 0;
        memcpy(val, esign + 1, val_len);
        val[val_len] = 0;

        items[i].key = var;
        items[i].value = val;
    }

    return items;

fail:
    for (e = 0; e < i; e++) {
        mdns_mem_free((char *)items[e].key);
        mdns_mem_free((char *)items[e].value);
    }
    mdns_mem_free(items);
    return NULL;
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_int *port;
    struct arg_str *instance;
    struct arg_str *host;
    struct arg_str *txt;
    struct arg_end *end;
} mdns_add_args;

static int cmd_mdns_service_add(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_add_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_add_args.end, argv[0]);
        return 1;
    }

    if (!mdns_add_args.service->sval[0] || !mdns_add_args.proto->sval[0] || !mdns_add_args.port->ival[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    const char *instance = NULL;
    if (mdns_add_args.instance->sval[0] && mdns_add_args.instance->sval[0][0]) {
        instance = mdns_add_args.instance->sval[0];
        printf("MDNS: Service Instance: %s\n", instance);
    }
    const char *host = NULL;
    if (mdns_add_args.host->count && mdns_add_args.host->sval[0]) {
        host = mdns_add_args.host->sval[0];
        printf("MDNS: Service for delegated host: %s\n", host);
    }
    mdns_txt_item_t *items = NULL;
    if (mdns_add_args.txt->count) {
        items = _convert_items(mdns_add_args.txt->sval, mdns_add_args.txt->count);
        if (!items) {
            printf("ERROR: No Memory!\n");
            return 1;

        }
    }

    ESP_ERROR_CHECK(mdns_service_add_for_host(instance, mdns_add_args.service->sval[0], mdns_add_args.proto->sval[0],
                                              host, mdns_add_args.port->ival[0], items, mdns_add_args.txt->count));
    mdns_mem_free(items);
    return 0;
}

static void register_mdns_service_add(void)
{
    mdns_add_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_add_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_add_args.port = arg_int1(NULL, NULL, "<port>", "Service Port");
    mdns_add_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_add_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_add_args.txt = arg_strn(NULL, NULL, "item", 0, 30, "TXT Items (name=value)");
    mdns_add_args.end = arg_end(2);

    const esp_console_cmd_t cmd_add = {
        .command = "mdns_service_add",
        .help = "Add service to MDNS",
        .hint = NULL,
        .func = &cmd_mdns_service_add,
        .argtable = &mdns_add_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_add));
}

static struct {
    struct arg_str *instance;
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_str *host;
    struct arg_end *end;
} mdns_remove_args;

static int cmd_mdns_service_remove(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_remove_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_remove_args.end, argv[0]);
        return 1;
    }

    if (!mdns_remove_args.service->sval[0] || !mdns_remove_args.proto->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }

    const char *instance = NULL;
    if (mdns_remove_args.instance->count && mdns_remove_args.instance->sval[0]) {
        instance = mdns_remove_args.instance->sval[0];
    }
    const char *host = NULL;
    if (mdns_remove_args.host->count && mdns_remove_args.host->sval[0]) {
        host = mdns_remove_args.host->sval[0];
    }

    ESP_ERROR_CHECK(mdns_service_remove_for_host(instance, mdns_remove_args.service->sval[0], mdns_remove_args.proto->sval[0], host));
    return 0;
}

static void register_mdns_service_remove(void)
{
    mdns_remove_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_remove_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_remove_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_remove_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_remove_args.end = arg_end(4);

    const esp_console_cmd_t cmd_remove = {
        .command = "mdns_service_remove",
        .help = "Remove service from MDNS",
        .hint = NULL,
        .func = &cmd_mdns_service_remove,
        .argtable = &mdns_remove_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_remove));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_str *instance;
    struct arg_str *host;
    struct arg_str *old_instance;
    struct arg_end *end;
} mdns_service_instance_set_args;

static int cmd_mdns_service_instance_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_service_instance_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_service_instance_set_args.end, argv[0]);
        return 1;
    }

    if (!mdns_service_instance_set_args.service->sval[0] || !mdns_service_instance_set_args.proto->sval[0] || !mdns_service_instance_set_args.instance->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    const char *host = NULL;
    if (mdns_service_instance_set_args.host->count && mdns_service_instance_set_args.host->sval[0]) {
        host = mdns_service_instance_set_args.host->sval[0];
    }
    const char *old_instance = NULL;
    if (mdns_service_instance_set_args.old_instance->count && mdns_service_instance_set_args.old_instance->sval[0]) {
        old_instance = mdns_service_instance_set_args.old_instance->sval[0];
    }
    esp_err_t err = mdns_service_instance_name_set_for_host(old_instance, mdns_service_instance_set_args.service->sval[0], mdns_service_instance_set_args.proto->sval[0], host, mdns_service_instance_set_args.instance->sval[0]);
    if (err != ESP_OK) {
        printf("mdns_service_instance_name_set_for_host() failed with %s\n", esp_err_to_name(err));
        return 1;
    }

    return 0;
}

static void register_mdns_service_instance_set(void)
{
    mdns_service_instance_set_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_service_instance_set_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_service_instance_set_args.instance = arg_str1(NULL, NULL, "<instance>", "Instance name");
    mdns_service_instance_set_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_service_instance_set_args.old_instance = arg_str0("i", "old_instance", "<old_instance>", "Instance name before update");
    mdns_service_instance_set_args.end = arg_end(4);

    const esp_console_cmd_t cmd_add = {
        .command = "mdns_service_instance_set",
        .help = "Set MDNS Service Instance Name",
        .hint = NULL,
        .func = &cmd_mdns_service_instance_set,
        .argtable = &mdns_service_instance_set_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_add));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_int *port;
    struct arg_str *host;
    struct arg_str *instance;
    struct arg_end *end;
} mdns_service_port_set_args;

static int cmd_mdns_service_port_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_service_port_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_service_port_set_args.end, argv[0]);
        return 1;
    }

    if (!mdns_service_port_set_args.service->sval[0] || !mdns_service_port_set_args.proto->sval[0] || !mdns_service_port_set_args.port->ival[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }

    const char *host = NULL;
    if (mdns_service_port_set_args.host->count && mdns_service_port_set_args.host->sval[0]) {
        host = mdns_service_port_set_args.host->sval[0];
    }
    const char *instance = NULL;
    if (mdns_service_port_set_args.instance->count && mdns_service_port_set_args.instance->sval[0]) {
        instance = mdns_service_port_set_args.instance->sval[0];
    }
    esp_err_t err = mdns_service_port_set_for_host(instance, mdns_service_port_set_args.service->sval[0], mdns_service_port_set_args.proto->sval[0], host, mdns_service_port_set_args.port->ival[0]);
    if (err != ESP_OK) {
        printf("mdns_service_port_set_for_host() failed with %s\n", esp_err_to_name(err));
        return 1;
    }
    return 0;
}

static void register_mdns_service_port_set(void)
{
    mdns_service_port_set_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_service_port_set_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_service_port_set_args.port = arg_int1(NULL, NULL, "<port>", "Service Port");
    mdns_service_port_set_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_service_port_set_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_service_port_set_args.end = arg_end(2);

    const esp_console_cmd_t cmd_add = {
        .command = "mdns_service_port_set",
        .help = "Set MDNS Service port",
        .hint = NULL,
        .func = &cmd_mdns_service_port_set,
        .argtable = &mdns_service_port_set_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_add));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_str *instance;
    struct arg_str *host;
    struct arg_str *txt;
    struct arg_end *end;
} mdns_txt_replace_args;

static int cmd_mdns_service_txt_replace(int argc, char **argv)
{
    mdns_txt_item_t *items = NULL;
    int nerrors = arg_parse(argc, argv, (void **) &mdns_txt_replace_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_txt_replace_args.end, argv[0]);
        return 1;
    }

    if (!mdns_txt_replace_args.service->sval[0] || !mdns_txt_replace_args.proto->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    const char *instance = NULL;
    if (mdns_txt_replace_args.instance->count && mdns_txt_replace_args.instance->sval[0]) {
        instance = mdns_txt_replace_args.instance->sval[0];
        printf("MDNS: Service Instance: %s\n", instance);
    }
    const char *host = NULL;
    if (mdns_txt_replace_args.host->count && mdns_txt_replace_args.host->sval[0]) {
        host = mdns_txt_replace_args.host->sval[0];
        printf("MDNS: Service for delegated host: %s\n", host);
    }
    if (mdns_txt_replace_args.txt->count) {
        items = _convert_items(mdns_txt_replace_args.txt->sval, mdns_txt_replace_args.txt->count);
        if (!items) {
            printf("ERROR: No Memory!\n");
            return 1;

        }
    }
    ESP_ERROR_CHECK(mdns_service_txt_set_for_host(instance, mdns_txt_replace_args.service->sval[0], mdns_txt_replace_args.proto->sval[0], host, items, mdns_txt_replace_args.txt->count));
    mdns_mem_free(items);
    return 0;
}

static void register_mdns_service_txt_replace(void)
{
    mdns_txt_replace_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_txt_replace_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_txt_replace_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_txt_replace_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_txt_replace_args.txt = arg_strn(NULL, NULL, "item", 0, 30, "TXT Items (name=value)");
    mdns_txt_replace_args.end = arg_end(5);

    const esp_console_cmd_t cmd_txt_set = {
        .command = "mdns_service_txt_replace",
        .help = "Replace MDNS service TXT items",
        .hint = NULL,
        .func = &cmd_mdns_service_txt_replace,
        .argtable = &mdns_txt_replace_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_txt_set));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_str *instance;
    struct arg_str *host;
    struct arg_str *var;
    struct arg_str *value;
    struct arg_end *end;
} mdns_txt_set_args;

static int cmd_mdns_service_txt_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_txt_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_txt_set_args.end, argv[0]);
        return 1;
    }

    if (!mdns_txt_set_args.service->sval[0] || !mdns_txt_set_args.proto->sval[0] || !mdns_txt_set_args.var->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    const char *instance = NULL;
    if (mdns_txt_set_args.instance->count && mdns_txt_set_args.instance->sval[0]) {
        instance = mdns_txt_set_args.instance->sval[0];
        printf("MDNS: Service Instance: %s\n", instance);
    }
    const char *host = NULL;
    if (mdns_txt_set_args.host->count && mdns_txt_set_args.host->sval[0]) {
        host = mdns_txt_set_args.host->sval[0];
        printf("MDNS: Service for delegated host: %s\n", host);
    }

    ESP_ERROR_CHECK(mdns_service_txt_item_set_for_host(instance, mdns_txt_set_args.service->sval[0], mdns_txt_set_args.proto->sval[0], host, mdns_txt_set_args.var->sval[0], mdns_txt_set_args.value->sval[0]));
    return 0;
}

static void register_mdns_service_txt_set(void)
{
    mdns_txt_set_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_txt_set_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_txt_set_args.var = arg_str1(NULL, NULL, "<var>", "Item Name");
    mdns_txt_set_args.value = arg_str1(NULL, NULL, "<value>", "Item Value");
    mdns_txt_set_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_txt_set_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_txt_set_args.end = arg_end(6);

    const esp_console_cmd_t cmd_txt_set = {
        .command = "mdns_service_txt_set",
        .help = "Add/Set MDNS service TXT item",
        .hint = NULL,
        .func = &cmd_mdns_service_txt_set,
        .argtable = &mdns_txt_set_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_txt_set));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_str *var;
    struct arg_str *instance;
    struct arg_str *host;
    struct arg_end *end;
} mdns_txt_remove_args;

static int cmd_mdns_service_txt_remove(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_txt_remove_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_txt_remove_args.end, argv[0]);
        return 1;
    }

    if (!mdns_txt_remove_args.service->sval[0] || !mdns_txt_remove_args.proto->sval[0] || !mdns_txt_remove_args.var->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    const char *instance = NULL;
    if (mdns_txt_remove_args.instance->count && mdns_txt_remove_args.instance->sval[0]) {
        instance = mdns_txt_remove_args.instance->sval[0];
    }
    const char *host = NULL;
    if (mdns_txt_remove_args.host->count && mdns_txt_remove_args.host->sval[0]) {
        host = mdns_txt_remove_args.host->sval[0];
    }
    ESP_ERROR_CHECK(mdns_service_txt_item_remove_for_host(instance, mdns_txt_remove_args.service->sval[0], mdns_txt_remove_args.proto->sval[0], host, mdns_txt_remove_args.var->sval[0]));
    return 0;
}

static void register_mdns_service_txt_remove(void)
{
    mdns_txt_remove_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_txt_remove_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_txt_remove_args.var = arg_str1(NULL, NULL, "<var>", "Item Name");
    mdns_txt_remove_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_txt_remove_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_txt_remove_args.end = arg_end(2);

    const esp_console_cmd_t cmd_txt_remove = {
        .command = "mdns_service_txt_remove",
        .help = "Remove MDNS service TXT item",
        .hint = NULL,
        .func = &cmd_mdns_service_txt_remove,
        .argtable = &mdns_txt_remove_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_txt_remove));
}

static int cmd_mdns_service_remove_all(int argc, char **argv)
{
    mdns_service_remove_all();
    return 0;
}

static void register_mdns_service_remove_all(void)
{
    const esp_console_cmd_t cmd_free = {
        .command = "mdns_service_remove_all",
        .help = "Remove all MDNS services",
        .hint = NULL,
        .func = &cmd_mdns_service_remove_all,
        .argtable = NULL
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_free));
}

#define MDNS_MAX_LOOKUP_RESULTS CONFIG_MDNS_MAX_SERVICES

static struct {
    struct arg_str *instance;
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_lit *delegated;
    struct arg_end *end;
} mdns_lookup_service_args;

static esp_err_t lookup_service(const char *instance, const char *service, const char *proto, size_t max_results,
                                mdns_result_t **result, bool delegated)
{
    if (delegated) {
        return mdns_lookup_delegated_service(instance, service, proto, max_results, result);
    }
    return mdns_lookup_selfhosted_service(instance, service, proto, max_results, result);
}

static int cmd_mdns_lookup_service(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_lookup_service_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_lookup_service_args.end, argv[0]);
        return 1;
    }

    if (!mdns_lookup_service_args.instance->sval[0] || !mdns_lookup_service_args.service->sval[0] || !mdns_lookup_service_args.proto->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    mdns_result_t *results = NULL;
    esp_err_t err = lookup_service(mdns_lookup_service_args.instance->count ? mdns_lookup_service_args.instance->sval[0] : NULL,
                                   mdns_lookup_service_args.service->sval[0], mdns_lookup_service_args.proto->sval[0],
                                   MDNS_MAX_LOOKUP_RESULTS, &results, mdns_lookup_service_args.delegated->count);
    if (err) {
        printf("Service lookup failed\n");
        return 1;
    }
    if (!results) {
        printf("No results found!\n");
        return 0;
    }
    mdns_print_results(results);
    mdns_query_results_free(results);
    return 0;
}

static void register_mdns_lookup_service(void)
{
    mdns_lookup_service_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_lookup_service_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_lookup_service_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_lookup_service_args.delegated = arg_lit0("d", "delegated", "Lookup delegated services");
    mdns_lookup_service_args.end = arg_end(4);

    const esp_console_cmd_t cmd_lookup_service = {
        .command = "mdns_service_lookup",
        .help = "Lookup registered service",
        .hint = NULL,
        .func = &cmd_mdns_lookup_service,
        .argtable = &mdns_lookup_service_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_lookup_service));
}

static struct {
    struct arg_str *hostname;
    struct arg_str *address;
    struct arg_end *end;
} mdns_delegate_host_args;

static int cmd_mdns_delegate_host(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_delegate_host_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_delegate_host_args.end, argv[0]);
        return 1;
    }

    if (!mdns_delegate_host_args.hostname->sval[0] || !mdns_delegate_host_args.address->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }

    mdns_ip_addr_t addr = { .next = NULL};
    esp_netif_str_to_ip4(mdns_delegate_host_args.address->sval[0], &addr.addr.u_addr.ip4);
    addr.addr.type = ESP_IPADDR_TYPE_V4;

    esp_err_t err = mdns_delegate_hostname_add(mdns_delegate_host_args.hostname->sval[0], &addr);
    if (err) {
        printf("mdns_delegate_hostname_add() failed\n");
        return 1;
    }
    return 0;
}

static void register_mdns_delegate_host(void)
{
    mdns_delegate_host_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Delegated hostname");
    mdns_delegate_host_args.address = arg_str1(NULL, NULL, "<address>", "Delegated hosts address");
    mdns_delegate_host_args.end = arg_end(2);

    const esp_console_cmd_t cmd_delegate_host = {
        .command = "mdns_delegate_host",
        .help = "Add delegated hostname",
        .hint = NULL,
        .func = &cmd_mdns_delegate_host,
        .argtable = &mdns_delegate_host_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_delegate_host));
}

static struct {
    struct arg_str *hostname;
    struct arg_end *end;
} mdns_undelegate_host_args;

static int cmd_mdns_undelegate_host(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_undelegate_host_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_undelegate_host_args.end, argv[0]);
        return 1;
    }

    if (!mdns_undelegate_host_args.hostname->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }

    if (mdns_delegate_hostname_remove(mdns_undelegate_host_args.hostname->sval[0]) != ESP_OK) {
        printf("mdns_delegate_hostname_remove() failed\n");
        return 1;
    }
    return 0;
}

static void register_mdns_undelegate_host(void)
{
    mdns_undelegate_host_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Delegated hostname");
    mdns_undelegate_host_args.end = arg_end(2);

    const esp_console_cmd_t cmd_undelegate_host = {
        .command = "mdns_undelegate_host",
        .help = "Remove delegated hostname",
        .hint = NULL,
        .func = &cmd_mdns_undelegate_host,
        .argtable = &mdns_undelegate_host_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_undelegate_host));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_str *sub;
    struct arg_str *instance;
    struct arg_str *host;
    struct arg_end *end;
} mdns_service_subtype_args;

static int cmd_mdns_service_subtype(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_service_subtype_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_service_subtype_args.end, argv[0]);
        return 1;
    }

    if (!mdns_service_subtype_args.service->sval[0] || !mdns_service_subtype_args.proto->sval[0] || !mdns_service_subtype_args.sub->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    const char *instance = NULL;
    if (mdns_service_subtype_args.instance->count && mdns_service_subtype_args.instance->sval[0]) {
        instance = mdns_service_subtype_args.instance->sval[0];
    }
    const char *host = NULL;
    if (mdns_service_subtype_args.host->count && mdns_service_subtype_args.host->sval[0]) {
        host = mdns_service_subtype_args.host->sval[0];
    }
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host(instance, mdns_service_subtype_args.service->sval[0], mdns_service_subtype_args.proto->sval[0], host, mdns_service_subtype_args.sub->sval[0]));
    return 0;
}

static void register_mdns_service_subtype_set(void)
{
    mdns_service_subtype_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_service_subtype_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_service_subtype_args.sub = arg_str1(NULL, NULL, "<sub>", "Subtype");
    mdns_service_subtype_args.instance = arg_str0("i", "instance", "<instance>", "Instance name");
    mdns_service_subtype_args.host = arg_str0("h", "host", "<hostname>", "Service for this (delegated) host");
    mdns_service_subtype_args.end = arg_end(5);

    const esp_console_cmd_t cmd_service_sub = {
        .command = "mdns_service_subtype",
        .help = "Adds subtype for service",
        .hint = NULL,
        .func = &cmd_mdns_service_subtype,
        .argtable = &mdns_service_subtype_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_service_sub));
}

static struct {
    struct arg_str *service;
    struct arg_str *proto;
    struct arg_end *end;
} mdns_browse_args;

static void mdns_browse_notifier(mdns_result_t *result)
{
    if (result) {
        mdns_print_results(result);
    }
}

static int cmd_mdns_browse(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_browse_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_browse_args.end, argv[0]);
        return 1;
    }

    if (!mdns_browse_args.service->sval[0] || !mdns_browse_args.proto->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    mdns_browse_t *handle = mdns_browse_new(mdns_browse_args.service->sval[0], mdns_browse_args.proto->sval[0], mdns_browse_notifier);
    return handle ? 0 : 1;
}

static void register_mdns_browse(void)
{
    mdns_browse_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_browse_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_browse_args.end = arg_end(2);

    const esp_console_cmd_t cmd_browse = {
        .command = "mdns_browse",
        .help = "Start browsing",
        .hint = NULL,
        .func = &cmd_mdns_browse,
        .argtable = &mdns_browse_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_browse));
}

static int cmd_mdns_browse_del(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &mdns_browse_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mdns_browse_args.end, argv[0]);
        return 1;
    }

    if (!mdns_browse_args.service->sval[0] || !mdns_browse_args.proto->sval[0]) {
        printf("ERROR: Bad arguments!\n");
        return 1;
    }
    esp_err_t err = mdns_browse_delete(mdns_browse_args.service->sval[0], mdns_browse_args.proto->sval[0]);
    return err == ESP_OK ? 0 : 1;
}

static void register_mdns_browse_del(void)
{
    mdns_browse_args.service = arg_str1(NULL, NULL, "<service>", "MDNS Service");
    mdns_browse_args.proto = arg_str1(NULL, NULL, "<proto>", "IP Protocol");
    mdns_browse_args.end = arg_end(2);

    const esp_console_cmd_t cmd_browse_del = {
        .command = "mdns_browse_del",
        .help = "Stop browsing",
        .hint = NULL,
        .func = &cmd_mdns_browse_del,
        .argtable = &mdns_browse_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_browse_del));
}

void mdns_console_register(void)
{
    register_mdns_init();
    register_mdns_free();
    register_mdns_set_hostname();
    register_mdns_set_instance();
    register_mdns_service_add();
    register_mdns_service_remove();
    register_mdns_service_instance_set();
    register_mdns_service_port_set();
    register_mdns_service_txt_replace();
    register_mdns_service_txt_set();
    register_mdns_service_txt_remove();
    register_mdns_service_remove_all();

    register_mdns_lookup_service();
    register_mdns_delegate_host();
    register_mdns_undelegate_host();
    register_mdns_service_subtype_set();

    register_mdns_browse();
    register_mdns_browse_del();

#ifdef CONFIG_LWIP_IPV4
    register_mdns_query_a();
#endif
#ifdef CONFIG_LWIP_IPV6
    register_mdns_query_aaaa();
#endif
    register_mdns_query_txt();
    register_mdns_query_srv();
    register_mdns_query_ptr();

    register_mdns_query_ip();
    register_mdns_query_svc();
}
