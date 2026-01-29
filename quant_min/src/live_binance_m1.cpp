#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <signal.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// ===================== utils =====================
static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static std::int64_t now_ns() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

// minimal base64 (for Sec-WebSocket-Key)
static std::string b64_encode(const unsigned char* data, size_t len) {
  static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    unsigned v = data[i] << 16;
    if (i + 1 < len) v |= data[i + 1] << 8;
    if (i + 2 < len) v |= data[i + 2];
    out.push_back(tbl[(v >> 18) & 63]);
    out.push_back(tbl[(v >> 12) & 63]);
    out.push_back((i + 1 < len) ? tbl[(v >> 6) & 63] : '=');
    out.push_back((i + 2 < len) ? tbl[v & 63] : '=');
  }
  return out;
}

static std::string sha1_base64(std::string_view s) {
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);
  return b64_encode(hash, SHA_DIGEST_LENGTH);
}

// ws frame send (client -> server must mask)
static bool ws_send_text(SSL* ssl, std::string_view payload) {
  const uint8_t opcode_text = 0x1;
  std::vector<uint8_t> frame;
  frame.reserve(payload.size() + 32);

  uint8_t b0 = 0x80 | opcode_text;  // FIN=1
  frame.push_back(b0);

  const bool mask = true;
  const uint8_t maskbit = mask ? 0x80 : 0;

  size_t n = payload.size();
  if (n <= 125) {
    frame.push_back(static_cast<uint8_t>(maskbit | n));
  } else if (n <= 0xFFFF) {
    frame.push_back(static_cast<uint8_t>(maskbit | 126));
    frame.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(n & 0xFF));
  } else {
    frame.push_back(static_cast<uint8_t>(maskbit | 127));
    for (int i = 7; i >= 0; --i) frame.push_back(static_cast<uint8_t>((n >> (8 * i)) & 0xFF));
  }

  uint8_t mask_key[4];
  if (RAND_bytes(mask_key, 4) != 1) return false;
  frame.insert(frame.end(), mask_key, mask_key + 4);

  // masked payload
  for (size_t i = 0; i < n; ++i) {
    frame.push_back(static_cast<uint8_t>(payload[i]) ^ mask_key[i & 3]);
  }

  size_t off = 0;
  while (off < frame.size()) {
    int w = SSL_write(ssl, frame.data() + off, (int)(frame.size() - off));
    if (w > 0) { off += (size_t)w; continue; }
    int err = SSL_get_error(ssl, w);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
    return false;
  }
  return true;
}

// very small ws frame parser for server->client (no mask)
// returns: consumed bytes in buf; payload bytes appended into out_payload (for text/binary)
static size_t ws_try_parse_one(std::vector<uint8_t>& buf, std::vector<uint8_t>& out_payload, uint8_t& out_opcode) {
  out_payload.clear();
  out_opcode = 0;

  if (buf.size() < 2) return 0;
  const uint8_t b0 = buf[0];
  const uint8_t b1 = buf[1];
  const bool fin = (b0 & 0x80) != 0;
  (void)fin;
  out_opcode = (b0 & 0x0F);

  const bool masked = (b1 & 0x80) != 0;
  uint64_t len = (b1 & 0x7F);

  size_t idx = 2;
  if (len == 126) {
    if (buf.size() < idx + 2) return 0;
    len = ((uint64_t)buf[idx] << 8) | buf[idx + 1];
    idx += 2;
  } else if (len == 127) {
    if (buf.size() < idx + 8) return 0;
    len = 0;
    for (int i = 0; i < 8; ++i) { len = (len << 8) | buf[idx + i]; }
    idx += 8;
  }

  uint8_t mask_key[4]{0,0,0,0};
  if (masked) {
    if (buf.size() < idx + 4) return 0;
    mask_key[0]=buf[idx]; mask_key[1]=buf[idx+1]; mask_key[2]=buf[idx+2]; mask_key[3]=buf[idx+3];
    idx += 4;
  }

  if (buf.size() < idx + (size_t)len) return 0;

  out_payload.resize((size_t)len);
  if (!masked) {
    std::memcpy(out_payload.data(), buf.data() + idx, (size_t)len);
  } else {
    for (size_t i = 0; i < (size_t)len; ++i) out_payload[i] = buf[idx + i] ^ mask_key[i & 3];
  }

  return idx + (size_t)len;
}

// ===================== network connect (tcp) =====================
static int tcp_connect_nonblock(const char* host, const char* port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  //hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* res = nullptr;
  int rc = getaddrinfo(host, port, &hints, &res);
  if (rc != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rc) << "\n";
    return -1;
  }

  int fd = -1;
  for (addrinfo* p = res; p; p = p->ai_next) {
    fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;

    if (set_nonblocking(fd) != 0) { ::close(fd); fd = -1; continue; }

    int c = ::connect(fd, p->ai_addr, p->ai_addrlen);
    if (c == 0) break;
    if (c < 0 && errno == EINPROGRESS) break;

    ::close(fd);
    fd = -1;
  }

  freeaddrinfo(res);
  return fd;
}

static bool wait_connect_writable(int epfd, int fd, int timeout_ms) {
  epoll_event ev{};
  ev.events = EPOLLOUT | EPOLLET;
  ev.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
    std::cerr << "epoll_ctl add: " << strerror(errno) << "\n";
    return false;
  }

  epoll_event events[4];
  int n = epoll_wait(epfd, events, 4, timeout_ms);
  if (n <= 0) return false;

  int err = 0;
  socklen_t len = sizeof(err);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) return false;
  return err == 0;
}

// ===================== main =====================
int main(int argc, char** argv) {
  ::signal(SIGPIPE, SIG_IGN);

  const char* host = "stream.binance.com";
  const char* port = "9443";
  //const char* port = "443";
  const char* path = "/ws"; // connect then SUBSCRIBE message (recommended by community examples)
  // Alternative raw stream URL: /ws/btcusdt@trade  (also works)
  // Binance docs describe /ws/<streamName> and /stream?streams=... :contentReference[oaicite:1]{index=1}

  std::string symbol = "btcusdt";
  std::string stream = symbol + "@trade"; // simplest

  if (argc >= 2) symbol = argv[1];
  stream = symbol + "@trade";

  // -------- SSL init --------
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  const SSL_METHOD* method = TLS_client_method();
  SSL_CTX* ctx = SSL_CTX_new(method);
  if (!ctx) { std::cerr << "SSL_CTX_new failed\n"; return 1; }

  // Enable SNI + hostname verify (best practice)
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
  SSL_CTX_set_default_verify_paths(ctx);

  // -------- TCP connect nonblock + epoll wait --------
  int fd = tcp_connect_nonblock(host, port);
  if (fd < 0) { std::cerr << "tcp connect failed\n"; return 1; }

  int epfd = epoll_create1(EPOLL_CLOEXEC);
  if (epfd < 0) { std::cerr << "epoll_create1 failed\n"; return 1; }

  if (!wait_connect_writable(epfd, fd, 3000)) {
    std::cerr << "connect timeout/fail\n";
    return 1;
  }

  // -------- SSL object --------
  SSL* ssl = SSL_new(ctx);
  if (!ssl) { std::cerr << "SSL_new failed\n"; return 1; }
  SSL_set_fd(ssl, fd);
  SSL_set_tlsext_host_name(ssl, host);

  // Nonblocking SSL handshake driven by epoll
  bool hs_ok = false;
  {
    // switch epoll to read/write for handshake
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

    for (int it = 0; it < 2000 && !hs_ok; ++it) {
      int r = SSL_connect(ssl);
      if (r == 1) { hs_ok = true; break; }
      int e = SSL_get_error(ssl, r);
      if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
        epoll_event events[4];
        (void)epoll_wait(epfd, events, 4, 5);
        continue;
      }
      unsigned long oe = ERR_get_error();
      std::cerr << "SSL_connect failed: " << ERR_error_string(oe, nullptr) << "\n";
      return 1;
    }
  }
  if (!hs_ok) { std::cerr << "TLS handshake failed\n"; return 1; }

  // -------- WebSocket handshake --------
  unsigned char key_raw[16];
  RAND_bytes(key_raw, sizeof(key_raw));
  std::string sec_key = b64_encode(key_raw, sizeof(key_raw));
  std::string accept_expected = sha1_base64(sec_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

  std::string req =
      "GET " + std::string(path) + " HTTP/1.1\r\n"
      "Host: " + std::string(host) + ":" + std::string(port) + "\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: " + sec_key + "\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n";

  // write handshake
  size_t off = 0;
  while (off < req.size()) {
    int w = SSL_write(ssl, req.data() + off, (int)(req.size() - off));
    if (w > 0) { off += (size_t)w; continue; }
    int e = SSL_get_error(ssl, w);
    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) continue;
    std::cerr << "SSL_write handshake failed\n";
    return 1;
  }

  // read handshake response (until \r\n\r\n)
  std::string resp;
  resp.reserve(4096);
  for (;;) {
    char tmp[2048];
    int r = SSL_read(ssl, tmp, (int)sizeof(tmp));
    if (r > 0) {
      resp.append(tmp, tmp + r);
      if (resp.find("\r\n\r\n") != std::string::npos) break;
      continue;
    }
    int e = SSL_get_error(ssl, r);
    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) continue;
    std::cerr << "SSL_read handshake resp failed\n";
    return 1;
  }

  if (resp.find(" 101 ") == std::string::npos && resp.find(" 101\r") == std::string::npos) {
    std::cerr << "WS upgrade not 101. resp:\n" << resp << "\n";
    return 1;
  }
  // very lightweight check for Sec-WebSocket-Accept
  auto pos = resp.find("Sec-WebSocket-Accept:");
  if (pos != std::string::npos) {
    auto end = resp.find("\r\n", pos);
    auto line = resp.substr(pos, end - pos);
    if (line.find(accept_expected) == std::string::npos) {
      std::cerr << "WS accept mismatch.\nExpected: " << accept_expected << "\nGot line: " << line << "\n";
      return 1;
    }
  }

  std::cout << "[OK] TLS + WS handshake complete. Subscribing to " << stream << "\n";

  // SUBSCRIBE message (Binance streams allow subscribe after connecting to /ws as well) :contentReference[oaicite:2]{index=2}
  std::string sub = std::string("{\"method\":\"SUBSCRIBE\",\"params\":[\"") + stream + "\"],\"id\":1}";
  if (!ws_send_text(ssl, sub)) {
    std::cerr << "send SUBSCRIBE failed\n";
    return 1;
  }

  // -------- epoll loop: read raw frames, do not parse JSON --------
  {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
  }

  std::vector<uint8_t> inbuf;
  inbuf.reserve(1 << 20);

  std::vector<uint8_t> framebuf; framebuf.reserve(1 << 20);
  std::vector<uint8_t> payload; payload.reserve(1 << 20);

  std::int64_t bytes_total = 0;
  std::int64_t frames = 0;
  std::int64_t t0 = now_ns();
  std::int64_t last_print = t0;

  while (true) {
    epoll_event events[16];
    int n = epoll_wait(epfd, events, 16, 1000);
    if (n < 0) {
      if (errno == EINTR) continue;
      std::cerr << "epoll_wait err: " << strerror(errno) << "\n";
      break;
    }

    if (n == 0) {
      // timeout: print stats
      auto tn = now_ns();
      double sec = (tn - t0) / 1e9;
      std::cout << "[stats] sec=" << sec
                << " frames=" << frames
                << " bytes=" << bytes_total
                << " MB/s=" << (bytes_total / (1024.0 * 1024.0)) / (sec > 0 ? sec : 1)
                << "\n";
      continue;
    }

    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd != fd) continue;

      // read all available data (ET)
      for (;;) {
        uint8_t tmp[8192];
        int r = SSL_read(ssl, tmp, (int)sizeof(tmp));
        if (r > 0) {
          bytes_total += r;
          framebuf.insert(framebuf.end(), tmp, tmp + r);

          // try parse as many frames as possible
          while (true) {
            uint8_t opcode = 0;
            size_t consumed = ws_try_parse_one(framebuf, payload, opcode);
            if (consumed == 0) break;

            framebuf.erase(framebuf.begin(), framebuf.begin() + (ptrdiff_t)consumed);
            ++frames;

            // opcode 0x1=text, 0x2=binary, 0x9=ping, 0xA=pong, 0x8=close
            if (opcode == 0x8) {
              std::cout << "server close\n";
              goto done;
            }
            if (opcode == 0x9) {
              // ping -> pong (optional for milestone 1; OpenSSL doesn't auto)
              // Minimal: ignore; Binance says server sends ping frames periodically; should respond in later milestone.
            } else if (opcode == 0x1 || opcode == 0x2) {
              // print first few bytes occasionally
              auto tn = now_ns();
              if (tn - last_print > 1'000'000'000LL) {
                last_print = tn;
                std::string_view sv(reinterpret_cast<const char*>(payload.data()),
                                    std::min<size_t>(payload.size(), 200));
                std::cout << "[frame] opcode=" << (int)opcode
                          << " len=" << payload.size()
                          << " head=" << sv
                          << "\n";
              }
            }
          }
          continue;
        }

        int e = SSL_get_error(ssl, r);
        if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) break;
        std::cerr << "SSL_read error, closing.\n";
        goto done;
      }
    }
  }

done:
  SSL_shutdown(ssl);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  close(fd);
  close(epfd);
  return 0;
}
