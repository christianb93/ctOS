/*
 * testnet.c
 *
 */


#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>


#define DO_CONNECT

/*
 * Global variables
 */
in_addr_t dest_addr;
unsigned short port;


volatile int alarm_raised = 0;
void signal_handler(int sig_no) {
    if (SIGALRM == sig_no)
        alarm_raised = 1;
}

/*
 * Number of ICMP requests send
 */
static int requests = 0;

/*
 * This is an ICMP header
 */
typedef struct {
    unsigned char type;                // Type of message
    unsigned char code;                // Message code
    unsigned short checksum;           // Header checksum
} __attribute__ ((packed)) icmp_hdr_t;

/*
 * The body of an ECHO request message
 */
typedef struct {
    unsigned short id;                 // Identifier
    unsigned short seq_no;             // Sequence number
} __attribute__ ((packed)) icmp_echo_request_t;

/*
 * ICMP message types
 */
#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

/*
 * Compute the IP checksum of a word array. The elements within the
 * array are assumed to be stored in network byte order
 * Parameter:
 * @words - pointer to array
 * @byte_count - number of bytes in array
 */
static unsigned short compute_checksum(unsigned short* words, int byte_count) {
    unsigned int sum = 0;
    unsigned short rc;
    int i;
    unsigned short last_byte = 0;
    /*
     * First sum up all words. We do all the sums in network byte order
     * and only convert the result
     */
    for (i = 0; i < byte_count / 2; i++) {
        sum = sum + words[i];
    }
    /*
     * If the number of bytes is odd, add left over byte << 8
     */
    if (1 == (byte_count % 2)) {
        last_byte = ((unsigned char*) words)[byte_count - 1];
        sum = sum + last_byte;
    }
    /*
     * Repeatedly add carry to LSB until carry is zero
     */
    while (sum >> 16)
        sum = (sum >> 16) + (sum & 0xFFFF);
    rc = sum;
    rc = ntohs(~rc);
    return rc;
}


/*
 * Some constants
 * NR_OF_PINGS - number of echo requests which we send
 * WAIT_TIME - seconds to wait after all requests have been sent
 */
#define NR_OF_PINGS 5
#define WAIT_TIME 2


/*
 * ICMP types
 */
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_DEST_UNREACHABLE 3
#define ICMP_TYPE_TIME_EXCEEDED 11


/*
 * Utility function to send all data in a buffer
 */
static int sendall(int fd, void* buffer, int len) {
    int rc = 0;
    int sent = 0;
    while (sent < len) {
        rc = send(fd, buffer + sent, len - sent, 0);
        if (-1 == rc) {
            if (EINTR != errno) {
                return -1;
            }
        }
        else
            sent += rc;
    }
    return sent;
}

/*
 * Utility function to read data into a buffer
 */
static int recvall(int fd, void* buffer, int len) {
    int rc = 0;
    int bytes_read = 0;
    while (bytes_read < len) {
        rc = recv(fd, buffer + bytes_read, len - bytes_read, 0);
        if (-1 == rc) {
            if (EINTR != errno) {
                return -1;
            }
        }
        else
            bytes_read += rc;
    }
    return bytes_read;
}

/*
 * Like sendall, but use write
 */
static int writeall(int fd, void* buffer, int len) {
    int rc = 0;
    int sent = 0;
    while (sent < len) {
        rc = write(fd, buffer + sent, len - sent);
        if (-1 == rc) {
            if (EINTR != errno) {
                return -1;
            }
        }
        else
            sent += rc;
    }
    return sent;
}

/*
 * Like recvall, but use read
 */
static int readall(int fd, void* buffer, int len) {
    int rc = 0;
    int bytes_read = 0;
    while (bytes_read < len) {
        rc = read(fd, buffer + bytes_read, len - bytes_read);
        if (-1 == rc) {
            if (EINTR != errno) {
                return -1;
            }
        }
        else
            bytes_read += rc;
    }
    return bytes_read;
}

/*
 * Send an ICMP ECHO request to a remote host
 * Parameter:
 * @fd - the file descriptor of a raw IP socket
 * The function will create an ICMP echo request with
 * ID = PID of current process
 * SEQ_NO = requests + 1
 * and will increase requests
 */
static void send_ping(int fd) {
    unsigned char buffer[256];
    unsigned char* request_data;
    unsigned short* id;
    unsigned short* seq_no;
    unsigned short chksum;
    int pid = getpid();
    int i;
    icmp_hdr_t* request_hdr;
    request_hdr = (icmp_hdr_t*) buffer;
    request_hdr->code = 0;
    request_hdr->type = ICMP_ECHO_REQUEST;
    request_hdr->checksum = 0;
    /*
      * Fill ICMP data area. The first two bytes are an identifier, the next two bytes the sequence number
      * We use 100 bytes for ICMP header and data
      */
    request_data = ((void*) request_hdr) + sizeof(icmp_hdr_t);
    id = (unsigned short*)(request_data);
    seq_no = id + 1;
    *id = htons(pid);
    *seq_no = htons(requests + 1);
    requests++;
    /*
     * Fill up remaining bytes. We have an IP payload of 100 bytes in total
     * ID and SEQ_NO consume four bytes
     */
    for (i = 0; i < 100 - 4 - sizeof(icmp_hdr_t); i++) {
        ((unsigned char*)(request_data + 4))[i] = i;
    }
    /*
     * Compute checksum over entire IP payload
     */
    chksum = compute_checksum((unsigned short*) request_hdr, 100);
    request_hdr->checksum = htons(chksum);
    /*
     * Finally send message
     */
    if (100 != send(fd, buffer, 100, 0)) {
        perror("send");
        return;
    }
}



/*
 * Open a raw IP socket and connect it
 * Parameter:
 * @dest - IP destination address (in network byte order)
 * Return value:
 * -1 if an error occured
 * file descriptor of socket otherwise
 */
static int open_socket(unsigned int dest) {
    struct sockaddr_in in;
    struct sockaddr_in sa;
    socklen_t addrlen;
    int fd;
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    /*
     * and connect it to remote host
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = dest;
    if (connect(fd, (struct sockaddr*) &in, sizeof(struct sockaddr_in))) {
        perror("connect");
        return -1;
    }
    /*
     * Validate socket address
     */
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(fd, (struct sockaddr*) &sa, &addrlen)) {
            perror("getsockname");
            return -1;
    }
    if (sizeof(struct sockaddr) != addrlen) {
        printf("Addrlen is %d, expected %d\n", addrlen, sizeof(struct sockaddr));
        return -1;
    }
    if (sa.sin_family != AF_INET) {
        printf("sin_family is %d, expected %d\n", sa.sin_family, AF_INET);
        return -1;
    }
    return fd;
}

/*
 * Process ICMP reply message. Return 0 if this is a matching reply
 */
static int process_reply(unsigned char* in_buffer, int len, unsigned int expected_src) {
    unsigned int src;
    int ip_hdr_length;
    int ip_payload_length;
    icmp_hdr_t* reply_hdr;
    unsigned char* reply_data;
    unsigned short* id;
    unsigned short* seq_no;
    int data_ok;
    int i;
    /*
     * Get IP header length and parse ICMP header
     * 1) hdr_type should be 0
     * 2) code should be 0
     */
    ip_hdr_length = (in_buffer[0] & 0xF)*sizeof(unsigned int);
    ip_payload_length = (in_buffer[2] << 8) + in_buffer[3] - ip_hdr_length;
    reply_hdr = (icmp_hdr_t*) (in_buffer + ip_hdr_length);
    /*
     * Check that IP source address (located at offset 12 in IP header)
     * is our target address
     */
    src = *((unsigned int*) (in_buffer + 12));
    if (src != expected_src) {
        return -1;
    }
    /*
     * Verify checksum
     */
    if (compute_checksum((unsigned short*) reply_hdr, ip_payload_length) != 0)
        return -1;
    /*
     * Extract ID and SEQ_NO
     */
    reply_data = ((void*) reply_hdr) + sizeof(icmp_hdr_t);
    id = (unsigned short*)(reply_data);
    seq_no = id + 1;
    /*
     * Check code
     */
    if ((ICMP_ECHO_REPLY != reply_hdr->type) || (0 != reply_hdr->code))
        return -1;
    /*
     * Length of entire packet should be 20 + 100
     */
    if (ip_payload_length != 100)
        return -1;
    /*
     * Check ID and SEQ_NO
     */
    if (*id != htons(getpid()))
        return -1;
    if ((ntohs(*seq_no) > requests) || (ntohs(*seq_no) < 1))
        return -1;
    /*
     * Finally check remaining data
     */
    data_ok = 1;
    for (i = 0; i < 100 - 4 - sizeof(icmp_hdr_t); i++) {
        if (((unsigned char*) reply_data + 4)[i] != (i % 256))
            data_ok = 0;
    }
    if (1 == data_ok)
        return 0;
    return -1;
}


/*
 * Macro for assertions in unit test cases
 */
#define ASSERT(x)  do { if (!(x)) { \
        printf("Assertion %s failed at line %d in %s..", #x, __LINE__, __FILE__ ); \
        return 1 ;   \
} \
} while (0)

/*
 * Set up statistics
 */
#define INIT  int __failed=0; int __passed=0; int __rc=0 ; \
        printf("------------------------------------------\n"); \
        printf("Starting unit test %s\n", __FILE__); \
        printf("------------------------------------------\n");

/*
 * Print statistic and return
 */
#define END printf("------------------------------------------\n"); \
        printf("Overall test results (%s):\n", __FILE__); \
        printf("------------------------------------------\n"); \
        printf("Failed: %d  Passed:  %d\n", __failed, __passed); \
        printf("------------------------------------------\n"); return __rc;

/*
 * Execute a test case
 */
#define RUN_CASE(x) do { __rc= do_test_case(x, testcase##x);  \
        if (__rc) __failed++; else __passed++;} while (0)

/*
 * Forward declaration - this is in kunit.o
 */
int do_test_case(int x, int (*testcase)());

/*
 * Testcase 1
 * Create and connect a raw IP socket
 */
int testcase1(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = 0;
    /*
     * Now open a raw IP socket for the address family AF_INET. Using IPPROTO_ICMP
     * implies that we will receive only packets with IP protocol type ICMP
     */
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        perror("Could not open raw socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
#ifdef DO_CONNECT
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        ASSERT(0);
        return 1;
    }
#endif
    return 0;
}

/*
 * Testcase 2
 * Create and connect a TCP socket at port 30000
 */
int testcase2(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
#ifdef DO_CONNECT
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        ASSERT(0);
        return 1;
    }
#endif
    return 0;
}

/*
 * Testcase 3
 * Create and connect a TCP socket at port 30000. Then send 512 bytes of data
 */
int testcase3(int argc, char** argv) {
    struct sockaddr_in dest;
    struct sockaddr_in peer;
    socklen_t addrlen;
    int fd;
    int i;
    int j;
    unsigned char buffer[512];
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * Check that getpeername returns correct address
     */
    addrlen = sizeof(struct sockaddr_in);
    if (getpeername(fd, (struct sockaddr*) &peer, &addrlen)) {
        perror("getpeername");
        return -1;
    }
    if (peer.sin_family != AF_INET) {
        printf("Expected address family %d, got %d\n", AF_INET, peer.sin_family);
        return -1;
    }
    if (peer.sin_port != ntohs(port)) {
        printf("Expected port number %d, got %d\n", ntohs(port), ntohs(peer.sin_port));
        return -1;
    }
    if (peer.sin_addr.s_addr != dest_addr) {
        printf("Expected IP address %x, got %x\n", dest_addr, peer.sin_addr.s_addr);
        return -1;
    }
    /*
     * Fill buffer with data and send it
     * We send 8 packets at 512 bytes each so that we fill
     * up 4k in the receive buffer
     */
    for (i = 0; i < 512; i++)
        buffer[i] = i;
    for (j = 0; j < 8; j++) {
        i = sendall(fd, buffer, 512);
        if (i != 512) {
            printf("Sendall did not return 512 as expected, but %d\n", i);
        }
        ASSERT(i == 512);
    }
    return 0;
}

/*
 * Testcase 4
 * Create and connect a TCP socket at port 30000. Then send 100 bytes and receive the echo
 */
int testcase4(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    int i;
    unsigned char snd_buffer[512];
    unsigned char rcv_buffer[512];
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * Fill buffer with data and send it
     */
    for (i = 0; i < 100; i++)
        snd_buffer[i] = i;
    i = sendall(fd, snd_buffer, 100);
    if (i != 100) {
        printf("Send did not return 100 as expected, but %d\n", i);
        ASSERT(i == 100);
    }
    /*
     * and wait for echo
     */
    i = recvall(fd, rcv_buffer, 100);
    if (i < 0) {
        perror("Could not receive data\n");
        ASSERT(0);
    }
    if (0 == i) {
        printf("recv returned 0 - EOF\n");
    }
    else
        ASSERT(100 == i);
    /*
     * compare
     */
    for (i = 0; i < 100; i++)
        ASSERT(snd_buffer[i] == rcv_buffer[i]);
    return 0;
}

/*
 * Testcase 5
 * Create and connect a TCP socket at port 30000. Then fill up receivers window
 */
int testcase5(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    int i;
    int j;
    unsigned char buffer[512];
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * Fill buffer with data and send it
     * We send 16384 packets at 512 bytes each, i.e. 8 MB
     */
    for (i = 0; i < 512; i++)
        buffer[i] = i;
    for (j = 0; j < 16384; j++) {
        ASSERT(512 == writeall(fd, buffer, 512));
    }
    return 0;
}

/*
 * Testcase 6:
 * Listen on a free port. Then transfer the port number to the server which is then expected to establish
 * a connection to this port
 */
int testcase6() {
    struct sockaddr_in dest;
    struct sockaddr_in listen_addr;
    struct sockaddr_in peer_addr;
    socklen_t len;
    int fd;
    int i;
    int rc;
    int listen_fd;
    int new_fd;
    unsigned char buffer[256];
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * Now open a server socket and LISTEN on it
     */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("Could not open server socket");
        printf("errno = %d\n", errno);
        ASSERT(0);
        return 1;
    }
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(port+1);
    if (-1 == bind(listen_fd, (struct sockaddr*) &listen_addr, sizeof(struct sockaddr_in))) {
        perror("Could not bind server socket\n");
        printf("port = %d, errno = %d\n", port, errno);
        ASSERT(0);
        return 1;
    }
    if (-1 == listen(listen_fd, 5)) {
        perror("Could not listen on socket\n");
        ASSERT(0);
        return 1;
    }
    /*
     * Now send one byte to peer. This will trigger a connection attempt on port port+1 one second later
     */
    send(fd, &fd, 1, 0);
    /*
     * and wait for connection
     */
    len = sizeof(struct sockaddr_in);
    new_fd = accept(listen_fd, (struct sockaddr*) &peer_addr, &len);
    if (-1 == new_fd) {
        perror("Could not accept new connection");
        return 1;
    }
    /*
     * Read 256 bytes
     */
    rc = readall(new_fd, buffer, 256);
    if (-1 == rc) {
        perror("Could not read from new socket");
        ASSERT(0);
        return 1;
    }
    ASSERT(256 == rc);
    /*
     * Check data
     */
    for (i = 0; i < 256; i++) {
        if (i != buffer[i])
            printf("Comparison failed at position %d\n", i);
        ASSERT(i == buffer[i]);
    }
    /*
     * and close socket
     */
    close(new_fd);
    return 0;
}

/*
 * Testcase 7
 * Create and connect a TCP socket at port 30000. Then wait until data arrives, using select
 */
int testcase7(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    int i;
    fd_set readfds;
    unsigned char rcv_buffer[512];
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * and wait for data using select
     */
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    select(FD_SETSIZE, &readfds, 0, 0, 0);
    ASSERT(FD_ISSET(fd, &readfds));
    /*
     * Now read data
     */
    i = recvall(fd, rcv_buffer, 100);
    if (i < 0) {
        perror("Could not receive data\n");
        ASSERT(0);
    }
    ASSERT(100 == i);
    /*
     * Wait for one second to give socket enough time to send delayed ACK
     */
    sleep(1);
    return 0;
}

/*
 * Testcase 8
 * Create and connect a TCP socket at port 30000. Then wait until data arrives, using select, and
 * enfore timeout
 */
int testcase8(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    fd_set readfds;
    struct timeval timeout;
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * and wait for data using select - use timeout of 500 ms
     */
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    select(FD_SETSIZE, &readfds, 0, 0, &timeout);
    ASSERT(0 == FD_ISSET(fd, &readfds));
    return 0;
}

/*
 * Testcase 9
 * Create and connect a TCP socket at port 30000. Then set an alarm and wait in a recv call until the alarm goes
 * off
 */
int testcase9(int argc, char** argv) {
    struct sockaddr_in dest;
    int fd;
    fd_set read_fd;
    struct sigaction sa;
    sigset_t set;
    unsigned char buffer[64];
    int rc;
    /*
     * Make sure that SIGALRM is not blocked
     */
    sigemptyset(&set);
    ASSERT(0 == sigprocmask(SIG_SETMASK, &set, 0));
    /*
     * Install signal handler
     */
    sa.sa_handler = signal_handler;
    ASSERT(0 == sigaction(SIGALRM, &sa, 0));
    alarm_raised = 0;
    /*
     * Fill address structure
     */
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * Now open a TCP socket for the address family AF_INET.
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not open TCP socket");
        ASSERT(0);
        return 1;
    }
    /*
     * Try to connect socket
     */
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        return 1;
    }
    /*
     * and wait for data after setting an alarm - use read instead of recv
     */
    alarm(1);
    rc = read(fd, buffer, 32);
    ASSERT(alarm_raised);
    ASSERT(-1 == rc);
    ASSERT(EINTR == errno);
    alarm(0);
    /*
     * Now wait in a select, again after setting an alarm
     */
    FD_ZERO(&read_fd);
    FD_SET(fd, &read_fd);
    alarm_raised = 0;
    alarm(1);
    ASSERT(-1 == select(FD_SETSIZE, &read_fd, 0, 0, 0));
    ASSERT(EINTR == errno);
    ASSERT(alarm_raised);
    alarm(0);
    return 0;
}

/*
 * Testcase 10: ping remote host
 */
int testcase10() {
    fd_set read_fd;
    struct timeval timeout;
    unsigned char in_buffer[100];
    int fd;
    /*
     * Open raw IP socket
     */
    ASSERT((fd = open_socket(dest_addr)) > 0);
    /*
     * Send ping message
     */
    send_ping(fd);
    /*
     * Use select to wait for reply
     */
    while (1) {
        FD_ZERO(&read_fd);
        FD_SET(fd, &read_fd);
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        select(FD_SETSIZE, &read_fd, 0, 0, &timeout);
        ASSERT(1 == FD_ISSET(fd, &read_fd));
        /*
         * and process reply
         */
        ASSERT(100 + 20 == recv(fd, in_buffer, 120, 0));
        if (0 == process_reply(in_buffer, 100, dest_addr))
            break;
    }
    /*
     * Close socket again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 11: send a UDP message to remote host and wait for response
 */
int testcase11() {
    int fd;
    int rc;
    struct sockaddr_in dest;
    unsigned char buffer[100];
    unsigned char in_buffer[100];
    int i;
    /*
     * Create UDP socket
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT(fd > 0);
    /*
     * connect socket to remote host
     */
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    rc = connect(fd, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    if (rc) {
        perror("connect");
    }
    ASSERT(0 == rc);
    /*
     * and send packet
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i*i;
    ASSERT(100 == send(fd, buffer, 100, 0));
    /*
     * Now we should be able to read data again
     */
    ASSERT(100 == recv(fd, in_buffer, 100, 0));
    for (i = 0; i < 100; i++) {
        if (buffer[i] != (unsigned char)(~in_buffer[i]))
            printf("No match at position %i, buffer is %x, in_buffer is %x\n", buffer[i], in_buffer[i]);
        ASSERT(buffer[i] == (unsigned char)(~in_buffer[i]));
    }
    /*
     * close socket again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 12: send a UDP message to remote host and wait for response using select
 */
int testcase12() {
    int fd;
    fd_set readfds;
    int rc;
    struct sockaddr_in dest;
    unsigned char buffer[100];
    unsigned char in_buffer[100];
    int i;
    /*
     * Create UDP socket
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT(fd > 0);
    /*
     * connect socket to remote host
     */
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    rc = connect(fd, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    if (rc) {
        perror("connect");
    }
    ASSERT(0 == rc);
    /*
     * and send packet
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i*i;
    ASSERT(100 == send(fd, buffer, 100, 0));
    /*
     * Wait using select
     */
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    ASSERT(1 == select(1024, &readfds, 0, 0, 0));
    /*
     * Now we should be able to read data again
     */
    ASSERT(100 == recv(fd, in_buffer, 100, 0));
    for (i = 0; i < 100; i++) {
        if (buffer[i] != (unsigned char)(~in_buffer[i]))
            printf("No match at position %i, buffer is %x, in_buffer is %x\n", buffer[i], in_buffer[i]);
        ASSERT(buffer[i] == (unsigned char)(~in_buffer[i]));
    }
    /*
     * close socket again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 13: send a UDP message to remote host using sendto and wait for response using recvfrom
 */
int testcase13() {
    int fd;
    socklen_t addrlen;
    struct sockaddr_in dest;
    struct sockaddr_in src;
    struct sockaddr_in msg_addr;
    unsigned char buffer[100];
    unsigned char in_buffer[100];
    int i;
    int rc;
    /*
     * Create UDP socket
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT(fd > 0);
    /*
     * Bind socket to local address to be able to receive data
     */
    src.sin_addr.s_addr = INADDR_ANY;
    src.sin_port = 0;
    src.sin_family = AF_INET;
    ASSERT(0 == bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)));
    /*
     * Fill destination address
     */
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * and send packet
     */
    addrlen = sizeof(struct sockaddr_in);
    for (i = 0; i < 100; i++)
        buffer[i] = i*i;
    ASSERT(100 == sendto(fd, buffer, 100, 0, (struct sockaddr*) &dest, addrlen));
    /*
     * Now we should be able to read data again
     */
    while (1) {
        rc = recvfrom(fd, in_buffer, 100, 0, (struct sockaddr*) &msg_addr, &addrlen);
        /*
         * If this is from the peer, check data and exit
         */
        if ((msg_addr.sin_port == ntohs(port)) && (msg_addr.sin_addr.s_addr == dest_addr) && (rc > 0)) {
            ASSERT(100 == rc);
            for (i = 0; i < 100; i++) {
                if (buffer[i] != (unsigned char)(~in_buffer[i]))
                    printf("No match at position %i, buffer is %x, in_buffer is %x\n", buffer[i], in_buffer[i]);
                ASSERT(buffer[i] == (unsigned char)(~in_buffer[i]));
            }
            break;
        }
    }
    /*
     * close socket again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 14: send a UDP message to remote host using sendto and wait for response - enforce fragmentation
 */
int testcase14() {
    int fd;
    socklen_t addrlen;
    struct sockaddr_in dest;
    struct sockaddr_in src;
    struct sockaddr_in msg_addr;
    unsigned char buffer[2048];
    unsigned char in_buffer[2048];
    int i;
    int rc;
    /*
     * Create UDP socket
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT(fd > 0);
    /*
     * Bind socket to local address to be able to receive data
     */
    src.sin_addr.s_addr = INADDR_ANY;
    src.sin_port = 0;
    src.sin_family = AF_INET;
    ASSERT(0 == bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)));
    /*
     * Fill destination address
     */
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * and send packet
     */
    addrlen = sizeof(struct sockaddr_in);
    for (i = 0; i < 2048; i++)
        buffer[i] = i*i;
    ASSERT(2048 == sendto(fd, buffer, 2048, 0, (struct sockaddr*) &dest, addrlen));
    /*
     * Now we should be able to read data again
     */
    while (1) {
        rc = recvfrom(fd, in_buffer, 2048, 0, (struct sockaddr*) &msg_addr, &addrlen);
        /*
         * If this is from the peer, check data and exit
         */
        if ((msg_addr.sin_port == ntohs(port)) && (msg_addr.sin_addr.s_addr == dest_addr) && (rc > 0)) {
            ASSERT(2048 == rc);
            for (i = 0; i < 2048; i++) {
                if (buffer[i] != (unsigned char)(~in_buffer[i]))
                    printf("No match at position %i, buffer is %x, in_buffer is %x\n", buffer[i], in_buffer[i]);
                ASSERT(buffer[i] == (unsigned char)(~in_buffer[i]));
            }
            break;
        }
    }
    /*
     * close socket again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 15: send a UDP message to remote host using sendto from a socket which is not bound - the reply should then
 * generate an ICMP message
 */
int testcase15() {
    int fd;
    socklen_t addrlen;
    struct sockaddr_in dest;
    unsigned char buffer[100];
    int i;
    /*
     * Create UDP socket
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT(fd > 0);
    /*
     * Fill destination address
     */
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    /*
     * and send packet
     */
    addrlen = sizeof(struct sockaddr_in);
    for (i = 0; i < 100; i++)
        buffer[i] = i*i;
    ASSERT(100 == sendto(fd, buffer, 100, 0, (struct sockaddr*) &dest, addrlen));
    /*
     * close socket again
     */
    close(fd);
    return 0;
}

int main(int argc, char** argv) {
    /*
     * Convert the arguments into an IP address and port number
     */
    if (argc <= 2) {
        printf("Usage: testnet  <dst_address> <dst_port>\n");
        printf("Will fall back to defaults (port 30000, IP 10.0.2.21\n");
        port = strtoull("30000", 0, 10);
        dest_addr = inet_addr("10.0.2.21");
    }
    else {
        dest_addr = inet_addr((const char*) argv[1]);
        if (argv[2]) {
            port = strtoull(argv[2], 0, 10);
        }
        else {
            printf("ARGV[2] is NULL\n");
            _exit(1);
        }
    }
    /*
     * Run tests
     */
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    END;
    return 0;
}
