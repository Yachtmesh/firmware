#pragma once

#include <unity.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "TcpServer.h"

// --- Helpers ---

static void tcpSetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Write into fd until the kernel send buffer is full (EAGAIN).
static void tcpFillSendBuffer(int fd) {
    char buf[4096] = {};
    while (write(fd, buf, sizeof(buf)) > 0) {
    }
}

// --- tcpSendFrame: atomic frame delivery ---
//
// Contract:
//   true  → all bytes sent, OR partial write (some bytes sent — client kept, frame corrupted),
//           OR EAGAIN (buffer full — frame dropped cleanly, client kept)
//   false → hard error only (ECONNRESET, EPIPE, etc. — caller should close client)

void test_tcpSendFrame_full_send_returns_true() {
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    TEST_ASSERT_TRUE(tcpSendFrame(fds[0], "hello", 5));

    close(fds[0]);
    close(fds[1]);
}

void test_tcpSendFrame_empty_frame_returns_true() {
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    TEST_ASSERT_TRUE(tcpSendFrame(fds[0], "x", 0));

    close(fds[0]);
    close(fds[1]);
}

void test_tcpSendFrame_eagain_returns_true_client_kept() {
    // When the send buffer is full (EAGAIN), the frame is silently dropped
    // but the client connection remains intact (return true).
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    tcpFillSendBuffer(fds[0]);

    TEST_ASSERT_TRUE(tcpSendFrame(fds[0], "frame", 5));

    close(fds[0]);
    close(fds[1]);
}

void test_tcpSendFrame_partial_write_keeps_connection() {
    // When the kernel accepts fewer bytes than requested (partial write),
    // the remaining bytes are lost for this frame but the connection must
    // be kept alive (return true). On lwip, a synchronous retry after a partial
    // write always hits EAGAIN anyway, so disconnecting would be wrong.
    // Force a partial write by setting a tiny receive buffer on the peer
    // and filling it to within less than our frame size.
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));

    int rcvbuf = 64;
    setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    tcpSetNonBlocking(fds[0]);

    // Fill all but the last few bytes of the receive buffer so that a
    // subsequent send of a larger frame triggers a partial write.
    int actual = 0;
    socklen_t optlen = sizeof(actual);
    getsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &actual, &optlen);
    // Leave 8 bytes free; a 16-byte frame will partially write then stop.
    int fillBytes = actual > 8 ? actual - 8 : 0;
    if (fillBytes > 0) {
        char fill[4096] = {};
        // write() in chunks up to fillBytes
        int written = 0;
        while (written < fillBytes) {
            int chunk = fillBytes - written < (int)sizeof(fill)
                            ? fillBytes - written
                            : (int)sizeof(fill);
            int n = (int)write(fds[0], fill, chunk);
            if (n <= 0) break;
            written += n;
        }
    }

    // Send a frame larger than remaining space — may result in partial write.
    // Either way, connection must be kept (return true).
    char frame[16] = {};
    TEST_ASSERT_TRUE(tcpSendFrame(fds[0], frame, sizeof(frame)));

    close(fds[0]);
    close(fds[1]);
}

void test_tcpSendFrame_connection_error_returns_false() {
    // When the remote end closes, sends should eventually fail.
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    close(fds[1]);  // Remote closes

    // First send may succeed (data buffered before FIN is processed);
    // keep trying until the OS reports the error.
    bool result = true;
    for (int i = 0; i < 10 && result; i++) {
        result = tcpSendFrame(fds[0], "frame", 5);
    }

    TEST_ASSERT_FALSE(result);

    close(fds[0]);
}

// --- tcpDrainRecv: dead client detection ---
//
// Contract:
//   true  → client still alive (no data or data drained successfully)
//   false → client gone (FIN received or socket error)

void test_tcpDrainRecv_no_data_returns_true() {
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    TEST_ASSERT_TRUE(tcpDrainRecv(fds[0]));

    close(fds[0]);
    close(fds[1]);
}

void test_tcpDrainRecv_with_pending_data_returns_true() {
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    write(fds[1], "ping", 4);

    TEST_ASSERT_TRUE(tcpDrainRecv(fds[0]));

    close(fds[0]);
    close(fds[1]);
}

void test_tcpDrainRecv_remote_closed_returns_false() {
    // recv() returning 0 means the remote sent FIN — client is gone.
    int fds[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    tcpSetNonBlocking(fds[0]);

    close(fds[1]);  // Remote closes (sends FIN)

    TEST_ASSERT_FALSE(tcpDrainRecv(fds[0]));

    close(fds[0]);
}
