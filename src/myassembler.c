
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#define COMMENT_SEP ';'
#define STRING_SEP '\''
#define ESCAPE_SEP '\\'

struct label {
  char *name;
  int offset;
  int line_num;
};

/* label_strcmp: compare labels ignoring case */
int (*label_strcmp)(const char *s1, const char *s2) = strcasecmp;

struct label **label_table;
int label_table_cap = 0;
int label_table_size = 0;
int label_address_offset = 0;

/* update current address offset according size of generated instruction */
static void address_inc(int size) {
  label_address_offset += size;
}

struct cmd {
  char *keyword;
  void (*run)(const char *arg, int line_num);
};

/* cmd_strcmp: compare keywords ignoring case */
int (*cmd_strcmp)(const char *s1, const char *s2) = strcasecmp;

const char *prog_name;

static void cmd_db(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_db: arg=[%s]\n", prog_name, arg);
  address_inc(1); /* FIXME */
}

static void cmd_equ(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_equ: arg=[%s]\n", prog_name, arg);
  address_inc(2); /* FIXME */
}

static void cmd_global(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_global: arg=[%s]\n", prog_name, arg);
}

static void cmd_int(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_int: arg=[%s]\n", prog_name, arg);
  address_inc(3); /* FIXME */
}

static void cmd_mov(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_mov: arg=[%s]\n", prog_name, arg);
  address_inc(4); /* FIXME */
}

static void cmd_section(const char *arg, int line_num) {
  fprintf(stderr, "%s: cmd_section: arg=[%s]\n", prog_name, arg);
}

/* the command table holds all known keywords, and function pointers to handle them */
struct cmd cmd_table[] = {
  { "db", cmd_db },
  { "equ", cmd_equ },
  { "global", cmd_global },
  { "int", cmd_int },
  { "mov", cmd_mov },
  { "section", cmd_section }
};

/* display label table for debugging */
static void show_label_table() {
  int i;
  fprintf(stderr, "%s: label table:\n", prog_name);
  for (i = 0; i < label_table_size; ++i) {
    struct label *lab = label_table[i];
    fprintf(stderr, "%s: label=%-15s offset=%4d line_num=%03d\n",
	    prog_name, lab->name, lab->offset, lab->line_num);
  }
}

/* search label in label table */
struct label* label_find(const char *label_name) {
  int i;
  for (i = 0; i < label_table_size; ++i) {
    if (!label_strcmp(label_name, label_table[i]->name))
      return label_table[i];
  }
  return NULL;
}

/* save new label into label table */
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
      label_table_cap = 100; /* starts with 100 elements */
    else
      label_table_cap <<= 1; /* then double size when needed */
    label_table = realloc(label_table, label_table_cap);
  }

  struct label *new_label = malloc(sizeof(*new_label));
  assert(new_label != NULL);

  /* new label data */
  new_label->name = strdup(label_name);
  new_label->offset = label_address_offset;
  new_label->line_num = line_num;

  /* append new label to array */
  label_table[label_table_size] = new_label;
  ++label_table_size;

  assert(label_find(label_name) != NULL);
}

/* lookup known keywords in command table */
struct cmd* cmd_find(const char *cmd_name) {
  int i;
  int cmd_table_size = sizeof(cmd_table) / sizeof(struct cmd);
  for (i = 0; i < cmd_table_size; ++i) {
    struct cmd *c = &cmd_table[i];
    if (!cmd_strcmp(cmd_name, c->keyword))
      return c;
  }
  return NULL;
}

/* handle tuple: label,cmd,arg */
static void do_cmd(const char *label, const char *cmd, const char *arg, int line_num) {
#if 0
  fprintf(stderr, "%s: do_cmd: line_num=%03d label=[%-15s] cmd=[%-10s] arg=[%s]\n",
	  prog_name, line_num, label, cmd, arg);
#endif

  /* save label into table, if any */
  if (label != NULL) 
    label_add(label, line_num);

  if (cmd == NULL) {
    /* line with label only, no command */
    if (arg != NULL) {
      /* sanity: refuse arg without command */
      fprintf(stderr, "%s: internal failure: missing command arg=[%s] at line_num=%d\n",
	      prog_name, arg, line_num);
      exit(1);
    }
    return;
  }

  /* lookup command */
  struct cmd *c = cmd_find(cmd);
  if (c == NULL) {
    fprintf(stderr, "%s: unknown keyword=%s at line_num=%d\n",
	    prog_name, cmd, line_num);
    exit(1);
  }

  /* then handle known keyword with its argument */
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

/*
  overwrite spaces from string ending with end-of-string marker ('\0')
  size: string original length
  returns: new string length
*/
static int trim_right(char *buf, int size) {

  for (; size > 0; --size) {
    int last = size - 1;
    int c = buf[last];
    if (isspace(c)) {
      buf[last] = '\0';
      continue;
    }
    break;
  }

  return size;
}

/*
  strip comment from line
  but not if comment delimiter is inside quoted string
 */
static int strip_comment(char *buf, int size) {
  int i;
  int status = 0; /* 0=outside_string 1=inside_string 2=inside_string_escaped_char */

  for (i = 0; i < size; ++i) {
    int c = buf[i];
    if (c == '\0')
      return i; /* found end of string */

    if (status == 0) {
      /* outside string */
      if (c == STRING_SEP) {
	/* found string */
	status = 1;
	continue;
      }
      if (c == COMMENT_SEP) {
	/* found comment separator */
	buf[i] = '\0'; /* force string termination at comment char */
	return i;
      }
      continue;
    }

    if (status == 1) {
      /* inside string */
      if (c == STRING_SEP) {
	/* string end */
	status = 0;
	continue;
      }
      if (c == ESCAPE_SEP) {
	/* found escape char: ignore next char */
	status = 2;
	continue;
      }
      continue;
    }

    if (status == 2) {
      /* inside string but escaped char */
      status = 1; /* back to inside string */
      continue;
    }

    assert(0);
  }

  /* unchanged */

  return size;
}

static void parse_line(const char *input_filename, const char* line_orig, int line_num) {
#if 0
  fprintf(stderr, "%s: input_filename=%s line_num=%d line=[%s]n",
	  prog_name, input_filename, line_num, line_orig);
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

  /* cut off comments */
  line_size = strip_comment(line_tmp, line_size);

  /* remove blanks from line_tmp ending */
  line_size = trim_right(line_tmp, line_size);

  /* now we parse the line_tmp buffer, expecting [label:] [cmd] [arg] */

  char *label = NULL;
  char *cmd = NULL;
  char *arg = NULL;

  /* find first word */
  char *first = first_non_space(line_tmp);
  if (first == NULL) {
    /* no first word: blank line */
    return;
  }

  /* find first word size (since it's space-terminated, not null-terminated) */
  int first_size = -1;
  char *first_end = first_space(first);
  if (first_end != NULL) {
    *first_end = '\0';
    first_size = first_end - first;
  }
  else
    first_size = strlen(first);

  assert(first_size > 0);

  /* first word is label or command? */
  if (first[first_size-1] == ':') {
    /* first word is label */
    label = first;
  }
  else {
    /* first word is command */
    cmd = first;
  }

  /* is there anything after first word? */
  if (first_end != NULL) {
    /* find second word */
    char *sec = first_non_space(first_end + 1);
    if (sec != NULL) {
      /* found second word */
      if (cmd == NULL) {
	/* second word is command */
	cmd = sec;

	char *sec_end = first_space(sec);
	if (sec_end != NULL) {
	  *sec_end = '\0';
	  arg = first_non_space(sec_end + 1);
	  /* arg holds the third word, if any */
	}
      }
      else {
	/* second word is argument */
	arg = sec;
      }
    }
  }

  /* finally handle the tuple: label,cmd,arg */

  do_cmd(label, cmd, arg, line_num);
}

static void scan_input(const char *input_filename) {
  FILE *input;
  char buf[1000];
  int err;

  input = fopen(input_filename, "r");  /* open for reading */
  if (input == NULL) {
    err = errno;
    fprintf(stderr, "%s: could not open: %s: errno=%d: %s\n",
	    prog_name, input_filename, err, strerror(err));
    exit(1);
  }

  /* read lines from input file */
  int line_num = 0;
  while(fgets(buf, sizeof(buf), input) != NULL) {
    ++line_num;
    parse_line(input_filename, buf, line_num);
  }

  /* haven't we hit an end-of-file ? */
  err = errno;
  if (!feof(input)) {
    fprintf(stderr, "%s: error reading: %s: errno=%d: %s\n",
	    prog_name, input_filename, err, strerror(err));
    exit(1);
  }

  fclose(input);
}

static void show_usage(FILE *out) {
  fprintf(out,
          "usage: %s [-h] input_filename\n",
	  prog_name);
}

int main(int argc, char *argv[]) {
  int i;
  const char *input_filename = NULL;

  prog_name = argv[0];

  assert(sizeof(cmd_table) / sizeof(struct cmd) == 6);

  /* scan command-line arguments */
  for (i = 1; i < argc; ++i) {
    const char *arg = argv[i];

    if (!strcmp(arg, "-h")) {
      show_usage(stdout);
      exit(0);
    }

    if (input_filename != NULL) {
      fprintf(stderr, "%s: input_filename redefinition old=%s new=%s\n",
	      prog_name, input_filename, arg);
      exit(1);
    }

    input_filename = arg;
  }

  if (input_filename == NULL) {
      fprintf(stderr, "%s: missing input_filename\n",
	      prog_name);
    show_usage(stdout);
    exit(1);
  }

  scan_input(input_filename);
  show_label_table();

  exit(0);
}
