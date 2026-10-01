// Highlight Fairuz examples without changing their text or copy order.
document.addEventListener('DOMContentLoaded', () => {
  const keywords = new Set(['دالة','اذا','غيره','طالما','لكل','في','ارجع','اخرج','اكمل','نوع','هذا','استورد','من','باسم','تاكد','صحيح','خطا','عدم','و','او','ليس']);
  const escape = s => s.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
  const token = /#[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_\u0621-\u063a\u0641-\u064a][A-Za-z_0-9\u0621-\u063a\u0640-\u064a\u0660-\u0669]*|[0-9\u0660-\u0669]+(?:\.[0-9]+)?|\*\*|:=|[+*/%=<>!-]+/gu;
  document.querySelectorAll('pre code.language-fa').forEach(code => {
    const source = code.textContent;
    let html = '', cursor = 0;
    for (const match of source.matchAll(token)) {
      html += escape(source.slice(cursor, match.index));
      const value = match[0];
      const kind = value[0] === '#' ? 'comment' : value[0] === '"' || value[0] === "'" ? 'string' : keywords.has(value) ? 'keyword' : /^[0-9\u0660-\u0669]/u.test(value) ? 'number' : /^[+*/%=<>!-]/.test(value) || value === ':=' ? 'operator' : '';
      const rendered = kind ? `<span class="fa-${kind}">${escape(value)}</span>` : escape(value);
      html += /^[A-Za-z_\u0621-\u063a\u0641-\u064a]/u.test(value) && /[\u0621-\u064a]/u.test(value)
        ? `<bdi dir="rtl">${rendered}</bdi>` : rendered;
      cursor = match.index + value.length;
    }
    code.innerHTML = html + escape(source.slice(cursor));
  });
});
