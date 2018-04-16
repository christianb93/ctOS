/*
 * icmp.c
 *
 */

#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "debug.h"
#include "eth.h"
#include "lib/string.h"
#include "lib/stdint.h"
#include "lib/arpa/inet.h"
#include "util.h"

extern int __net_loglevel;
#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)



/*
 * Process an ECHO request
 */
static void do_echo_request(net_msg_t* net_msg, u32 ip_src) {
    net_msg_t* reply;
    icmp_hdr_t* request_hdr = (icmp_hdr_t*) net_msg->icmp_hdr;
    icmp_hdr_t* reply_hdr;
    void* request_data = ((void*) request_hdr) + sizeof(icmp_hdr_t);
    void* reply_data;
    int data_length = net_msg->ip_length - sizeof(icmp_hdr_t);
    u16 chksum;
    NET_DEBUG("Got ICMP ECHO request\n");
    /*
     * Create an answer
     */
    if (0 == (reply = net_msg_new(net_msg->ip_length))) {
        ERROR("Discarding ICMP ECHO request - not enough memory\n");
        return;
    }
    reply_hdr = (icmp_hdr_t*) net_msg_append(reply, sizeof(icmp_hdr_t));
    reply_data = net_msg_append(reply, data_length);
    reply_hdr->code = ICMP_CODE_NONE;
    reply_hdr->type = ICMP_ECHO_REPLY;
    reply_hdr->checksum = 0;
    memcpy(reply_data, request_data, data_length);
    chksum = net_compute_checksum((u16*) reply_hdr, net_msg->ip_length);
    reply_hdr->checksum = htons(chksum);
    /*
     * Destroy original message
     */
    net_msg_destroy(net_msg);
    /*
     * and send packet back
     */
    reply->ip_dest = ip_src;
    reply->ip_proto = IP_PROTO_ICMP;
    reply->ip_src = 0;
    reply->ip_length = net_msg->ip_length;
    reply->ip_df = 0;
    ip_tx_msg(reply);
}

/*
 * Receive an ICMP packet
 */
void icmp_rx_msg(net_msg_t* net_msg) {
    icmp_hdr_t* icmp_hdr = (icmp_hdr_t*) net_msg->icmp_hdr;
    u16 chksum;
    u32 ip_src = net_msg->ip_src;
    NET_DEBUG("Got ICMP packet, code=%x, type=%x, chksum = %x \n", icmp_hdr->code, icmp_hdr->type, icmp_hdr->checksum);
    /*
     * Verify checksum
     */
    chksum = net_compute_checksum((u16*) icmp_hdr, net_msg->ip_length);
    NET_DEBUG("Checksum: %x\n", chksum);
    if (0x0 != chksum) {
        ERROR("Got invalid checksum %x in ICMP header\n", chksum);
        return;
    }
    switch (icmp_hdr->type) {
        case ICMP_ECHO_REQUEST:
            do_echo_request(net_msg, ip_src);
            break;
        case ICMP_ECHO_REPLY:
            NET_DEBUG("Got ECHO reply\n");
            break;
        default:
            break;
    }
}

/*
 * Send an error message as a response to an incoming message. According to RFC 1122, each error
 * message needs to contain the IP header of the offending message and at least 8 additional octets.
 * Parameter:
 * @net_msg - the offending message
 * @code - the ICMP code
 * @type - the ICMP type
 * We assume that the following fields in the message are filled
 * ip_src
 * ip_length
 */
void icmp_send_error(net_msg_t* net_msg, int code, int type) {
    net_msg_t* error;
    u8* data;
    int payload_bytes;
    icmp_hdr_t* icmp_hdr;
    u8* second_hdr;
    u16 chksum;
    int rc;
    /*
     * Do not send if source address of offending packet is 0
     */
    if (INADDR_ANY == net_msg->ip_src)
        return;
    /*
     * Create a new network message
     */
    error = net_msg_new(sizeof(icmp_hdr_t) + sizeof(ip_hdr_t) + ICMP_SECOND_HDR_SIZE + ICMP_ERROR_OCTETS);
    /*
     * Fill fields required by ip_tx_msg
     */
    error->ip_proto = IPPROTO_ICMP;
    error->ip_df = 0;
    error->ip_dest = net_msg->ip_src;
    error->ip_src = INADDR_ANY;
    /*
     * Fill ICMP header
     */
    icmp_hdr = (icmp_hdr_t*) net_msg_append(error, sizeof(icmp_hdr_t));
    KASSERT(icmp_hdr);
    icmp_hdr->checksum = 0;
    icmp_hdr->code = code;
    icmp_hdr->type = type;
    /*
     * Now add error specific header
     */
    switch (type) {
        case ICMP_DEST_UNREACH:
            second_hdr = net_msg_append(error, ICMP_SECOND_HDR_SIZE);
            KASSERT(second_hdr);
            memset((void*) second_hdr, 0, ICMP_SECOND_HDR_SIZE);
            break;
        default:
            NET_DEBUG("Unknown ICMP type %d\n", type);
            net_msg_destroy(error);
            return;
    }
    /*
     * and data - copy IP header and ICMP_ERROR_OCTETS bytes from original message
     */
    payload_bytes = MIN(ICMP_ERROR_OCTETS, net_msg->ip_length);
    data = net_msg_append(error, payload_bytes + sizeof(ip_hdr_t));
    KASSERT(data);
    memcpy((void*) data, net_msg->ip_hdr, payload_bytes + sizeof(ip_hdr_t));
    /*
     * Now compute checksum
     */
    chksum = net_compute_checksum((u16*) icmp_hdr, payload_bytes + ICMP_SECOND_HDR_SIZE + sizeof(icmp_hdr_t) + sizeof(ip_hdr_t));
    icmp_hdr->checksum = htons(chksum);
    /*
     * and send packet
     */
    rc = ip_tx_msg(error);
    if (rc) {
        NET_DEBUG("IP layer could not transmit ICMP error message, return code is - %d\n", (-1)*rc);
    }
}

