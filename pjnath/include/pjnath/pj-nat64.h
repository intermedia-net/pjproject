#ifndef PJ_NAT64_H_
#define PJ_NAT64_H_

/**
 * Use algorithmic map
 *
 * @see https://datatracker.ietf.org/doc/html/rfc6052#section-2.1
 */
#ifndef USE_RFC6052
# define USE_RFC6052 0
#endif

PJ_BEGIN_DECL

/**
 * Enable nat64 rewriting module.
 *
 */
pj_status_t pj_nat64_enable_rewrite_module();

/**
 * Disable rewriting module, for instance when on a ipv4 network
 *
 */
pj_status_t pj_nat64_disable_rewrite_module();

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

/**
 * Debug options
 *
 */
void pj_nat64_set_debug(pj_bool_t yesno);

PJ_END_DECL

#endif
