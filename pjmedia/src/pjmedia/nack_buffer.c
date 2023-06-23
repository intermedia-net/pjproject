#include <pjmedia/nack_buffer.h>
#include <pjmedia/rtcp_fb.h>
#include <pjmedia/types.h>
#include <pj/pool.h>
#include <pj/errno.h>
#include <pj/assert.h>
#include <pj/log.h>

#define THIS_FILE   "nack_buffer.c"

struct pjmedia_nack_buffer {
    unsigned head;
    unsigned tail;
    unsigned count;
    unsigned size;
    pjmedia_rtcp_fb_nack *packets;
};

static pj_bool_t blp_contains(pjmedia_rtcp_fb_nack *packet, uint16_t sequence_num) {
    PJ_ASSERT_RETURN(sequence_num > packet->pid, PJ_FALSE);
    uint16_t diff = sequence_num - packet->pid;

    if (diff <= 16 && (packet->blp & (1 << diff - 1))) {
        return PJ_TRUE;
    } else {
        return PJ_FALSE;
    }    
}

PJ_DEF(pj_status_t)
pjmedia_nack_buffer_create(pj_pool_t *pool,
                           unsigned size,
                           pjmedia_nack_buffer **buffer) {

    pjmedia_nack_buffer *nack_buffer = PJ_POOL_ZALLOC_T(pool, pjmedia_nack_buffer);
    PJ_ASSERT_RETURN(nack_buffer != NULL, PJ_ENOMEM);

    nack_buffer->packets = pj_pool_alloc(pool, size * sizeof(pjmedia_rtcp_fb_nack));
    PJ_ASSERT_RETURN(nack_buffer->packets != NULL, PJ_ENOMEM);

    nack_buffer->size = size;
    nack_buffer->count = 0;
    nack_buffer->head = 0;
    nack_buffer->tail = 0;

    *buffer = nack_buffer;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)
pjmedia_nack_buffer_push(pjmedia_nack_buffer *buffer,
                         pjmedia_rtcp_fb_nack nack) {

    PJ_ASSERT_RETURN(buffer, PJ_EINVAL);
    
    if (buffer->count < buffer->size) {
        buffer->count++;
    } else {
        PJ_LOG(3, (THIS_FILE, "Buffer is full, overwriting oldest packet."));
        buffer->tail = (buffer->tail + 1) % buffer->size;
    }

    buffer->packets[buffer->head] = nack;
    buffer->head = (buffer->head + 1) % buffer->size;

    return PJ_SUCCESS;
}

// Function to check if a packet with the given sequence number was NACKed and remove older packets.
PJ_DECL(pj_bool_t)
pjmedia_nack_buffer_frame_dequeued(pjmedia_nack_buffer *buffer,
                                   uint16_t sequence_num) {
    if (buffer->count == 0) {
        return PJ_FALSE; 
    }
    
    int lower = 0;
    int upper = buffer->count - 1;
    int index = -1;

    // Perform a binary search for the packet with the closest PID <= the sequence number
    while (lower <= upper) {
        int middle = (lower + upper) / 2;
        uint16_t pid = buffer->packets[(buffer->tail + middle) % buffer->size].pid;

        if (pid <= sequence_num) {
            index = middle;
            lower = middle + 1;
        } else {
            upper = middle - 1;
        }
    }

    PJ_LOG(3,(THIS_FILE, "Packet %u. Index: %i", sequence_num, index));

    if (index == -1) {
        return PJ_FALSE;
    }

    pjmedia_rtcp_fb_nack *packet = &buffer->packets[(buffer->tail + index) % buffer->size];

    PJ_LOG(3,(THIS_FILE, "Found packet pid %u.", packet->pid));

    if (packet->pid == sequence_num || blp_contains(packet, sequence_num)) {
        PJ_LOG(3,(THIS_FILE, "Packet found"));
        // Remove all older packets
        buffer->tail = (buffer->tail + index) % buffer->size;
        buffer->count -= index;
        return PJ_TRUE;
    } else {
        PJ_LOG(3,(THIS_FILE, "Packet not found"));
        // Remove all older packets
        buffer->tail = (buffer->tail + index + 1) % buffer->size;
        buffer->count -= index + 1;
        return PJ_FALSE;
    }
}

unsigned pjmedia_nack_buffer_len(pjmedia_nack_buffer *buffer) {
    return buffer->count;
}

