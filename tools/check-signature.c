/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <openssl/pem.h>
#include <stdint.h>
#include <glib.h>
#include <string.h>
#include <errno.h>

static const char *option_key_file = NULL;
static const char *option_data_file = NULL;
static const char *option_out_file = NULL;

#define EXIT_ARGUMENT_ERROR 2
#define EXIT_IO_ERROR 3
#define EXIT_CRYPT_ERROR 4
#define EXIT_FORMAT_ERROR 5
#define EXIT_INVALID_SIGNATURE_ERROR 6

/* The data file is prefixed with a fixed size RSA signature. The
 * signature signs the SHA256 sum of the remainder of the file */
#define SIGNATURE_SIZE 128

typedef enum {
    CRYPT_ERROR_INIT,
    CRYPT_ERROR_HASH,
    CRYPT_ERROR_VERIFY,
    CRYPT_ERROR_READ_KEY,
    CRYPT_ERROR_INVALID_SIGNATURE
} CryptError;

#define CRYPT_ERROR (crypt_error_quark())
G_DEFINE_QUARK(crypt - error - quark, crypt_error);

static GOptionEntry options[] = {
    { "key",            'k',                   0,    G_OPTION_ARG_STRING,
      &option_key_file, "The public key file", "KEY" },
    { "data",            'd',             0,     G_OPTION_ARG_STRING,
      &option_data_file, "The data file", "DATA" },
    { "out",
      'o',
      0,
      G_OPTION_ARG_STRING,
      &option_out_file,
      "Location to place the extracted data from the data file",
      "OUT" },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
};

static gboolean
hash_stream(FILE *stream, uint8_t *hash, GError **error)
{
    uint8_t buf[1024];
    SHA256_CTX ctx;
    gboolean ret = TRUE;

    if (!SHA256_Init(&ctx)) {
        g_set_error(error,
                    CRYPT_ERROR,
                    CRYPT_ERROR_INIT,
                    "Failed to initialise an SHA256 context");
        ret = FALSE;
    } else {
        int final_result;

        while (TRUE) {
            size_t got = fread(buf, 1, sizeof(buf), stream);

            if (!SHA256_Update(&ctx, buf, got)) {
                g_set_error(error,
                            CRYPT_ERROR,
                            CRYPT_ERROR_HASH,
                            "Error calculating the SHA256 hash");
                ret = FALSE;
            } else if (got != sizeof(buf)) {
                if (ferror(stream)) {
                    g_set_error_literal(error,
                                        G_FILE_ERROR,
                                        g_file_error_from_errno(errno),
                                        strerror(errno));
                    ret = FALSE;
                }

                break;
            }
        }

        /* I think we have to call this regardless of whether there was
         * an error in order to clear up the resources */
        final_result = SHA256_Final((unsigned char *)hash, &ctx);

        if (!final_result && ret) {
            g_set_error(error,
                        CRYPT_ERROR,
                        CRYPT_ERROR_HASH,
                        "Error calculating the SHA256 hash");
            ret = FALSE;
        }
    }

    return ret;
}

static gboolean
process_arguments(int *argc, char ***argv, GError **error)
{
    GOptionContext *context;
    gboolean ret;
    GOptionGroup *group;

    group = g_option_group_new(NULL, /* name */
                               NULL, /* description */
                               NULL, /* help_description */
                               NULL, /* user_data */
                               NULL /* destroy notify */);
    g_option_group_add_entries(group, options);
    context = g_option_context_new("- Verifies a signed data file "
                                   "with the given key");
    g_option_context_set_main_group(context, group);
    ret = g_option_context_parse(context, argc, argv, error);
    g_option_context_free(context);

    if (ret) {
        if (*argc > 1) {
            g_set_error(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_UNKNOWN_OPTION,
                        "Unknown option '%s'",
                        (*argv)[1]);
            ret = FALSE;
        } else if (option_data_file == NULL || option_data_file == NULL) {
            g_set_error(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        "The -k and -d options are required. See --help");
            ret = FALSE;
        }
    }

    return ret;
}

static int
handle_error(GError *error)
{
    fprintf(stderr, "%s\n", error->message);

    if (error->domain == G_OPTION_ERROR)
        return EXIT_ARGUMENT_ERROR;
    else if (error->domain == CRYPT_ERROR)
        return (error->code == CRYPT_ERROR_INVALID_SIGNATURE
                ? EXIT_INVALID_SIGNATURE_ERROR
                : EXIT_CRYPT_ERROR);
    else if (error->domain == G_FILE_ERROR)
        return EXIT_IO_ERROR;
    else
        return EXIT_FAILURE;
}

static EVP_PKEY *
read_public_key(const char *keyfile, GError **error)
{
    EVP_PKEY *ret = NULL;
    FILE *infile;

    infile = fopen(keyfile, "r");

    if (infile == NULL) {
        g_set_error_literal(error,
                            G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            strerror(errno));
    } else {
        ret = PEM_read_PUBKEY(infile,
                              NULL, /* optional pre-allocated key struct */
                              NULL, /* optional password callback */
                              NULL /* data for callback */);

        if (ret == NULL)
            g_set_error_literal(error,
                                CRYPT_ERROR,
                                CRYPT_ERROR_READ_KEY,
                                "Error reading the public key");

        fclose(infile);
    }

    return ret;
}

static gboolean
verify_signature(const uint8_t *hash,
                 const uint8_t *signature,
                 EVP_PKEY *pubkey,
                 GError **error)
{
    EVP_PKEY_CTX *pkey_ctx = EVP_PKEY_CTX_new(pubkey, NULL);
    gboolean ret = TRUE;

    if (pkey_ctx == NULL) {
        g_set_error_literal(error,
                            CRYPT_ERROR,
                            CRYPT_ERROR_INIT,
                            "Error allocating pkey context");
        ret = FALSE;
    } else {
        if (EVP_PKEY_verify_init(pkey_ctx) <= 0 ||
            EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING) <= 0 ||
            EVP_PKEY_CTX_set_signature_md(pkey_ctx, EVP_sha256()) <= 0) {
            g_set_error_literal(error,
                                CRYPT_ERROR,
                                CRYPT_ERROR_INIT,
                                "Error initialising pkey context");
            ret = FALSE;
        } else {
            int validate_ret = EVP_PKEY_verify(pkey_ctx,
                                               signature,
                                               SIGNATURE_SIZE,
                                               hash,
                                               SHA256_DIGEST_LENGTH);

            if (validate_ret == 0) {
                g_set_error_literal(error,
                                    CRYPT_ERROR,
                                    CRYPT_ERROR_INVALID_SIGNATURE,
                                    "Signature is invalid");
                ret = FALSE;
            } else if (validate_ret != 1) {
                g_set_error_literal(error,
                                    CRYPT_ERROR,
                                    CRYPT_ERROR_VERIFY,
                                    "Error verifying signature");
                ret = FALSE;
            }
        }

        EVP_PKEY_CTX_free(pkey_ctx);
    }

    return ret;
}

static gboolean
write_out_file(FILE *infile, const char *out_filename, GError **error)
{
    gboolean is_stdout;
    FILE *outfile;
    uint8_t buf[1024];
    gboolean ret = TRUE;
    int flush_ret;
    size_t got;

    /* Skip back to the start of the data */
    if (fseek(infile, SIGNATURE_SIZE, SEEK_SET)) {
        g_set_error_literal(error,
                            G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            strerror(errno));
        return FALSE;
    }

    if (!strcmp(out_filename, "-")) {
        is_stdout = TRUE;
        outfile = stdout;
    } else {
        outfile = fopen(out_filename, "wb");

        if (outfile == NULL) {
            g_set_error_literal(error,
                                G_FILE_ERROR,
                                g_file_error_from_errno(errno),
                                strerror(errno));
            return FALSE;
        }

        is_stdout = FALSE;
    }

    while (TRUE) {
        got = fread(buf, 1, sizeof(buf), infile);

        if (fwrite(buf, 1, got, outfile) != got) {
            g_set_error_literal(error,
                                G_FILE_ERROR,
                                g_file_error_from_errno(errno),
                                strerror(errno));
            ret = FALSE;
            break;
        }

        if (got != sizeof(buf)) {
            if (ferror(infile)) {
                g_set_error_literal(error,
                                    G_FILE_ERROR,
                                    g_file_error_from_errno(errno),
                                    strerror(errno));
                ret = FALSE;
            }

            break;
        }
    }

    if (is_stdout)
        flush_ret = fflush(stdout);
    else
        flush_ret = fclose(outfile);

    if (flush_ret == EOF && ret) {
        g_set_error_literal(error,
                            G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            strerror(errno));
        ret = FALSE;
    }

    return ret;
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    int retval = EXIT_SUCCESS;
    EVP_PKEY *pubkey;

    if (!process_arguments(&argc, &argv, &error)) {
        retval = handle_error(error);
        g_clear_error(&error);
    } else if ((pubkey = read_public_key(option_key_file, &error)) == NULL) {
        retval = handle_error(error);
        g_clear_error(&error);
    } else {
        FILE *infile = fopen(option_data_file, "rb");

        if (infile == NULL) {
            fprintf(stderr, "%s: %s\n", option_data_file, strerror(errno));
            retval = EXIT_IO_ERROR;
        } else {
            uint8_t hash[SHA256_DIGEST_LENGTH];
            uint8_t signature[SIGNATURE_SIZE];

            if (fread(signature, 1, sizeof(signature), infile) !=
                sizeof(signature)) {
                if (ferror(infile)) {
                    fprintf(
                        stderr, "%s: %s\n", option_data_file, strerror(errno));
                    retval = EXIT_IO_ERROR;
                } else {
                    fprintf(stderr,
                            "%s: File too short for signature\n",
                            option_data_file);
                    retval = EXIT_FORMAT_ERROR;
                }
            } else if (!hash_stream(infile, hash, &error) ||
                       !verify_signature(hash, signature, pubkey, &error)) {
                retval = handle_error(error);
                g_clear_error(&error);
            } else if (option_out_file &&
                       !write_out_file(infile, option_out_file, &error)) {
                retval = handle_error(error);
                g_clear_error(&error);
            }

            fclose(infile);
        }

        EVP_PKEY_free(pubkey);
    }

    return retval;
}
