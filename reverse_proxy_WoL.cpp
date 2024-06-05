
// A simple reverse proxy (proxying the raw content of TCP streams) that lets a
// frontend seamlessly access a backend that might be asleep or powered down.
// The backend must have wake-on-LAN enabled.
// The machine running this program must have `sudo etherwake` be passwordless.

// THINGS YOU NEED TO CONFIGURE
// =============================================================
#error configure me!!!!! fill in the defines in this section then compile again.
#define BACKEND_IP "192.168.11.11"
#define BACKEND_PORT 8080
#define BACKEND_MAC "ab:cd:ef:01:23:45"
// what network interface (on the machine running this program) should etherwake use?
#define WOL_SOURCE_IFACE "enp0s31f6"
// choose whatever port you like, and then point your frontend at
// http://ip_of_machine_running_this_program:THIS_PROGRAM_LISTEN_PORT/
#define THIS_PROGRAM_LISTEN_PORT "3939"
// =============================================================


#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <mutex>
using namespace std;

void proxyHalf(int fd_from, int fd_to, bool* closed, mutex* mu)
{
  uint8_t buf[2048];
  int res = 39393939;
  while (res > 0)
  {
    res = recv(fd_from, buf, 2048, 0);
    if (res > 0 && send(fd_to, buf, res, 0) < res)
      break;
  }
  if (res == 0)
    shutdown(fd_to, SHUT_WR);
  else
  {
    lock_guard<mutex> lock(*mu);
    if (!*closed)
    {
      *closed = true;
      close(fd_to);
      close(fd_from);
    }
  }
}

int connectTCP1sTimeout();
void proxySession(int fd_accepted_from_fe)
{
  // start waking immediately if needed. no effect if already awake.
  thread wake(system, "sudo etherwake -i " WOL_SOURCE_IFACE " " BACKEND_MAC);
  wake.detach();
  int fd_to_backend = -1;
  for (int i = 0; i < 300 && fd_to_backend < 0; i++)
  {
    fd_to_backend = connectTCP1sTimeout();
    if (fd_to_backend == -1) // -2 means timeout, -1 likely immediate failure
      this_thread::sleep_for(chrono::seconds(1)); // so don't blow thru all 300
  }
  if (fd_to_backend < 0)
  {
    fprintf(stderr, "failed hundreds of attempts to connect to %s:%d\n",
            BACKEND_IP, BACKEND_PORT);
    close(fd_accepted_from_fe);
    return;
  }
  bool closed = false;
  mutex mu;
  thread f_to_b(proxyHalf, fd_accepted_from_fe, fd_to_backend, &closed, &mu);
  thread b_to_f(proxyHalf, fd_to_backend, fd_accepted_from_fe, &closed, &mu);
  f_to_b.join();
  b_to_f.join();
  lock_guard<mutex> lock(mu);
  if (!closed)
  {
    close(fd_to_backend);
    close(fd_accepted_from_fe);
  }
}

int listenTCP();
int main()
{
  int listen_fd = listenTCP();
  while (true)
  {
    int fd = accept(listen_fd, nullptr, nullptr);
    thread proxy_session(proxySession, fd);
    proxy_session.detach();
  }
}



int connectTCP1sTimeout()
{
  struct sockaddr_in dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  if (inet_pton(AF_INET, BACKEND_IP, &dest_addr.sin_addr) != 1)
    fprintf(stderr, "inet_pton didn't like %s\n", BACKEND_IP);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(BACKEND_PORT);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    perror("socket");
    return -1;
  }
  fd_set fdset; FD_ZERO(&fdset); FD_SET(sockfd, &fdset);
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  if (connect(sockfd, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) == -1 && errno != EINPROGRESS)
  {
    close(sockfd);
    return -1;
  }

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (select(sockfd+1, NULL, &fdset, NULL, &timeout) > 0)
  {
    int so_error; socklen_t len = sizeof(so_error);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error == 0)
    {
      fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) & ~O_NONBLOCK);
      return sockfd; // success
    }
    close(sockfd);
    return -1; // probably rejected
  }
  close(sockfd);
  return -2; // timeout
}

int listenTCP()
{
  int fd;  // listen on sock_fd
  struct addrinfo hints, *servinfo, *p;
  int yes=1;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, THIS_PROGRAM_LISTEN_PORT, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next)
  {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      perror("setsockopt SO_REUSEADDR");
      exit(1);
    }
    if (bind(fd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(fd);
      perror("bind() syscall");
      continue;
    }
    break;
  }
  freeaddrinfo(servinfo);

  if (p == NULL)
  {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }
  if (listen(fd, 10) == -1) // allow connection backlog of up to 10
  {
    perror("listen() syscall");
    exit(1);
  }
  fprintf(stderr, "successfully bound to %s\n", THIS_PROGRAM_LISTEN_PORT);
  return fd;
}
