# VERSION: 3.0
# AUTHORS: nKlido
# Local fix 3.0: the HTML scraper no longer matches solidtorrents.to; use its
# public JSON API (/api/v1/search) instead and build magnet links from infohash.

# LICENSING INFORMATION
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import json
from datetime import datetime
from urllib.parse import quote, unquote

from helpers import retrieve_url
from novaprinter import prettyPrinter


class solidtorrents:
    url = 'https://solidtorrents.to'
    name = 'Solid Torrents'
    supported_categories = {'all': 'all', 'music': 'Audio', 'books': 'eBook'}

    TRACKERS = ''.join(
        '&tr=' + quote(t, safe='') for t in (
            'udp://tracker.opentrackr.org:1337/announce',
            'udp://open.stealth.si:80/announce',
            'udp://tracker.torrent.eu.org:451/announce',
            'udp://exodus.desync.com:6969/announce',
            'udp://tracker.openbittorrent.com:6969/announce',
        ))

    def search(self, what: str, cat: str = 'all') -> None:
        category = self.supported_categories[cat]
        query = quote(unquote(what))
        for page in range(1, 4):
            raw = retrieve_url(f'{self.url}/api/v1/search?q={query}&category={category}'
                               f'&sort=seeders&page={page}')
            try:
                data = json.loads(raw)
            except ValueError:
                return
            results = data.get('results') or []
            for item in results:
                try:
                    stamp = int(datetime.fromisoformat(
                        item['updatedAt'].replace('Z', '+00:00')).timestamp())
                except (KeyError, ValueError):
                    stamp = -1
                prettyPrinter({
                    'link': f"magnet:?xt=urn:btih:{item['infohash']}&dn={quote(item['title'])}{self.TRACKERS}",
                    'name': item['title'],
                    'size': str(item.get('size', -1)),
                    'seeds': int(item.get('seeders', 0)),
                    'leech': int(item.get('leechers', 0)),
                    'engine_url': self.url,
                    'desc_link': f"{self.url}/torrents/{item['id']}",
                    'pub_date': stamp,
                })
            if not (data.get('pagination') or {}).get('hasNext') or len(results) < 20:
                break
