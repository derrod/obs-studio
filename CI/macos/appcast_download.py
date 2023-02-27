import os
import sys

import requests
import xmltodict


def download_build(url):
    print(f'Downloading build "{url}"...')
    filename = url.rpartition('/')[2]
    r = requests.get(url)
    if r.status_code == 200:
        with open(f'artifacts/{filename}', 'wb') as f:
            f.write(r.content)
    else:
        print(f'Build download failed, status code: {r.status_code}')
        sys.exit(1)


def read_appcast(url, fallback):
    r = requests.get(url)
    if r.status_code == 404:
        print('Using fallback Appcast URL...')

        r = requests.get(fallback)
        if r.status_code != 200:
            print(f'Appcast download failed, status code: {r.status_code}')
            sys.exit(1)

    filename = url.rpartition('/')[2]
    with open(f'builds/{filename}', 'wb') as f:
        f.write(r.content)

    appcast = xmltodict.parse(r.content, force_list=('item',))

    dl = 0
    for item in appcast['rss']['channel']['item']:
        channel = item.get('sparkle:channel', 'stable')
        if channel != target_branch:
            continue

        if dl == max_old_vers:
            break
        download_build(item['enclosure']['@url'])
        dl += 1


if __name__ == '__main__':
    appcast_urls = {
        'x86_64': (
            'https://obsproject.com/osx_update/updates_x86_64_v2.xml',
            'https://obsproject.com/osx_update/stable/updates_x86_64.xml',
        ),
        'arm64': (
            'https://obsproject.com/osx_update/updates_arm64_v2.xml',
            'https://obsproject.com/osx_update/stable/updates_arm64.xml',
        ),
    }
    target_branch = os.getenv('BRANCH')
    target_arch = os.getenv('ARCH')
    max_old_vers = int(os.getenv('DELTAS'))
    read_appcast(*appcast_urls[target_arch])
