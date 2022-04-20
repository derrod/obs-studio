import json
import socket
import ssl
import requests

from random import randbytes
from urllib.parse import urlparse

TIMEOUT = 10
SKIPPED_SERVICES = {'YouNow', 'SHOWROOM', 'Dacast'}
SERVICES_FILE = 'plugins/rtmp-services/data/services.json'
PACKAGE_FILE = 'plugins/rtmp-services/data/package.json'

with open(SERVICES_FILE, encoding='utf-8') as services_file:
    services = json.load(services_file)

context = ssl.create_default_context()

removed_something = False

# create temporary new list
new_services = services.copy()
new_services['services'] = []

for service in services['services']:
    # skip services that do custom stuff that we can't easily check
    if service['name'] in SKIPPED_SERVICES:
        new_services['services'].append(service)
        continue

    new_service = service.copy()
    new_service['servers'] = []

    service_type = service.get('recommended', {}).get('output', 'rtmp')
    if service_type == 'ftl_output':
        # FTL - just check if the name resolves
        for server in service['servers']:
            try:
                socket.getaddrinfo(server['url'], 8084, proto=socket.IPPROTO_UDP)
            except socket.gaierror as e:
                print(f'⚠️ Could not resolve hostname for server: {server["url"]}')
            else:
                new_service['servers'].append(server)

    elif service_type == 'ffmpeg_hls_muxer':
        # HLS - try connecting with requests, ignore error if < 500
        for server in service['servers']:
            try:
                r = requests.post(server['url'], timeout=TIMEOUT)
                if r.status_code >= 500:
                    raise Exception(f'Server responded with {r.status_code}')
            except Exception as e:
                print(f'⚠️ Could not connect to HLS server: {server["url"]} (Exception: {e})')
            else:
                new_service['servers'].append(server)

    else:
        # RTMP(S) - try sending payload and check if valid handshake comes back
        for server in service['servers']:
            parsed = urlparse(server['url'])
            hostname, port = parsed.netloc.partition(':')[::2]
            port = int(port) if port else 1935

            try:
                recv = b''
                with socket.create_connection((hostname, port), timeout=TIMEOUT) as sock:
                    # RTMP handshake is \x03 + 4 bytes time (can be 0) + 4 zero bytes + 1528 bytes random
                    handshake = b'\x03\x00\x00\x00\x00\x00\x00\x00\x00' + randbytes(1528)
                    if parsed.scheme == 'rtmps':
                        with context.wrap_socket(sock, server_hostname=hostname) as ssock:
                            ssock.sendall(handshake)
                            while True:
                                _tmp = ssock.recv(4096)
                                recv += _tmp
                                if len(recv) >= 1536 or not _tmp:
                                    break
                    else:
                        sock.sendall(handshake)
                        while True:
                            _tmp = sock.recv(4096)
                            recv += _tmp
                            if len(recv) >= 1536 or not _tmp:
                                break

                if len(recv) < 1536 or recv[0] != 3:
                    raise ValueError('Invalid RTMP handshake received from server')

            except Exception as e:
                print(f'⚠️ Connection to server failed: {server["url"]} (Exception: {e})')
            else:
                new_service['servers'].append(server)

    # remove services with no valid servers
    if (diff := len(service['servers']) - len(new_service['servers'])) > 0:
        print(f'ℹ️ Removed {diff} server(s) from {service["name"]}')
        removed_something = True

    if not new_service['servers']:
        print(f'💀 Service "{service["name"]}" has no valid servers left, removing!')
        continue

    new_services['services'].append(new_service)

if removed_something:
    print('Writing new services/package file.')
    with open(SERVICES_FILE, 'w', encoding='utf-8') as services_file:
        json.dump(new_services, services_file, indent=4, ensure_ascii=False)

    # increment package version
    with open(PACKAGE_FILE, encoding='utf-8') as package_file:
        package = json.load(package_file)
    package['version'] += 1
    package['files'][0]['version'] += 1
    with open(PACKAGE_FILE, 'w', encoding='utf-8') as package_file:
        json.dump(package, package_file, indent=4)
