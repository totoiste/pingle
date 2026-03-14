/* -- pingle — Minimal and light ICMP ping ------------------------------*/
/*
   This code is derived from software contributed to Berkeley by
   Mike MUUSS.

   Copyright (C) 2026  Pascal MARTINEZ

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License.

   This program is distributed in the hope that it will be useful,
   for the planet 🌱, but WITHOUT ANY WARRANTY; without even the
   implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE. See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>
*/

/*
   Version 1.0
   * GNU coding standards
   * IPv4 support only
*/

#include <stdio.h>             /* printf, fprintf                        */
#include <string.h>            /* memset                                 */
#include <errno.h>             /* strerror                               */
#include <stdarg.h>            /* va_list, vfprintf                      */
#include <stdlib.h>            /* exit                                   */
#include <unistd.h>            /* close, sleep                           */
#include <signal.h>            /* sigaction, sig_atomic_t, SA_RESTART    */
#include <sys/time.h>          /* gettimeofday, struct timeval           */
#include <sys/socket.h>        /* socket, connect, send, recv            */
#include <netinet/in.h>        /* sockaddr_in, IPPROTO_ICMP, htons       */
#include <netinet/ip_icmp.h>   /* icmphdr, ICMP_ECHO, ICMP_ECHOREPLY     */
#include <arpa/inet.h>         /* inet_ntop, INET_ADDRSTRLEN             */
#include <netdb.h>             /* getaddrinfo, gai_strerror              */
#include <math.h>              /* sqrt (mdev computation)                */

/* Probe timeout in seconds.  */
#define TIMEOUT_SEC  1

/* Extra bytes after the ICMP header. 0 = 8-byte minimal packet.
   Set to 56 to match iputils and waste resources 🍂 */
#define PAYLOAD_SIZE 0

/* -- Signal handler ----------------------------------------------------*/
/* sig_atomic_t: the only type safe to write from a signal handler.  */
static volatile sig_atomic_t running = 1;

/* Set running=0 on Ctrl-C so the statistics block always prints.  */
static void
on_sigint (int s)
{
  (void) s;
  running = 0;
}

/* -- Global Counters -----------------------------------------------------*/
static int sent, received;

/* -- Helpers -------------------------------------------------------------*/
/* Print usage to stderr and exit 2 (usage error convention).  */
static void
usage (void)
{
  fprintf (stderr,
           "Usage\n"
           "  pingle <destination>\n"
           "\nOptions:\n"
           "  <destination>   DNS name or IP address\n");
  exit (2);
}

/* Print formatted error to stderr, close fd if >= 0, then exit 1.  */
static void
error (int fd, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
  fputc ('\n', stderr);
  if (fd >= 0)
    close (fd);
  exit (1);
}

/* -- Entry point ---------------------------------------------------------*/
int
main (int argc, char *argv[])
{
  if (argc != 2)
    usage ();

  /* AF_INET: IPv4 only */
  struct addrinfo hints = { .ai_family = AF_INET }, *res;

  /* getaddrinfo() returns its own error codes, not errno.  */
  int ret_val = getaddrinfo (argv[1], NULL, &hints, &res);
  if (ret_val)
    error (-1, "getaddrinfo %s: %s", argv[1], gai_strerror (ret_val));

  /* Resolve IP string early so it appears in all error messages.  */
  char ip[INET_ADDRSTRLEN];
  inet_ntop (AF_INET,
             &((struct sockaddr_in *) res->ai_addr)->sin_addr,
             ip, sizeof ip);

  /* SOCK_DGRAM: kernel handles IP header and checksum.
     No CAP_NET_RAW needed (sysctl ping_group_range).  */
  int fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
  if (fd < 0)
    error (-1, "socket (SOCK_DGRAM/ICMP): %s", strerror (errno));

  /* connect() binds the destination so send()/recv() work without
     sockaddr args, and the kernel drops replies from other hosts.  */
  if (connect (fd, res->ai_addr, res->ai_addrlen) < 0)
    error (fd, "connect to %s: %s", ip, strerror (errno));

  printf ("PING %s (%s) 🐧\n", argv[1], ip);

  /* Free resolver result before entering the loop.  */
  freeaddrinfo (res);

  /* sigaction() guarantees stable handler across SVR4-derived systems;
     SA_RESTART avoids EINTR on slow syscalls. */
  struct sigaction sa = { .sa_handler = on_sigint, .sa_flags = SA_RESTART };
  sigaction (SIGINT, &sa, NULL);

  /* Without SO_RCVTIMEO, recv() would block forever on packet loss.  */
  struct timeval tv = { .tv_sec = TIMEOUT_SEC };
  if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0)
    error (fd, "setsockopt SO_RCVTIMEO: %s", strerror (errno));

  /* Single 8-byte buffer reused for send and recv.  */
  uint8_t pkt[sizeof (struct icmphdr) + PAYLOAD_SIZE];

  /* uint16_t: wraps at 65535, matching the ICMP sequence field width.  */
  uint16_t seq = 0;

  struct timeval start_time, recv_time, wall_start, wall_end;

  /* rtt_sum2 accumulates squared RTTs for single-pass mdev:
     mdev = sqrt(E[rtt²] - E[rtt]²).  */
  double rtt_min = 1e9, rtt_max = 0.0, rtt_sum = 0.0, rtt_sum2 = 0.0;

  gettimeofday (&wall_start, NULL);

  while (running)
    {
      /* Clear packet before each send; avoids leaking stack data.  */
      memset (pkt, 0, sizeof pkt);
      struct icmphdr *icmp   = (struct icmphdr *) pkt;
      icmp->type             = ICMP_ECHO;
      icmp->un.echo.sequence = htons (++seq);  /* RFC 792: big-endian */

      /* Timestamp before send to include scheduling jitter in RTT.  */
      gettimeofday (&start_time, NULL);

      /* Skip sent++ on failure to keep loss statistics accurate.  */
      if (send (fd, pkt, sizeof pkt, 0) < 0)
        {
          fprintf (stderr, "send: %s\n", strerror (errno));
          sleep (1);
          continue;
        }
      sent++;

      ssize_t n = recv (fd, pkt, sizeof pkt, 0);
      if (n < 0)
        {
          printf ("icmp_seq=%d timeout > %d s\n", seq, TIMEOUT_SEC);
        }
      else
        {
          /* Reject ICMP errors and out-of-order replies.  */
          if (icmp->type != ICMP_ECHOREPLY
              || ntohs (icmp->un.echo.sequence) != seq)
            {
              printf ("icmp_seq=%d unexpected reply (type=%d)\n",
                      seq, icmp->type);
            }
          else
            {
              gettimeofday (&recv_time, NULL);
              received++;

              /* Explicit cast long → double; silences -Wconversion.  */
              double rtt =
                (double)(recv_time.tv_sec  - start_time.tv_sec)  * 1e3
              + (double)(recv_time.tv_usec - start_time.tv_usec) / 1e3;

              if (rtt < rtt_min) rtt_min = rtt;
              if (rtt > rtt_max) rtt_max = rtt;
              rtt_sum  += rtt;
              rtt_sum2 += rtt * rtt;

              printf ("%zd bytes from %s: icmp_seq=%d time=%.2f ms\n",
                      n, ip, seq, rtt);  /* %zd: ssize_t (C99) */
            }
        }

      sleep (1);
    }
/* -- Calculate Final Statistics ------------------------------------------*/
  gettimeofday (&wall_end, NULL);
  long wall_ms = (wall_end.tv_sec  - wall_start.tv_sec)  * 1000
               + (wall_end.tv_usec - wall_start.tv_usec) / 1000;

  printf ("\n--- %s ping statistics ---\n", argv[1]);
  printf ("%d packets transmitted, %d received, %d%% packet loss, time %ldms\n",
          sent, received,
          sent ? (sent - received) * 100 / sent : 0,  /* guard /0 */
          wall_ms);

  if (received > 0)
    {
      double avg  = rtt_sum / received;
      double mdev = sqrt (rtt_sum2 / received - avg * avg);
      printf ("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
              rtt_min, avg, rtt_max, mdev);
    }

/* -- End of program - exit -----------------------------------------------*/
  close (fd);
  return !received;  /* 0 = success, 1 = no reply (iputils convention) */
}
