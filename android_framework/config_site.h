#define PJ_CONFIG_ANDROID 1
#include <pj/config_site_sample.h>
#define PJMEDIA_HAS_VIDEO 0
#define PJMEDIA_VIDEO_DEV_HAS_ANDROID 0
#define PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI 0
#define PJMEDIA_AUDIO_DEV_HAS_OPENSL 1
#define PJSIP_AUTH_AUTO_SEND_NEXT 0
#define PJMEDIA_HAS_OPUS_CODEC 1
#define PJSUA_DETECT_MERGED_REQUESTS 0
#define PJ_HAS_IPV6 1
#define PJMEDIA_HAS_BCG729 1
#define PJSIP_TRANSPORT_IDLE_TIME 1
#define PJMEDIA_HAS_RTCP_XR 1

/* APPS-30898: codecs not used by the app. Must come after the sample include.
 * G.722.1 was previously force-enabled here; it needs a Polycom licence.
 * Keep in sync with the --disable-*-codec flags in build_android.sh.
 */
#undef  PJMEDIA_HAS_G7221_CODEC
#define PJMEDIA_HAS_G7221_CODEC     0
#undef  PJMEDIA_HAS_ILBC_CODEC
#define PJMEDIA_HAS_ILBC_CODEC      0
#undef  PJMEDIA_HAS_GSM_CODEC
#define PJMEDIA_HAS_GSM_CODEC       0
#undef  PJMEDIA_HAS_SPEEX_CODEC
#define PJMEDIA_HAS_SPEEX_CODEC     0
