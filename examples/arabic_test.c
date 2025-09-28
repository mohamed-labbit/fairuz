#include "../../util/myutil.h"
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>


wchar_t* _read_file(FILE* _file) {
  setlocale(LC_ALL, "en_US.UTF-8");

  if (_file == NULL)
    return NULL;

  if (fseek(_file, 0, SEEK_END) != 0)
    return NULL;

  long f_size = ftell(_file);

  if (f_size == -1L)
    return NULL;

  if (fseek(_file, 0, SEEK_SET) != 0)
    return NULL;

  if (f_size < 0)
    return NULL;

  char* buf = (char*) malloc((f_size + 1) * sizeof(char));
  if (buf == NULL)
  {
    fprintf(stderr, "Malloc failed\n");
    return NULL;
  }

  size_t read_size = fread(buf, 1, (size_t) f_size, _file);
  if (read_size != (size_t) f_size)
  {
    free(buf);

    if (feof(_file))
      fprintf(stderr, "Unexpected end of file\n");
    else if (ferror(_file))
      fprintf(stderr, "fread failed\n");

    return NULL;
  }

  buf[f_size] = '\0';  // nul terminate

  wchar_t* wbuf = (wchar_t*) malloc((f_size + 1) * sizeof(wchar_t));
  if (wbuf == NULL)
  {
    free(buf);
    return NULL;
  }

  mbstowcs(wbuf, buf, f_size + 1);
  free(buf);
  return wbuf;
}

bool is_arabic(wchar_t c) { return (c >= 0x0600 && c <= 0x06FF); }

int main(void) {
  FILE*       fp  = fopen("code.txt", "r");
  const char* src = read_file(fp);
  if (src == NULL)
    return 1;

  const wchar_t* wsrc = _read_file(fp);
  if (wsrc == NULL)
  {
    free((void*) src);
    return 1;
  }

  printf("%s\n", src);
  printf("\n%ls\n", wsrc);


  printf("src:\n");
  for (size_t i = 0, s = strlen(src); i < s; i++)
  {
    if (is_arabic(src[i]))
      printf("True!\n");
    else
      printf("False!\n");
  }

  printf("wsrc:\n");
  for (size_t i = 0, s = wcslen(wsrc); i < s; i++)
  {
    if (is_arabic(wsrc[i]))
      printf("True!\n");
    else
      printf("False!\n");
  }

  free((void*) src);
  free((void*) wsrc);
  fclose(fp);

  return 0;
}
