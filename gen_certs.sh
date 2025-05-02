#!/bin/bash
CLIENT_NAME=$1
CA_PRIVATE_KEY=$2

CRT_DIR=main/certs

# generate client key (in pem format)
openssl genrsa -out $CRT_DIR/client.key 2048

# generate a client csr file
openssl req -new -key $CRT_DIR/client.key -subj "/CN=${CLIENT_NAME}" | \
openssl x509 -req -CA $CRT_DIR/ca.crt -CAkey $CA_PRIVATE_KEY -CAcreateserial \
  -out $CRT_DIR/client.crt -days 3650 -sha256 -set_serial 0x$(openssl rand -hex 8)