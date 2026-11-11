# VERSION: 1.5
# AUTHORS: BurningMop (burning.mop@yandex.com)
# Local fix 1.4: adapted to the current therarbg.com layout (title no longer
# starts with "Search for", rows are <tr class="list-entry">) and magnet
# links are resolved concurrently so the search finishes in a few seconds.

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

import re
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime

from helpers import download_file, retrieve_url
from novaprinter import prettyPrinter


class therarbg(object):
    url = 'https://therarbg.com'
    name = 'The RarBg'
    supported_categories = {
        'all': 'All',
        'movies': 'Movies',
        'tv': 'TV',
        'music': 'Music',
        'games': 'Games',
        'anime': 'Anime',
        'software': 'Apps'
    }

    MAX_PAGES = 2
    row_regex = re.compile(r'<tr class="list-entry">(.*?)</tr>', re.S)
    cell_regex = re.compile(r'<td class="(\w+)">(.*?)</td>', re.S)
    link_regex = re.compile(r'<td class="n"><a href="([^"]+)" title="([^"]*)"', re.S)
    magnet_regex = re.compile(r'href=["\'](magnet:[^"\']+)["\']')
    tag_regex = re.compile(r'<[^>]+>')

    def download_torrent(self, info):
        print(download_file(info))

    def getPageUrl(self, what, cat, page):
        if cat != 'All':
            return f'{self.url}/get-posts/order:-se:category:{cat}:keywords:{what}/?page={page}'
        return f'{self.url}/get-posts/order:-se:keywords:{what}/?page={page}'

    def _text(self, html):
        return self.tag_regex.sub('', html).strip()

    def _parse_rows(self, html):
        rows = []
        for row_html in self.row_regex.findall(html):
            link = self.link_regex.search(row_html)
            if not link:
                continue
            cells = dict(self.cell_regex.findall(row_html))
            name = link.group(2) or self._text(link.group(0))
            category = self._text(cells.get('k', ''))
            ts = -1
            try:
                ts = int(datetime.strptime(self._text(cells.get('d', '')), '%Y-%m-%d').timestamp())
            except ValueError:
                pass
            rows.append({
                'desc_link': self.url + link.group(1),
                'name': f'{name} ({category})' if category else name,
                'size': self._text(cells.get('s', '')).replace(',', '.').replace('\xa0', ' '),
                'seeds': self._text(cells.get('se', '0')) or '0',
                'leech': self._text(cells.get('le', '0')) or '0',
                'engine_url': self.url,
                'pub_date': ts,
            })
        return rows

    def _resolve(self, row):
        try:
            page = retrieve_url(row['desc_link'])
            match = self.magnet_regex.search(page)
            if match:
                row['link'] = match.group(1)
                prettyPrinter(row)
        except Exception:  # noqa: BLE001
            pass

    def search(self, what, cat='all'):
        category = self.supported_categories[cat]
        for page in range(1, self.MAX_PAGES + 1):
            rows = self._parse_rows(retrieve_url(self.getPageUrl(what, category, page)))
            if not rows:
                break
            with ThreadPoolExecutor(max_workers=4) as pool:
                list(pool.map(self._resolve, rows))
            if len(rows) < 40:
                break
