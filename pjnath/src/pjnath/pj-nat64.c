
#include <pj/types.h>
#include <pjsip-ua/sip_inv.h>
#include <pjsip/sip_types.h>
#include <pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#include <pjnath/pj-nat64.h>

#define THIS_FILE "pj_nat64.c"

/* Interface */

static void patch_sdp_attr(pj_pool_t *pool, pjmedia_sdp_media *media, pjmedia_sdp_attr *attr, pj_str_t addr_type, pj_str_t addr);
static void patch_sdp_addr(pj_pool_t *pool, pjmedia_sdp_session *sdp, pj_str_t addr_type, pj_str_t addr);

static void patch_msg_sdp_body_old(pj_pool_t *pool, pjsip_msg *msg, pjmedia_sdp_session *sdp);
static void patch_msg_sdp_body(pj_pool_t *pool, pjsip_msg *msg, pjmedia_sdp_session *sdp);

static void modern_sip_msg_dump(const char *title, pjsip_msg *msg);
static void modern_sip_msg_sdp_dump(const char *title, pjsip_msg *msg);
static void modern_sdp_dump(const char *title, pjmedia_sdp_session *sdp);

static pjmedia_sdp_session *modern_sip_sdp_parse(pj_pool_t *pool, pjsip_msg *msg);

static void modern_update_rx_data(pjsip_rx_data *rdata, pjmedia_sdp_session *sdp);
static void modern_update_tx_data(pjsip_tx_data *tdata, pjsip_msg *msg);

static void patch_sdp_ipv6_with_ipv4(pjsip_tx_data *tdata, pjsip_msg *msg);
static void patch_sdp_ipv4_with_ipv6(pjsip_rx_data *rdata);

/* Consts */

static pj_caching_pool cp;
static pj_bool_t mod_enable = PJ_TRUE;
static pj_bool_t mod_debug = PJ_FALSE;
static pj_pool_t *mod_pool = NULL;
static pj_str_t hpbx_in6_addr = { .slen = 0, .ptr = NULL };
static pj_str_t hpbx_in4_addr = { .slen = 0, .ptr = NULL };
static pj_str_t acc_in6_addr = { .slen = 0, .ptr = NULL };
static pj_str_t acc_in4_addr = { .slen = 0, .ptr = NULL };

/* Implementation */

/**
 * Modern SDP replace routine on SDP attribute
 *
**/
static void patch_sdp_attr(pj_pool_t *pool, pjmedia_sdp_media *media, pjmedia_sdp_attr *attr, pj_str_t addr_type, pj_str_t addr)
{
    char rtcp_buf[128] = { 0 };
    pj_str_t rtcp_name = pj_str("rtcp");
    pjmedia_sdp_rtcp_attr ra;
    pj_status_t s2;

    if ((attr == NULL) || (attr->name.slen == 0)) {
        return;
    }

    PJ_LOG(5, (THIS_FILE, "Process SDP attribute %.*s", attr->name.slen, attr->name.ptr));

    if (strncmp(attr->name.ptr, rtcp_name.ptr, rtcp_name.slen) == 0) {
        s2 = pjmedia_sdp_attr_get_rtcp(attr, &ra);
        if (s2 != PJ_SUCCESS) {
            PJ_LOG(1, (THIS_FILE, "Error: Patch SDP attribute 'rtcp'"));
            return;
        }

        /* Patch */
        ra.addr_type = addr_type;
        ra.addr = addr;

        // a=rtcp:4001 IN IP6 2001:db8::1
        pj_ansi_snprintf(rtcp_buf, 128, "%u %.*s %.*s %.*s",
            ra.port,
            ra.net_type.slen, ra.net_type.ptr,
            ra.addr_type.slen, ra.addr_type.ptr,
            ra.addr.slen, ra.addr.ptr
        );
        //
        PJ_LOG(5, (THIS_FILE, "Patch SDP attribute rtcp = %s", rtcp_buf));

        // Update value
        pj_strdup2(pool, &attr->value, rtcp_buf);
    }

    // TODO - other attribute may process here...

}

/**
 * Replace SDP address
 *
**/
static void patch_sdp_addr(pj_pool_t *pool, pjmedia_sdp_session *sdp, pj_str_t addr_type, pj_str_t addr)
{

    /* Debug message */
    PJ_LOG(5, (THIS_FILE, "Patch SDP body: TYPE -> %.*s ADDR -> %.*s",
        addr_type.slen, addr_type.ptr,
        addr.slen, addr.ptr
    ));

    /* Patch origin */
    sdp->origin.addr_type = addr_type;
    sdp->origin.addr = addr;

    /* Patch connection */
    if (sdp->conn != NULL) {
        pjmedia_sdp_conn *conn = sdp->conn;
        /* Update connection */
        conn->addr_type = addr_type;
        conn->addr = addr;
    }

    /* Patch attributes */
    for(unsigned idx=0; idx < sdp->attr_count; idx++) {
        pjmedia_sdp_attr *attr = sdp->attr[idx];
        patch_sdp_attr(pool, NULL, attr, addr_type, addr);
    }

    /* Patch each media */
    for(unsigned idx=0; idx < sdp->media_count; idx++) {
        pjmedia_sdp_media *media = sdp->media[idx];
        if (media->conn != NULL) {
            pjmedia_sdp_conn *conn = media->conn;
            /* Update connection */
            conn->addr_type = addr_type;
            conn->addr = addr;
        }
        for(unsigned idx2=0; idx2 < sdp->attr_count; idx2++) {
            pjmedia_sdp_attr *attr = media->attr[idx2];
            patch_sdp_attr(pool, media, attr, addr_type, addr);
        }
    }

}

/**
 * Update TX data
 *
**/
static void modern_update_tx_data(pjsip_tx_data *tdata, pjsip_msg *msg)
{
    pj_pool_t *pool = tdata->pool;
    char *payload = NULL;
    int size = 0;

    /* Step 1. Update TX buffer */
    if (tdata->buf.start == NULL) {
        PJ_LOG(1, (THIS_FILE, "Error TX buffer does not create by sender"));
        return;
    }
    tdata->buf.cur = tdata->buf.start;
    tdata->buf.end = tdata->buf.start + PJSIP_MAX_PKT_LEN;
    size = pjsip_msg_print(msg, tdata->buf.start, tdata->buf.end - tdata->buf.start);
    if (size <= 0) {
        PJ_LOG(1, (THIS_FILE, "Error print SIP message"));
        return;
    }
    tdata->buf.cur[size] = '\0';
    tdata->buf.cur += size;

}

/**
 * Modern SDP replace routine
 *
**/
static void patch_sdp_ipv6_with_ipv4(pjsip_tx_data *tdata, pjsip_msg *msg)
{
    pj_pool_t *pool = tdata->pool;
    pj_status_t status;
    pjmedia_sdp_session *sdp = NULL;
    pjmedia_sdp_session *new_sdp = NULL;
    int size = 0;
    pj_str_t addr_type;
    pj_str_t addr;
    pj_str_t sip_msg;
    pj_str_t sdp_payload;
    pj_str_t new_sdp_payload;
    pj_str_t new_sip_msg;

    /* Step 1. Determine our IPv4 address */
    pj_strdup2(pool, &addr_type, "IP4");
    pj_strdup(pool, &addr, &acc_in4_addr);

    /* Step 2. Get SIP message SDP body */
    pjsip_tdata_sdp_info *sdp_info = pjsip_tdata_get_sdp_info(tdata);
    sdp = sdp_info->sdp;
    if (sdp == NULL) {
        PJ_LOG(5, (THIS_FILE, "No SDP info on TX"));
        return;
    }
    new_sdp = pjmedia_sdp_session_clone(pool, sdp);

    /* Step 3. Process SDP body attrs */
    patch_sdp_addr(pool, new_sdp, addr_type, addr);

    /* Step 4. Patch SDP body */
    patch_msg_sdp_body(pool, msg, new_sdp);

}

/**
 * Patch SIP message with new SDP body
 *
**/
static void patch_msg_sdp_body(pj_pool_t *pool, pjsip_msg *msg, pjmedia_sdp_session *sdp)
{
    pj_status_t status;

    /* Step 0. Debug message */
    PJ_LOG(5, (THIS_FILE, "Patch SDP body"));

    /* Step 1. Remove SIP automatic update headers */
    while (pjsip_msg_find_remove_hdr(msg, PJSIP_H_CONTENT_LENGTH, NULL) != NULL);
    while (pjsip_msg_find_remove_hdr(msg, PJSIP_H_CONTENT_TYPE, NULL) != NULL);

    /* Step 2. Update SDP body */
    status = pjsip_create_sdp_body(pool, sdp, &msg->body);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Error: Create SDP structure"));
        return;
    }

}

static void modern_sip_msg_dump(const char *title, pjsip_msg *msg)
{
    char packet[PJSIP_MAX_PKT_LEN] = { 0 };
    int size;

    /* Step 0. Check debug mode */
    if (!mod_debug) {
        return;
    }

    /* Step 1. Print SIP message */
    size = pjsip_msg_print(msg, packet, PJSIP_MAX_PKT_LEN);
    if (size <= 0) {
        PJ_LOG(1, (THIS_FILE, "Error dump SIP message"));
        return;
    }

    /* Step 2. Debug SIP message */
    PJ_LOG(4, (THIS_FILE, "--- BEGIN %s ---\n%.*s\n--- END %s ---", title, size, packet, title));

}

static pjmedia_sdp_session *modern_sip_sdp_parse(pj_pool_t *pool, pjsip_msg *msg)
{
    pj_status_t status;
    char *packet = NULL;
    char *payload = NULL;
    int size;
    pjmedia_sdp_session *sdp = NULL;
    pjsip_msg *tmp_msg;

    /* Step 1. Print SIP message */
    packet = pj_pool_alloc(pool, PJSIP_MAX_PKT_LEN);
    size = pjsip_msg_print(msg, packet, PJSIP_MAX_PKT_LEN);
    if (size <= 0) {
        PJ_LOG(1, (THIS_FILE, "Error print SIP message"));
        return NULL;
    }

    /* Step 2. Parse SIP message */
    tmp_msg = pjsip_parse_msg(pool, packet, size, NULL);
    if (tmp_msg == NULL) {
        PJ_LOG(1, (THIS_FILE, "Error parse SIP message"));
        return NULL;
    }

    /* Step 3. Check SDP body exists */
    if (tmp_msg->body == NULL) {
        PJ_LOG(5, (THIS_FILE, "SIP message without SIP payload"));
        return NULL;
    }

    /* Step 4. Print SDP body */
    payload = pj_pool_alloc(pool, PJSIP_MAX_PKT_LEN);
    size = pjsip_print_text_body(tmp_msg->body, payload, PJSIP_MAX_PKT_LEN);
    if (size <= 0) {
        PJ_LOG(1, (THIS_FILE, "Error print SIP message"));
        return NULL;
    }

    /* Step 5. Parse SDP body */
    status = pjmedia_sdp_parse(pool, payload, size, &sdp);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Error parse SDP body"));
        return NULL;
    }

    /* Step 6. Done */
    return sdp;

}

static void modern_sdp_dump(const char *title, pjmedia_sdp_session *sdp)
{
    char payload[PJSIP_MAX_PKT_LEN] = { 0 };
    int size;

    /* Step 0. Check debug mode */
    if (!mod_debug) {
        return;
    }

    /* Step 1. Print SDP payload */
    size = pjmedia_sdp_print(sdp, payload, PJSIP_MAX_PKT_LEN);
    if (size <= 0) {
        PJ_LOG(1, (THIS_FILE, "Error dump SDP body"));
        return;
    }

    /* Step 2. Debug SDP payload */
    PJ_LOG(4, (THIS_FILE, "--- BEGIN %s ---\n%.*s\n--- END %s ---\n", title, size, payload, title));

}

static void modern_update_rx_data(pjsip_rx_data *rdata, pjmedia_sdp_session *sdp)
{
    pj_pool_t *pool = rdata->tp_info.pool;
    char *payload = NULL;
    int size = 0;
    pjsip_msg *msg = rdata->msg_info.msg;

    /* Step 1. Update SDP parameters */
    pjsip_rdata_sdp_info *sdp_info = pjsip_rdata_get_sdp_info(rdata);
    modern_sdp_dump("RX SDP INFO", sdp_info->sdp);
    sdp_info->sdp = sdp;
    modern_sdp_dump("NEW RX SDP INFO", sdp_info->sdp);
    if (sdp != NULL) {
        payload = pj_pool_alloc(pool, PJSIP_MAX_PKT_LEN);
        size = pjmedia_sdp_print(sdp, payload, PJSIP_MAX_PKT_LEN);
    }
    sdp_info->body.ptr = payload;
    sdp_info->body.slen = size;

}

/**
 * Modern replace
 *
**/
static void patch_sdp_ipv4_with_ipv6(pjsip_rx_data *rdata)
{
    pj_pool_t *pool = rdata->tp_info.pool;
    pj_status_t status;
    pjmedia_sdp_session *sdp = NULL;
    pjmedia_sdp_session *new_sdp = NULL;
    pjsip_msg *msg = NULL;
    pj_str_t sip_msg;
    pj_str_t new_sip_msg;
    pj_str_t sdp_payload;
    pj_str_t new_sdp_payload;
    pj_str_t addr_type;
    pj_str_t addr;
    int size;

    /* Step 1. Prepare HPBX server IPv6 address */
    pj_strdup2(pool, &addr_type, "IP6");
    pj_strdup(pool, &addr, &hpbx_in6_addr);

    /* Step 2. Get SIP message SDP body */
    pjsip_rdata_sdp_info *sdp_info = pjsip_rdata_get_sdp_info(rdata);
    sdp = sdp_info->sdp;
    if (sdp == NULL) {
        PJ_LOG(5, (THIS_FILE, "No SDP info on RX"));
        return;
    }
    new_sdp = pjmedia_sdp_session_clone(pool, sdp);

    /* Step 3. Patch SDP address */
    patch_sdp_addr(pool, new_sdp, addr_type, addr);

    /* Step 4. Update SIP message with new SDP body */
    patch_msg_sdp_body(pool, rdata->msg_info.msg, new_sdp);

    /* Step 5. Update SIP message */
    modern_update_rx_data(rdata, new_sdp);

}

#if defined(USE_RFC6052) && (USE_RFC6052 == 1)
static void map_ipv4_with_ipv6(pj_pool_t *pool, pj_str_t *dst, pj_str_t *src)
{
    pj_sockaddr src_addr;
    pj_sockaddr dst_addr;
    char prefix[4] = { 0x00, 0x64, 0xff, 0x9b }; // IPv6 map "64:ff96::/96"

    if (0 == pj_strcmp2(src, "127.0.0.1")) {
        pj_strdup2(pool, dst, "[::1]");
    } else {
        /* Step 1. Parse source IP address */
        // TODO - ...

        if (src_addr.addr.sa_family == pj_AF_INET()) {

            /* Step 1. Initialize result */
            dst_addr.addr.sa_family = pj_AF_INET6();
            dst_addr.ipv6.sin6_family = pj_AF_INET6();
            dst_addr.ipv6.sin6_port = src_addr->ipv4.sin_port;
            dst_addr.ipv6.sin6_flowinfo = 0;
            dst_addr.ipv6.sin6_scope_id = 0;

            /* Step 2. Map address with "::/96" subnet */
            memset(&dst_addr.ipv6.sin6_addr, 0, sizeof(dst_addr.ipv6.sin6_addr));
            memcpy(&dst_addr.ipv6.sin6_addr, prefix, sizeof(prefix));
            memcpy(&dst_addr.ipv6.sin6_addr + 10, &src_addr.ipv4.sin_addr, sizeof(src_addr.ipv4.sin_addr));

            /* Step 3. Print result */
            // TODO - print address at destination...

        } else {
            PJ_LOG(4, (THIS_FILE, "Map IPv4 -> IPv6 address: provide IPv6 address: '%.*s'",
                (int)src.slen. src.ptr
            ));
        }
    }

    /* Step 2. Debug message */
    PJ_LOG(4, (THIS_FILE, "AMap IPv4 -> IPv6 address %.*s -> %.*s",
        src->slen, src->ptr,
        dst->slen, dst->ptr
    ));

}
#else
static void map_ipv4_with_ipv6(pj_pool_t *pool, pj_str_t *dst, pj_str_t *src)
{

    /* Step 1. Map */
    if (0 == pj_strcmp2(src, "127.0.0.1")) {
        pj_strdup2(pool, dst, "[::1]");
    } else if (0 == pj_strcmp(src, &acc_in4_addr)) {
        pj_strdup(pool, dst, &acc_in6_addr);
    } else if (0 == pj_strcmp(src, &hpbx_in4_addr)) {
        pj_strdup(pool, dst, &hpbx_in6_addr);
    } else {
        pj_strdup(pool, dst, &hpbx_in6_addr);
    }

    /* Step 2. Debug message */
    PJ_LOG(4, (THIS_FILE, "TMap IPv4 -> IPv6 address %.*s -> %.*s",
        src->slen, src->ptr,
        dst->slen, dst->ptr
    ));

}
#endif

#if defined(USE_RFC6052) && (USE_RFC6052 == 1)
static void map_ipv6_with_ipv4(pj_pool_t *pool, pj_str_t *dst, pj_str_t *src)
{
    char tmp[128] = { 0 };
    pj_sockaddr src_addr;
    pj_sockaddr dst_addr;
    pj_status_t status;

    /* Step 1. Map */
    if (0 == pj_strcmp2(src, "[::1]")) {
        pj_strdup2(pool, dst, "127.0.0.1");
    } else {

        int af = pj_AF_INET6();
        unsigned options = 0;
        status = pj_sockaddr_parse(af, options, (const pj_str_t *)src, &src_addr);
        if (status != PJ_SUCCESS) {
            // TODO - What on error? Try resolve?
        }

        if (src_addr.addr.sa_family == pj_AF_INET6()) {

// TODO - check `pj_sockaddr_synthesize` ...

            /* Step 1. Initialize result */
            dst_addr.addr.sa_family = pj_AF_INET();
//            dst_addr.ipv6.sin6_family = pj_AF_INET();
//            dst_addr.ipv6.sin6_port = src_addr->ipv6.sin_port;
//            dst_addr.ipv6.sin6_flowinfo = 0;
//            dst_addr.ipv6.sin6_scope_id = 0;

            /* Step 2. Map address with "::/96" subnet */
//            memset(&dst_addr.ipv6.sin6_addr, 0, sizeof(dst_addr.ipv6.sin6_addr));
//            memcpy(&dst_addr.ipv6.sin6_addr, prefix, sizeof(prefix));
//            memcpy(&dst_addr.ipv6.sin6_addr + 10, &src_addr.ipv4.sin_addr, sizeof(src_addr.ipv4.sin_addr));

            /* Step 3. Print result */
            // TODO - print address at destination...
            unsigned flags = 2;
            pj_sockaddr_print(&dst_addr, tmp, 128, flags);
            pj_strdup(pool, dst, tmp);

        } else {
            PJ_LOG(4, (THIS_FILE, "AMap IPv6 -> IPv4 address: provide IPv4 address: '%.*s'",
                (int)src.slen. src.ptr
            ));
        }
    }

    /* Step 2. Debug message */
    PJ_LOG(4, (THIS_FILE, "AMap IPv6 -> IPv4 address '%.*s' -> '%.*s'",
        src->slen, src->ptr,
        dst->slen, dst->ptr
    ));

}
#else
static void map_ipv6_with_ipv4(pj_pool_t *pool, pj_str_t *dst, pj_str_t *src)
{

    /* Step 1. Map */
    if (0 == pj_strcmp2(src, "[::1]")) {
        pj_strdup2(pool, dst, "127.0.0.1");
    } else if (0 == pj_strcmp(src, &acc_in6_addr)) {
        pj_strdup(pool, dst, &acc_in4_addr);
    } else if (0 == pj_strcmp(src, &hpbx_in6_addr)) {
        pj_strdup(pool, dst, &hpbx_in4_addr);
    } else {
        pj_strdup(pool, dst, &hpbx_in4_addr);
    }

    /* Step 2. Debug message */
    PJ_LOG(4, (THIS_FILE, "TMap IPv6 -> IPv4 address '%.*s' -> '%.*s'",
        src->slen, src->ptr,
        dst->slen, dst->ptr
    ));

}
#endif

/**
 * Replace SIP message "Record-Route" headers
 *
**/
static void patch_record_route_ipv4_with_ipv6(pjsip_rx_data *rdata)
{
    pj_pool_t *pool = rdata->tp_info.pool;
    pjsip_route_hdr *prev_rr_hdr = NULL;
    pjsip_route_hdr *update_rr_hdr = NULL;
    pjsip_msg *msg = rdata->msg_info.msg;
    pj_str_t addr;
    pjsip_hdr *hdr = (pjsip_hdr *)msg->hdr.next;
    pjsip_hdr *end = &msg->hdr;

    /* Step 1. Search */
    for (; hdr != end; hdr = hdr->next) {
        if (hdr->type == PJSIP_H_RECORD_ROUTE) {
            pjsip_route_hdr *current_rr_hdr = (pjsip_route_hdr *)hdr;
            prev_rr_hdr = update_rr_hdr;
            update_rr_hdr = current_rr_hdr;
        }
    }

    /* Step 2. Update previus */
    if (prev_rr_hdr) {
        if (prev_rr_hdr->name_addr.uri != NULL) {
            /* Step 2. Replace */
            pjsip_sip_uri *sip_uri = NULL;
            sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(prev_rr_hdr->name_addr.uri);
            pj_str_t map_addr = { .slen = 0, .ptr = 0 };
            map_ipv4_with_ipv6(pool, &map_addr, &sip_uri->host);
            if (map_addr.slen != 0) {
                PJ_LOG(5, (THIS_FILE, "RX patch1 Record-Route header %.*s -> %.*s",
                    sip_uri->host.slen, sip_uri->host.ptr,
                    map_addr.slen, map_addr.ptr
                ));
                sip_uri->host.slen = map_addr.slen;
                sip_uri->host.ptr = map_addr.ptr;
            }
        }
    }

    /* Step 3. Update current */
    if (update_rr_hdr) {
        if (update_rr_hdr->name_addr.uri != NULL) {
            /* Step 2. Replace */
            pjsip_sip_uri *sip_uri = NULL;
            sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(update_rr_hdr->name_addr.uri);
            pj_str_t map_addr = { .slen = 0, .ptr = 0 };
            map_ipv4_with_ipv6(pool, &map_addr, &sip_uri->host);
            if (map_addr.slen != 0) {
                PJ_LOG(5, (THIS_FILE, "RX patch2 Record-Route header %.*s -> %.*s",
                    sip_uri->host.slen, sip_uri->host.ptr,
                    map_addr.slen, map_addr.ptr
                ));
                sip_uri->host.slen = map_addr.slen;
                sip_uri->host.ptr = map_addr.ptr;
            }
        }
    }

}

/**
 * Replace SIP message "Record-Route" headers
 *
**/
static void patch_record_route_ipv6_with_ipv4(pjsip_tx_data *tdata, pjsip_msg *msg) {
    pj_pool_t *pool = tdata->pool;
    pjsip_route_hdr *prev_rr_hdr = NULL;
    pjsip_route_hdr *update_rr_hdr = NULL;
    pj_str_t addr;
    pjsip_hdr *hdr = (pjsip_hdr *)msg->hdr.next;
    pjsip_hdr *end = &msg->hdr;

    /* Step 1. Search */
    for (; hdr != end; hdr = hdr->next) {
        if (hdr->type == PJSIP_H_RECORD_ROUTE) {
            pjsip_route_hdr *current_rr_hdr = (pjsip_route_hdr *)hdr;
            prev_rr_hdr = update_rr_hdr;
            update_rr_hdr = current_rr_hdr;
        }
    }

    /* Step 2. Previous */
    if (prev_rr_hdr) {
        if (prev_rr_hdr->name_addr.uri != NULL) {
            /* Step 2. Replace */
            pjsip_sip_uri *sip_uri = NULL;
            sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(prev_rr_hdr->name_addr.uri);
            pj_str_t map_addr = { .slen = 0, .ptr = 0 };
            map_ipv6_with_ipv4(pool, &map_addr, &sip_uri->host);
            if (map_addr.slen > 0) {
                PJ_LOG(5, (THIS_FILE, "TX patch1 Record-Route header '%.*s' -> '%.*s'",
                    (int)sip_uri->host.slen, sip_uri->host.ptr,
                    (int)map_addr.slen, map_addr.ptr
                ));
                sip_uri->host.slen = map_addr.slen;
                sip_uri->host.ptr = map_addr.ptr;
            }
        }
    }

    /* Step 3. Update */
    if (update_rr_hdr) {
        if (update_rr_hdr->name_addr.uri != NULL) {
            /* Step 2. Replace */
            pjsip_sip_uri *sip_uri = NULL;
            sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(update_rr_hdr->name_addr.uri);
            pj_str_t map_addr = { .slen = 0, .ptr = 0 };
            map_ipv6_with_ipv4(pool, &map_addr, &sip_uri->host);
            if (map_addr.slen > 0) {
                PJ_LOG(5, (THIS_FILE, "TX patch2 Record-Route header '%.*s' -> '%.*s'",
                    (int)sip_uri->host.slen, sip_uri->host.ptr,
                    (int)map_addr.slen, map_addr.ptr
                ));
                sip_uri->host.slen = map_addr.slen;
                sip_uri->host.ptr = map_addr.ptr;
            }
        }
    }

}

/**
 * Replace SIP message "Contact" back header
 *
**/
static void patch_contact_ipv4_with_ipv6(pjsip_rx_data *rdata)
{
    pj_pool_t *pool = rdata->tp_info.pool;
    pjsip_contact_hdr *update_contact_hdr = NULL;
    pjsip_msg *msg = rdata->msg_info.msg;
    pj_str_t addr;
    pjsip_hdr *hdr = (pjsip_hdr *)msg->hdr.next;
    pjsip_hdr *end = &msg->hdr;

    /* Step 1. Search */
    for (; hdr != end; hdr = hdr->next) {
        if (hdr->type == PJSIP_H_CONTACT) {
            pjsip_contact_hdr *current_contact_hdr = (pjsip_contact_hdr *)hdr;
            update_contact_hdr = current_contact_hdr;
        }
    }

    /* Step 2. Update */
    if (update_contact_hdr != NULL) {
        /* Step 1. Extract server address */
        pj_strdup2(pool, &addr, rdata->pkt_info.src_name);

        /* Step 2. Replace */
        pjsip_sip_uri * sip_uri = NULL;
        sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(update_contact_hdr->uri);
        PJ_LOG(5, (THIS_FILE, "RX patch Contact header %.*s -> %.*s",
            sip_uri->host.slen, sip_uri->host.ptr,
            addr.slen, addr.ptr
        ));
        sip_uri->host.slen = addr.slen;
        sip_uri->host.ptr = addr.ptr;
    } else {
        PJ_LOG(5, (THIS_FILE, "RX no-patch Contact header"));
    }

}

/**
 * Patch SIP message "Route" front header
 *
**/
static void patch_route_ipv6_with_ipv4(pjsip_tx_data *tdata, pjsip_msg *msg)
{
    pj_pool_t *pool = tdata->pool;
    pjsip_hdr *hdr = (pjsip_hdr *)msg->hdr.next;
    pjsip_hdr *end = &msg->hdr;
    pjsip_sip_uri *sip_uri = NULL;
    pj_str_t addr;

    for (; hdr != end; hdr = hdr->next) {
        if (hdr->type == PJSIP_H_ROUTE) {
            pjsip_route_hdr *cur = (pjsip_route_hdr *)hdr;
            sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(cur->name_addr.uri);
            pj_str_t map_addr = { .slen = 0, .ptr = 0 };
            map_ipv6_with_ipv4(pool, &map_addr, &sip_uri->host);
            if (map_addr.slen > 0) {
                PJ_LOG(5, (THIS_FILE, "TX patch Route header '%.*s' -> '%.*s'",
                    (int)sip_uri->host.slen, sip_uri->host.ptr,
                    (int)map_addr.slen, map_addr.ptr
                ));
                sip_uri->host.slen = map_addr.slen;
                sip_uri->host.ptr = map_addr.ptr;
            }
        }
    }

}

static void patch_r_uri_ipv6_with_ipv4(pjsip_tx_data *tdata, pjsip_msg *msg)
{
    pj_pool_t *pool = tdata->pool;

    /* Step 1. Patch R-URI on request */
    if (msg->type == PJSIP_REQUEST_MSG) {
        pjsip_sip_uri *sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(msg->line.req.uri);
        if (hpbx_in4_addr.slen > 0) {
            PJ_LOG(5, (THIS_FILE, "TX patch R-URI at SIP message: '%.*s' -> '%.*s'",
                sip_uri->host.slen, sip_uri->host.ptr,
                hpbx_in4_addr.slen, hpbx_in4_addr.ptr
            ));
            sip_uri->host.slen = hpbx_in4_addr.slen;
            sip_uri->host.ptr = hpbx_in4_addr.ptr;
        } else {
            PJ_LOG(5, (THIS_FILE, "TX no patch R-URI at SIP message: no IPv4 addr"));
        }
    }

}

/**
 * Monitoring REGISTER answer with Via parameters with HPBX server address
 *
**/
static void search_ipv4_client_address(pjsip_rx_data *rdata)
{
    pj_pool_t *pool = rdata->tp_info.pool;
    pjsip_via_hdr *cur_via_hdr = NULL;
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_cseq_hdr *cseq = rdata->msg_info.cseq;
    pj_str_t addr;
    pjsip_hdr *hdr = (pjsip_hdr *)msg->hdr.next;
    pjsip_hdr *end = &msg->hdr;

    /* Step 1. Process response on REGISTER */
    if ((msg->type == PJSIP_RESPONSE_MSG) && (cseq->method.id == PJSIP_REGISTER_METHOD)) {
    } else {
        return;
    }

    /* Step 2. Search "Via" header on response */
    for (; hdr != end; hdr = hdr->next) {
        if (hdr->type == PJSIP_H_VIA) {
            pjsip_via_hdr *via_hdr = (pjsip_via_hdr *)hdr;
            cur_via_hdr = via_hdr;
        }
    }
    if (cur_via_hdr != NULL) {
        pj_str_t addr = cur_via_hdr->recvd_param;
        pj_nat64_set_client_addr(&addr);
    }

}

/**
 * Monitoring REGISTER answer with Via parameters with HPBX server address
 *
**/
static void search_ipv4_server_address(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_cseq_hdr *cseq = rdata->msg_info.cseq;

    /* Step 1. Process INVITE request */
    if ((msg->type == PJSIP_REQUEST_MSG) && (cseq->method.id == PJSIP_INVITE_METHOD)) {
        /* Step 1. Parse contact */
        pjsip_contact_hdr *contact = (pjsip_contact_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
        if (contact == NULL) {
            return;
        }
        if (contact->uri == NULL) {
            return;
        }
        pjsip_sip_uri *sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(contact->uri);
        if (sip_uri == NULL) {
            return;
        }
        pj_nat64_set_server_addr(&sip_uri->host);
    }

    /* Step 2. Process INVITE response */
    if ((msg->type == PJSIP_RESPONSE_MSG) && (cseq->method.id == PJSIP_INVITE_METHOD)) {
        /* Step 1. Parse contact */
        pjsip_contact_hdr *contact = (pjsip_contact_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
        if (contact == NULL) {
            return;
        }
        if (contact->uri == NULL) {
            return;
        }
        pjsip_sip_uri *sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(contact->uri);
        if (sip_uri == NULL) {
            return;
        }
        pj_nat64_set_server_addr(&sip_uri->host);
    }

}

/**
 * Monitoring source SIP message address
 *
**/
static void search_ipv6_client_address(pjsip_rx_data *rdata)
{
    // TODO - Not yet implemented...
}

/**
 * Monitoring source SIP message address
 *
**/
static void search_ipv6_server_address(pjsip_rx_data *rdata)
{
    /* Step 1. Update HPBX server IPv6 address */
    pj_str_t addr;
    pj_cstr(&addr, rdata->pkt_info.src_name);
    // TODO - check IPv6 address...
    pj_nat64_set_server_addr6(&addr);
}

/**
 * Check IPv6 support on stransport
 *
**/
static pj_bool_t is_ipv6(pjsip_transport *transport)
{
    pjsip_transport_key *key = &transport->key;
    pj_bool_t result = PJ_FALSE;
    result |= key->type == PJSIP_TRANSPORT_UDP6;
    result |= key->type == PJSIP_TRANSPORT_TCP6;
    result |= key->type == PJSIP_TRANSPORT_TLS6;
    return result;
}

static pj_bool_t ipv6_mod_on_rx(pjsip_rx_data *rdata)
{
    pjsip_cseq_hdr *cseq = rdata->msg_info.cseq;
    if (cseq == NULL) {
        PJ_LOG(1, (THIS_FILE, "No C-Seq header"));
        return PJ_FALSE;
    }

    /* Step 0. Check enable module */
    if (!mod_enable) {
        PJ_LOG(5, (THIS_FILE, "NAT64 module was disable."));
        return PJ_FALSE;
    }
    if (!is_ipv6(rdata->tp_info.transport)) {
        PJ_LOG(5, (THIS_FILE, "Use non IPv6 transport. NAT64 module was disable."));
        return PJ_FALSE;
    }

    /* Step 1. Debug message */
    PJ_LOG(4, (THIS_FILE, "Process RX message %.*s", cseq->method.name.slen, cseq->method.name.ptr));
    modern_sip_msg_dump("RX source SIP message", rdata->msg_info.msg);

    /* Step 2. Peek HPBX server address */
    PJ_LOG(5, (THIS_FILE, "RX search HPBX server addr"));
    search_ipv4_client_address(rdata);
    search_ipv6_client_address(rdata);
    search_ipv4_server_address(rdata);
    search_ipv6_server_address(rdata);

    /* Step 2. Patch SIP message headers */
    PJ_LOG(5, (THIS_FILE, "RX patch SIP message headers: IPv4 -> IPv6"));
    patch_record_route_ipv4_with_ipv6(rdata);
    patch_contact_ipv4_with_ipv6(rdata);

    /* Step 3. Replace SDP body of SIP message */
    if ((cseq != NULL) && (cseq->method.id == PJSIP_INVITE_METHOD)) {
        PJ_LOG(5, (THIS_FILE, "RX patch SDP on INVITE: IPv4 -> IPv6"));
        patch_sdp_ipv4_with_ipv6(rdata);
    }

    /* Step 4. Dump source SIP message */
    modern_sip_msg_dump("RX patched SIP message", rdata->msg_info.msg);

    return PJ_FALSE;
}

pj_status_t ipv6_mod_on_tx(pjsip_tx_data *tdata)
{
    pjsip_msg *msg = pjsip_msg_clone(tdata->pool, tdata->msg);
    if (msg == NULL) {
        PJ_LOG(1, (THIS_FILE, "No memory"));
        return PJ_ENOMEM;
    }

    pjsip_cseq_hdr *cseq = (pjsip_cseq_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    if (cseq == NULL) {
        PJ_LOG(1, (THIS_FILE, "No C-Seq header"));
        return PJ_SUCCESS;
    }

    /* Step 0. Check enable module */
    if (!mod_enable) {
        PJ_LOG(5, (THIS_FILE, "NAT64 module was disable."));
        return PJ_SUCCESS;
    }
    if (!is_ipv6(tdata->tp_info.transport)) {
        PJ_LOG(5, (THIS_FILE, "NAT64 processing IPv4 module was disable."));
        return PJ_SUCCESS;
    }

    /* Step 1. Debug message */
    PJ_LOG(4, (THIS_FILE, "Process TX message %.*s", cseq->method.name.slen, cseq->method.name.ptr));
    modern_sip_msg_dump("TX source SIP message", msg);

    /* Step 2. Patch SIP message headers */
    PJ_LOG(5, (THIS_FILE, "TX patch SIP message headers: IPv6 -> IPv4"));
    patch_record_route_ipv6_with_ipv4(tdata, msg);
    patch_route_ipv6_with_ipv4(tdata, msg);
    patch_r_uri_ipv6_with_ipv4(tdata, msg);

    /* Step 3. Replace SDP body of SIP message */
    if ((cseq != NULL) && (cseq->method.id == PJSIP_INVITE_METHOD)) {
        PJ_LOG(5, (THIS_FILE, "TX patch SDP on INVITE: IPv6 -> IPv4"));
        patch_sdp_ipv6_with_ipv4(tdata, msg);
    }

    /* Step 4. Update SIP message */
    modern_update_tx_data(tdata, msg);

    /* Step 5. Dump  */
    modern_sip_msg_dump("TX patched SIP message", msg);

    return PJ_SUCCESS;
}

/* L1 rewrite module for sdp info.*/
static pjsip_module ipv6_module = {
    NULL, NULL,                       /* prev, next.      */
    { "mod-ipv6", 8},                 /* Name.            */
    -1,                               /* Id               */
    0,                                /* Priority         */
    NULL,                             /* load()           */
    NULL,                             /* start()          */
    NULL,                             /* stop()           */
    NULL,                             /* unload()         */
    &ipv6_mod_on_rx,                  /* on_rx_request()  */
    &ipv6_mod_on_rx,                  /* on_rx_response() */
    &ipv6_mod_on_tx,                  /* on_tx_request.   */
    &ipv6_mod_on_tx,                  /* on_tx_response() */
    NULL,                             /* on_tsx_state()   */
};

static void check_network()
{
    pj_status_t status;
    int dst_af;
    pj_sockaddr dst_addr = { 0 };
    pj_sockaddr src_addr = { 0 };

    /* Step 1. Get Well Know IPv4 network address, example: 8.8.8.8 */
//    unsigned options = 0;
//    pj_str_t addr = { .slen = 7, .ptr = "8.8.8.8" };
//    status = pj_sockaddr_parse(PJ_AF_INET, options, &addr, &src_addr);
//    if (status != PJ_SUCCESS) {
//        PJ_LOG(5, (THIS_FILE, "Error parse IPv4 address"));
//        return;
//    }

    /* Step 2. Create IPv6 addres */
//    status = pj_sockaddr_synthesize(dst_af, &dst_addr, &src_addr);
//    if (status == PJ_SUCCESS) {
        // TODO - print new network address...
//    } else {
        // TODO - no network additional features...
//    }

}

pj_status_t pj_nat64_enable_rewrite_module()
{
    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
    pj_status_t result;
    /* Step 1. Create module memory home */
    if (mod_pool == NULL) {
#ifdef MOD_IPV6_ENDPT_MEMORY
        PJ_LOG(5, (THIS_FILE, "Create memory home on endpoint"));
        /* Step 1. Create module memory home */
        mod_pool = pjsip_endpt_create_pool(endpt, "ipv6", 512, 512);
#else
        PJ_LOG(5, (THIS_FILE, "Create memory home on caching pool"));
        /* Step 1. Create module memory home */
        pj_caching_pool_init( &cp, &pj_pool_factory_default_policy, 0);
        mod_pool = pj_pool_create(&cp.factory, "ipv6", 512, 512, NULL);
#endif
        /* Step 2. Set random HPBX server address */
        pj_str_t addr = { .ptr = "10.0.0.1", .slen = 8 };
        pj_nat64_set_client_addr(&addr);
    }
    /* Step 2. Register module */
    result = pjsip_endpt_register_module(endpt, &ipv6_module);
    /* Step 3. Try detect IPv6 subnet */
    check_network();
    return result;
}

pj_status_t pj_nat64_disable_rewrite_module()
{
    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
    pj_status_t result;
    /* Step 1. Unregister module */
    result = pjsip_endpt_unregister_module(endpt, &ipv6_module);
    /* Step 2. Release memory home */
    if (mod_pool != NULL) {
#ifdef MOD_IPV6_ENDPT_MEMORY
        PJ_LOG(5, (THIS_FILE, "Release memory home on endpoint"));
        pjsip_endpt_release_pool(endpt, mod_pool);
#else
        PJ_LOG(5, (THIS_FILE, "Release memory home on caching pool"));
        pj_pool_release(mod_pool);
#endif
        mod_pool = NULL;
    }
    return result;
}

/**
 * Enable/disable NAT64 features
 *
**/
void pj_nat64_set_enable(pj_bool_t yesno)
{
    mod_enable = yesno;
}

void pj_nat64_dump()
{
    PJ_LOG(5, (THIS_FILE, "NAT64 HPBX [ IN4 = '%.*s' IN6 = '%.*s' ]",
        hpbx_in4_addr.slen, hpbx_in4_addr.ptr,
        hpbx_in6_addr.slen, hpbx_in6_addr.ptr
    ));
    PJ_LOG(5, (THIS_FILE, "NAT64 UA [ IN4 = '%.*s' IN6 = '%.*s' ]",
        acc_in4_addr.slen, acc_in4_addr.ptr,
        acc_in6_addr.slen, acc_in6_addr.ptr
    ));
}

/**
 * Set HPBX server IPv4 address
 *
**/
void pj_nat64_set_server_addr(pj_str_t *addr)
{
    if (mod_pool == NULL) {
        PJ_LOG(1, (THIS_FILE, "No module memory pool"));
        return;
    }
    /* Step 1. Check arguments*/
    if (addr == NULL) {
        return;
    }
    if ((addr->slen == 0) || (addr->ptr == NULL)) {
        return;
    }
    /* Step 2. Check change exists */
    if (0 == pj_strcmp(&hpbx_in4_addr, addr)) {
        return;
    }
    /* Step 3. Debug message */
    PJ_LOG(5, (THIS_FILE, "NAT64 update HPBX IN4 address '%.*s' -> '%.*s'",
        hpbx_in4_addr.slen, hpbx_in4_addr.ptr,
        addr->slen, addr->ptr
    ));
    /* Step 4. Update HPBX server addr value */
    pj_strdup_with_null(mod_pool, &hpbx_in4_addr, addr);

    /* Step 5. Dump maps */
    pj_nat64_dump();
}

void pj_nat64_set_server_addr6(pj_str_t *addr)
{
    if (mod_pool == NULL) {
        PJ_LOG(1, (THIS_FILE, "No module memory pool"));
        return;
    }
    /* Step 1. Check arguments*/
    if (addr == NULL) {
        return;
    }
    if ((addr->slen == 0) || (addr->ptr == NULL)) {
        return;
    }
    /* Step 2. Check change exists */
    if (0 == pj_strcmp(&hpbx_in6_addr, addr)) {
        return;
    }
    /* Step 3. Debug message */
    PJ_LOG(5, (THIS_FILE, "NAT64 update HPBX IN6 address '%.*s' -> '%.*s'",
        hpbx_in6_addr.slen, hpbx_in6_addr.ptr,
        addr->slen, addr->ptr
    ));
    /* Step 4. Update HPBX server addr value */
    pj_strdup_with_null(mod_pool, &hpbx_in6_addr, addr);

    /* Step 5. Dump maps */
    pj_nat64_dump();
}

void pj_nat64_set_client_addr(pj_str_t *addr)
{
    if (mod_pool == NULL) {
        PJ_LOG(1, (THIS_FILE, "No module memory pool"));
        return;
    }
    /* Step 1. Check arguments */
    if (addr == NULL) {
        return;
    }
    if ((addr->slen == 0) || (addr->ptr == NULL)) {
        return;
    }
    /* Step 2. Check change exists */
    if (0 == pj_strcmp(&acc_in4_addr, addr)) {
        return;
    }
    /* Step 3. Debug message */
    PJ_LOG(5, (THIS_FILE, "NAT64 update ACC IN4 address '%.*s' -> '%.*s'",
        acc_in4_addr.slen, acc_in4_addr.ptr,
        addr->slen, addr->ptr
    ));
    /* Step 4. Update HPBX server addr value */
    pj_strdup_with_null(mod_pool, &acc_in4_addr, addr);
    /* Step 5. Dump maps */
    pj_nat64_dump();
}

void pj_nat64_set_client_addr6(pj_str_t *addr)
{
    if (mod_pool == NULL) {
        PJ_LOG(1, (THIS_FILE, "No module memory pool"));
        return;
    }
    /* Step 1. Check arguments */
    if (addr == NULL) {
        return;
    }
    if ((addr->slen == 0) || (addr->ptr == NULL)) {
        return;
    }
    /* Step 2. Check change exists */
    if (0 == pj_strcmp(&acc_in6_addr, addr)) {
        return;
    }
    /* Step 3. Debug message */
    PJ_LOG(5, (THIS_FILE, "NAT64 update ACC IN6 address '%.*s' -> '%.*s'",
        acc_in6_addr.slen, acc_in6_addr.ptr,
        addr->slen, addr->ptr
    ));
    /* Step 4. Update HPBX server addr value */
    pj_strdup_with_null(mod_pool, &acc_in6_addr, addr);
    /* Step 5. Dump maps */
    pj_nat64_dump();
}

/**
 * Debug options
 *
 */
void pj_nat64_set_debug(pj_bool_t yesno)
{
    mod_debug = yesno;
}
