#ifndef PJ_NAT64_H_
#define PJ_NAT64_H_

PJ_BEGIN_DECL

/**
 * Options for NAT64 rewriting. Probably you want to enable all of them */
typedef enum nat64_options {
    /** Replace outgoing IPv6 with IPv4 on SDP */
    NAT64_REWRITE_OUTGOING_SDP          = 1,
    /** Replace incoming IPv4 with IPv6 on SDP */
    NAT64_REWRITE_INCOMING_SDP          = 2,
    /** Replace Route address on SIP message */
    NAT64_REWRITE_ROUTE                 = 4,
    /** Replace Record-Route address on SIP message */
    NAT64_REWRITE_RECORD_ROUTE          = 8,
    /** Replace Record-Route address on SIP message */
    NAT64_REWRITE_CONTACT               = 16,

} nat64_options;

/**
 * Enable nat64 rewriting module.
 *
 * @param options       Bitmap of #nat64_options.
 *                      NAT64_REWRITE_OUTGOING_SDP | NAT64_REWRITE_INCOMING_SDP | ...
 *
 * @default             0 - No nat64 rewriting is done.
 */
pj_status_t pj_nat64_enable_rewrite_module();

/**
 * Disable rewriting module, for instance when on a ipv4 network
 *
 */
pj_status_t pj_nat64_disable_rewrite_module();

/**
 * Update IPv6 rewriting module options
 *
 */
void pj_nat64_set_options(nat64_options options);

/**
 * Enable/disable all IPv6 rewriting module options
 *
 */
void pj_nat64_set_enable(pj_bool_t yesno);

/**
 * Set HPBX server IPv4 address
 *
**/
void pj_nat64_set_server_addr(pj_str_t *addr);

/**
 * Set HPBX server IPv6 address
 *
**/
void pj_nat64_set_server_addr6(pj_str_t *addr);

/**
 * Set account IPv4 address
 *
**/
void pj_nat64_set_client_addr(pj_str_t *addr);

/**
 * Set account IPv6 address
 *
**/
void pj_nat64_set_client_addr6(pj_str_t *addr);

/** Dump maps
 *
**/
void pj_nat64_dump();

PJ_END_DECL

#endif
