#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

int is_arabic(wchar_t c) {
  // Arabic block (basic)
  if (c >= 0x0600 && c <= 0x06FF)
    return 1;
  // Extended Arabic ranges can be added here if needed
  return 0;
}

int main(void) {
  setlocale(LC_ALL, "");  // enable Unicode in console

  wchar_t c;

  wprintf(L"Enter char (q to quit): ");
  while (wscanf(L" %lc", &c) == 1)
  {
    if (c == L'q')  // quit if 'q'
      break;

    if (is_arabic(c))
      wprintf(L"True!\n");
    else
      wprintf(L"False!\n");

    wprintf(L"Enter char: ");
  }

  return 0;
}