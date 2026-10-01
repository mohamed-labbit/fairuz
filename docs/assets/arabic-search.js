// MkDocs' default Lunr tokenizer does not index Arabic words. Add a literal
// substring fallback for queries containing Arabic text.
document.addEventListener('DOMContentLoaded', async () => {
  const input = document.querySelector('#mkdocs-search-query');
  const results = document.querySelector('#mkdocs-search-results');
  const script = document.querySelector('script[src*="arabic-search.js"]');
  if (!input || !results || !script) return;
  const indexURL = new URL('../search/search_index.json', script.src);
  let docs;
  try { docs = (await (await fetch(indexURL)).json()).docs; } catch (_) { return; }
  const base = new URL('../', script.src);
  function render() {
    const query = input.value.trim().toLocaleLowerCase();
    if (!/[\u0600-\u06ff]/u.test(query)) return;
    const matches = docs.filter(d => (d.title + ' ' + d.text).toLocaleLowerCase().includes(query))
      .sort((a, b) => Number(b.title.toLocaleLowerCase().includes(query)) - Number(a.title.toLocaleLowerCase().includes(query)))
      .slice(0, 12);
    results.replaceChildren();
    if (!matches.length) { results.textContent = 'No results found'; return; }
    for (const match of matches) {
      const item = document.createElement('div');
      item.className = 'search-result';
      const heading = document.createElement('h3');
      const link = document.createElement('a');
      link.href = new URL(match.location, base);
      link.textContent = match.title;
      heading.append(link);
      const excerpt = document.createElement('p');
      excerpt.textContent = match.text.slice(0, 180);
      item.append(heading, excerpt);
      results.append(item);
    }
  }
  input.addEventListener('input', () => setTimeout(render, 150));
});
