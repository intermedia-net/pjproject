#define PJ_CONFIG_IPHONE 1
#define PJ_HAS_IPV6 1
#define PJMEDIA_HAS_BCG729 1
#define PJMEDIA_HAS_OPUS_CODEC 1
#define PJMEDIA_HAS_VIDEO 0
#define PJMEDIA_HAS_VID_TOOLBOX_CODEC 0
#define PJSUA_DETECT_MERGED_REQUESTS 0
#define PJ_QOS_IMPLEMENTATION PJ_QOS_BSD
#define PJSIP_TRANSPORT_IDLE_TIME 1
#define PJMEDIA_HAS_RTCP_XR 1
#include <pj/config_site_sample.h>

/* APPS-30897: codecs not used by the app. Must come after the sample,
 * which force-enables iLBC under PJ_CONFIG_IPHONE. Keep in sync with the
 * --disable-*-codec flags in build_pjsip_and_dependencies.sh.
 */
#undef  PJMEDIA_HAS_ILBC_CODEC
#define PJMEDIA_HAS_ILBC_CODEC      0
#undef  PJMEDIA_HAS_GSM_CODEC
#define PJMEDIA_HAS_GSM_CODEC       0
#undef  PJMEDIA_HAS_SPEEX_CODEC
#define PJMEDIA_HAS_SPEEX_CODEC     0
