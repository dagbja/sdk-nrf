# Based on https://github.com/eclipse/leshan/issues/374.
# Author: chme@nordicsemi.no
#
# Key is "glennssecret".
# Public key is "urn:imei-msisdn:352656100003501-0011600025".
# Has to be converted to decimal.

import requests
import json

# URL of the server
BASE_URL = "http://vmi36865.contabo.host:8081/"

# The client endpoint here
#url = BASE_URL + "api/bootstrap/urn:imei-msisdn:352656100003501-0011600025"
# "publicKeyOrId": [117, 114, 110, 58, 105, 109, 101, 105, 45, 109, 115, 105, 115, 100, 110, 58, 51, 53, 50, 54, 53, 54, 49, 48, 48, 48, 48, 51, 53, 48, 49, 45, 48, 48, 49, 49, 54, 48, 48, 48, 50, 53],
url = BASE_URL + "api/bootstrap/urn:imei-msisdn:352656100001844-0011600025"


data = {"servers": {
    "1": {"shortId": 102},
    "2": {"shortId": 1000}},
    "security": {  # bootstrap
    "0": {"uri": "coaps://vmi36865.contabo.host:5784",
          "bootstrapServer": True,
          "securityMode": "PSK",
          "publicKeyOrId": [117, 114, 110, 58, 105, 109, 101, 105, 45, 109, 115, 105, 115, 100, 110, 58, 51, 53, 50, 54, 53, 54, 49, 48, 48, 48, 48, 49, 56, 52, 52, 45, 48, 48, 49, 49, 54, 48, 48, 48, 50, 53],
          "serverPublicKey": [],
          "secretKey": [103, 108, 101, 110, 110, 115, 115, 101, 99, 114, 101, 116],
          "smsSecurityMode": "NO_SEC",
          "smsBindingKeyParam": [],
          "smsBindingKeySecret": [],
          "serverSmsNumber": "+3343577911",
          "serverId": "0",
          "clientOldOffTime": 20},
    # DM server
    "1": {"uri": "coaps://vmi36865.contabo.host:5684",
          "bootstrapServer": False,
          "securityMode": "PSK",
          "publicKeyOrId": [117, 114, 110, 58, 105, 109, 101, 105, 45, 109, 115, 105, 115, 100, 110, 58, 51, 53, 50, 54, 53, 54, 49, 48, 48, 48, 48, 49, 56, 52, 52, 45, 48, 48, 49, 49, 54, 48, 48, 48, 50, 53],
          "serverPublicKey": [],
          "secretKey": [103, 108, 101, 110, 110, 115, 115, 101, 99, 114, 101, 116],
          "smsSecurityMode": "NO_SEC",
          "smsBindingKeyParam": [],
          "smsBindingKeySecret": [],
          "serverSmsNumber": "+3343577464",
          "serverId": "102",
          "clientOldOffTime": 1},
    # data repository server
    "2": {"uri": "coaps://vmi36865.contabo.host:6684",
          "bootstrapServer": False,
          "securityMode": "PSK",
          "publicKeyOrId": [117, 114, 110, 58, 105, 109, 101, 105, 45, 109, 115, 105, 115, 100, 110, 58, 51, 53, 50, 54, 53, 54, 49, 48, 48, 48, 48, 49, 56, 52, 52, 45, 48, 48, 49, 49, 54, 48, 48, 48, 50, 53],
          "serverPublicKey": [],
          "secretKey": [103, 108, 101, 110, 110, 115, 115, 101, 99, 114, 101, 116],
          "smsSecurityMode": "NO_SEC",
          "smsBindingKeyParam": [],
          "smsBindingKeySecret": [],
          "serverSmsNumber": "+3343577464",
          "serverId": "1000",
          "clientOldOffTime": 1}
}}

headers = {'Content-type': 'application/json', 'Accept': 'text/plain'}
r = requests.post(url, data=json.dumps(data), headers=headers)
print(r.status_code)
print(r.content)
