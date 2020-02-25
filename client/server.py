# Based on https://github.com/eclipse/leshan/issues/374.
# Author: chme@nordicsemi.no
#
# Key is "nordicsecret".
# Public key is "urn:imei:xxxxxxxxxxxxxxx".

import requests
import json

# URL of the server
BASE_URL = "http://vmi36865.contabo.host:8081/"

urn = "urn:imei:352656100003501"
password = "nordicsecret"
url = BASE_URL + "api/bootstrap/" + urn

data = {"servers": {
    "1": {"shortId": 102},
    "3": {"shortId": 1000}},
    "security": {  # bootstrap
    "0": {"uri": "coaps://vmi36865.contabo.host:5784",
          "bootstrapServer": True,
          "securityMode": "PSK",
          "publicKeyOrId": [ord(i) for i in urn],
          "serverPublicKey": [],
          "secretKey": [ord(i) for i in password],
          "smsSecurityMode": "NO_SEC",
          "smsBindingKeyParam": [],
          "smsBindingKeySecret": [],
          "serverSmsNumber": "+3343577911",
          "serverId": "0",
          "clientHoldOffTime": 20},
    # DM server
    "1": {"uri": "coaps://vmi36865.contabo.host:5684",
          "bootstrapServer": False,
          "securityMode": "PSK",
          "publicKeyOrId": [ord(i) for i in urn],
          "serverPublicKey": [],
          "secretKey": [ord(i) for i in password],
          "smsSecurityMode": "NO_SEC",
          "smsBindingKeyParam": [],
          "smsBindingKeySecret": [],
          "serverSmsNumber": "+3343577464",
          "serverId": "102",
          "clientHoldOffTime": 1},
    # data repository server
    "3": {"uri": "coaps://vmi36865.contabo.host:6684",
          "bootstrapServer": False,
          "securityMode": "PSK",
          "publicKeyOrId": [ord(i) for i in urn],
          "serverPublicKey": [],
          "secretKey": [ord(i) for i in password],
          "smsSecurityMode": "NO_SEC",
          "smsBindingKeyParam": [],
          "smsBindingKeySecret": [],
          "serverSmsNumber": "+3343577464",
          "serverId": "1000",
          "clientHoldOffTime": 1}
}}

headers = {'Content-type': 'application/json', 'Accept': 'text/plain'}
r = requests.post(url, data=json.dumps(data), headers=headers)
print(r.status_code)
print(r.content)
