// Translate the few MkDocs theme controls that its locale setting leaves in English.
document.addEventListener('DOMContentLoaded', () => {
  const labels = new Map([
    ['Search', 'بحث'], ['Previous', 'السابق'], ['Next', 'التالي'],
    ['Documentation built with', 'بُني التوثيق باستخدام'],
    ['From here you can search these documents. Enter your search terms below.', 'ابحث في صفحات هذا التوثيق بإدخال الكلمات أدناه.'],
    ['No results found', 'لم تُعثر على نتائج']
  ]);
  function translate(root) {
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    let node;
    while ((node = walker.nextNode())) {
      const original = node.nodeValue;
      const trimmed = original.trim();
      if (labels.has(trimmed)) node.nodeValue = original.replace(trimmed, labels.get(trimmed));
    }
    const input = document.querySelector('#mkdocs-search-query');
    if (input) input.placeholder = 'اكتب كلمة البحث هنا';
    const close = document.querySelector('#mkdocs_search_modal button.close');
    if (close) close.setAttribute('aria-label', 'إغلاق');
  }
  translate(document.body);
  const modal = document.querySelector('#mkdocs_search_modal');
  if (modal) new MutationObserver(() => translate(modal)).observe(modal, {childList:true, subtree:true});
});
