/* Rekta Design, the showcase's service worker. build-site.mjs fills in the files and a version that changes with
   them. Everything the page needs is cached on install, so it works offline; a new version waits until the page
   asks it to take over ("reload" in the update toast), so nothing changes under someone mid-task. */
const VERSION = '__RK_VERSION__';
const FILES = __RK_FILES__;
const CACHE = `rekta-site-${VERSION}`;
self.addEventListener('install', (e) => { e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES))); });
self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys().then((keys) => Promise.all(keys.filter((k) => k.startsWith('rekta-site-') && k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener('message', (e) => { if (e.data === 'skip-waiting') self.skipWaiting(); });
self.addEventListener('fetch', (e) => {
  const r = e.request;
  if (r.method !== 'GET' || new URL(r.url).origin !== self.location.origin) return;
  e.respondWith(caches.match(r, { ignoreSearch: r.mode === 'navigate' }).then((hit) => hit || fetch(r).catch(() => (r.mode === 'navigate' ? caches.match('./') : Response.error()))));
});
