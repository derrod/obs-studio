import json
import socket
import ssl
import os
import time
import requests

from random import randbytes
from urllib.parse import urlparse

MINIMUM_PURGE_AGE = 1.75 * 24 * 60 * 60  # slightly less than 2 days
TIMEOUT = 10
SKIPPED_SERVICES = {'YouNow', 'SHOWROOM', 'Dacast'}
SERVICES_FILE = 'plugins/rtmp-services/data/services.json'
PACKAGE_FILE = 'plugins/rtmp-services/data/package.json'
CACHE_FILE = 'other/timestamps.json'

with open(SERVICES_FILE, encoding='utf-8') as services_file:
    services = json.load(services_file)

# attempt to load last check result cache
fail_timestamps = dict()
if os.path.exists(CACHE_FILE):
    print('Found cache file:', CACHE_FILE)
    with open(CACHE_FILE, encoding='utf-8') as check_file:
        fail_timestamps = json.load(check_file)

context = ssl.create_default_context()

start_time = int(time.time())
removed_something = False

# create temporary new list
new_services = services.copy()
new_services['services'] = []


def check_ftl_server(hostname) -> bool:
    """Check if hostname resolves to a valid address - FTL handshake not implemented"""
    try:
        socket.getaddrinfo(hostname, 8084, proto=socket.IPPROTO_UDP)
    except socket.gaierror as e:
        print(f'⚠️ Could not resolve hostname for server: {server["url"]}')
        return False
    else:
        return True


def check_hls_server(uri) -> bool:
    """Check if URL responds with status code < 500 and not 404, indicating that at least there's *something* there"""
    try:
        r = requests.post(uri, timeout=TIMEOUT)
        if r.status_code >= 500 or r.status_code == 404:
            raise Exception(f'Server responded with {r.status_code}')
    except Exception as e:
        print(f'⚠️ Could not connect to HLS server: {server["url"]} (Exception: {e})')
        return False
    else:
        return True


def check_rtmp_server(uri) -> bool:
    """Try connecting and sending a RTMP handshake (with SSL if necessary)"""
    parsed = urlparse(uri)
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
        return False
    else:
        return True


for service in services['services']:
    # skip services that do custom stuff that we can't easily check
    if service['name'] in SKIPPED_SERVICES:
        new_services['services'].append(service)
        continue

    service_type = service.get('recommended', {}).get('output', 'rtmp_output')
    if service_type not in {'rtmp_output', 'ffmpeg_hls_muxer', 'ftl_output'}:
        print('Unknown service type:', service_type)
        new_services['services'].append(service)
        continue
    
    # create a copy to mess with
    new_service = service.copy()
    new_service['servers'] = []

    # run checks for all the servers, and store results in timestamp cache
    for server in service['servers']:        
        if service_type == 'ftl_output':
            is_ok = check_ftl_server(server['url'])
        elif service_type == 'ffmpeg_hls_muxer':
            is_ok = check_hls_server(server['url'])
        else:  # rtmp
            is_ok = check_rtmp_server(server['url'])
        
        if not is_ok:
            if ts := fail_timestamps.get(server['url'], None):
                if (delta := start_time - ts) >= MINIMUM_PURGE_AGE:
                    print(f'🗑️ Purging server "{server["url"]}", it has been unresponsive '
                          f'for {round(delta/60/60/24)} days.')
                    # continuing here means not adding it to the new list, thus dropping it
                    continue
            else:
                fail_timestamps[server['url']] = start_time
        elif is_ok and server['url'] in fail_timestamps:
            # remove timestamp of failed check if server is back
            delta = start_time - fail_timestamps[server['url']]
            print(f'💡 Server "{server["url"]}" is back after {round(delta/60/60/24)} days!')
            del fail_timestamps[server['url']]

        new_service['servers'].append(server)

    if (diff := len(service['servers']) - len(new_service['servers'])) > 0:
        print(f'ℹ️ Removed {diff} server(s) from {service["name"]}')
        removed_something = True

    # remove services with no valid servers
    if not new_service['servers']:
        print(f'💀 Service "{service["name"]}" has no valid servers left, removing!')
        continue

    new_services['services'].append(new_service)

# write cache file
os.makedirs('other', exist_ok=True)
print('Writing cache file')
with open(CACHE_FILE, 'w', encoding='utf-8') as cache_file:
    json.dump(fail_timestamps, cache_file)

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
