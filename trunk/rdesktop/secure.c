/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Protocol services - RDP encryption and licensing
   Copyright (C) Matthew Chapman 1999-2002

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "rdesktop.h"

#ifdef WITH_OPENSSL
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#else
#include "crypto/rc4.h"
#include "crypto/md5.h"
#include "crypto/sha.h"
#include "crypto/bn.h"
#endif

extern char hostname[16];
extern int width;
extern int height;
extern int keylayout;
extern BOOL encryption;
extern BOOL licence_issued;
extern BOOL use_rdp5;
extern int server_bpp;

static int rc4_key_len;
static RC4_KEY rc4_decrypt_key;
static RC4_KEY rc4_encrypt_key;
static RSA *server_public_key;

static uint8 sec_sign_key[16];
static uint8 sec_decrypt_key[16];
static uint8 sec_encrypt_key[16];
static uint8 sec_decrypt_update_key[16];
static uint8 sec_encrypt_update_key[16];
static uint8 sec_crypted_random[SEC_MODULUS_SIZE];

uint16 server_rdp_version = 0;

/*
 * General purpose 48-byte transformation, using two 32-byte salts (generally,
 * a client and server salt) and a global salt value used for padding.
 * Both SHA1 and MD5 algorithms are used.
 */
void
sec_hash_48(uint8 * out, uint8 * in, uint8 * salt1, uint8 * salt2, uint8 salt)
{
	uint8 shasig[20];
	uint8 pad[4];
	SHA_CTX sha;
	MD5_CTX md5;
	int i;

	for (i = 0; i < 3; i++)
	{
		memset(pad, salt + i, i + 1);

		SHA1_Init(&sha);
		SHA1_Update(&sha, pad, i + 1);
		SHA1_Update(&sha, in, 48);
		SHA1_Update(&sha, salt1, 32);
		SHA1_Update(&sha, salt2, 32);
		SHA1_Final(shasig, &sha);

		MD5_Init(&md5);
		MD5_Update(&md5, in, 48);
		MD5_Update(&md5, shasig, 20);
		MD5_Final(&out[i * 16], &md5);
	}
}

/*
 * Weaker 16-byte transformation, also using two 32-byte salts, but
 * only using a single round of MD5.
 */
void
sec_hash_16(uint8 * out, uint8 * in, uint8 * salt1, uint8 * salt2)
{
	MD5_CTX md5;

	MD5_Init(&md5);
	MD5_Update(&md5, in, 16);
	MD5_Update(&md5, salt1, 32);
	MD5_Update(&md5, salt2, 32);
	MD5_Final(out, &md5);
}

/* Reduce key entropy from 64 to 40 bits */
static void
sec_make_40bit(uint8 * key)
{
	key[0] = 0xd1;
	key[1] = 0x26;
	key[2] = 0x9e;
}

/* Generate a session key and RC4 keys, given client and server randoms */
static void
sec_generate_keys(uint8 * client_key, uint8 * server_key, int rc4_key_size)
{
	uint8 session_key[48];
	uint8 temp_hash[48];
	uint8 input[48];

	/* Construct input data to hash */
	memcpy(input, client_key, 24);
	memcpy(input + 24, server_key, 24);

	/* Generate session key - two rounds of sec_hash_48 */
	sec_hash_48(temp_hash, input, client_key, server_key, 65);
	sec_hash_48(session_key, temp_hash, client_key, server_key, 88);

	/* Store first 16 bytes of session key, for generating signatures */
	memcpy(sec_sign_key, session_key, 16);

	/* Generate RC4 keys */
	sec_hash_16(sec_decrypt_key, &session_key[16], client_key, server_key);
	sec_hash_16(sec_encrypt_key, &session_key[32], client_key, server_key);

	if (rc4_key_size == 1)
	{
		DEBUG(("40-bit encryption enabled\n"));
		sec_make_40bit(sec_sign_key);
		sec_make_40bit(sec_decrypt_key);
		sec_make_40bit(sec_encrypt_key);
		rc4_key_len = 8;
	}
	else
	{
		DEBUG(("rc_4_key_size == %d, 128-bit encryption enabled\n", rc4_key_size));
		rc4_key_len = 16;
	}

	/* Save initial RC4 keys as update keys */
	memcpy(sec_decrypt_update_key, sec_decrypt_key, 16);
	memcpy(sec_encrypt_update_key, sec_encrypt_key, 16);

	/* Initialise RC4 state arrays */
	RC4_set_key(&rc4_decrypt_key, rc4_key_len, sec_decrypt_key);
	RC4_set_key(&rc4_encrypt_key, rc4_key_len, sec_encrypt_key);
}

static uint8 pad_54[40] = {
	54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
	54, 54, 54,
	54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
	54, 54, 54
};

static uint8 pad_92[48] = {
	92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
	92, 92, 92, 92, 92, 92, 92,
	92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
	92, 92, 92, 92, 92, 92, 92
};

/* Output a uint32 into a buffer (little-endian) */
void
buf_out_uint32(uint8 * buffer, uint32 value)
{
	buffer[0] = (value) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
	buffer[2] = (value >> 16) & 0xff;
	buffer[3] = (value >> 24) & 0xff;
}

/* Generate a signature hash, using a combination of SHA1 and MD5 */
void
sec_sign(uint8 * signature, int siglen, uint8 * session_key, int keylen, uint8 * data, int datalen)
{
	uint8 shasig[20];
	uint8 md5sig[16];
	uint8 lenhdr[4];
	SHA_CTX sha;
	MD5_CTX md5;

	buf_out_uint32(lenhdr, datalen);

	SHA1_Init(&sha);
	SHA1_Update(&sha, session_key, keylen);
	SHA1_Update(&sha, pad_54, 40);
	SHA1_Update(&sha, lenhdr, 4);
	SHA1_Update(&sha, data, datalen);
	SHA1_Final(shasig, &sha);

	MD5_Init(&md5);
	MD5_Update(&md5, session_key, keylen);
	MD5_Update(&md5, pad_92, 48);
	MD5_Update(&md5, shasig, 20);
	MD5_Final(md5sig, &md5);

	memcpy(signature, md5sig, siglen);
}

/* Update an encryption key - similar to the signing process */
static void
sec_update(uint8 * key, uint8 * update_key)
{
	uint8 shasig[20];
	SHA_CTX sha;
	MD5_CTX md5;
	RC4_KEY update;

	SHA1_Init(&sha);
	SHA1_Update(&sha, update_key, rc4_key_len);
	SHA1_Update(&sha, pad_54, 40);
	SHA1_Update(&sha, key, rc4_key_len);
	SHA1_Final(shasig, &sha);

	MD5_Init(&md5);
	MD5_Update(&md5, update_key, rc4_key_len);
	MD5_Update(&md5, pad_92, 48);
	MD5_Update(&md5, shasig, 20);
	MD5_Final(key, &md5);

	RC4_set_key(&update, rc4_key_len, key);
	RC4(&update, rc4_key_len, key, key);

	if (rc4_key_len == 8)
		sec_make_40bit(key);
}

/* Encrypt data using RC4 */
static void
sec_encrypt(uint8 * data, int length)
{
	static int use_count;

	if (use_count == 4096)
	{
		sec_update(sec_encrypt_key, sec_encrypt_update_key);
		RC4_set_key(&rc4_encrypt_key, rc4_key_len, sec_encrypt_key);
		use_count = 0;
	}

	RC4(&rc4_encrypt_key, length, data, data);
	use_count++;
}

/* Decrypt data using RC4 */
void
sec_decrypt(uint8 * data, int length)
{
	static int use_count;

	if (use_count == 4096)
	{
		sec_update(sec_decrypt_key, sec_decrypt_update_key);
		RC4_set_key(&rc4_decrypt_key, rc4_key_len, sec_decrypt_key);
		use_count = 0;
	}

	RC4(&rc4_decrypt_key, length, data, data);
	use_count++;
}

static void
reverse(uint8 * p, int len)
{
	int i, j;
	uint8 temp;

	for (i = 0, j = len - 1; i < j; i++, j--)
	{
		temp = p[i];
		p[i] = p[j];
		p[j] = temp;
	}
}

/* Perform an RSA public key encryption operation */
static void
sec_rsa_encrypt(uint8 * out, uint8 * in, int len, uint8 * modulus, uint8 * exponent)
{
	BN_CTX *ctx;
	BIGNUM mod, exp, x, y;
	uint8 inr[SEC_MODULUS_SIZE];
	int outlen;

	reverse(modulus, SEC_MODULUS_SIZE);
	reverse(exponent, SEC_EXPONENT_SIZE);
	memcpy(inr, in, len);
	reverse(inr, len);

	ctx = BN_CTX_new();
	BN_init(&mod);
	BN_init(&exp);
	BN_init(&x);
	BN_init(&y);

	BN_bin2bn(modulus, SEC_MODULUS_SIZE, &mod);
	BN_bin2bn(exponent, SEC_EXPONENT_SIZE, &exp);
	BN_bin2bn(inr, len, &x);
	BN_mod_exp(&y, &x, &exp, &mod, ctx);
	outlen = BN_bn2bin(&y, out);
	reverse(out, outlen);
	if (outlen < SEC_MODULUS_SIZE)
		memset(out + outlen, 0, SEC_MODULUS_SIZE - outlen);

	BN_free(&y);
	BN_clear_free(&x);
	BN_free(&exp);
	BN_free(&mod);
	BN_CTX_free(ctx);
}

/* Initialise secure transport packet */
STREAM
sec_init(uint32 flags, int maxlen)
{
	int hdrlen;
	STREAM s;

	if (!licence_issued)
		hdrlen = (flags & SEC_ENCRYPT) ? 12 : 4;
	else
		hdrlen = (flags & SEC_ENCRYPT) ? 12 : 0;
	s = mcs_init(maxlen + hdrlen);
	s_push_layer(s, sec_hdr, hdrlen);

	return s;
}

/* Transmit secure transport packet */
void
sec_send(STREAM s, uint32 flags)
{
	int datalen;

	s_pop_layer(s, sec_hdr);
	if (!licence_issued || (flags & SEC_ENCRYPT))
		out_uint32_le(s, flags);

	if (flags & SEC_ENCRYPT)
	{
		flags &= ~SEC_ENCRYPT;
		datalen = s->end - s->p - 8;

#if WITH_DEBUG
		DEBUG(("Sending encrypted packet:\n"));
		hexdump(s->p + 8, datalen);
#endif

		sec_sign(s->p, 8, sec_sign_key, rc4_key_len, s->p + 8, datalen);
		sec_encrypt(s->p + 8, datalen);
	}

	mcs_send(s);
}

/* Transfer the client random to the server */
static void
sec_establish_key(void)
{
	uint32 length = SEC_MODULUS_SIZE + SEC_PADDING_SIZE;
	uint32 flags = SEC_CLIENT_RANDOM;
	STREAM s;

	s = sec_init(flags, 76);

	out_uint32_le(s, length);
	out_uint8p(s, sec_crypted_random, SEC_MODULUS_SIZE);
	out_uint8s(s, SEC_PADDING_SIZE);

	s_mark_end(s);
	sec_send(s, flags);
}

/* Output connect initial data blob */
static void
sec_out_mcs_data(STREAM s)
{
	int hostlen = 2 * strlen(hostname);
	int length = 158 + 76 + 12 + 4 + 20;

	if (hostlen > 30)
		hostlen = 30;

	out_uint16_be(s, 5);	/* unknown */
	out_uint16_be(s, 0x14);
	out_uint8(s, 0x7c);
	out_uint16_be(s, 1);

	out_uint16_be(s, (length | 0x8000));	/* remaining length */

	out_uint16_be(s, 8);	/* length? */
	out_uint16_be(s, 16);
	out_uint8(s, 0);
	out_uint16_le(s, 0xc001);
	out_uint8(s, 0);

	out_uint32_le(s, 0x61637544);	/* "Duca" ?! */
	out_uint16_be(s, ((length - 14) | 0x8000));	/* remaining length */

	/* Client information */
	out_uint16_le(s, SEC_TAG_CLI_INFO);
	out_uint16_le(s, 212);	/* length */
	out_uint16_le(s, use_rdp5 ? 4 : 1);	/* RDP version. 1 == RDP4, 4 == RDP5. */
	out_uint16_le(s, 8);
	out_uint16_le(s, width);
	out_uint16_le(s, height);
	out_uint16_le(s, 0xca01);
	out_uint16_le(s, 0xaa03);
	out_uint32_le(s, keylayout);
	out_uint32_le(s, 2600);	/* Client build. We are now 2600 compatible :-) */

	/* Unicode name of client, padded to 32 bytes */
	rdp_out_unistr(s, hostname, hostlen);
	out_uint8s(s, 30 - hostlen);

	out_uint32_le(s, 4);
	out_uint32(s, 0);
	out_uint32_le(s, 12);
	out_uint8s(s, 64);	/* reserved? 4 + 12 doublewords */

	switch (server_bpp)
	{
		case 8:
			out_uint16_le(s, 0xca01);
			break;
		case 15:
			out_uint16_le(s, 0xca02);
			break;
		case 16:
			out_uint16_le(s, 0xca03);
			break;
		case 24:
			out_uint16_le(s, 0xca04);
			break;
	}
	out_uint16(s, 1);

	out_uint32(s, 0);
	out_uint8(s, server_bpp);
	out_uint16_le(s, 0x0700);
	out_uint8(s, 0);
	out_uint32_le(s, 1);
	out_uint8s(s, 64);	/* End of client info */

	out_uint16_le(s, SEC_TAG_CLI_4);
	out_uint16_le(s, 12);
	out_uint32_le(s, 9);
	out_uint32_le(s, 0);

	/* Client encryption settings */
	out_uint16_le(s, SEC_TAG_CLI_CRYPT);
	out_uint16_le(s, 12);	/* length */
	out_uint32_le(s, encryption ? 0x3 : 0);	/* encryption supported, 128-bit supported */
	out_uint32_le(s, 0);	/* Unknown */

	out_uint16_le(s, SEC_TAG_CLI_CHANNELS);
	out_uint16_le(s, 20);	/* length */
	out_uint32_le(s, 1);	/* number of virtual channels */
	out_uint8p(s, "cliprdr", 8);	/* name padded to 8(?) */
	out_uint16(s, 0);
	out_uint16_le(s, 0xc0a0);	/* Flags. Rumours tell this is documented in MSDN. */

	s_mark_end(s);
}

/* Parse a public key structure */
static BOOL
sec_parse_public_key(STREAM s, uint8 ** modulus, uint8 ** exponent)
{
	uint32 magic, modulus_len;

	in_uint32_le(s, magic);
	if (magic != SEC_RSA_MAGIC)
	{
		error("RSA magic 0x%x\n", magic);
		return False;
	}

	in_uint32_le(s, modulus_len);
	if (modulus_len != SEC_MODULUS_SIZE + SEC_PADDING_SIZE)
	{
		error("modulus len 0x%x\n", modulus_len);
		return False;
	}

	in_uint8s(s, 8);	/* modulus_bits, unknown */
	in_uint8p(s, *exponent, SEC_EXPONENT_SIZE);
	in_uint8p(s, *modulus, SEC_MODULUS_SIZE);
	in_uint8s(s, SEC_PADDING_SIZE);

	return s_check(s);
}

static BOOL
sec_parse_x509_key(X509 * cert)
{
	EVP_PKEY *epk = NULL;
	/* By some reason, Microsoft sets the OID of the Public RSA key to
	   the oid for "MD5 with RSA Encryption" instead of "RSA Encryption"

	   Kudos to Richard Levitte for the following (. intiutive .) 
	   lines of code that resets the OID and let's us extract the key. */
	if (OBJ_obj2nid(cert->cert_info->key->algor->algorithm) == NID_md5WithRSAEncryption)
	{
		DEBUG_RDP5(("Re-setting algorithm type to RSA in server certificate\n"));
		cert->cert_info->key->algor->algorithm = OBJ_nid2obj(NID_rsaEncryption);
	}
	epk = X509_get_pubkey(cert);
	if (NULL == epk)
	{
		error("Failed to extract public key from certificate\n");
		return False;
	}

	server_public_key = (RSA *) epk->pkey.ptr;

	return True;
}


/* Parse a crypto information structure */
static BOOL
sec_parse_crypt_info(STREAM s, uint32 * rc4_key_size,
		     uint8 ** server_random, uint8 ** modulus, uint8 ** exponent)
{
	uint32 crypt_level, random_len, rsa_info_len;
	uint32 cacert_len, cert_len;
	X509 *cacert, *server_cert;
	uint16 tag, length;
	uint8 *next_tag, *end;

	in_uint32_le(s, *rc4_key_size);	/* 1 = 40-bit, 2 = 128-bit */
	in_uint32_le(s, crypt_level);	/* 1 = low, 2 = medium, 3 = high */
	if (crypt_level == 0)	/* no encryption */
		return False;
	in_uint32_le(s, random_len);
	in_uint32_le(s, rsa_info_len);

	if (random_len != SEC_RANDOM_SIZE)
	{
		error("random len %d, expected %d\n", random_len, SEC_RANDOM_SIZE);
		return False;
	}

	in_uint8p(s, *server_random, random_len);

	/* RSA info */
	end = s->p + rsa_info_len;
	if (end > s->end)
		return False;

	if (!use_rdp5 || 1 == server_rdp_version)
	{
		DEBUG_RDP5(("We're going for the RDP4-style encryption\n"));
		in_uint8s(s, 12);	/* unknown */

		while (s->p < end)
		{
			in_uint16_le(s, tag);
			in_uint16_le(s, length);

			next_tag = s->p + length;

			switch (tag)
			{
				case SEC_TAG_PUBKEY:
					if (!sec_parse_public_key(s, modulus, exponent))
						return False;
					DEBUG_RDP5(("Got Public key, RDP4-style\n"));

					break;

				case SEC_TAG_KEYSIG:
					/* Is this a Microsoft key that we just got? */
					/* Care factor: zero! */
					/* Actually, it would probably be a good idea to check if the public key is signed with this key, and then store this 
					   key as a known key of the hostname. This would prevent some MITM-attacks. */
					break;

				default:
					unimpl("crypt tag 0x%x\n", tag);
			}

			s->p = next_tag;
		}
	}
	else if (4 == server_rdp_version)
	{
		DEBUG_RDP5(("We're going for the RDP5-style encryption\n"));
		in_uint8s(s, 8);	/* Unknown */

		/* Do da funky X.509 stuffy 

		   "How did I find out about this?  I looked up and saw a
		   bright light and when I came to I had a scar on my forehead
		   and knew about X.500"
		   - Peter Gutman in a early version of 
		   http://www.cs.auckland.ac.nz/~pgut001/pubs/x509guide.txt
		 */

		in_uint32_le(s, cacert_len);
		cacert = d2i_X509(NULL, &(s->p), cacert_len);
		/* Note: We don't need to move s->p here - d2i_X509 is
		   "kind" enough to do it for us */
		if (NULL == cacert)
		{
			error("Couldn't load CA Certificate from server\n");
			return False;
		}

		/* Currently, we don't use the CA Certificate. 
		   FIXME: 
		   *) Verify the server certificate (server_cert) with the 
		   CA certificate.
		   *) Store the CA Certificate with the hostname of the 
		   server we are connecting to as key, and compare it
		   when we connect the next time, in order to prevent
		   MITM-attacks.
		 */

		in_uint32_le(s, cert_len);
		server_cert = d2i_X509(NULL, &(s->p), cert_len);
		if (NULL == server_cert)
		{
			error("Couldn't load Certificate from server\n");
			return False;
		}

		in_uint8s(s, 16);	/* Padding */

		/* Note: Verifying the server certificate must be done here, 
		   before sec_parse_public_key since we'll have to apply
		   serious violence to the key after this */

		if (!sec_parse_x509_key(server_cert))
		{
			DEBUG_RDP5(("Didn't parse X509 correctly\n"));
			return False;
		}
		return True;	/* There's some garbage here we don't care about */
	}
	else
	{
		error("Unknown Server RDP version %d", server_rdp_version);
		return False;
	}
	return s_check_end(s);
}

/* Process crypto information blob */
static void
sec_process_crypt_info(STREAM s)
{
	uint8 *server_random, *modulus, *exponent;
	uint8 client_random[SEC_RANDOM_SIZE];
	uint32 rc4_key_size;
	uint8 inr[SEC_MODULUS_SIZE];

	if (!sec_parse_crypt_info(s, &rc4_key_size, &server_random, &modulus, &exponent))
	{
		DEBUG(("Failed to parse crypt info\n"));
		return;
	}

	DEBUG(("Generating client random\n"));
	/* Generate a client random, and hence determine encryption keys */
	generate_random(inr);
	// This is what the MS client do:
	//      memset(inr, 0, SEC_RANDOM_SIZE);
	// *ARIGL!*
	generate_random(client_random);
	if (NULL != server_public_key)
	{			/* Which means we should use 
				   RDP5-style encryption */

		memcpy(inr + SEC_RANDOM_SIZE, client_random, SEC_RANDOM_SIZE);
		reverse(inr + SEC_RANDOM_SIZE, SEC_RANDOM_SIZE);

		RSA_public_encrypt(SEC_MODULUS_SIZE,
				   inr, sec_crypted_random, server_public_key, RSA_NO_PADDING);

		reverse(sec_crypted_random, SEC_MODULUS_SIZE);

	}
	else
	{			/* RDP4-style encryption */
		sec_rsa_encrypt(sec_crypted_random,
				client_random, SEC_RANDOM_SIZE, modulus, exponent);
	}
	sec_generate_keys(client_random, server_random, rc4_key_size);
}


/* Process SRV_INFO, find RDP version supported by server */
static void
sec_process_srv_info(STREAM s)
{
	in_uint16_le(s, server_rdp_version);
	DEBUG_RDP5(("Server RDP version is %d\n", server_rdp_version));
}


/* Process connect response data blob */
void
sec_process_mcs_data(STREAM s)
{
	uint16 tag, length;
	uint8 *next_tag;
	uint8 len;

	in_uint8s(s, 21);	/* header (T.124 stuff, probably) */
	in_uint8(s, len);
	if (len & 0x80)
		in_uint8(s, len);

	while (s->p < s->end)
	{
		in_uint16_le(s, tag);
		in_uint16_le(s, length);

		if (length <= 4)
			return;

		next_tag = s->p + length - 4;

		switch (tag)
		{
			case SEC_TAG_SRV_INFO:
				sec_process_srv_info(s);
				break;

			case SEC_TAG_SRV_3:
				break;

			case SEC_TAG_SRV_CRYPT:
				sec_process_crypt_info(s);
				break;

			default:
				unimpl("response tag 0x%x\n", tag);
		}

		s->p = next_tag;
	}
}

/* Receive secure transport packet */
STREAM
sec_recv(void)
{
	uint32 sec_flags;
	uint16 channel;
	STREAM s;

	while ((s = mcs_recv(&channel)) != NULL)
	{
		if (encryption || !licence_issued)
		{
			in_uint32_le(s, sec_flags);

			if (sec_flags & SEC_LICENCE_NEG)
			{
				if (sec_flags & SEC_ENCRYPT) {
					DEBUG_RDP5(("Encrypted license detected\n"));
				} 
				licence_process(s);
				continue;
			}

			if (sec_flags & SEC_ENCRYPT)
			{
				in_uint8s(s, 8);	/* signature */
				sec_decrypt(s->p, s->end - s->p);
			}
		}

		if (MCS_GLOBAL_CHANNEL == channel)
		{
			return s;
		}
		else
			rdp5_process_channel(s, channel);

	}

	return NULL;
}

/* Establish a secure connection */
BOOL
sec_connect(char *server, char *username)
{
	struct stream mcs_data;

	/* We exchange some RDP data during the MCS-Connect */
	mcs_data.size = 512;
	mcs_data.p = mcs_data.data = xmalloc(mcs_data.size);
	sec_out_mcs_data(&mcs_data);

	if (!mcs_connect(server, &mcs_data, username))
		return False;

	//      sec_process_mcs_data(&mcs_data);
	if (encryption)
		sec_establish_key();
	xfree(mcs_data.data);
	return True;
}

/* Disconnect a connection */
void
sec_disconnect(void)
{
	mcs_disconnect();
}
