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

#ifndef TLS_SERVER_H_
#define TLS_SERVER_H_

#include <stddef.h>

/*
 * Server side TLS for stream protocols (HTSP).
 *
 * tls_server_accept() runs the TLS handshake on a connected socket.
 * The protocol code then reads and writes through tls_conn_read() and
 * tls_conn_write(), which may be called from two threads at once.
 *
 * The certificate and the private key are PEM files. When no path is
 * configured, a self-signed certificate is generated in the
 * configuration directory on first use. tls_server_check() reloads
 * changed files, so a renewed certificate needs no restart.
 */

typedef struct tls_conn tls_conn_t;

/* Call once before the other functions. */
void tls_server_init(void);

void tls_server_done(void);

/*
 * Reload the certificate if the configured paths or the files changed.
 * Returns 0 when a usable certificate is loaded.
 */
int tls_server_check(const char *cert_path, const char *key_path);

/* SHA-256 fingerprint of the loaded certificate ("AB:CD:..."), or "". */
void tls_server_fingerprint(char *dst, size_t dstsize);

/*
 * Run the TLS handshake on the connected socket fd and make it
 * non-blocking. The caller keeps owning fd. Returns NULL on failure.
 */
tls_conn_t *tls_server_accept(int fd, const char *peer);

/* Read exactly len bytes: 0, or an errno value (like tcp_read()). */
int tls_conn_read(tls_conn_t *conn, void *buf, size_t len);

/* Write all of buf: 0, or 1 on error or timeout (like tvh_write()). */
int tls_conn_write(tls_conn_t *conn, const void *buf, size_t len);

/* Send the TLS close notification (before the socket is shut down). */
void tls_conn_close(tls_conn_t *conn);

/* Free the session; the reader and the writer must have stopped. */
void tls_conn_destroy(tls_conn_t *conn);

#endif /* TLS_SERVER_H_ */
