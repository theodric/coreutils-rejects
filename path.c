/***************************************************************************
 Copyright (C) 2009 Takaaki Ota

 Filename   : path.c
 Version    : 0.1
 Created    : Tue Jul 21 14:36:13 2009 (PDT)
 Revised    : Thu Jul 23 15:14:42 2009 ()
 Purpose    : programming, convenience
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

typedef struct {
  int argc;
  char **argv;
  int i;
  int base, quote;
} context_t;

typedef struct path {
  char *str;
  int is_negative;
  struct path *next;
} path_t;

static path_t *canonicalize_path(path_t *path)
/* get rid of /.. and simplify the path
   the result path->str may become shorter without reallocation
   but this is harmless with minor waste of space */
{
  if(path) {
    const char *dotdot = "/..";
    int dotdot_len = strlen(dotdot);
    char *p = path->str;
    while((p = strstr(p, dotdot)) != NULL) {
      char *q, *r;
      if(p == path->str /* starting from /.. */
         || (p[dotdot_len] && p[dotdot_len] != '/') /* .. is a part of name */
         ) {
        p += dotdot_len;
        continue;
      }
      for(q = p - 1; q > path->str && *q != '/'; q--);
      p += dotdot_len;
      r = q;
      if(r == path->str
         && *r != '/'
         && *p == '/')
        /* keep it relative path */
        p++;
      if(r == path->str
         && *r == '/'
         && *p != '/') {
        /* preserve the root directory */
        r++;
      }
      while((*r++ = *p++));
      p = q;
    }
  }
  return path;
}

static path_t *new_path(const char *s)
/* allocate storage space for a path
   with str initialized with the given string s */
{
  path_t *path = (path_t *)malloc(sizeof(path_t));
  path->is_negative = 0;
  path->next = NULL;
  if((path->str = strdup(s)) == NULL) {
    perror("strdup failed\n");
    exit(1);
  }
  return canonicalize_path(path);
}

static path_t *dup_path(path_t *path)
/* duplicate the path and return */
{
  path_t *p0 = NULL;
  if(path) {
    p0 = new_path(path->str);
    p0->is_negative = path->is_negative;
    p0->next = dup_path(path->next);
  }
  return p0;
}

static void delete_path(path_t *path)
/* free the storage space allocated for this path */
{
  if(path) {
    if(path->str)
      free(path->str);
    delete_path(path->next);
    free(path);
  }
}

static path_t *negate_path(path_t *p0)
/* set negate flag of the path */
{
  if(p0)
    p0->is_negative = !p0->is_negative;
  return p0;
}

static path_t *list_path(path_t *p0, path_t *p1)
/* create a list of the given two paths p0 and p1.
   upon returning from this function p1 now belongs to p0 thus
   p1 should be abandoned without deletion */
{
  path_t **p;
  if(p0 == NULL)
    return p1;
  for(p = &p0->next; *p; p = &(*p)->next);
  *p = p1;
  return p0;
}

static path_t *concatenate_path(path_t *p0, path_t *p1)
/* concatenate two paths or path lists */
{
  if(p0 == NULL)
    return p1;
  if(p1 == NULL)
    return p0;
  if(p0->next || p1->next) {
    /* if either one of them is a list then list them up */
    p0 = list_path(p0, p1);
    p1 = 0;
  } else if(p0->is_negative && p1->is_negative) {
    fprintf(stderr, "double negative makes no sense\n");
    exit(1);
  } else if(p0->is_negative) {
    /* eliminate p0 from the head of p1 */
    char *p, *q;
    if(strstr(p1->str, p0->str) != p1->str) {
      /* p0 doesn't match the head of p1 */
      fprintf(stderr, "\"%s\" is not found at the head of \"%s\"\n",
              p0->str, p1->str);
      exit(2);
    }
    for(p = p1->str, q = p1->str + strlen(p0->str);
        (*p++ = *q++);
        );
    free(p0->str);
    p0->str = p1->str;
    p0->is_negative = 0;
    p1->str = NULL;
  } else if(p1->is_negative) {
    /* eliminate p1 from the tail of p0 */
    int p0_len = strlen(p0->str);
    int p1_len = strlen(p1->str);
    if(strstr(p0->str, p1->str) != p0->str + p0_len - p1_len) {
      fprintf(stderr, "\"%s\" is not found at the tail of \"%s\"\n",
              p1->str, p0->str);
      exit(2);
    }
    p0->str[p0_len - p1_len] = '\0';
  } else {
    /* simply concatenate two paths into one */
    int len, p0_len = strlen(p0->str);
    char *new_str;
    if((len = snprintf(NULL, 0, "%s%s%s",
                       p0->str,
                       p0->str[p0_len - 1] == '/' ? "" : "/",
                       p1->str[0] == '/' ? p1->str + 1 : p1->str)) == -1) {
      /* pre glibc 2.1 demands some more work*/
      int guess;
      char *buf;
      for(guess = 1024, buf = NULL;
          len == -1;
          guess *= 2) {
        if((buf = (char *)realloc(buf, guess)) == NULL) {
          perror("realloc failed\n");
          exit(1);
        }
        len = snprintf(buf, guess, "%s%s%s",
                       p0->str,
                       p0->str[p0_len - 1] == '/' ? "" : "/",
                       p1->str[0] == '/' ? p1->str + 1 : p1->str);
      }
      free(buf);
    }
    if((new_str = (char*)malloc(len + 1)) == NULL) {
      perror("malloc failed\n");
      exit(1);
    }
    snprintf(new_str, len + 1, "%s%s%s",
             p0->str,
             p0->str[p0_len - 1] == '/' ? "" : "/",
             p1->str[0] == '/' ? p1->str + 1 : p1->str);
    free(p0->str);
    p0->str = new_str;
  }
  delete_path(p1);
  return p0;
}

static path_t *print_path(FILE *fp, path_t *p, int base, int quote)
{
  if(p) {
    char *s = p->str;
    if(base) {
      char *p;
      for(p = s + strlen(s) - 1;
          *p == '/' && p > s;
          *p-- = '\0');
      while(*p != '/' && p > s)
        p--;
      if(*p == '/' && p[1]) p++;
      s = p;
    }
    if(quote)
      fprintf(fp, "%c%s%c", quote, s, quote);
    else
      fprintf(fp, "%s", s);
    if(p->next) {
      fputc(' ', fp);
      print_path(fp, p->next, base, quote);
    } else {
      fputc('\n', fp);
    }
  }
  return p;
}

static path_t *eval_list_path(context_t *c);

static path_t *eval_term_path(context_t *c)
{
  if(c->i >= c->argc) {
    return NULL;
  } else if(strcmp(c->argv[c->i], "(") == 0) {
    path_t *path;
    c->i++;
    path = eval_list_path(c);
    if(strcmp(c->argv[c->i], ")") != 0) {
      fprintf(stderr, "missing closing parenthesis before \"%s\"\n", 
c->argv[c->i]);
      exit(3);
    }
    c->i++;
    return path;
  } else {
    return new_path(c->argv[c->i++]);
  }
}

static path_t *eval_unary_path(context_t *c)
{
  if(c->i < c->argc && strcmp(c->argv[c->i], "-") == 0) {
    c->i++;
    return negate_path(eval_unary_path(c));
  } else {
    return eval_term_path(c);
  }
}

static path_t *eval_multiplicative_path(context_t *c)
{
  path_t *p0 = eval_unary_path(c);
  while(c->i < c->argc) {
    if(strcmp(c->argv[c->i], "*") == 0) {
      path_t *p1, *p, *q;
      c->i++;
      p1 = eval_unary_path(c);
      p = p0;
      p0 = NULL;
      /* in the next loop p is nibbled as q and totally consumed */
      while((q = p)) {
        path_t *r, *s;
        p = p->next;
        q->next = NULL;
        s = dup_path(p1);
        /* in the next loop s is nibbled as r and totally consumed */
        while((r = s)) {
          s = s->next;
          r->next = NULL;
          p0 = list_path(p0, concatenate_path(dup_path(q), r));
        }
        delete_path(q);
      }
      delete_path(p1);
    } else {
      return p0;
    }
  }
  return p0;
}

static path_t *eval_additive_path(context_t *c)
{
  path_t *p0 = eval_multiplicative_path(c);
  while(c->i < c->argc) {
    if(strcmp(c->argv[c->i], "+") == 0) {
      c->i++;
      p0 = concatenate_path(p0, eval_multiplicative_path(c));
    } else if(strcmp(c->argv[c->i], "-") == 0) {
      c->i++;
      p0 = concatenate_path(p0, negate_path(eval_multiplicative_path(c)));
    } else {
      return p0;
    }
  }
  return p0;
}

static path_t *eval_list_path(context_t *c)
{
  path_t *p0 = eval_additive_path(c);
  if(c->i < c->argc
     && strcmp(c->argv[c->i], ")") != 0) {
    p0 = list_path(p0, eval_list_path(c));
  }
  return p0;
}

int main(int argc, char **argv)
{
  int base = 0, quote = 0;
  context_t c;
  while(1) {
    int o = getopt(argc, argv, "bqsh");
    if(o == -1)
      break;
    switch(o) {
    case 'b':
      base = 1;
      break;
    case 'q':
      quote = '"';
      break;
    case 's':
      quote = '\'';
      break;
    case 'h':
      fprintf(stderr,
              "Usage: path [OPTION]... [PATH EXPRESSION]\n\
Compute path strings from the given path expression.\n\
\n\
Options:\n\
  -b                         Basename - strip directory and suffix from paths\n\
  -q                         Quote each of the result path string\n\
  -s                         Quote with a single quote character\n\
\n\
Ptha Expression Operators: (in the order of precedence)\n\
\n\
Unary Operators:\n\
-                            Negate\n\
\n\
Binary Operators:\n\
*                            Distribute additive operation\n\
+, -                         Concatinate, Subtract\n\
SPC                          Construct a path list\n\
\n\
Example expressions and evaluation result:\n\
\n\
path /etc/system/../include/sys\n\
=> /etc/include/sys\n\
\n\
path /etc/system/ + include\n\
=> /etc/system/include\n\
\n\
path /etc/system/ - /system/\n\
=> /etc\n\
\n\
path /etc \\* \\( sys config script \\)\n\
=> /etc/sys /etc/config /etc/script\n\
\n\
path \\( sys/temp config/temp script/temp \\) \\* - temp \\* tmp\n\
=> sys/tmp config/tmp script/tmp\n\
");
      break;
      return 0;
    case '?':
      fprintf(stderr, "Try `path -h' for more information.\n");
      exit(1);
    }
  }
  c.argc = argc;
  c.argv = argv;
  c.i = optind;
  delete_path(print_path(stdout, eval_list_path(&c), base, quote));
  return 0;
}

