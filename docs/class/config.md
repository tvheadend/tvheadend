<tvh_include>inc/config_contents</tvh_include>

---

<tvh_include>inc/config_overview</tvh_include>

!['General Base'](static/img/doc/config/base.png)

<tvh_include>inc/config_notes</tvh_include>

### Encrypted HTSP (STARTTLS)

HTSP is not encrypted by default: the user name, the EPG, the
recordings and the streams can be read by anyone on the network path,
and the session can be tampered with. Only the password is protected
(challenge-response).

Tick *Enable encryption (STARTTLS)* to let clients switch the HTSP
connection to TLS. It uses the same port (9982): the client sends
*hello*, sees `starttls` in the server capabilities, sends *startTLS*
and continues over TLS. The change is applied on *Save*, without a
restart, and the *Status* shows whether a certificate is available.
Clients without TLS support keep working unencrypted.

* Tick *Require encryption* to refuse credentials and requests (other
  than *hello*) before the client has started TLS. Clients without TLS
  support can then no longer log in. It applies to new connections.
* With no *Certificate file* set, Tvheadend generates a self-signed
  certificate in the configuration directory (`tls/htsp-cert.pem` and
  `tls/htsp-key.pem`). Compare the *Certificate fingerprint* shown here
  with the one the client shows on the first connection. Delete both
  files and restart Tvheadend to get a new certificate.
* To use a certificate from a certificate authority (e.g. Let's
  Encrypt), set the *Certificate file* (full chain) and the *Private key
  file*. The files are checked on each startTLS, so a renewed
  certificate is used without a restart. When they cannot be loaded,
  startTLS is refused (see the *Status* and the log). A key protected
  by a pass phrase is not supported.
* TLS 1.2 or newer is required.
* Access rights, users and passwords are the same as without TLS.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
