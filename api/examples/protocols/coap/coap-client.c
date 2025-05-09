/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* coap-client -- simple CoAP client
 *
 * Copyright (C) 2010--2023 Olaf Bergmann <bergmann@tzi.org> and others
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms of
 * use.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cmsis_os.h>

#include <scm_log.h>
#include <cli.h>

#include <coap3/coap.h>

#define MAX_USER 128 /* Maximum length of a user name (i.e., PSK
                      * identity) in bytes. */
#define MAX_KEY   64 /* Maximum length of a key (i.e., PSK) in bytes. */
#define LIBCOAP_PACKAGE_VERSION "4.3.4"
#define PATH_MAX  64

const static char *TAG = "CoAP_client";

typedef struct {
  coap_binary_t *token;
  int observe;
} track_token;

#define FLAGS_BLOCK 0x01

#define REPEAT_DELAY_MS 1000

typedef struct ih_def_t {
  char *hint_match;
  coap_bin_const_t *new_identity;
  coap_bin_const_t *new_key;
} ih_def_t;

typedef struct valid_ihs_t {
  size_t count;
  ih_def_t *ih_list;
} valid_ihs_t;

typedef unsigned char method_t;

#define DEFAULT_WAIT_TIME 60

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

uint8_t ip_got_flag = 0;

struct coapclient_test
{
  int flags;

  unsigned char _token_data[24]; /* With support for RFC8974 */
  coap_binary_t the_token;

  track_token *tracked_tokens;
  size_t tracked_tokens_count;

  coap_optlist_t *optlist;
  /* Request URI. */
  coap_uri_t uri;
  coap_uri_t proxy;
  valid_ihs_t valid_ihs;

  int proxy_scheme_option;
  int uri_host_option;
  unsigned int ping_seconds;

  size_t repeat_count;

  /* reading is done when this flag is set */
  int ready;

  /* processing a block response when this flag is set */
  int doing_getting_block;
  int single_block_requested;
  uint32_t block_mode;

  coap_string_t output_file;  /* output file name */
  FILE *file;  /* output file stream */

  coap_string_t payload;  /* optional payload to send */

  int reliable;

  int add_nl;
  int is_mcast;
  uint32_t csm_max_message_size;
  unsigned char msgtype;  /* usually, requests are sent confirmable */

  char cert_filestore[PATH_MAX+1];
  char *cert_file;  /* certificate and optional private key in PEM, or PKCS11 URI*/
  char key_file[PATH_MAX+1];  /* private key in PEM, DER or PKCS11 URI */
  char pkcs11_pin[PATH_MAX+1];  /* PKCS11 pin to unlock access to token */
  char ca_filestore[PATH_MAX+1];
  char *ca_file;  /* CA for cert_file - for cert checking in PEM, DER or PKCS11 URI */
  char root_ca_filestore[PATH_MAX+1];
  char *root_ca_file;  /* List of trusted Root CAs in PEM */
  int is_rpk_not_cert;  /* Cert is RPK if set */
  uint8_t *cert_mem;  /* certificate and private key in PEM_BUF */
  uint8_t *key_mem;  /* private key in PEM_BUF */
  uint8_t *ca_mem;  /* CA for cert checking in PEM_BUF */
  size_t cert_mem_len;
  size_t key_mem_len;
  size_t ca_mem_len;
  int verify_peer_cert;  /* PKI granularity - by default set */

  unsigned char key[MAX_KEY];
  ssize_t key_length;
  unsigned char user[MAX_USER];
  ssize_t user_length;

  method_t method;  /* the method we are using in our requests */

  coap_block_t block;
  uint16_t last_block1_mid;

  unsigned int wait_seconds;  /* default timeout in seconds */
  unsigned int wait_ms;
  int obs_started;
  unsigned int obs_seconds;  /* default observe time */
  unsigned int obs_ms;  /* timeout for current subscription */
  int obs_ms_reset;
  int doing_observe;

  coap_oscore_conf_t *oscore_conf;
  int doing_oscore;

  int quit;
  char oscore_conf_file[PATH_MAX+1];

  char port_str[NI_MAXSERV];
  char node_str[NI_MAXHOST];

  int create_uri_opts;
  coap_log_t log_level;
  coap_log_t dtls_log_level;
};


static int
append_to_output(const uint8_t *data, size_t len, struct coapclient_test *test) {
  size_t written;

  if (!test->file) {
    if (!test->output_file.s || (test->output_file.length && test->output_file.s[0] == '-')) {
      test->file = stdout;
    } else {
      if (!(test->file = fopen((char *)test->output_file.s, "w"))) {
        perror("fopen");
        return -1;
      }
    }
  }

  do {
    written = fwrite(data, 1, len, test->file);
    len -= written;
    data += written;
  } while (written && len);

  return 0;
}

static void
close_output(struct coapclient_test *test) {
  if (test->file != stdout) {

    /* add a newline before closing if no option '-o' was specified */
    if (!test->output_file.s)
      (void)fwrite("\n", 1, 1, test->file);

    fclose(test->file);
  }
}

static void
free_xmit_data(coap_session_t *session COAP_UNUSED, void *app_ptr) {
  coap_free(app_ptr);
  return;
}

static void
track_new_token(size_t tokenlen, uint8_t *token, struct coapclient_test *test) {
  track_token *new_list = realloc(test->tracked_tokens,
                                  (test->tracked_tokens_count + 1) * sizeof(test->tracked_tokens[0]));
  if (!new_list) {
    coap_log_info("Unable to track new token\n");
    return;
  }
  test->tracked_tokens = new_list;
  test->tracked_tokens[test->tracked_tokens_count].token = coap_new_binary(tokenlen);
  if (!test->tracked_tokens[test->tracked_tokens_count].token)
    return;
  memcpy(test->tracked_tokens[test->tracked_tokens_count].token->s, token, tokenlen);
  test->tracked_tokens[test->tracked_tokens_count].observe = test->doing_observe;
  test->tracked_tokens_count++;
}

static int
track_check_token(coap_bin_const_t *token, struct coapclient_test *test) {
  size_t i;

  for (i = 0; i < test->tracked_tokens_count; i++) {
    if (coap_binary_equal(token, test->tracked_tokens[i].token)) {
      return 1;
    }
  }
  return 0;
}

static void
track_flush_token(coap_bin_const_t *token, int force, struct coapclient_test *test) {
  size_t i;

  for (i = 0; i < test->tracked_tokens_count; i++) {
    if (coap_binary_equal(token, test->tracked_tokens[i].token)) {
      if (force || !test->tracked_tokens[i].observe || !test->obs_started) {
        /* Only remove if not Observing */
        coap_delete_binary(test->tracked_tokens[i].token);
        if (test->tracked_tokens_count-i > 1) {
          memmove(&test->tracked_tokens[i],
                  &test->tracked_tokens[i+1],
                  (test->tracked_tokens_count-i-1) * sizeof(test->tracked_tokens[0]));
        }
        test->tracked_tokens_count--;
      }
      break;
    }
  }
}


static coap_pdu_t *
coap_new_request(coap_context_t *ctx,
                 coap_session_t *session,
                 method_t m,
                 coap_optlist_t **options,
                 unsigned char *data,
                 size_t length,
                 struct coapclient_test *test) {
  coap_pdu_t *pdu;
  uint8_t token[8];
  size_t tokenlen;
  (void)ctx;

  if (!(pdu = coap_new_pdu(test->msgtype, m, session))) {
    free_xmit_data(session, data);
    return NULL;
  }

  /*
   * Create unique token for this request for handling unsolicited /
   * delayed responses.
   * Note that only up to 8 bytes are returned
   */
  if (test->the_token.length > COAP_TOKEN_DEFAULT_MAX) {
    coap_session_new_token(session, &tokenlen, token);
    /* Update the last part 8 bytes of the large token */
    memcpy(&test->the_token.s[test->the_token.length - tokenlen], token, tokenlen);
  } else {
    coap_session_new_token(session, &test->the_token.length, test->the_token.s);
  }
  track_new_token(test->the_token.length, test->the_token.s, test);
  if (!coap_add_token(pdu, test->the_token.length, test->the_token.s)) {
    coap_log_debug("cannot add token to request\n");
  }

  if (options)
    coap_add_optlist_pdu(pdu, options);

  if (length) {
    /* Let the underlying libcoap decide how this data should be sent */
    coap_add_data_large_request(session, pdu, length, data,
                                free_xmit_data, data);
  }

  return pdu;
}

static int
event_handler(coap_session_t *session,
              const coap_event_t event) {
  struct coapclient_test *test = coap_session_get_app_data(session);

  switch (event) {
  case COAP_EVENT_DTLS_CLOSED:
  case COAP_EVENT_TCP_CLOSED:
  case COAP_EVENT_SESSION_CLOSED:
  case COAP_EVENT_OSCORE_DECRYPTION_FAILURE:
  case COAP_EVENT_OSCORE_NOT_ENABLED:
  case COAP_EVENT_OSCORE_NO_PROTECTED_PAYLOAD:
  case COAP_EVENT_OSCORE_NO_SECURITY:
  case COAP_EVENT_OSCORE_INTERNAL_ERROR:
  case COAP_EVENT_OSCORE_DECODE_ERROR:
  case COAP_EVENT_WS_PACKET_SIZE:
  case COAP_EVENT_WS_CLOSED:
    test->quit = 1;
    break;
  case COAP_EVENT_DTLS_CONNECTED:
  case COAP_EVENT_DTLS_RENEGOTIATE:
  case COAP_EVENT_DTLS_ERROR:
  case COAP_EVENT_TCP_CONNECTED:
  case COAP_EVENT_TCP_FAILED:
  case COAP_EVENT_SESSION_CONNECTED:
  case COAP_EVENT_SESSION_FAILED:
  case COAP_EVENT_PARTIAL_BLOCK:
  case COAP_EVENT_XMIT_BLOCK_FAIL:
  case COAP_EVENT_SERVER_SESSION_NEW:
  case COAP_EVENT_SERVER_SESSION_DEL:
  case COAP_EVENT_BAD_PACKET:
  case COAP_EVENT_MSG_RETRANSMITTED:
  case COAP_EVENT_WS_CONNECTED:
  case COAP_EVENT_KEEPALIVE_FAILURE:
  default:
    break;
  }
  return 0;
}

static void
nack_handler(coap_session_t *session,
             const coap_pdu_t *sent,
             const coap_nack_reason_t reason,
             const coap_mid_t mid COAP_UNUSED) {
  struct coapclient_test *test = coap_session_get_app_data(session);
  if (sent) {
    coap_bin_const_t token = coap_pdu_get_token(sent);

    if (!track_check_token(&token, test)) {
      coap_log_err("nack_handler: Unexpected token\n");
    }
  }

  switch (reason) {
  case COAP_NACK_TOO_MANY_RETRIES:
  case COAP_NACK_NOT_DELIVERABLE:
  case COAP_NACK_RST:
  case COAP_NACK_TLS_FAILED:
  case COAP_NACK_WS_FAILED:
  case COAP_NACK_TLS_LAYER_FAILED:
  case COAP_NACK_WS_LAYER_FAILED:
    coap_log_err("cannot send CoAP pdu\n");
    test->quit = 1;
    break;
  case COAP_NACK_ICMP_ISSUE:
  case COAP_NACK_BAD_RESPONSE:
  default:
    ;
  }
  return;
}

/*
 * Response handler used for coap_send() responses
 */
static coap_response_t
message_handler(coap_session_t *session,
                const coap_pdu_t *sent,
                const coap_pdu_t *received,
                const coap_mid_t id COAP_UNUSED) {

  coap_opt_t *block_opt;
  coap_opt_iterator_t opt_iter;
  size_t len;
  const uint8_t *databuf;
  size_t offset;
  size_t total;
  coap_pdu_code_t rcv_code = coap_pdu_get_code(received);
  coap_pdu_type_t rcv_type = coap_pdu_get_type(received);
  coap_bin_const_t token = coap_pdu_get_token(received);
  struct coapclient_test *test = coap_session_get_app_data(session);

  coap_log_debug("** process incoming %d.%02d response:\n",
                 COAP_RESPONSE_CLASS(rcv_code), rcv_code & 0x1F);
  if (coap_get_log_level() < COAP_LOG_DEBUG)
    coap_show_pdu(COAP_LOG_INFO, received);

  /* check if this is a response to our original request */
  if (!track_check_token(&token, test)) {
    /* drop if this was just some message, or send RST in case of notification */
    if (!sent && (rcv_type == COAP_MESSAGE_CON ||
                  rcv_type == COAP_MESSAGE_NON)) {
      /* Cause a CoAP RST to be sent */
      return COAP_RESPONSE_FAIL;
    }
    return COAP_RESPONSE_OK;
  }

  if (rcv_type == COAP_MESSAGE_RST) {
    coap_log_info("got RST\n");
    return COAP_RESPONSE_OK;
  }

  /* output the received data, if any */
  if (COAP_RESPONSE_CLASS(rcv_code) == 2) {

    /* set obs timer if we have successfully subscribed a resource */
    if (test->doing_observe && !test->obs_started &&
        coap_check_option(received, COAP_OPTION_OBSERVE, &opt_iter)) {
      coap_log_debug("observation relationship established, set timeout to %d\n",
                     test->obs_seconds);
      test->obs_started = 1;
      test->obs_ms = test->obs_seconds * 1000;
      test->obs_ms_reset = 1;
    }

    if (coap_get_data_large(received, &len, &databuf, &offset, &total)) {
      append_to_output(databuf, len, test);
      if ((len + offset == total) && test->add_nl)
        append_to_output((const uint8_t *)"\n", 1, test);
    }

    /* Check if Block2 option is set */
    block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
    if (!test->single_block_requested && block_opt) { /* handle Block2 */

      /* TODO: check if we are looking at the correct block number */
      if (coap_opt_block_num(block_opt) == 0) {
        /* See if observe is set in first response */
        test->ready = test->doing_observe ? coap_check_option(received,
                                                  COAP_OPTION_OBSERVE, &opt_iter) == NULL : 1;
      }
      if (COAP_OPT_BLOCK_MORE(block_opt)) {
        test->doing_getting_block = 1;
      } else {
        test->doing_getting_block = 0;
        if (!test->is_mcast)
          track_flush_token(&token, 0, test);
      }
      return COAP_RESPONSE_OK;
    }
  } else {      /* no 2.05 */
    /* check if an error was signaled and output payload if so */
    if (COAP_RESPONSE_CLASS(rcv_code) >= 4) {
      fprintf(stderr, "%d.%02d", COAP_RESPONSE_CLASS(rcv_code),
              rcv_code & 0x1F);
      if (coap_get_data_large(received, &len, &databuf, &offset, &total)) {
        fprintf(stderr, " ");
        while (len--) {
          fprintf(stderr, "%c", isprint(*databuf) ? *databuf : '.');
          databuf++;
        }
      }
      fprintf(stderr, "\n");
      track_flush_token(&token, 1, test);
    }

  }
  if (!test->is_mcast)
    track_flush_token(&token, 0, test);

  /* our job is done, we can exit at any time */
  test->ready = test->doing_observe ? coap_check_option(received,
                                            COAP_OPTION_OBSERVE, &opt_iter) == NULL : 1;
  return COAP_RESPONSE_OK;
}

static void
usage(const char *program, const char *version) {
  const char *p;
  char buffer[120];
  const char *lib_build = coap_package_build();

  p = strrchr(program, '/');
  if (p)
    program = ++p;

  fprintf(stderr, "%s v%s -- a small CoAP implementation\n"
          "Copyright (C) 2010-2023 Olaf Bergmann <bergmann@tzi.org> and others\n\n"
          "Build: %s\n"
          "%s\n"
          , program, version, lib_build,
          coap_string_tls_version(buffer, sizeof(buffer)));
  fprintf(stderr, "%s\n", coap_string_tls_support(buffer, sizeof(buffer)));
  fprintf(stderr, "\n"
          "Usage: %s [-a addr] [-b [num,]size] [-e text] [-l loss]\n"
          "\t\t[-m method] [-o file] [-p port] [-r] [-s duration] [-t type]\n"
          "\t\t[-v num] [-w] [-A type] [-B seconds]\n"
          "\t\t[-E oscore_conf_file] [-G count] [-H hoplimit]\n"
          "\t\t[-K interval] [-N] [-O num,text] [-P scheme://address[:port]\n"
          "\t\t[-T token] [-U]  [-V num] [-X size]\n"
          "\t\t[[-h match_hint_file] [-k key] [-u user]]\n"
          "\t\t[[-c certfile] [-j keyfile] [-n] [-C cafile]\n"
          "\t\t[-J pkcs11_pin] [-R trust_casfile]\n"
          "\t\t[-S match_pki_sni_file]] URI\n"
          "\tURI can be an absolute URI or a URI prefixed with scheme and host\n\n"
          "General Options\n"
          "\t-a addr\t\tThe local interface address to use\n"
          "\t-b [num,]size\tBlock size to be used in GET/PUT/POST requests\n"
          "\t       \t\t(value must be a multiple of 16 not larger than 1024)\n"
          "\t       \t\tIf num is present, the request chain will start at\n"
          "\t       \t\tblock num\n"
          "\t-e text\t\tInclude text as payload (use percent-encoding for\n"
          "\t       \t\tnon-ASCII characters)\n"
          "\t-f file\t\tFile to send with PUT/POST (use '-' for STDIN)\n"
          "\t-l list\t\tFail to send some datagrams specified by a comma\n"
          "\t       \t\tseparated list of numbers or number ranges\n"
          "\t       \t\t(for debugging only)\n"
          "\t-l loss%%\tRandomly fail to send datagrams with the specified\n"
          "\t       \t\tprobability - 100%% all datagrams, 0%% no datagrams\n"
          "\t-m method\tRequest method (get|put|post|delete|fetch|patch|ipatch),\n"
          "\t       \t\tdefault is 'get'\n"
          "\t-o file\t\tOutput received data to this file (use '-' for STDOUT)\n"
          "\t-p port\t\tSend from the specified port\n"
          "\t-r     \t\tUse reliable protocol (TCP or TLS); requires TCP support\n"
          "\t-s duration\tSubscribe to / Observe resource for given duration\n"
          "\t       \t\tin seconds\n"
          "\t-t type\t\tContent format for given resource for PUT/POST\n"
          "\t-v num \t\tVerbosity level (default 4, maximum is 8) for general\n"
          "\t       \t\tCoAP logging\n"
          "\t-w     \t\tAppend a newline to received data\n"
          "\t-A type\t\tAccepted media type\n"
          "\t-B seconds\tBreak operation after waiting given seconds\n"
          "\t       \t\t(default is %d)\n"
          "\t-E oscore_conf_file\n"
          "\t       \t\toscore_conf_file contains OSCORE configuration. See\n"
          "\t       \t\tcoap-oscore-conf(5) for definitions.\n"
          "\t-G count\tRepeat the Request 'count' times with a second delay\n"
          "\t       \t\tbetween each one. Must have a value between 1 and 255\n"
          "\t       \t\tinclusive. Default is '1'\n"
          "\t-H hoplimit\tSet the Hop Limit count to hoplimit for proxies. Must\n"
          "\t       \t\thave a value between 1 and 255 inclusive.\n"
          "\t       \t\tDefault is '16'\n"
          "\t-K interval\tSend a ping after interval seconds of inactivity\n"
          "\t-L value\tSum of one or more COAP_BLOCK_* flag values for block\n"
          "\t       \t\thandling methods. Default is 1 (COAP_BLOCK_USE_LIBCOAP)\n"
          "\t       \t\t(Sum of one or more of 1,2 and 16)\n"
          "\t-N     \t\tSend NON-confirmable message\n"
          "\t-O num,text\tAdd option num with contents text to request. If the\n"
          "\t       \t\ttext begins with 0x, then the hex text (two [0-9a-f] per\n"
          "\t       \t\tbyte) is converted to binary data\n"
          "\t-P scheme://address[:port]\n"
          "\t       \t\tScheme, address and optional port to define how to\n"
          "\t       \t\tconnect to a CoAP proxy (automatically adds Proxy-Uri\n"
          "\t       \t\toption to request) to forward the request to.\n"
          "\t       \t\tScheme is one of coap, coaps, coap+tcp and coaps+tcp\n"
          "\t-T token\tDefine the initial starting token (up to 24 characters)\n"
          "\t-U     \t\tNever include Uri-Host or Uri-Port options\n"
          "\t-V num \t\tVerbosity level (default 3, maximum is 7) for (D)TLS\n"
          "\t       \t\tlibrary logging\n"
          "\t-X size\t\tMaximum message size to use for TCP based connections\n"
          "\t       \t\t(default is 8388864). Maximum value of 2^32 -1\n"
          ,program, DEFAULT_WAIT_TIME);
  fprintf(stderr,
          "PSK Options (if supported by underlying (D)TLS library)\n"
          "\t-h match_hint_file\n"
          "\t       \t\tThis is a file that contains one or more lines of\n"
          "\t       \t\treceived Identity Hints to match to use different\n"
          "\t       \t\tuser identity and associated pre-shared key (PSK) (comma\n"
          "\t       \t\tseparated) instead of the '-k key' and '-u user'\n"
          "\t       \t\toptions. E.g., per line\n"
          "\t       \t\t hint_to_match,use_user,with_key\n"
          "\t       \t\tNote: -k and -u still need to be defined for the default\n"
          "\t       \t\tin case there is no match\n"
          "\t-k key \t\tPre-shared key for the specified user identity\n"
          "\t-u user\t\tUser identity to send for pre-shared key mode\n"
          "PKI Options (if supported by underlying (D)TLS library)\n"
          "\tNote: If any one of '-c certfile', '-j keyfile' or '-C cafile' is in\n"
          "\tPKCS11 URI naming format (pkcs11: prefix), then any remaining non\n"
          "\tPKCS11 URI file definitions have to be in DER, not PEM, format.\n"
          "\tOtherwise all of '-c certfile', '-j keyfile' or '-C cafile' are in\n"
          "\tPEM format.\n\n"
          "\t-c certfile\tPEM file or PKCS11 URI for the certificate. The private\n"
          "\t       \t\tkey can also be in the PEM file, or has the same PKCS11\n"
          "\t       \t\tURI. If not, the private key is defined by '-j keyfile'\n"
          "\t-j keyfile\tPEM file or PKCS11 URI for the private key for the\n"
          "\t       \t\tcertificate in '-c certfile' if the parameter is\n"
          "\t       \t\tdifferent from certfile in '-c certfile'\n"
          "\t-n     \t\tDisable remote peer certificate checking\n"
          "\t-C cafile\tPEM file or PKCS11 URI for the CA certificate that was\n"
          "\t       \t\tused to sign the server certfile. Ideally the client\n"
          "\t       \t\tcertificate should be signed by the same CA so that\n"
          "\t       \t\tmutual authentication can take place. The contents of\n"
          "\t       \t\tcafile are added to the trusted store of root CAs.\n"
          "\t       \t\tUsing the -C or -R options will trigger the\n"
          "\t       \t\tvalidation of the server certificate unless overridden\n"
          "\t       \t\tby the -n option\n"
          "\t-J pkcs11_pin\tThe user pin to unlock access to the PKCS11 token\n"
          "\t-R trust_casfile\n"
          "\t       \t\tPEM file containing the set of trusted root CAs\n"
          "\t       \t\tthat are to be used to validate the server certificate.\n"
          "\t       \t\tAlternatively, this can point to a directory containing\n"
          "\t       \t\ta set of CA PEM files.\n"
          "\t       \t\tUsing '-R trust_casfile' disables common CA mutual\n"
          "\t       \t\tauthentication which can only be done by using\n"
          "\t       \t\t'-C cafile'.\n"
          "\t       \t\tUsing the -C or -R options will will trigger the\n"
          "\t       \t\tvalidation of the server certificate unless overridden\n"
          "\t       \t\tby the -n option\n"
         );
  fprintf(stderr,
          "Examples:\n"
          "\tcoap-client -m get coap://[::1]/\n"
          "\tcoap-client -m get coap://[::1]/.well-known/core\n"
          "\tcoap-client -m get coap+tcp://[::1]/.well-known/core\n"
          "\tcoap-client -m get coap://%%2Funix%%2Fdomain%%2Fpath%%2Fdgram/.well-known/core\n"
          "\tcoap-client -m get coap+tcp://%%2Funix%%2Fdomain%%2Fpath%%2Fstream/.well-known/core\n"
          "\tcoap-client -m get coaps://[::1]/.well-known/core\n"
          "\tcoap-client -m get coaps+tcp://[::1]/.well-known/core\n"
          "\tcoap-client -m get coaps://%%2Funix%%2Fdomain%%2Fpath%%2Fdtls/.well-known/core\n"
          "\tcoap-client -m get coaps+tcp://%%2Funix%%2Fdomain%%2Fpath%%2Ftls/.well-known/core\n"
          "\tcoap-client -m get -T cafe coap://[::1]/time\n"
          "\techo -n 1000 | coap-client -m put -T cafe coap://[::1]/time -f -\n"
         );
}

typedef struct {
  unsigned char code;
  const char *media_type;
} content_type_t;

static void
cmdline_content_type(struct coapclient_test *test, char *arg, uint16_t key) {
  static content_type_t content_types[] = {
    {  0, "plain" },
    {  0, "text/plain" },
    { 40, "link" },
    { 40, "link-format" },
    { 40, "application/link-format" },
    { 41, "xml" },
    { 41, "application/xml" },
    { 42, "binary" },
    { 42, "octet-stream" },
    { 42, "application/octet-stream" },
    { 47, "exi" },
    { 47, "application/exi" },
    { 50, "json" },
    { 50, "application/json" },
    { 60, "cbor" },
    { 60, "application/cbor" },
    { 255, NULL }
  };
  coap_optlist_t *node;
  unsigned char i;
  uint16_t value;
  uint8_t buf[4];

  if (isdigit((int)arg[0])) {
    value = atoi(arg);
  } else {
    for (i=0;
         content_types[i].media_type &&
         strncmp(arg, content_types[i].media_type, strlen(arg)) != 0 ;
         ++i)
      ;

    if (content_types[i].media_type) {
      value = content_types[i].code;
    } else {
      coap_log_warn("W: unknown content-format '%s'\n",arg);
      return;
    }
  }

  node = coap_new_optlist(key, coap_encode_var_safe(buf, sizeof(buf), value), buf);
  if (node) {
    coap_insert_optlist(&test->optlist, node);
  }
}

static int
cmdline_hop_limit(struct coapclient_test *test, char *arg) {
  coap_optlist_t *node;
  uint32_t value;
  uint8_t buf[4];

  value = strtol(arg, NULL, 10);
  if (value < 1 || value > 255) {
    return 0;
  }
  node = coap_new_optlist(COAP_OPTION_HOP_LIMIT, coap_encode_var_safe(buf, sizeof(buf), value), buf);
  if (node) {
    coap_insert_optlist(&test->optlist, node);
  }
  return 1;
}

#ifndef USE_LOCALDEBUG
static uint8_t *
read_file_mem(const char *filename, size_t *length) {
  FILE *f;
  uint8_t *buf;
  struct stat statbuf;

  *length = 0;
  if (!filename || !(f = fopen(filename, "r")))
    return NULL;

  if (stat(filename, &statbuf) == -1) {
    fclose(f);
    return NULL;
  }

  buf = coap_malloc(statbuf.st_size+1);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  if (fread(buf, 1, statbuf.st_size, f) != (size_t)statbuf.st_size) {
    fclose(f);
    coap_free(buf);
    return NULL;
  }
  buf[statbuf.st_size] = '\000';
  *length = (size_t)(statbuf.st_size + 1);
  fclose(f);
  return buf;
}
#endif

static coap_oscore_conf_t *
get_oscore_conf(struct coapclient_test *test) {
  coap_str_const_t file_mem;

#ifdef USE_LOCALDEBUG
  extern const unsigned char coap_client_oscore_conf[];
  extern const unsigned int coap_client_oscore_conf_size;

  file_mem.s = coap_client_oscore_conf;
  file_mem.length = coap_client_oscore_conf_size;
  test->oscore_conf = coap_new_oscore_conf(file_mem,
                                     NULL, NULL, 0);
  if (test->oscore_conf == NULL) {
    fprintf(stderr, "OSCORE configuration error\n");
    return NULL;
  }
#else
  uint8_t *buf;
  size_t length;

  /* Need a rw var to free off later and file_mem.s is a const */
  buf = read_file_mem(test->oscore_conf_file, &length);
  if (buf == NULL) {
    fprintf(stderr, "OSCORE configuration file error: %s\n", test->oscore_conf_file);
    return NULL;
  }
  file_mem.s = buf;
  file_mem.length = length;
  test->oscore_conf = coap_new_oscore_conf(file_mem,
                                     NULL,
                                     NULL, 0);
  coap_free(buf);
  if (test->oscore_conf == NULL) {
    fprintf(stderr, "OSCORE configuration file error: %s\n", test->oscore_conf_file);
    return NULL;
  }
#endif
  return test->oscore_conf;
}

static int
cmdline_oscore(struct coapclient_test *test, char *arg) {
  if (coap_oscore_is_supported()) {
    strncpy(test->oscore_conf_file, arg, PATH_MAX);
    return 1;
  }
  fprintf(stderr, "OSCORE support not enabled\n");
  return 0;
}

/**
 * Sets global URI options according to the URI passed as @p arg.
 * This function returns 0 on success or -1 on error.
 *
 * @param arg             The URI string.
 * @param create_uri_opts Flags that indicate whether Uri-Host and
 *                        Uri-Port should be suppressed.
 * @return 0 on success, -1 otherwise
 */
static int
cmdline_uri(struct coapclient_test *test, char *arg) {

  if (!test->proxy_scheme_option && test->proxy.host.length) {
    /* create Proxy-Uri from argument */
    size_t len = strlen(arg);
    if (len > 1034) {
      coap_log_err("Absolute URI length must be <= 1034 bytes for a proxy\n");
      return -1;
    }

    coap_insert_optlist(&test->optlist,
                        coap_new_optlist(COAP_OPTION_PROXY_URI,
                                         len,
                                         (unsigned char *)arg));

  } else {      /* split arg into Uri-* options */
    if (coap_split_uri((unsigned char *)arg, strlen(arg), &test->uri) < 0) {
      coap_log_err("invalid CoAP URI\n");
      return -1;
    }

    /* Need to special case use of reliable */
    if (test->uri.scheme == COAP_URI_SCHEME_COAPS && test->reliable) {
      if (!coap_tls_is_supported()) {
        coap_log_emerg("coaps+tcp URI scheme not supported in this version of libcoap\n");
        return -1;
      } else {
        test->uri.scheme = COAP_URI_SCHEME_COAPS_TCP;
      }
    }

    if (test->uri.scheme == COAP_URI_SCHEME_COAP && test->reliable) {
      if (!coap_tcp_is_supported()) {
        coap_log_emerg("coap+tcp URI scheme not supported in this version of libcoap\n");
        return -1;
      } else {
        test->uri.scheme = COAP_URI_SCHEME_COAP_TCP;
      }
    }
  }
  return 0;
}

static int
cmdline_blocksize(struct coapclient_test *test,char *arg) {
  uint16_t size;

again:
  size = 0;
  while (*arg && *arg != ',')
    size = size * 10 + (*arg++ - '0');

  if (*arg == ',') {
    arg++;
    test->block.num = size;
    if (size != 0) {
      /* Random access selection - only handle single response */
      test->single_block_requested = 1;
    }
    goto again;
  }

  if (size < 16) {
    coap_log_warn("Minimum block size is 16\n");
    return 0;
  } else if (size > 1024) {
    coap_log_warn("Maximum block size is 1024\n");
    return 0;
  } else if ((size % 16) != 0) {
    coap_log_warn("Block size %u is not a multiple of 16\n", size);
    return 0;
  }
  if (size)
    test->block.szx = (coap_fls(size >> 4) - 1) & 0x07;

  test->flags |= FLAGS_BLOCK;
  return 1;
}

/* Called after processing the options from the commandline to set
 * Block1, Block2, Q-Block1 or Q-Block2 depending on method. */
static void
set_blocksize(struct coapclient_test *test) {
  static unsigned char buf[4];        /* hack: temporarily take encoded bytes */
  uint16_t opt;
  unsigned int opt_length;

  if (test->method != COAP_REQUEST_DELETE) {
    if (test->method == COAP_REQUEST_GET || test->method == COAP_REQUEST_FETCH) {
      if (coap_q_block_is_supported() && test->block_mode & COAP_BLOCK_TRY_Q_BLOCK)
        opt = COAP_OPTION_Q_BLOCK2;
      else
        opt = COAP_OPTION_BLOCK2;
    } else {
      if (coap_q_block_is_supported() && test->block_mode & COAP_BLOCK_TRY_Q_BLOCK)
        opt = COAP_OPTION_Q_BLOCK1;
      else
        opt = COAP_OPTION_BLOCK1;
    }

    test->block.m = (opt == COAP_OPTION_BLOCK1 || opt == COAP_OPTION_Q_BLOCK1) &&
              ((1ull << (test->block.szx + 4)) < test->payload.length);

    opt_length = coap_encode_var_safe(buf, sizeof(buf),
                                      (test->block.num << 4 | test->block.m << 3 | test->block.szx));

    coap_insert_optlist(&test->optlist, coap_new_optlist(opt, opt_length, buf));
  }
}

static void
cmdline_subscribe(struct coapclient_test *test, char *arg) {
  uint8_t buf[4];

  test->obs_seconds = atoi(arg);
  coap_insert_optlist(&test->optlist,
                      coap_new_optlist(COAP_OPTION_OBSERVE,
                                       coap_encode_var_safe(buf, sizeof(buf),
                                                            COAP_OBSERVE_ESTABLISH), buf)
                     );
  test->doing_observe = 1;
}

static int
cmdline_proxy(struct coapclient_test *test, char *arg) {
  if (coap_split_uri((unsigned char *)arg, strlen(arg), &test->proxy) < 0 ||
      test->proxy.path.length != 0 || test->proxy.query.length != 0) {
    coap_log_err("invalid CoAP Proxy definition\n");
    return 0;
  }
  return 1;
}

static inline void
cmdline_token(struct coapclient_test *test, char *arg) {
  test->the_token.length = min(sizeof(test->_token_data), strlen(arg));
  if (test->the_token.length > 0) {
    memcpy((char *)test->the_token.s, arg, test->the_token.length);
  }
}

/**
 * Utility function to convert a hex digit to its corresponding
 * numerical value.
 *
 * param c  The hex digit to convert. Must be in [0-9A-Fa-f].
 *
 * return The numerical representation of @p c.
 */
static uint8_t
hex2char(char c) {
  assert(isxdigit(c));
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  else if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  else
    return c - '0';
}

/**
 * Converts the sequence of hex digits in src to a sequence of bytes.
 *
 * This function returns the number of bytes that have been written to
 * @p dst.
 *
 * param[in]  src  The null-terminated hex string to convert.
 * param[out] dst  Conversion result.
 *
 * return The length of @p dst.
 */
static size_t
convert_hex_string(const char *src, uint8_t *dst) {
  uint8_t *p = dst;
  while (isxdigit((int)src[0]) && isxdigit((int)src[1])) {
    *p++ = (hex2char(src[0]) << 4) + hex2char(src[1]);
    src += 2;
  }
  if (src[0] != '\0') { /* error in hex input */
    coap_log_warn("invalid hex string in option '%s'\n", src);
  }
  return p - dst;
}

static void
cmdline_option(struct coapclient_test *test, char *arg) {
  unsigned int num = 0;

  while (*arg && *arg != ',') {
    num = num * 10 + (*arg - '0');
    ++arg;
  }
  if (*arg == ',')
    ++arg;

  /* read hex string when arg starts with "0x" */
  if (arg[0] == '0' && arg[1] == 'x') {
    /* As the command line option is part of our environment we can do
     * the conversion in place. */
    size_t len = convert_hex_string(arg + 2, (uint8_t *)arg);

    /* On success, 2 * len + 2 == strlen(arg) */
    coap_insert_optlist(&test->optlist,
                        coap_new_optlist(num, len, (unsigned char *)arg));
  } else { /* null-terminated character string */
    coap_insert_optlist(&test->optlist,
                        coap_new_optlist(num, strlen(arg), (unsigned char *)arg));
  }
  if (num == COAP_OPTION_PROXY_SCHEME) {
    test->proxy_scheme_option = 1;
    if (strcasecmp(arg, "coaps+tcp") == 0) {
      test->proxy.scheme = COAP_URI_SCHEME_COAPS_TCP;
      test->proxy.port = COAPS_DEFAULT_PORT;
    } else if (strcasecmp(arg, "coap+tcp") == 0) {
      test->proxy.scheme = COAP_URI_SCHEME_COAP_TCP;
      test->proxy.port = COAP_DEFAULT_PORT;
    } else if (strcasecmp(arg, "coaps") == 0) {
      test->proxy.scheme = COAP_URI_SCHEME_COAPS;
      test->proxy.port = COAPS_DEFAULT_PORT;
    } else if (strcasecmp(arg, "coap") == 0) {
      test->proxy.scheme = COAP_URI_SCHEME_COAP;
      test->proxy.port = COAP_DEFAULT_PORT;
    } else {
      coap_log_warn("%s is not a supported CoAP Proxy-Scheme\n", arg);
    }
  }
  if (num == COAP_OPTION_URI_HOST) {
    test->uri_host_option = 1;
  }
}

/**
 * Calculates decimal value from hexadecimal ASCII character given in
 * @p c. The caller must ensure that @p c actually represents a valid
 * heaxdecimal character, e.g. with isxdigit(3).
 *
 * @hideinitializer
 */
#define hexchar_to_dec(c) ((c) & 0x40 ? ((c) & 0x0F) + 9 : ((c) & 0x0F))

/**
 * Decodes percent-encoded characters while copying the string @p seg
 * of size @p length to @p buf. The caller of this function must
 * ensure that the percent-encodings are correct (i.e. the character
 * '%' is always followed by two hex digits. and that @p buf provides
 * sufficient space to hold the result. This function is supposed to
 * be called by make_decoded_option() only.
 *
 * @param seg     The segment to decode and copy.
 * @param length  Length of @p seg.
 * @param buf     The result buffer.
 */
static void
decode_segment(const uint8_t *seg, size_t length, unsigned char *buf) {

  while (length--) {

    if (*seg == '%') {
      *buf = (hexchar_to_dec(seg[1]) << 4) + hexchar_to_dec(seg[2]);

      seg += 2;
      length -= 2;
    } else {
      *buf = *seg;
    }

    ++buf;
    ++seg;
  }
}

/**
 * Runs through the given path (or query) segment and checks if
 * percent-encodings are correct. This function returns @c -1 on error
 * or the length of @p s when decoded.
 */
static int
check_segment(const uint8_t *s, size_t length) {

  int n = 0;

  while (length) {
    if (*s == '%') {
      if (length < 2 || !(isxdigit(s[1]) && isxdigit(s[2])))
        return -1;

      s += 2;
      length -= 2;
    }

    ++s;
    ++n;
    --length;
  }

  return n;
}

static int
cmdline_input(char *text, coap_string_t *buf) {
  int len;
  len = check_segment((unsigned char *)text, strlen(text));

  if (len < 0)
    return 0;

  buf->s = (unsigned char *)coap_malloc(len);
  if (!buf->s)
    return 0;

  buf->length = len;
  decode_segment((unsigned char *)text, strlen(text), buf->s);
  return 1;
}

static method_t
cmdline_method(char *arg) {
  static const char *methods[] =
  { 0, "get", "post", "put", "delete", "fetch", "patch", "ipatch", 0};
  unsigned char i;

  for (i=1; methods[i] && strcasecmp(arg,methods[i]) != 0 ; ++i)
    ;

  return i;     /* note that we do not prevent illegal methods */
}

static ssize_t
cmdline_read_user(char *arg, unsigned char *buf, size_t maxlen) {
  size_t len = strnlen(arg, maxlen);
  if (len) {
    strncpy((char *)buf, arg, len);
    /* len is the size or less, so 0 terminate to maxlen */
    (buf)[len] = '\000';
  }
  /* 0 length Identity is valid */
  return len;
}

static ssize_t
cmdline_read_key(char *arg, unsigned char *buf, size_t maxlen) {
  size_t len = strnlen(arg, maxlen);
  if (len) {
    strncpy((char *)buf, arg, len);
    return len;
  }
  /* Need at least one byte for the pre-shared key */
  coap_log_crit("Invalid Pre-Shared Key specified\n");
  return -1;
}

static int
cmdline_read_hint_check(struct coapclient_test *test, const char *arg) {
  FILE *fp = fopen(arg, "r");
  static char tmpbuf[256];
  if (fp == NULL) {
    coap_log_err("Hint file: %s: Unable to open\n", arg);
    return 0;
  }
  while (fgets(tmpbuf, sizeof(tmpbuf), fp) != NULL) {
    char *cp = tmpbuf;
    char *tcp = strchr(cp, '\n');

    if (tmpbuf[0] == '#')
      continue;
    if (tcp)
      *tcp = '\000';

    tcp = strchr(cp, ',');
    if (tcp) {
      ih_def_t *new_ih_list;
      new_ih_list = realloc(test->valid_ihs.ih_list,
                            (test->valid_ihs.count + 1)*sizeof(test->valid_ihs.ih_list[0]));
      if (new_ih_list == NULL) {
        break;
      }
      test->valid_ihs.ih_list = new_ih_list;
      test->valid_ihs.ih_list[test->valid_ihs.count].hint_match = strndup(cp, tcp-cp);
      cp = tcp+1;
      tcp = strchr(cp, ',');
      if (tcp) {
        test->valid_ihs.ih_list[test->valid_ihs.count].new_identity =
            coap_new_bin_const((const uint8_t *)cp, tcp-cp);
        cp = tcp+1;
        test->valid_ihs.ih_list[test->valid_ihs.count].new_key =
            coap_new_bin_const((const uint8_t *)cp, strlen(cp));
        test->valid_ihs.count++;
      } else {
        /* Badly formatted */
        free(test->valid_ihs.ih_list[test->valid_ihs.count].hint_match);
      }
    }
  }
  fclose(fp);
  return test->valid_ihs.count > 0;
}

static int
verify_cn_callback(const char *cn,
                   const uint8_t *asn1_public_cert COAP_UNUSED,
                   size_t asn1_length COAP_UNUSED,
                   coap_session_t *session COAP_UNUSED,
                   unsigned depth,
                   int validated COAP_UNUSED,
                   void *arg COAP_UNUSED) {
  coap_log_info("CN '%s' presented by server (%s)\n",
                cn, depth ? "CA" : "Certificate");
  return 1;
}

static const coap_dtls_cpsk_info_t *
verify_ih_callback(coap_str_const_t *hint,
                   coap_session_t *c_session,
                   void *arg) {
  coap_dtls_cpsk_info_t *psk_info = (coap_dtls_cpsk_info_t *)arg;
  char lhint[COAP_DTLS_HINT_LENGTH];
  static coap_dtls_cpsk_info_t psk_identity_info;
  struct coapclient_test *test = coap_session_get_app_data(c_session);
  size_t i;

  snprintf(lhint, sizeof(lhint), "%.*s", (int)hint->length, hint->s);
  coap_log_info("Identity Hint '%s' provided\n", lhint);

  /* Test for hint to possibly change identity + key */
  for (i = 0; i < test->valid_ihs.count; i++) {
    if (strcmp(lhint, test->valid_ihs.ih_list[i].hint_match) == 0) {
      /* Preset */
      psk_identity_info = *psk_info;
      if (test->valid_ihs.ih_list[i].new_key) {
        psk_identity_info.key = *test->valid_ihs.ih_list[i].new_key;
      }
      if (test->valid_ihs.ih_list[i].new_identity) {
        psk_identity_info.identity = *test->valid_ihs.ih_list[i].new_identity;
      }
      coap_log_info("Switching to using '%s' identity + '%s' key\n",
                    psk_identity_info.identity.s, psk_identity_info.key.s);
      return &psk_identity_info;
    }
  }
  /* Just use the defined key for now as passed in by arg */
  return psk_info;
}

static coap_dtls_pki_t *
setup_pki(coap_context_t *ctx, struct coapclient_test *test) {
  static coap_dtls_pki_t dtls_pki;
  static char client_sni[256];

  /* If general root CAs are defined */
  if (test->root_ca_file) {
    struct stat stbuf;
    if ((stat(test->root_ca_file, &stbuf) == 0) && S_ISDIR(stbuf.st_mode)) {
      coap_context_set_pki_root_cas(ctx, NULL, test->root_ca_file);
    } else {
      coap_context_set_pki_root_cas(ctx, test->root_ca_file, NULL);
    }
  }
  
  memset(client_sni, 0, sizeof(client_sni));
  memset(&dtls_pki, 0, sizeof(dtls_pki));
  dtls_pki.version = COAP_DTLS_PKI_SETUP_VERSION;
  if (test->ca_file || test->root_ca_file) {
    /*
     * Add in additional certificate checking.
     * This list of enabled can be tuned for the specific
     * requirements - see 'man coap_encryption'.
     *
     * Note: root_ca_file is setup separately using
     * coap_context_set_pki_root_cas(), but this is used to define what
     * checking actually takes place.
     */
    dtls_pki.verify_peer_cert        = test->verify_peer_cert;
    dtls_pki.check_common_ca         = !test->root_ca_file;
    dtls_pki.allow_self_signed       = 1;
    dtls_pki.allow_expired_certs     = 1;
    dtls_pki.cert_chain_validation   = 1;
    dtls_pki.cert_chain_verify_depth = 2;
    dtls_pki.check_cert_revocation   = 1;
    dtls_pki.allow_no_crl            = 1;
    dtls_pki.allow_expired_crl       = 1;
  } else if (test->is_rpk_not_cert) {
    dtls_pki.verify_peer_cert        = test->verify_peer_cert;
  }
  dtls_pki.is_rpk_not_cert = test->is_rpk_not_cert;
  dtls_pki.validate_cn_call_back = verify_cn_callback;
  if ((test->uri.host.length == 3 && memcmp(test->uri.host.s, "::1", 3) != 0) ||
      (test->uri.host.length == 9 && memcmp(test->uri.host.s, "127.0.0.1", 9) != 0))
    memcpy(client_sni, test->uri.host.s, min(test->uri.host.length, sizeof(client_sni)-1));
  else
    memcpy(client_sni, "localhost", 9);

  dtls_pki.client_sni = client_sni;
  if ((test->key_file && strncasecmp(test->key_file, "pkcs11:", 7) == 0) ||
      (test->cert_file && strncasecmp(test->cert_file, "pkcs11:", 7) == 0) ||
      (test->ca_file && strncasecmp(test->ca_file, "pkcs11:", 7) == 0)) {
    dtls_pki.pki_key.key_type = COAP_PKI_KEY_PKCS11;
    dtls_pki.pki_key.key.pkcs11.public_cert = test->cert_file;
    dtls_pki.pki_key.key.pkcs11.private_key = test->key_file ?
                                              test->key_file : test->cert_file;
    dtls_pki.pki_key.key.pkcs11.ca = test->ca_file;
    dtls_pki.pki_key.key.pkcs11.user_pin = test->pkcs11_pin;
#if 0 /* MBEDTLS_FS_IO isnot defined */
  } else if (!is_rpk_not_cert) {
    dtls_pki.pki_key.key_type = COAP_PKI_KEY_PEM;
    dtls_pki.pki_key.key.pem.public_cert = cert_file;
    dtls_pki.pki_key.key.pem.private_key = key_file ? key_file : cert_file;
    dtls_pki.pki_key.key.pem.ca_file = ca_file;
#endif
  } else {
    /* Map file into memory */
    if (test->ca_mem == 0 && test->cert_mem == 0 && test->key_mem == 0) {
#ifndef USE_LOCALDEBUG
      test->ca_mem = read_file_mem(test->ca_file, &test->ca_mem_len);
      test->cert_mem = read_file_mem(test->cert_file, &test->cert_mem_len);
      test->key_mem = read_file_mem(test->key_file, &test->key_mem_len);
#endif
    }
    dtls_pki.pki_key.key_type = COAP_PKI_KEY_PEM_BUF;
#ifdef USE_LOCALDEBUG
    extern const unsigned char coap_ca_pem[];
    extern const unsigned int coap_ca_pem_size;
    extern const unsigned char coap_client_crt[];
    extern const unsigned int coap_client_crt_size;
    extern const unsigned char coap_client_key[];
    extern const unsigned int coap_client_key_size;

    dtls_pki.pki_key.key.pem_buf.ca_cert = coap_ca_pem;
    dtls_pki.pki_key.key.pem_buf.public_cert = coap_client_crt;
    dtls_pki.pki_key.key.pem_buf.private_key = coap_client_key;
    dtls_pki.pki_key.key.pem_buf.ca_cert_len = coap_ca_pem_size;
    dtls_pki.pki_key.key.pem_buf.public_cert_len = coap_client_crt_size;
    dtls_pki.pki_key.key.pem_buf.private_key_len = coap_client_key_size;
#else
    dtls_pki.pki_key.key.pem_buf.ca_cert = test->ca_mem;
    dtls_pki.pki_key.key.pem_buf.public_cert = test->cert_mem;
    dtls_pki.pki_key.key.pem_buf.private_key = test->key_mem ? test->key_mem : test->cert_mem;
    dtls_pki.pki_key.key.pem_buf.ca_cert_len = test->ca_mem_len;
    dtls_pki.pki_key.key.pem_buf.public_cert_len = test->cert_mem_len;
    dtls_pki.pki_key.key.pem_buf.private_key_len = test->key_mem ?
                                                   test->key_mem_len : test->cert_mem_len;
#endif
  }
  return &dtls_pki;
}

static coap_dtls_cpsk_t *
setup_psk(const uint8_t *identity,
          size_t identity_len,
          const uint8_t *key,
          size_t key_len,
          struct coapclient_test *test) {
  static coap_dtls_cpsk_t dtls_psk;
  static char client_sni[256];

  memset(client_sni, 0, sizeof(client_sni));
  memset(&dtls_psk, 0, sizeof(dtls_psk));
  dtls_psk.version = COAP_DTLS_CPSK_SETUP_VERSION;
  dtls_psk.validate_ih_call_back = verify_ih_callback;
  dtls_psk.ih_call_back_arg = &dtls_psk.psk_info;
  if (test->uri.host.length)
    memcpy(client_sni, test->uri.host.s,
           min(test->uri.host.length, sizeof(client_sni) - 1));
  else
    memcpy(client_sni, "localhost", 9);
  dtls_psk.client_sni = client_sni;
  dtls_psk.psk_info.identity.s = identity;
  dtls_psk.psk_info.identity.length = identity_len;
  dtls_psk.psk_info.key.s = key;
  dtls_psk.psk_info.key.length = key_len;
  return &dtls_psk;
}

static coap_session_t *
open_session(coap_context_t *ctx,
             coap_proto_t proto,
             coap_address_t *bind_addr,
             coap_address_t *dst,
             const uint8_t *identity,
             size_t identity_len,
             const uint8_t *key,
             size_t key_len,
             struct coapclient_test *test) {
  coap_session_t *session;

  if (proto == COAP_PROTO_DTLS || proto == COAP_PROTO_TLS ||
      proto == COAP_PROTO_WSS) {
    /* Encrypted session */
    if (test->root_ca_file || test->ca_file || test->cert_file) {
      /* Setup PKI session */
      coap_dtls_pki_t *dtls_pki = setup_pki(ctx, test);
      if (test->doing_oscore) {
        session = coap_new_client_session_oscore_pki(ctx, bind_addr, dst,
                                                     proto, dtls_pki,
                                                     test->oscore_conf);
      } else
        session = coap_new_client_session_pki(ctx, bind_addr, dst, proto,
                                              dtls_pki);
    } else if (identity || key) {
      /* Setup PSK session */
      coap_dtls_cpsk_t *dtls_psk = setup_psk(identity, identity_len,
                                             key, key_len, test);
      if (test->doing_oscore) {
        session = coap_new_client_session_oscore_psk(ctx, bind_addr, dst,
                                                     proto, dtls_psk,
                                                     test->oscore_conf);
      } else
        session = coap_new_client_session_psk2(ctx, bind_addr, dst, proto,
                                               dtls_psk);
    } else {
      /* No PKI or PSK defined, as encrypted, use PKI */
      coap_dtls_pki_t *dtls_pki = setup_pki(ctx, test);
      if (test->doing_oscore) {
        session = coap_new_client_session_oscore_pki(ctx, bind_addr, dst,
                                                     proto, dtls_pki,
                                                     test->oscore_conf);
      } else
        session = coap_new_client_session_pki(ctx, bind_addr, dst, proto,
                                              dtls_pki);
    }
  } else {
    /* Non-encrypted session */
    if (test->doing_oscore) {
      session = coap_new_client_session_oscore(ctx, bind_addr, dst, proto,
                                               test->oscore_conf);
    } else
      session = coap_new_client_session(ctx, bind_addr, dst, proto);
  }
  if (session && (proto == COAP_PROTO_WS || proto == COAP_PROTO_WSS)) {
    coap_ws_set_host_request(session, &test->uri.host);
  }
  return session;
}

static coap_session_t *
get_session(coap_context_t *ctx,
            const char *local_addr,
            const char *local_port,
            coap_uri_scheme_t scheme,
            coap_proto_t proto,
            coap_address_t *dst,
            const uint8_t *identity,
            size_t identity_len,
            const uint8_t *key,
            size_t key_len,
            struct coapclient_test *test) {
  coap_session_t *session = NULL;

  test->is_mcast = coap_is_mcast(dst);
  if (local_addr || coap_is_af_unix(dst)) {
    if (coap_is_af_unix(dst)) {
      coap_address_t bind_addr;

      if (local_addr) {
        if (!coap_address_set_unix_domain(&bind_addr,
                                          (const uint8_t *)local_addr,
                                          strlen(local_addr))) {
          fprintf(stderr, "coap_address_set_unix_domain: %s: failed\n",
                  local_addr);
          return NULL;
        }
      } else {
        char buf[COAP_UNIX_PATH_MAX];

        /* Need a unique address */
        snprintf(buf, COAP_UNIX_PATH_MAX,
                 "/tmp/coap-client");
        if (!coap_address_set_unix_domain(&bind_addr, (const uint8_t *)buf,
                                          strlen(buf))) {
          fprintf(stderr, "coap_address_set_unix_domain: %s: failed\n",
                  buf);
          remove(buf);
          return NULL;
        }
        (void)remove(buf);
      }
      session = open_session(ctx, proto, &bind_addr, dst,
                             identity, identity_len, key, key_len, test);
    } else {
      coap_addr_info_t *info_list = NULL;
      coap_addr_info_t *info;
      coap_str_const_t local;
      uint16_t port = local_port ? atoi(local_port) : 0;

      local.s = (const uint8_t *)local_addr;
      local.length = strlen(local_addr);
      /* resolve local address where data should be sent from */
      info_list = coap_resolve_address_info(&local, port, port, port, port,
                                            AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV | AI_ALL,
                                            1 << scheme,
                                            COAP_RESOLVE_TYPE_LOCAL);
      if (!info_list) {
        fprintf(stderr, "coap_resolve_address_info: %s: failed\n", local_addr);
        return NULL;
      }

      /* iterate through results until success */
      for (info = info_list; info != NULL; info = info->next) {
        session = open_session(ctx, proto, &info->addr, dst,
                               identity, identity_len, key, key_len, test);
        if (session)
          break;
      }
      coap_free_address_info(info_list);
    }
  } else if (local_port) {
    coap_address_t bind_addr;

    coap_address_init(&bind_addr);
    bind_addr.size = dst->size;
    bind_addr.addr.sa.sa_family = dst->addr.sa.sa_family;
    coap_address_set_port(&bind_addr, atoi(local_port));
    session = open_session(ctx, proto, &bind_addr, dst,
                           identity, identity_len, key, key_len, test);
  } else {
    session = open_session(ctx, proto, NULL, dst,
                           identity, identity_len, key, key_len, test);
  }
  return session;
}

static void coapclient_defaults(struct coapclient_test* test)
{
  test->the_token.length = 0;
  test->the_token.s = test->_token_data;

  test->proxy_scheme_option = 0;
  test->uri_host_option = 0;
  test->ping_seconds = 0;
  test->repeat_count = 1;
  test->ready = 0;

  test->doing_getting_block = 0;
  test->single_block_requested = 0;
  test->block_mode = COAP_BLOCK_USE_LIBCOAP;

  test->output_file.length = 0;
  test->output_file.s = NULL;
  test->file = NULL;  /* output file stream */

  test->reliable = 0;

  test->add_nl = 0;
  test->is_mcast = 0;
  test->csm_max_message_size = 0;

  test->msgtype = COAP_MESSAGE_CON;

  test->cert_file = NULL;
  test->ca_file = NULL;
  test->root_ca_file = NULL;
  
  test->is_rpk_not_cert = 0;
  test->cert_mem = NULL;
  test->key_mem = NULL;
  test->ca_mem = NULL;
  test->verify_peer_cert = 1;

  test->method = 1;
  test->block.m = 0;
  test->block.num = 0;
  test->block.szx = 6;
  test->last_block1_mid = 0;
  test->wait_seconds = DEFAULT_WAIT_TIME;  /* default timeout in seconds */
  test->wait_ms = 0;
  test->obs_started = 0;
  test->obs_seconds = 30;  /* default observe time */
  test->obs_ms = 0;  /* timeout for current subscription */
  test->obs_ms_reset = 0;
  test->doing_observe = 0;

  test->oscore_conf = NULL;
  test->doing_oscore = 0;

  test->quit = 0;

  test->user_length = -1;

  test->create_uri_opts = 1;
  test->log_level = COAP_LOG_WARN;
  test->dtls_log_level = COAP_LOG_ERR;

}

static int
coapclient_parse_arguments(struct coapclient_test *test, int argc, char **argv)
{
  int opt;

  while ((opt = getopt(argc, argv,
                       "a:b:c:e:h:j:k:l:m:no:p:rs:t:u:v:wA:B:C:E:G:H:J:K:L:M:NO:P:R:T:UV:X:")) != -1) {
    switch (opt) {
    case 'a':
      strncpy(test->node_str, optarg, NI_MAXHOST - 1);
      test->node_str[NI_MAXHOST - 1] = '\0';
      break;
    case 'b':
      cmdline_blocksize(test, optarg);
      break;
    case 'B':
      test->wait_seconds = atoi(optarg);
      break;
    case 'c':
      strncpy(test->cert_filestore, optarg, PATH_MAX);
      test->cert_filestore[PATH_MAX] = '\0';
      test->cert_file = test->cert_filestore;
      break;
    case 'C':
      strncpy(test->ca_filestore, optarg, PATH_MAX);
      test->ca_filestore[PATH_MAX] = '\0';
      test->ca_file = test->ca_filestore;
      break;
    case 'R':
      strncpy(test->root_ca_filestore, optarg, PATH_MAX);
      test->root_ca_filestore[PATH_MAX] = '\0';
      test->root_ca_file = test->root_ca_filestore;
      break;
    case 'e':
      if (!cmdline_input(optarg, &test->payload))
        test->payload.length = 0;
      break;
    case 'j' :
      strncpy(test->key_file, optarg, PATH_MAX);
      test->key_file[PATH_MAX] = '\0';
      break;
    case 'J' :
      strncpy(test->pkcs11_pin, optarg, PATH_MAX);
      test->pkcs11_pin[PATH_MAX] = '\0';
      break;
    case 'k':
      test->key_length = cmdline_read_key(optarg, test->key, MAX_KEY);
      break;
    case 'L':
      test->block_mode = strtoul(optarg, NULL, 0);
      if (!(test->block_mode & COAP_BLOCK_USE_LIBCOAP)) {
        fprintf(stderr, "Block mode must include COAP_BLOCK_USE_LIBCOAP (1)\n");
        goto failed;
      }
      break;
    case 'p':
      strncpy(test->port_str, optarg, NI_MAXSERV - 1);
      test->port_str[NI_MAXSERV - 1] = '\0';
      break;
    case 'm':
      test->method = cmdline_method(optarg);
      break;
    case 'w':
      test->add_nl = 1;
      break;
    case 'N':
      test->msgtype = COAP_MESSAGE_NON;
      break;
    case 's':
      cmdline_subscribe(test, optarg);
      break;
    case 'o':
      test->output_file.length = strlen(optarg);
      test->output_file.s = (unsigned char *)coap_malloc(test->output_file.length + 1);

      if (!test->output_file.s) {
        fprintf(stderr, "cannot set output file: insufficient memory\n");
        goto failed;
      } else {
        /* copy filename including trailing zero */
        memcpy(test->output_file.s, optarg, test->output_file.length + 1);
      }
      break;
    case 'A':
      cmdline_content_type(test, optarg, COAP_OPTION_ACCEPT);
      break;
    case 't':
      cmdline_content_type(test, optarg, COAP_OPTION_CONTENT_TYPE);
      break;
    case 'O':
      cmdline_option(test, optarg);
      break;
    case 'P':
      if (!cmdline_proxy(test, optarg)) {
        fprintf(stderr, "error specifying proxy address\n");
        goto failed;
      }
      break;
    case 'T':
      cmdline_token(test, optarg);
      break;
    case 'u':
      test->user_length = cmdline_read_user(optarg, test->user, MAX_USER);
      break;
    case 'U':
      test->create_uri_opts = 0;
      break;
    case 'v':
      test->log_level = strtol(optarg, NULL, 10);
      break;
    case 'V':
      test->dtls_log_level = strtol(optarg, NULL, 10);
      break;
    case 'l':
      if (!coap_debug_set_packet_loss(optarg)) {
        usage(argv[0], LIBCOAP_PACKAGE_VERSION);
        goto failed;
      }
      break;
    case 'r':
      test->reliable = coap_tcp_is_supported();
      break;
    case 'K':
      test->ping_seconds = atoi(optarg);
      break;
    case 'h':
      if (!cmdline_read_hint_check(test, optarg)) {
        usage(argv[0], LIBCOAP_PACKAGE_VERSION);
        goto failed;
      }
      break;
    case 'H':
      if (!cmdline_hop_limit(test, optarg))
        fprintf(stderr, "Hop Limit has to be > 0 and < 256\n");
      break;
    case 'n':
      test->verify_peer_cert = 0;
      break;
    case 'G':
      test->repeat_count = atoi(optarg);
      if (!test->repeat_count || test->repeat_count > 255) {
        fprintf(stderr, "'-G count' has to be > 0 and < 256\n");
        test->repeat_count = 1;
      }
      break;
    case 'X':
      test->csm_max_message_size = strtol(optarg, NULL, 10);
      break;
    case 'E':
      test->doing_oscore = cmdline_oscore(test, optarg);
      if (!test->doing_oscore) {
        goto failed;
      }
      break;
    default:
      usage(argv[0], LIBCOAP_PACKAGE_VERSION);
      goto failed;
    }
  }

  if (optind < argc) {
    if (cmdline_uri(test, argv[optind]) < 0) {
      goto failed;
    }
  } else {
    usage(argv[0], LIBCOAP_PACKAGE_VERSION);
    goto failed;
  }

  if (test->key_length < 0) {
    coap_log_crit("Invalid pre-shared key specified\n");
    goto failed;
  }

  return 0;
failed:
  return -1;
}

static void
coapclient_free_test(struct coapclient_test *test)
{
  int i;

  if (test->payload.s) {
    coap_free(test->payload.s);
    test->payload.length = 0;
  }
  if (test->optlist) {
    coap_delete_optlist(test->optlist);
    test->optlist = NULL;
  }

  if (test->output_file.s) {
    coap_free(test->output_file.s);
    test->output_file.s = NULL;
  }

  for (i = 0; i < test->valid_ihs.count; i++) {
    free(test->valid_ihs.ih_list[i].hint_match);
    coap_delete_bin_const(test->valid_ihs.ih_list[i].new_identity);
    coap_delete_bin_const(test->valid_ihs.ih_list[i].new_key);
  }
  if (test->valid_ihs.count) {
    free(test->valid_ihs.ih_list);
    test->valid_ihs.count = 0;
    test->valid_ihs.ih_list = NULL;
  }

  free(test);
}

static uint32_t log_mapping[] = {
	[COAP_LOG_EMERG ... COAP_LOG_ERR] 	= WISE_LOG_ERROR,
	[COAP_LOG_WARN] 					= WISE_LOG_WARN,
	[COAP_LOG_NOTICE ... COAP_LOG_INFO]	= WISE_LOG_INFO,
	[COAP_LOG_DEBUG] 					= WISE_LOG_DEBUG,
	[COAP_LOG_OSCORE] 					= WISE_LOG_VERBOSE,
};

static void
coap_log_handler (coap_log_t level, const char *message)
{
    uint32_t wise_level = log_mapping[level];
    const char *cp = strchr(message, '\n');

    while (cp) {
        WISE_LOG_LEVEL(wise_level, TAG, "%.*s", (int)(cp - message), message);
        message = cp + 1;
        cp = strchr(message, '\n');
    }
    if (message[0] != '\000') {
        WISE_LOG_LEVEL(wise_level, TAG, "%s", message);
    }
}

static void run_coap_client(void *arg)
{
  struct coapclient_test *test = arg;
  coap_context_t  *ctx = NULL;
  coap_session_t *session = NULL;
  coap_address_t dst;
  int result = -1;
  coap_pdu_t  *pdu;
  static coap_str_const_t server;
  uint16_t port = COAP_DEFAULT_PORT;
  size_t i;
  coap_uri_scheme_t scheme;
  coap_proto_t proto;
  uint32_t repeat_ms = REPEAT_DELAY_MS;
  uint8_t *data = NULL;
  size_t data_len = 0;
  coap_addr_info_t *info_list = NULL;
  char tmpbuf[INET6_ADDRSTRLEN];
#define BUFSIZE 100
  static unsigned char buf[BUFSIZE];

  /* Initialize libcoap library */
  coap_startup();

  /* Set up the CoAP logging */
  coap_set_log_handler(coap_log_handler);
  coap_set_log_level(test->log_level);
  coap_dtls_set_log_level(test->dtls_log_level);

  if (test->proxy.host.length) {
    server = test->proxy.host;
    port = test->proxy.port;
    scheme = test->proxy.scheme;
  } else {
    server = test->uri.host;
    port = test->proxy_scheme_option ? test->proxy.port : test->uri.port;
    scheme = test->proxy_scheme_option ? test->proxy.scheme : test->uri.scheme;
  }

  /* resolve destination address where data should be sent */
  info_list = coap_resolve_address_info(&server, port, port, port, port,
                                        0,
                                        1 << scheme,
                                        COAP_RESOLVE_TYPE_REMOTE);

  if (info_list == NULL) {
    coap_log_err("failed to resolve address\n");
    goto failed;
  }
  proto = info_list->proto;
  memcpy(&dst, &info_list->addr, sizeof(dst));
  coap_free_address_info(info_list);

  ctx = coap_new_context(NULL);
  if (!ctx) {
    coap_log_emerg("cannot create context\n");
    goto failed;
  }

  if (test->doing_oscore) {
    if (get_oscore_conf(test) == NULL)
      goto failed;
  }

  coap_context_set_keepalive(ctx, test->ping_seconds);
  coap_context_set_block_mode(ctx, test->block_mode);
  if (test->csm_max_message_size)
    coap_context_set_csm_max_message_size(ctx, test->csm_max_message_size);
  coap_register_response_handler(ctx, message_handler);
  coap_register_event_handler(ctx, event_handler);
  coap_register_nack_handler(ctx, nack_handler);
  if (test->the_token.length > COAP_TOKEN_DEFAULT_MAX)
    coap_context_set_max_token_size(ctx, test->the_token.length);

  session = get_session(ctx,
                        test->node_str[0] ? test->node_str : NULL,
                        test->port_str[0] ? test->port_str : NULL,
                        scheme,
                        proto,
                        &dst,
                        test->user_length >= 0 ? test->user : NULL,
                        test->user_length >= 0 ? test->user_length : 0,
                        test->key_length > 0 ? test->key : NULL,
                        test->key_length > 0 ? test->key_length : 0,
                        test
                       );

  if (!session) {
    coap_log_err("cannot create client session\n");
    goto failed;
  }
  coap_session_set_app_data(session, test);
  /*
   * Prime the base token value, which coap_session_new_token() will increment
   * every time it is called to get an unique token.
   * [Option '-T token' is used to seed a different value]
   * Note that only the first 8 bytes of the token are used as the prime.
   */
  coap_session_init_token(session, test->the_token.length, test->the_token.s);

  /* Convert provided uri into CoAP options */
  if (coap_uri_into_options(&test->uri, !test->uri_host_option && !test->proxy.host.length ?
                            &dst : NULL,
                            &test->optlist, test->create_uri_opts,
                            buf, sizeof(buf)) < 0) {
    coap_log_err("Failed to create options for URI\n");
    goto failed;
  }
  /* This is to keep the test suites happy */
  coap_print_ip_addr(&dst, tmpbuf, sizeof(tmpbuf));
  WISE_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);

  /* set block option if requested at commandline */
  if (test->flags & FLAGS_BLOCK)
    set_blocksize(test);

  /* Send the first (and may be only PDU) */
  if (test->payload.length) {
    /* Create some new data to use for this iteration */
    data = coap_malloc(test->payload.length);
    if (data == NULL)
      goto failed;
    memcpy(data, test->payload.s, test->payload.length);
    data_len = test->payload.length;
  }
  if (!(pdu = coap_new_request(ctx, session, test->method, &test->optlist, data,
                               data_len, test))) {
    goto failed;
  }

  if (test->is_mcast && test->wait_seconds == DEFAULT_WAIT_TIME)
    /* Allow for other servers to respond within DEFAULT_LEISURE RFC7252 8.2 */
    test->wait_seconds = coap_session_get_default_leisure(session).integer_part + 1;

  test->wait_ms = test->wait_seconds * 1000;
  coap_log_debug("timeout is set to %u seconds\n", test->wait_seconds);

  coap_log_debug("sending CoAP request:\n");
  if (coap_get_log_level() < COAP_LOG_DEBUG)
    coap_show_pdu(COAP_LOG_INFO, pdu);

  if (coap_send(session, pdu) == COAP_INVALID_MID) {
    coap_log_err("cannot send CoAP pdu\n");
    test->quit = 1;
  }
  if (test->repeat_count)
    test->repeat_count--;

  while (!test->quit &&                /* immediate quit not required .. and .. */
         (test->tracked_tokens_count || /* token not responded to or still observe */
          test->is_mcast ||             /* mcast active */
          test->repeat_count ||         /* more repeat transmissions to go */
          coap_io_pending(ctx))) { /* i/o not yet complete */
    uint32_t timeout_ms;

    /*
     * 3 factors determine how long to wait in coap_io_process()
     *   Remaining overall wait time (wait_ms)
     *   Remaining overall observe unsolicited response time (obs_ms)
     *   Delay of up to one second before sending off the next request
     */
    if (test->obs_ms) {
      timeout_ms = min(test->wait_ms, test->obs_ms);
    } else {
      timeout_ms = test->wait_ms;
    }
    if (test->repeat_count) {
      timeout_ms = min(timeout_ms, repeat_ms);
    }

    result = coap_io_process(ctx, timeout_ms);

    if (result >= 0) {
      if (test->wait_ms > 0) {
        if ((unsigned)result >= test->wait_ms) {
          coap_log_info("timeout\n");
          break;
        } else {
          test->wait_ms -= result;
        }
      }
      if (test->obs_ms > 0 && !test->obs_ms_reset) {
        if ((unsigned)result >= test->obs_ms) {
          coap_log_debug("clear observation relationship\n");
          for (i = 0; i < test->tracked_tokens_count; i++) {
            if (test->tracked_tokens[i].observe) {
              coap_cancel_observe(session, test->tracked_tokens[i].token, test->msgtype);
              test->tracked_tokens[i].observe = 0;
              coap_io_process(ctx, COAP_IO_NO_WAIT);
            }
          }
          test->doing_observe = 0;

          /* make sure that the obs timer does not fire again */
          test->obs_ms = 0;
          test->obs_seconds = 0;
        } else {
          test->obs_ms -= result;
        }
      }

      if (test->ready && test->repeat_count) {
        /* Send off next request if appropriate */
        if (repeat_ms > (unsigned)result) {
          repeat_ms -= (unsigned)result;
        } else {
          /* Doing this once a second */
          repeat_ms = REPEAT_DELAY_MS;
          if (test->payload.length) {
            /* Create some new data to use for this iteration */
            data = coap_malloc(test->payload.length);
            if (data == NULL)
              goto failed;
            memcpy(data, test->payload.s, test->payload.length);
            data_len = test->payload.length;
          }
          if (!(pdu = coap_new_request(ctx, session, test->method, &test->optlist,
                                       data, data_len, test))) {
            goto failed;
          }
          coap_log_debug("sending CoAP request:\n");
          if (coap_get_log_level() < COAP_LOG_DEBUG)
            coap_show_pdu(COAP_LOG_INFO, pdu);

          test->ready = 0;
          if (coap_send(session, pdu) == COAP_INVALID_MID) {
            coap_log_err("cannot send CoAP pdu\n");
            test->quit = 1;
          }
          test->repeat_count--;
        }
      }
      test->obs_ms_reset = 0;
    }
  }

finish:
  for (i = 0; i < test->tracked_tokens_count; i++) {
    if (test->tracked_tokens[i].observe) {
      coap_cancel_observe(session, test->tracked_tokens[i].token, test->msgtype);
      test->tracked_tokens[i].observe = 0;
      coap_io_process(ctx, COAP_IO_NO_WAIT);
    }
  }

  /* Clean up library usage */
  coap_session_release(session);
  coap_free_context(ctx);

  /* Clean up local usage */
  coap_free(test->ca_mem);
  test->ca_mem = NULL;
  test->ca_mem_len = 0;
  coap_free(test->cert_mem);
  test->cert_mem = NULL;
  test->cert_mem_len = 0;
  coap_free(test->key_mem);
  test->key_mem = NULL;
  test->key_mem_len = 0;
  coap_free(test->payload.s);
  test->payload.length = 0;
  test->payload.s = NULL;

  for (i = 0; i < test->valid_ihs.count; i++) {
    free(test->valid_ihs.ih_list[i].hint_match);
    coap_delete_bin_const(test->valid_ihs.ih_list[i].new_identity);
    coap_delete_bin_const(test->valid_ihs.ih_list[i].new_key);
  }
  if (test->valid_ihs.count)
    free(test->valid_ihs.ih_list);
  test->valid_ihs.count = 0;
  test->valid_ihs.ih_list = NULL;

  for (i = 0; i < test->tracked_tokens_count; i++) {
    coap_delete_binary(test->tracked_tokens[i].token);
  }
  free(test->tracked_tokens);
  test->the_token.length = 0;
  test->tracked_tokens_count = 0;
  test->tracked_tokens = NULL;

  coap_delete_optlist(test->optlist);
  test->optlist = NULL;
  close_output(test);
  coap_free(test->output_file.s);
  test->output_file.s = NULL;

  free(test);
  osThreadExit();

  return;

failed:
  goto finish;
}

static struct coapclient_test *
coapclient_new_test()
{
  struct coapclient_test *test;

  test = (struct coapclient_test *) malloc(sizeof(struct coapclient_test));
  if (!test) {
    return NULL;
  }
  /* initialize everything to zero */
  memset(test, 0, sizeof(struct coapclient_test));

  return test;
}

void coapclient_ip_got_flag(uint8_t flag)
{
  ip_got_flag = flag;
}

static const osThreadAttr_t attr = {
	.name = "coap_client",
	.stack_size = 6 * 1024,
	.priority = osPriorityNormal,
};

static int
do_coap_cli(int argc, char **argv)
{
  struct coapclient_test *test;

  if (!ip_got_flag) {
    WISE_LOGE(TAG, "Network is not up.\n");
    return CMD_RET_FAILURE;
  }

  test = coapclient_new_test();
  if (!test) {
    WISE_LOGE(TAG, "create new test error.\n");
    return CMD_RET_FAILURE;
  }

  coapclient_defaults(test);  /* sets defaults */

  if (coapclient_parse_arguments(test, argc, argv) < 0) {
    WISE_LOGE(TAG, "parameter error.\n");
    coapclient_free_test(test);
    return CMD_RET_FAILURE;
  }

  osThreadNew(run_coap_client, test, &attr);

  return CMD_RET_SUCCESS;
}

CMD(coap_client, do_coap_cli,
	"CLI for CoAP client operations",
	"coap_client for help"
);

