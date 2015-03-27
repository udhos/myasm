
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

struct label {
  char *name;
  int offset;
  int line_num;
};

struct label **label_table;
int label_table_cap = 0;
int label_table_size = 0;

struct cmd {
  char *keyword;
  void (*run)(const char *arg, int line_num);
};

const char *prog_name;

static void cmd_db(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_db: arg=[%s]\n", prog_name, arg);
}

static void cmd_equ(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_equ: arg=[%s]\n", prog_name, arg);
}

static void cmd_global(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_global: arg=[%s]\n", prog_name, arg);
}

static void cmd_int(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_int: arg=[%s]\n", prog_name, arg);
}

static void cmd_mov(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_mov: arg=[%s]\n", prog_name, arg);
}

static void cmd_section(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_section: arg=[%s]\n", prog_name, arg);
}

struct cmd cmd_table[] = {
  { "db", cmd_db },
  { "equ", cmd_equ },
  { "global", cmd_global },
  { "int", cmd_int },
  { "mov", cmd_mov },
  { "section", cmd_section }
};

static void show_label_table() {
  int i;
  fprintf(stderr, "%s: label table:\n", prog_name);
  for (i = 0; i < label_table_size; ++i) {
    struct label *lab = label_table[i];
    fprintf(stderr, "%s: label=%-15s offset=%4d line_num=%03d\n",
	    prog_name, lab->name, lab->offset, lab->line_num);
  }
}

struct label* label_find(const char *label_name) {
  int i;
  for (i = 0; i < label_table_size; ++i) {
    if (!strcmp(label_name, label_table[i]->name))
      return label_table[i];
  }
  return NULL;
}

static void label_add(const char *label_name, int line_num) {
  struct label *old = label_find(label_name);
  if (old != NULL) {
    fprintf(stderr, "%s: label=%s redefinition at line_num=%d\n",
	    prog_name, label_name, line_num);
    exit(1);
  }

  assert(label_find(label_name) == NULL);

  /* grow array? */
  if (label_table_size >= label_table_cap) {
    if (label_table_cap < 1)
      label_table_cap = 10;
    else
      label_table_cap <<= 1;
    label_table = realloc(label_table, label_table_cap);
  }

  struct label *new_label = malloc(sizeof(*new_label));
  assert(new_label != NULL);

  new_label->name = strdup(label_name);
  new_label->offset = -1;
  new_label->line_num = line_num;

  /* push label */
  label_table[label_table_size] = new_label;
  ++label_table_size;

  assert(label_find(label_name) != NULL);
}

struct cmd* cmd_find(const char *cmd_name) {
  int i;
  int cmd_table_size = sizeof(cmd_table) / sizeof(struct cmd);
  for (i = 0; i < cmd_table_size; ++i) {
    struct cmd *c = &cmd_table[i];
    if (!strcmp(cmd_name, c->keyword))
      return c;
  }
  return NULL;
}

static void do_cmd(const char *label, const char *cmd, const char *arg, int line_num) {
#if 0
  fprintf(stderr, "%s: do_cmd: line_num=%03d label=[%-15s] cmd=[%-10s] arg=[%s]\n",
	  prog_name, line_num, label, cmd, arg);
#endif

  if (label != NULL) 
    label_add(label, line_num);

  if (cmd == NULL) {
    /* line with label only, no command */
    if (arg != NULL) {
      /* sanity: refuse arg without command */
      fprintf(stderr, "%s: internal failure: missing command arg=[%s] at line_num=%d\n",
	      prog_name, arg, line_num);
    }
    return;
  }

  struct cmd *c = cmd_find(cmd);
  if (c == NULL) {
    fprintf(stderr, "%s: unknown keyword=%s at line_num=%d\n",
	    prog_name, cmd, line_num);
    exit(1);
  }

  c->run(arg, line_num);
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

static void scan_input(const char *filename) {
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

static void show_usage(FILE *out) {
  fprintf(out,
          "usage: %s [-h] filename\n",
	  prog_name);
}

int main(int argc, char *argv[]) {
  int i;
  const char *filename = NULL;

  prog_name = argv[0];

  assert(sizeof(cmd_table) / sizeof(struct cmd) == 6);

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

  scan_input(filename); /* parse input file */
  show_label_table();

  exit(0);
}
