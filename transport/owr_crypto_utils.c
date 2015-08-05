/*
 * Copyright (c) 2015, Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/*/
\*\ Owr crypto utils
/*/

/**
 * 
 *
 * Functions to get certificate, fingerprint and private key.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "owr_crypto_utils.h"
#include "owr_private.h"
#include "owr_utils.h"


#ifdef OWR_STATIC
#include <stdlib.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

/* PUBLIC */

/**
 * owr_crypto_create_crypto_data:
 * @types:
 * @callback: (scope async):
 */

/**
 * OwrCaptureSourcesCallback:
 * @privatekey: (transfer none)
 * @certificate: (transfer none)
 * @user_data: (allow-none): 
 *
 * Prototype for the callback passed to owr_get_capture_sources()
 */


void owr_crypto_create_crypto_data(OwrCryptoDataCallback callback)
{

  GClosure *closure;

  g_return_if_fail(callback);

  closure = g_cclosure_new(G_CALLBACK(callback), NULL, NULL);
  g_closure_set_marshal(closure, g_cclosure_marshal_generic);

  X509 *cert;

  X509_NAME *name = NULL;


  EVP_PKEY *key_pair;
 
  RSA *rsa;

  #define GST_DTLS_BIO_BUFFER_SIZE 4096
  BIO *bio_cert;
  gchar buffer_cert[GST_DTLS_BIO_BUFFER_SIZE] = { 0 };
  gint len_cert;
  gchar *pem_cert = NULL;
  BIO *bio_key;
  gchar buffer_key[GST_DTLS_BIO_BUFFER_SIZE] = { 0 };
  gint len_key;
  gchar *pem_key = NULL;

  bio_cert = BIO_new (BIO_s_mem ());
  bio_key = BIO_new (BIO_s_mem ());

  GString *string_fprint = NULL;
  gchar *char_fprint = NULL;
  guint j;
  const EVP_MD *fprint_type = NULL;
  fprint_type = EVP_sha256();
  guchar fprint[EVP_MAX_MD_SIZE];

  guint fprint_size;

 
  cert = X509_new ();

  key_pair = EVP_PKEY_new ();

  rsa = RSA_generate_key (2048, RSA_F4, NULL, NULL);

  EVP_PKEY_assign_RSA (key_pair, rsa);

  X509_set_version (cert, 2);
  ASN1_INTEGER_set (X509_get_serialNumber (cert), 0);
  X509_gmtime_adj (X509_get_notBefore (cert), 0);
  X509_gmtime_adj (X509_get_notAfter (cert), 31536000L);  /* A year */
  X509_set_pubkey (cert, key_pair);

  name = X509_get_subject_name (cert);
  X509_NAME_add_entry_by_txt (name, "C", MBSTRING_ASC, (unsigned char *) "SE",
      -1, -1, 0);
  X509_NAME_add_entry_by_txt (name, "CN", MBSTRING_ASC,
      (unsigned char *) "OpenWebRTC", -1, -1, 0);
  X509_set_issuer_name (cert, name);
  name = NULL;

  X509_sign (cert, key_pair, EVP_sha256 ());

  if (!X509_digest(cert, fprint_type, fprint, &fprint_size)) {
    g_print("Error creating the certificate fingerprint.\n");
    goto beach;
  }

  string_fprint = g_string_new (NULL);

  for (j=0; j < fprint_size; j++) { 
    g_string_append_printf(string_fprint, "%02X%c", fprint[j], (j+1 == fprint_size) ?'\n':':'); 
  }

  char_fprint = g_string_free(string_fprint, FALSE);

  g_print("char_fprint: %s", char_fprint);


  if (!PEM_write_bio_X509 (bio_cert, (X509 *) cert)) {
    g_print("could not write certificate bio \n");
    goto beach;
  }

  if (!PEM_write_bio_PrivateKey (bio_key, (EVP_PKEY *) key_pair, NULL, NULL, 0, 0, NULL)) {
    g_print("could not write PrivateKey bio \n");
    goto beach;
  }

  len_cert = BIO_read (bio_cert, buffer_cert, GST_DTLS_BIO_BUFFER_SIZE);
  if (!len_cert) {
    goto beach;
  }

  len_key = BIO_read (bio_key, buffer_key, GST_DTLS_BIO_BUFFER_SIZE);
  if (!len_key) {
    goto beach;
  }

  pem_cert = g_strndup (buffer_cert, len_cert);
  pem_key = g_strndup (buffer_key, len_key);

beach:
  BIO_free (bio_cert);
  BIO_free (bio_key);


  GValue params[3] = { G_VALUE_INIT, G_VALUE_INIT, G_VALUE_INIT };

  g_value_init(&params[0], G_TYPE_STRING);
  g_value_set_string(&params[0], pem_key);
  g_value_init(&params[1], G_TYPE_STRING);
  g_value_set_string(&params[1], pem_cert);
  g_value_init(&params[2], G_TYPE_STRING);
  g_value_set_string(&params[2], char_fprint);

  g_closure_invoke(closure, NULL, 3, (const GValue *)&params, NULL);
  g_closure_unref(closure);

  g_value_unset(&params[0]);
  g_value_unset(&params[1]);
  g_value_unset(&params[2]);
}


