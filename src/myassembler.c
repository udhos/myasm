
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

const char *prog_name;

static void show_usage(FILE *out) {
  fprintf(out,
          "usage: %s [-h] filename\n",
	  prog_name);
}

void do_cmd(const char *label, const char *cmd, const char *arg, int line_num) {
  fprintf(stderr, "%s: do_cmd: line_num=%03d label=[%-15s] cmd=[%-10s] arg=[%s]\n",
	  prog_name, line_num, label, cmd, arg);
}

char *first_space(char *str) {
  for (;;++str) {
    int c = *str;
    if (c == '\0')
      return NULL;
    if (isspace(c))
      break;
  }
  return str;
}

char *first_non_space(char *str) {
  for (;;++str) {
    int c = *str;
    if (c == '\0')
      return NULL;
    if (isspace(c))
      continue;
    break;
  }
  return str;
}

static void parse_line(const char *filename, const char* line_orig, int line_num) {
#if 0
  fprintf(stderr, "%s: filename=%s line_num=%d line=[%s]n",
	  prog_name, filename, line_num, line_orig);
#endif

  char line_tmp[1000];

  /*
    line_orig: raw line as read from input line (good for error reporting)
    line_tmp: temporary line buffer we will use for parsing

    here we copy from line_orig into line_tmp
  */
  int line_size = strlen(line_orig);
  if (line_size >= sizeof(line_tmp)) {
    fprintf(stderr, "%s: line buffer overflow: line_size=%d >= buffer_size=%d at line_num=%d\n",
	    prog_name, line_size, sizeof(line_tmp), line_num);
    exit(1);
  }
  memcpy(line_tmp, line_orig, line_size);
  line_tmp[line_size] = '\0';

  assert(line_size == strlen(line_tmp));

  //fprintf(stderr, "B strlen=%d\n", strlen(line_tmp));

  /* cut off comments */
  char *comment = strchr(line_tmp, ';');
  if (comment != NULL) {
    *comment = '\0'; /* force string termination at comment char */
    line_size = comment - line_tmp;
  }

  /* remove blanks from line_tmp ending */
  for (; line_size > 0; --line_size) {
    int last = line_size - 1;
    int c = line_tmp[last];
    if (isspace(c) || isblank(c)) {
      line_tmp[last] = '\0';
      continue;
    }
    break;
  }

  //fprintf(stderr, "A strlen=%d\n", strlen(line_tmp));

  /* now we parse the line_tmp buffer */

  char *label = NULL;
  char *cmd = NULL;
  char *arg = NULL;

  char *first = first_non_space(line_tmp);
  if (first == NULL) {
    /* blank line */
    return;
  }

  char *first_end = first_space(first);
  if (first_end != NULL) {
    *first_end = '\0';
  }

  int first_size = strlen(first);
  if (first[first_size-1] == ':') {
    label = first;
  }
  else {
    cmd = first;
  }

  if (first_end != NULL) {
    char *sec = first_non_space(first_end + 1);
    if (sec != NULL) {
      if (cmd == NULL) {
	cmd = sec;

	char *sec_end = first_space(sec);
	if (sec_end != NULL) {
	  *sec_end = '\0';
	  arg = first_non_space(sec_end + 1);
	}
      }
      else {
	arg = sec;
      }
    }
  }

  do_cmd(label, cmd, arg, line_num);
}

static void assemble(const char *filename) {
  FILE *input;
  char buf[1000];
  int err;

  input = fopen(filename, "r");  /* open for reading */
  if (input == NULL) {
    err = errno;
    fprintf(stderr, "%s: could not open: %s: errno=%d: %s\n",
	    prog_name, filename, err, strerror(err));
    exit(1);
  }

  int line_num = 0;
  while(fgets(buf, sizeof(buf), input) != NULL) {
    ++line_num;
    parse_line(filename, buf, line_num);
  }

  /* have we hit an end-of-file ? */
  err = errno;
  if (!feof(input)) {
    fprintf(stderr, "%s: error reading: %s: errno=%d: %s\n",
	    prog_name, filename, err, strerror(err));
    exit(1);
  }

  fclose(input);
}

int main(int argc, char *argv[]) {
  int i;
  const char *filename = NULL;

  prog_name = argv[0];

  /* scan command-line arguments */
  for (i = 1; i < argc; ++i) {
    const char *arg = argv[i];

    if (!strcmp(arg, "-h")) {
      show_usage(stdout);
      exit(0);
    }

    if (filename != NULL) {
      fprintf(stderr, "%s: filename redefinition old=%s new=%s\n",
	      prog_name, filename, arg);
      exit(1);
    }

    filename = arg;
  }

  if (filename == NULL) {
      fprintf(stderr, "%s: missing filename\n",
	      prog_name);
    show_usage(stdout);
    exit(1);
  }

  assemble(filename); /* real work */

  exit(0);
}
