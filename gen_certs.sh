#!/bin/bash
CLIENT_NAME=$1
CA_PRIVATE_KEY=$2

CRT_DIR=main/certs

# generate client key (in pem format)
openssl genrsa -out $CRT_DIR/client.key 2048

# generate a client csr file
openssl req -new -key $CRT_DIR/client.key -out $CRT_DIR/client.csr -subj "/CN=${CLIENT_NAME}"

# generate a client crt file (pem format)
openssl x509 -req -in $CRT_DIR/client.csr -CA $CRT_DIR/ca.crt -CAkey $CA_PRIVATE_KEY -CAcreateserial \
  -out $CRT_DIR/client.crt -days 3650 -sha256 -set_serial 0x$(openssl rand -hex 8)