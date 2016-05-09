/**
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WEBSOCKET_H_INCLUDED
#define WEBSOCKET_H_INCLUDED

#include <netinet/in.h>
#include <limits.h>
#include <sys/select.h>

#if defined(__linux__) || defined(__CYGWIN__)
#  include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  if !defined(be16toh)
#    define be16toh(x) betoh16(x)
#  endif
#  if !defined(be32toh)
#    define be32toh(x) betoh32(x)
#  endif
#  if !defined(be64toh)
#    define be64toh(x) betoh64(x)
#  endif
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define htobe16(x) OSSwapHostToBigInt16(x)
#  define htobe64(x) OSSwapHostToBigInt64(x)
#  define be16toh(x) OSSwapBigToHostInt16(x)
#  define be32toh(x) OSSwapBigToHostInt32(x)
#  define be64toh(x) OSSwapBigToHostInt64(x)
#else
#  error Platform not supported!
#endif

#include "gslist.h"

#define WS_PIPEIN "/tmp/wspipein.fifo"
#define WS_PIPEOUT "/tmp/wspipeout.fifo"

#define WS_BAD_REQUEST_STR "HTTP/1.1 400 Invalid Request\r\n\r\n"
#define WS_SWITCH_PROTO_STR "HTTP/1.1 101 Switching Protocols"
#define WS_TOO_BUSY_STR "HTTP/1.1 503 Service Unavailable\r\n\r\n"

#define CRLF "\r\n"
#define SHA_DIGEST_LENGTH     20

#define WS_MAX_FRM_SZ         1048576   /* 1 MiB */
/* packet header is 2 unit32_t : type, size */
#define HDR_SIZE              3 * 4

#define WS_MAGIC_STR "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_PAYLOAD_EXT16      126
#define WS_PAYLOAD_EXT64      127
#define WS_PAYLOAD_FULL       125
#define WS_FRM_HEAD_SZ         16       /* frame header size */

#define WS_FRM_FIN(x)         (((x) >> 7) & 0x01)
#define WS_FRM_MASK(x)        (((x) >> 7) & 0x01)
#define WS_FRM_R1(x)          (((x) >> 6) & 0x01)
#define WS_FRM_R2(x)          (((x) >> 5) & 0x01)
#define WS_FRM_R3(x)          (((x) >> 4) & 0x01)
#define WS_FRM_OPCODE(x)      ((x) & 0x0F)
#define WS_FRM_PAYLOAD(x)     ((x) & 0x7F)

#define WS_CLOSE_NORMAL       1000
#define WS_CLOSE_GOING_AWAY   1001
#define WS_CLOSE_PROTO_ERR    1002
#define WS_CLOSE_INVALID_UTF8 1007
#define WS_CLOSE_TOO_LARGE    1009
#define WS_CLOSE_UNEXPECTED   1011

typedef enum WSSTATUS
{
  WS_OK = 0,
  WS_ERR = (1 << 0),
  WS_CLOSE = (1 << 1),
  WS_READING = (1 << 2),
  WS_SENDING = (1 << 3),
} WSStatus;

typedef enum WSOPCODE
{
  WS_OPCODE_CONTINUATION = 0x00,
  WS_OPCODE_TEXT = 0x01,
  WS_OPCODE_BIN = 0x02,
  WS_OPCODE_END = 0x03,
  WS_OPCODE_CLOSE = 0x08,
  WS_OPCODE_PING = 0x09,
  WS_OPCODE_PONG = 0x0A,
} WSOpcode;

typedef struct WSQueue_
{
  char *queued;                 /* queue data */
  int qlen;                     /* queue length */
} WSQueue;

typedef struct WSPacket_
{
  uint32_t type;                /* packet type (fixed-size) */
  uint32_t size;                /* payload size in bytes (fixed-size) */
  char *data;                   /* payload */
  int len;                      /* payload buffer len */
} WSPacket;

/* WS HTTP Headers */
typedef struct WSHeaders_
{
  int reading;
  int buflen;
  char buf[BUFSIZ + 1];

  char *agent;
  char *path;
  char *method;
  char *protocol;
  char *host;
  char *origin;
  char *upgrade;
  char *referer;
  char *connection;
  char *ws_protocol;
  char *ws_key;
  char *ws_sock_ver;

  char *ws_accept;
  char *ws_resp;
} WSHeaders;

/* A WebSocket Message */
typedef struct WSFrame_
{
  /* frame format */
  WSOpcode opcode;              /* frame opcode */
  unsigned char fin;            /* frame fin flag */
  unsigned char mask[4];        /* mask key */
  uint8_t res;                  /* extensions */
  int payload_offset;           /* end of header/start of payload */
  int payloadlen;               /* payload length (for each frame) */

  /* status flags */
  int reading;                  /* still reading frame's header part? */
  int masking;                  /* are we masking the frame? */

  char buf[WS_FRM_HEAD_SZ + 1]; /* frame's header */
  int buflen;                   /* recv'd buf length so far (for each frame) */
} WSFrame;

/* A WebSocket Message */
typedef struct WSMessage_
{
  WSOpcode opcode;              /* frame opcode */
  int fragmented;               /* reading a fragmented frame */
  int mask_offset;              /* for fragmented frames */

  char *payload;                /* payload message */
  int payloadsz;                /* total payload size (whole message) */
  int buflen;                   /* recv'd buf length so far (for each frame) */
} WSMessage;

/* FD event states */
typedef struct WSEState_
{
  fd_set master;
  fd_set rfds;
  fd_set wfds;
} WSEState;

/* A WebSocket Client */
typedef struct WSClient_
{
  /* socket data */
  int listener;                 /* socket */
  char remote_ip[INET6_ADDRSTRLEN];     /* client IP */

  WSQueue *sockqueue;           /* sending buffer */
  WSEState *state;              /* FDs states */
  WSHeaders *headers;           /* HTTP headers */
  WSFrame *frame;               /* frame headers */
  WSMessage *message;           /* message */
  WSStatus status;              /* connection status */

  struct timeval start_proc;
  struct timeval end_proc;
} WSClient;

/* Config OOptions */
typedef struct WSPipeIn_
{
  int fd;                       /* named pipe FD */

  WSPacket *packet;             /* FIFO data's buffer */
  WSEState *state;              /* FDs states */

  char hdr[HDR_SIZE];           /* FIFO header's buffer */
  int hlen;
} WSPipeIn;

/* Pipe Out */
typedef struct WSPipeOut_
{
  int fd;                       /* named pipe FD */
  WSEState *state;              /* FDs states */
  WSQueue *fifoqueue;           /* FIFO out queue */
} WSPipeOut;

/* Config OOptions */
typedef struct WSConfig_
{
  /* Config Options */
  const char *accesslog;
  const char *host;
  const char *origin;
  const char *pipein;
  const char *pipeout;
  const char *port;
  int echomode;
  int strict;
  int max_frm_size;
} WSConfig;

/* A WebSocket Instance */
typedef struct WSServer_
{
  /* Server Status */
  int closing;

  /* Callbacks */
  int (*onclose) (WSPipeOut * pipeout, WSClient * client);
  int (*onmessage) (WSPipeOut * pipeout, WSClient * client);
  int (*onopen) (WSPipeOut * pipeout, WSClient * client);

  /* FIFO reader */
  WSPipeIn *pipein;
  /* FIFO writer */
  WSPipeOut *pipeout;
  /* Connected Clients */
  GSLList *colist;
} WSServer;

int ws_send_data (WSClient * client, WSOpcode opcode, const char *p, int sz);
int ws_write_fifo (WSPipeOut * pipeout, char *buffer, int len);
size_t pack_uint32 (void *buf, uint32_t val);
size_t unpack_uint32 (const void *buf, uint32_t * val);
void ws_set_config_accesslog (const char *accesslog);
void ws_set_config_echomode (int echomode);
void ws_set_config_frame_size (int max_frm_size);
void ws_set_config_host (const char *host);
void ws_set_config_origin (const char *origin);
void ws_set_config_pipein (const char *pipein);
void ws_set_config_pipeout (const char *pipeout);
void ws_set_config_port (const char *port);
void ws_set_config_strict (int strict);
void ws_start (WSServer * server);
void ws_stop (WSServer * server);
WSServer *ws_init (const char *host, const char *port);

#endif // for #ifndef WEBSOCKET_H
