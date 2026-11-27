// SPDX-License-Identifier: GPL-3.0-or-later
/*
 *  tvheadend, TLS server side support
 *  Copyright (C) 2026 Tvheadend contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "tvheadend.h"
#include "settings.h"
#include "tls_server.h"

/* The handshake must finish within this time. */
#define TLS_HANDSHAKE_TIMEOUT  10
/* A message must be sent within this time, as in tvh_write(). */
#define TLS_WRITE_TIMEOUT      25
/* Validity of the generated self-signed certificate. */
#define TLS_CERT_DAYS          3650

#define TLS_AUTO_CERT          "tls/htsp-cert.pem"
#define TLS_AUTO_KEY           "tls/htsp-key.pem"

struct tls_conn {
  SSL         *ssl;
  int          fd;
  int          failed;  /* fatal error: no SSL_shutdown() */
  tvh_mutex_t  lock;    /* the reader and the writer thread share ssl */
};

typedef struct tls_file_id {
  dev_t     dev;
  ino_t     ino;
  off_t     size;
  time_t    mtime;
  long      mtime_nsec;
  time_t    ctime;      /* a permission change makes a file readable */
  long      ctime_nsec;
} tls_file_id_t;

typedef struct tls_source {
  char          *cert;       /* resolved file names */
  char          *key;
  tls_file_id_t  cert_id;
  tls_file_id_t  key_id;
} tls_source_t;

static tvh_mutex_t   tls_lock;
static int           tls_lock_init;
static int           tls_done;
static SSL_CTX      *tls_ctx;
static tls_source_t  tls_loaded;   /* what tls_ctx was built from */
static tls_source_t  tls_failed;   /* last source that did not load */
static char          tls_fp[3 * EVP_MAX_MD_SIZE + 1];

/*
 *
 */
static void
tls_log_errors(int level, const char *peer, const char *what)
{
  unsigned long e;
  char buf[256];
  int logged = 0;

  while ((e = ERR_get_error()) != 0) {
    ERR_error_string_n(e, buf, sizeof(buf));
    tvhlog(level, LS_TLS, "%s%s%s: %s", peer ?: "", peer ? ": " : "", what, buf);
    logged = 1;
  }
  if (!logged)
    tvhlog(level, LS_TLS, "%s%s%s", peer ?: "", peer ? ": " : "", what);
}

static void
tls_source_free(tls_source_t *s)
{
  free(s->cert);
  free(s->key);
  memset(s, 0, sizeof(*s));
}

static int
tls_file_id(const char *path, tls_file_id_t *id)
{
  struct stat st;

  memset(id, 0, sizeof(*id));
  if (stat(path, &st))
    return -1;
  id->dev   = st.st_dev;
  id->ino   = st.st_ino;
  id->size  = st.st_size;
  id->mtime = st.st_mtime;
  id->ctime = st.st_ctime;
#if defined(PLATFORM_DARWIN)
  id->mtime_nsec = st.st_mtimespec.tv_nsec;
  id->ctime_nsec = st.st_ctimespec.tv_nsec;
#else
  id->mtime_nsec = st.st_mtim.tv_nsec;
  id->ctime_nsec = st.st_ctim.tv_nsec;
#endif
  return 0;
}

static int
tls_source_equal(const tls_source_t *a, const tls_source_t *b)
{
  return a->cert && b->cert && a->key && b->key &&
         !strcmp(a->cert, b->cert) && !strcmp(a->key, b->key) &&
         !memcmp(&a->cert_id, &b->cert_id, sizeof(a->cert_id)) &&
         !memcmp(&a->key_id, &b->key_id, sizeof(a->key_id));
}

/* Keys protected by a pass phrase are not supported: never prompt */
static int
tls_no_passphrase(char *buf, int size, int rwflag, void *u)
{
  return 0;
}

/*
 * Self-signed certificate
 */
static int
tls_add_ext(X509 *x, int nid, const char *value)
{
  X509V3_CTX ctx;
  X509_EXTENSION *ext;
  char buf[256];
  int r;

  strlcpy(buf, value, sizeof(buf));
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, x, x, NULL, NULL, 0);
  ext = X509V3_EXT_conf_nid(NULL, &ctx, nid, buf);
  if (ext == NULL)
    return -1;
  r = X509_add_ext(x, ext, -1) ? 0 : -1;
  X509_EXTENSION_free(ext);
  return r;
}

static int
tls_valid_dns_name(const char *s)
{
  if (*s == '\0' || *s == '-' || *s == '.')
    return 0;
  for ( ; *s; s++)
    if (!isalnum((unsigned char)*s) && *s != '-' && *s != '.')
      return 0;
  return 1;
}

static int
tls_write_pem(const char *path, int mode, EVP_PKEY *pkey, X509 *x)
{
  char tmp[PATH_MAX];
  FILE *f;
  int fd, ok;

  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  fd = tvh_open(tmp, O_CREAT | O_TRUNC | O_WRONLY, mode);
  if (fd < 0) {
    tvherror(LS_TLS, "unable to create %s: %s", tmp, strerror(errno));
    return -1;
  }
  f = fdopen(fd, "w");
  if (f == NULL) {
    close(fd);
    unlink(tmp);
    return -1;
  }
  if (pkey)
    ok = PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL);
  else
    ok = PEM_write_X509(f, x);
  if (fclose(f))
    ok = 0;
  if (!ok || rename(tmp, path)) {
    tvherror(LS_TLS, "unable to write %s", path);
    unlink(tmp);
    return -1;
  }
  return 0;
}

static int
tls_generate(const char *cert_path, const char *key_path)
{
  EVP_PKEY_CTX *pctx = NULL;
  EVP_PKEY *pkey = NULL;
  X509 *x = NULL;
  X509_NAME *name;
  BIGNUM *bn = NULL;
  uint8_t serial[16];
  char host[256], san[300];
  int r = -1;

  tvhinfo(LS_TLS, "generating a self-signed certificate %s", cert_path);

  if (hts_settings_makedirs(cert_path) || hts_settings_makedirs(key_path))
    return -1;

  pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  if (pctx == NULL ||
      EVP_PKEY_keygen_init(pctx) <= 0 ||
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0 ||
      EVP_PKEY_keygen(pctx, &pkey) <= 0) {
    tls_log_errors(LOG_ERR, NULL, "key generation failed");
    goto end;
  }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EC_KEY_set_asn1_flag(EVP_PKEY_get0(pkey), OPENSSL_EC_NAMED_CURVE);
#endif

  if (gethostname(host, sizeof(host)))
    host[0] = '\0';
  host[sizeof(host) - 1] = '\0';
  if (!tls_valid_dns_name(host))
    strlcpy(host, "tvheadend", sizeof(host));

  x = X509_new();
  if (x == NULL || RAND_bytes(serial, sizeof(serial)) != 1)
    goto fail;
  serial[0] &= 0x7f;
  bn = BN_bin2bn(serial, sizeof(serial), NULL);
  if (bn == NULL ||
      !X509_set_version(x, 2) ||
      !BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(x)) ||
      !X509_gmtime_adj(X509_get_notBefore(x), -86400) ||
      !X509_gmtime_adj(X509_get_notAfter(x), (long)TLS_CERT_DAYS * 86400) ||
      !X509_set_pubkey(x, pkey))
    goto fail;

  name = X509_get_subject_name(x);
  if (!X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8,
                                  (const unsigned char *)"Tvheadend", -1, -1, 0) ||
      !X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
                                  (const unsigned char *)host, -1, -1, 0) ||
      !X509_set_issuer_name(x, name))
    goto fail;

  snprintf(san, sizeof(san), "DNS:%s", host);
  if (tls_add_ext(x, NID_basic_constraints, "critical,CA:FALSE") ||
      tls_add_ext(x, NID_key_usage, "critical,digitalSignature") ||
      tls_add_ext(x, NID_ext_key_usage, "serverAuth") ||
      tls_add_ext(x, NID_subject_alt_name, san))
    goto fail;

  if (!X509_sign(x, pkey, EVP_sha256()))
    goto fail;

  /* The key first: a certificate without its key is useless. */
  if (tls_write_pem(key_path, S_IRUSR | S_IWUSR, pkey, NULL) ||
      tls_write_pem(cert_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, NULL, x))
    goto end;

  r = 0;
  goto end;

fail:
  tls_log_errors(LOG_ERR, NULL, "certificate generation failed");
end:
  BN_free(bn);
  X509_free(x);
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);
  return r;
}

/*
 * Context
 */
static SSL_CTX *
tls_ctx_create(const tls_source_t *src)
{
  SSL_CTX *ctx;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ctx = SSL_CTX_new(SSLv23_server_method());
  if (ctx == NULL)
    goto fail;
  SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                           SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
  SSL_CTX_set_ecdh_auto(ctx, 1);
#else
  ctx = SSL_CTX_new(TLS_server_method());
  if (ctx == NULL)
    goto fail;
  if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION))
    goto fail;
#endif
  SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION |
                           SSL_OP_CIPHER_SERVER_PREFERENCE
#ifdef SSL_OP_NO_RENEGOTIATION
                           | SSL_OP_NO_RENEGOTIATION
#endif
#ifdef SSL_OP_PRIORITIZE_CHACHA
                           /* ChaCha20 for clients that prefer it (no
                            * AES hardware, e.g. small ARM boxes) */
                           | SSL_OP_PRIORITIZE_CHACHA
#endif
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
                           /* HTSP messages carry their length, a
                            * truncation cannot go unnoticed: treat a
                            * close without close_notify as a close */
                           | SSL_OP_IGNORE_UNEXPECTED_EOF
#endif
                           );
  SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE |
                        SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  /* no session resumption: the ticket key would outlive the sessions */
  SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);
  SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  SSL_CTX_set_num_tickets(ctx, 0);
#endif
  /* TLS 1.2 cipher suites: forward secrecy only */
  if (SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4:!3DES") != 1)
    goto fail;

  SSL_CTX_set_default_passwd_cb(ctx, tls_no_passphrase);
  if (SSL_CTX_use_certificate_chain_file(ctx, src->cert) != 1) {
    tls_log_errors(LOG_ERR, NULL, "unable to load the certificate");
    goto err;
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, src->key, SSL_FILETYPE_PEM) != 1) {
    tls_log_errors(LOG_ERR, NULL, "unable to load the private key"
                   " (a key protected by a pass phrase is not supported)");
    goto err;
  }
  if (SSL_CTX_check_private_key(ctx) != 1) {
    tls_log_errors(LOG_ERR, NULL, "the private key does not match the certificate");
    goto err;
  }
  return ctx;

fail:
  tls_log_errors(LOG_ERR, NULL, "unable to create the TLS context");
err:
  SSL_CTX_free(ctx);
  return NULL;
}

static void
tls_update_fingerprint(SSL_CTX *ctx)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int i, len = 0;
  X509 *x = SSL_CTX_get0_certificate(ctx);
  size_t off = 0;

  tls_fp[0] = '\0';
  if (x == NULL || !X509_digest(x, EVP_sha256(), md, &len))
    return;
  for (i = 0; i < len && off < sizeof(tls_fp); i++)
    off += snprintf(tls_fp + off, sizeof(tls_fp) - off,
                    i ? ":%02X" : "%02X", md[i]);
}

/*
 * The configuration is checked on each connection; a persisting error
 * is logged once (tls_lock held).
 */
static char tls_last_error[512];

static void __attribute__((format(printf, 1, 2)))
tls_resolve_error(const char *fmt, ...)
{
  char buf[sizeof(tls_last_error)];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (strcmp(buf, tls_last_error)) {
    strlcpy(tls_last_error, buf, sizeof(tls_last_error));
    tvherror(LS_TLS, "%s", buf);
  }
}

/*
 * Resolve the configured paths into the files to load. An empty
 * certificate path selects the generated certificate. An empty key path
 * means that the key is stored in the certificate file.
 */
static int
tls_resolve(const char *cert_cfg, const char *key_cfg, tls_source_t *src)
{
  char path[PATH_MAX];
  tls_file_id_t id;

  memset(src, 0, sizeof(*src));

  if (cert_cfg == NULL || *cert_cfg == '\0') {
    if (key_cfg && *key_cfg) {
      tls_resolve_error("a private key is configured without a certificate");
      return -1;
    }
    if (hts_settings_buildpath(path, sizeof(path), TLS_AUTO_CERT))
      return -1;
    src->cert = strdup(path);
    hts_settings_buildpath(path, sizeof(path), TLS_AUTO_KEY);
    src->key = strdup(path);
    if (tls_file_id(src->cert, &id) || tls_file_id(src->key, &id))
      if (tls_generate(src->cert, src->key))
        return -1;
  } else {
    src->cert = strdup(cert_cfg);
    src->key  = strdup(key_cfg && *key_cfg ? key_cfg : cert_cfg);
  }

  if (tls_file_id(src->cert, &src->cert_id)) {
    tls_resolve_error("certificate %s: %s", src->cert, strerror(errno));
    return -1;
  }
  if (tls_file_id(src->key, &src->key_id)) {
    tls_resolve_error("private key %s: %s", src->key, strerror(errno));
    return -1;
  }
  tls_last_error[0] = '\0';
  return 0;
}

/* No certificate: running sessions keep their reference */
static void
tls_unload(void)
{
  SSL_CTX_free(tls_ctx);
  tls_ctx = NULL;
  tls_source_free(&tls_loaded);
  tls_fp[0] = '\0';
}

int
tls_server_check(const char *cert_cfg, const char *key_cfg)
{
  tls_source_t src;
  SSL_CTX *ctx;
  int r;

  tvh_mutex_lock(&tls_lock);

  /* connections may still run after tls_server_done() */
  if (tls_done) {
    tvh_mutex_unlock(&tls_lock);
    return -1;
  }

  if (tls_resolve(cert_cfg, key_cfg, &src)) {
    tls_unload();
    goto end;
  }

  /* Nothing changed since the last (successful or failed) attempt */
  if (tls_source_equal(&src, &tls_loaded) || tls_source_equal(&src, &tls_failed))
    goto end;

  ctx = tls_ctx_create(&src);
  if (ctx == NULL) {
    tvherror(LS_TLS, "%s: certificate not loaded", src.cert);
    tls_unload();
    tls_source_free(&tls_failed);
    tls_failed = src;
    memset(&src, 0, sizeof(src));
    goto end;
  }

  SSL_CTX_free(tls_ctx);
  tls_ctx = ctx;
  tls_source_free(&tls_loaded);
  tls_source_free(&tls_failed);
  tls_loaded = src;
  memset(&src, 0, sizeof(src));
  tls_update_fingerprint(ctx);
  tvhinfo(LS_TLS, "using certificate %s (SHA-256 fingerprint %s)",
          tls_loaded.cert, tls_fp);

end:
  tls_source_free(&src);
  r = tls_ctx ? 0 : -1;
  tvh_mutex_unlock(&tls_lock);
  return r;
}

void
tls_server_init(void)
{
  if (!tls_lock_init) {
    tvh_mutex_init(&tls_lock, NULL);
    tls_lock_init = 1;
  }
}

void
tls_server_done(void)
{
  if (!tls_lock_init)
    return;
  tvh_mutex_lock(&tls_lock);
  tls_done = 1;
  SSL_CTX_free(tls_ctx);
  tls_ctx = NULL;
  tls_source_free(&tls_loaded);
  tls_source_free(&tls_failed);
  tls_fp[0] = '\0';
  tvh_mutex_unlock(&tls_lock);
}

void
tls_server_fingerprint(char *dst, size_t dstsize)
{
  if (!tls_lock_init) {
    if (dstsize)
      *dst = '\0';
    return;
  }
  tvh_mutex_lock(&tls_lock);
  strlcpy(dst, tls_fp, dstsize);
  tvh_mutex_unlock(&tls_lock);
}

/*
 * Connections
 */
static int
tls_set_nonblock(int fd)
{
  int fl = fcntl(fd, F_GETFL);
  return fl < 0 ? -1 : fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

/* Wait (without the lock) until the socket is ready for the TLS call. */
static int
tls_wait(int fd, int err, int ms)
{
  struct pollfd pfd;

  pfd.fd = fd;
  pfd.events = err == SSL_ERROR_WANT_WRITE ? POLLOUT : POLLIN;
  pfd.revents = 0;
  if (poll(&pfd, 1, ms) < 0 && !ERRNO_AGAIN(errno))
    return -1;
  return 0;
}

static int
tls_want(int err)
{
  return err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE;
}

tls_conn_t *
tls_server_accept(int fd, const char *peer)
{
  tls_conn_t *tc;
  SSL *ssl = NULL;
  int64_t deadline;
  int r, err, ms;

  tvh_mutex_lock(&tls_lock);
  if (tls_ctx)
    ssl = SSL_new(tls_ctx);
  tvh_mutex_unlock(&tls_lock);
  if (ssl == NULL) {
    tvherror(LS_TLS, "%s: no TLS context", peer);
    return NULL;
  }

  if (tls_set_nonblock(fd) || !SSL_set_fd(ssl, fd)) {
    tvherror(LS_TLS, "%s: unable to set up the TLS session", peer);
    goto fail;
  }

  deadline = mclk() + sec2mono(TLS_HANDSHAKE_TIMEOUT);
  while (1) {
    ERR_clear_error();
    r = SSL_accept(ssl);
    if (r == 1)
      break;
    err = SSL_get_error(ssl, r);
    if (!tls_want(err)) {
      tls_log_errors(LOG_INFO, peer, "TLS handshake failed");
      goto fail;
    }
    ms = (deadline - mclk()) / (MONOCLOCK_RESOLUTION / 1000);
    if (ms <= 0 || !tvheadend_is_running()) {
      tvhinfo(LS_TLS, "%s: TLS handshake timeout", peer);
      goto fail;
    }
    if (tls_wait(fd, err, MIN(ms, 1000)))
      goto fail;
  }

  tvhdebug(LS_TLS, "%s: %s connection, cipher %s", peer,
           SSL_get_version(ssl), SSL_get_cipher_name(ssl));

  tc = calloc(1, sizeof(*tc));
  if (tc == NULL)
    goto fail;
  tc->ssl = ssl;
  tc->fd  = fd;
  tvh_mutex_init(&tc->lock, NULL);
  return tc;

fail:
  SSL_free(ssl);
  ERR_clear_error();
  return NULL;
}

int
tls_conn_read(tls_conn_t *tc, void *buf, size_t len)
{
  int r, err;

  while (len) {
    tvh_mutex_lock(&tc->lock);
    ERR_clear_error();
    r = SSL_read(tc->ssl, buf, len);
    err = r > 0 ? SSL_ERROR_NONE : SSL_get_error(tc->ssl, r);
    if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
      tc->failed = 1;
    tvh_mutex_unlock(&tc->lock);
    if (r > 0) {
      buf += r;
      len -= r;
    } else if (!tls_want(err) || !tvheadend_is_running()) {
      return ECONNRESET;
    } else {
      /* a short timeout: the writer may have consumed the readiness */
      if (tls_wait(tc->fd, err, 1000))
        return errno;
    }
  }
  return 0;
}

int
tls_conn_write(tls_conn_t *tc, const void *buf, size_t len)
{
  int64_t limit = mclk() + sec2mono(TLS_WRITE_TIMEOUT);
  int r, err;

  while (len) {
    tvh_mutex_lock(&tc->lock);
    ERR_clear_error();
    r = SSL_write(tc->ssl, buf, len);
    err = r > 0 ? SSL_ERROR_NONE : SSL_get_error(tc->ssl, r);
    if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
      tc->failed = 1;
    tvh_mutex_unlock(&tc->lock);
    if (r > 0) {
      buf += r;
      len -= r;
    } else if (!tls_want(err) || mclk() > limit) {
      return 1;
    } else if (tls_wait(tc->fd, err, 1000)) {
      return 1;
    }
  }
  return 0;
}

void
tls_conn_close(tls_conn_t *tc)
{
  tvh_mutex_lock(&tc->lock);
  ERR_clear_error();
  if (!tc->failed)
    SSL_shutdown(tc->ssl);      /* one attempt, the socket is non-blocking */
  ERR_clear_error();
  tvh_mutex_unlock(&tc->lock);
}

void
tls_conn_destroy(tls_conn_t *tc)
{
  if (tc == NULL)
    return;
  SSL_free(tc->ssl);
  tvh_mutex_destroy(&tc->lock);
  free(tc);
}
