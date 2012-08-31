#ifndef _BSD_SOURCE
#define _BSD_SOURCE 
#endif
#ifndef _BSD_SOURCE
#define _POSIX_SOURCE 
#endif

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <readline/readline.h>
#include <sys/stat.h>
#include <glib-2.0/glib.h>
#include <libsoup-2.4/libsoup/soup.h>
#define API_BASE "https://api.bitbucket.org/1.0/repositories/portix/dwb_extensions/src/tip/src/?format=yaml"
#define REPO_BASE "https://bitbucket.org/portix/dwb_extensions/raw/tip/src" 
#define SKIP(line, c) do{ \
  while(*line && *line != c)line++; line++; } while(0)
#define EXTENSION_DIR() (g_build_filename(g_get_user_data_dir(), "dwb", "extensions", NULL))
#ifndef false
#define false 0
#endif
#ifndef true
#define true (!false)
#endif
#define TMPL_CONFIG "___CONFIG"
#define TMPL_SCRIPT "___SCRIPT"
#define TMPL_DISABLED "___DISABLED"
#define REGEX_REPLACE(X, name) (g_strdup_printf("(?<=^|\n)//<%s"TMPL_##X".*//>%s"TMPL_##X"\\s*(?=\n|$|/*)\\s*", name, name))
#define REGEX(X, name) (g_strdup_printf("(?<=^|\n)//<%s"TMPL_##X"|//>%s"TMPL_##X"\\s*(?=\n|$|/*)\\s*", name, name))
#define EXT(name) "\033[1m"#name"\033[0m"
#define FREE0(X) (X == NULL ? NULL : (X = (g_free(X), NULL)))
enum {
  F_NO_CONFIG = 1<<0,
  F_BIND = 1<<1,
  F_FORCE = 1<<2,
  F_UPDATE = 1<<2
};

static char *m_installed;
static char *m_loader;
static char *m_meta_data;
static const char *m_system_dir;
static char *m_user_dir;
static const char *m_editor;
static const char *m_diff;
static SoupSession *session;

void
vprint_error(const char *format, va_list args) {
  fputs("\033[31m!!!\033[0m ", stderr);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
}
void
print_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprint_error(format, args);
  va_end(args);
}
void
notify(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fputs("\033[32m==>\033[0m ", stderr);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  va_end(args);
}

void 
clean_up() {
  g_free(m_installed);
  g_free(m_loader);
  g_free(m_meta_data);
  g_free(m_user_dir);
  g_object_unref(session);
}

void 
die(int status, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprint_error(format, args);
  va_end(args);
  clean_up();
  exit(status);
}
void 
check_dir(const char *dir) {
  if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents(dir, 0700);
}

static int
regex_replace_file(const char *path, const char *regex, const char *replacement) {
  int ret = -1;
  char *content = NULL; 
  if (g_file_get_contents(path, &content, NULL, NULL)) {
    char **matches = g_regex_split_simple(regex, content, G_REGEX_DOTALL, 0);
    if (matches[0]) {
      g_free(content);
      content = g_strdup_printf("%s%s%s", matches[0], replacement ? replacement : "", matches[1] ? matches[1] : "");
      if (g_file_set_contents(path, content, -1, NULL))
        ret = 0;
    }
    g_strfreev(matches);
  }
  g_free(content);
  return ret;
}
static char *
get_data(const char *name, const char *template, gboolean multiline) {
  char *format;
  if (multiline)
    format = "(?<=^|\n)/\\*<%s%s|%s%s>\\*/\\s*(?=\n|$|/*)\\s*";
  else
    format = "(?<=^|\n)//<%s%s|//>%s%s\\s*(?=\n|$|/*)\\s*";
  char *regex = g_strdup_printf(format, name, template, name, template);
  char *ret = NULL;
  char *content = NULL;
  if (g_file_get_contents(m_loader, &content, NULL, NULL)) {
    char **matches = g_regex_split_simple(regex, content, G_REGEX_DOTALL, 0);
    if (matches[1] != NULL)
      ret = g_strdup(matches[1]);
    g_strfreev(matches);
  }
  g_free(content);
  g_free(regex);
  return ret;
}


size_t
grep(const char *filename, const char *beg, char *buffer, size_t length) {
  FILE *f = fopen(filename, "r");
  char line[128];
  int i = -1;
  if (f == NULL)
    return -1;
  while(fgets(line, 128, f) != NULL) {
    if (g_str_has_prefix(line, beg)) {
      i=0;
      while (line[i] && line[i] != '\n' && i<length-1) {
        buffer[i] = line[i];
        i++;
      }
      buffer[i++] = '\0';
      break;
    }
  }
  fclose(f);
  return i;
}
static int
update_installed(const char *name, const char *meta) {
  char buffer[128];
  snprintf(buffer, 128, "%s\n", meta);
  char *regex = g_strdup_printf("(?<=^|\n)%s\\s\\w+\n", name);
  regex_replace_file(m_installed, regex, buffer);
  g_free(regex);
  return 0;

}
static int 
yes_no(int preset, const char *format, ...) {
  va_list args; 
  char *response;
  va_start(args, format);
  int ret = -1;
  char buffer[512];
  char cformat[512];
  snprintf(cformat, 512, "\033[34m:::\033[0m %s %s? ", format, preset ? "(Y/n)" : "(y/N)");
  va_start(args, format);
  vsnprintf(buffer, 512, cformat, args);
  va_end(args);
  while (ret == -1) {
    response = readline(buffer);
    if (response != NULL) {
      char *backup = response;
      g_strstrip(response);
      if (*response == '\0')
        ret = preset;
      else if (! strcasecmp("y", response))
        ret = 1;
      else if (! strcasecmp("n", response))
        ret = 0;
      else 
        print_error("try again");
      g_free(backup);
    }
    else 
      ret = preset;
  }
  va_end(args);
  return ret;
}
char *
get_response(const char *format, ...) {
  char buffer[512];
  char cformat[512];
  va_list args; 
  snprintf(cformat, 512, "\033[34m:::\033[0m %s ", format);
  va_start(args, format);
  vsnprintf(buffer, 512, cformat, args);
  va_end(args);
  return readline(buffer);
}
int
diff(const char *text1, const char *text2, char **ret) {
  if (g_strcmp0(text1, text2) == 0 || text1 == NULL || text2 == NULL) {
    *ret = NULL;
    return 0;
  }
  gboolean spawn_success = true;
  GError *e = NULL;
  char *new_text = NULL;
  char file1[32], file2[32];
  strcpy(file1, "/tmp/tmpXXXXXXorig.js");
  strcpy(file2, "/tmp/tmpXXXXXXnew.js");
  char *text2_new = g_strdup_printf("// THIS FILE WILL BE DISCARDED\n%s// THIS FILE WILL BE DISCARDED", text2);
  if (g_file_set_contents(file1, text1, -1, &e) && g_file_set_contents(file2, text2_new, -1, &e)) {
    char *args[4];
    args[0] = (char *)m_diff;
    args[1] = file1;
    args[2] = file2;
    args[3] = NULL;
    spawn_success = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN, NULL, NULL, NULL, NULL, NULL, NULL);
    if (spawn_success) 
      g_file_get_contents(file1, &new_text, NULL, NULL);
    unlink(file1);
    unlink(file2);
  }
  else {
    print_error("Cannot diff : %s", e->message);
  }
  g_free(text2_new);
  if (!spawn_success)
    die(1, "Cannot spawn "EXT(%s)", please set "EXT(EDITOR)" to an appropriate value", m_editor);
  *ret = new_text;
  return 1;
}
char *
edit(const char *text) {
  char *new_config = NULL;
  char buffer[32];
  strcpy(buffer, "/tmp/tmpXXXXXX.js");
  mkstemps(buffer, 3);
  gboolean spawn_success = true;
  if (g_file_set_contents(buffer, text, -1, NULL)) {
    char *args[3]; 
    args[0] = (char*)m_editor;
    args[1] = buffer;
    args[2] = NULL;

    spawn_success = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN, NULL, NULL, NULL, NULL, NULL, NULL);
    if (spawn_success)
      g_file_get_contents(buffer, &new_config, NULL, NULL);
    unlink(buffer);
  }
  if (!spawn_success)
    die(1, "Cannot spawn "EXT(%s)", please set "EXT(EDITOR)" to an appropriate value", m_editor);
  return new_config;
}
static void 
set_loader(const char *name, const char *config, int flags) {
  char *script = NULL;
  char *shortcut = NULL, *command = NULL;
  gboolean load = true;
  notify("Adding "EXT(%s)" to extension loader", name, m_loader);
  if (flags & F_BIND) {
    while ((shortcut = get_response("Shortcut for toggling "EXT(%s)"?", name)) == NULL || *shortcut == '\0') 
      g_free(shortcut);
    load = yes_no(1, "Load "EXT(%s)" on startup", name);
    command = get_response("Command for toggling "EXT(%s)"?", name);
  }
  if (config == NULL || flags & F_NO_CONFIG) {
    if (flags & F_BIND)
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\nextensions.bind(\"%s\", \"%s\", {\n"
              "  load : %s%s%s%s\n});\n//>%s"TMPL_SCRIPT"\n", 
          name, name, shortcut,
          load ? "true" : "false",
          command != NULL && *command != '\0' ? ",\n  command : \"" : "",
          command != NULL && *command != '\0' ? command : "",
          command != NULL && *command != '\0' ? "\"" : "",
          name);
    else {
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\nextensions.load(\"%s\");\n//>%s"TMPL_SCRIPT"\n", name, name, name);
    }
  }
  else {
    if (flags & F_BIND) {
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\n"
              "var config_%s = {\n"
              "//<%s"TMPL_CONFIG"%s//<%s"TMPL_CONFIG"\n};\n"
              "extensions.bind(\"%s\", \"%s\", {\n"
              "  config : config_%s,\n"
              "  load : %s%s%s%s\n});\n//>%s"TMPL_SCRIPT"\n", 
          name, name, name, config, name, name, shortcut, name, 
          load ? "true" : "false",
          command != NULL && *command != '\0' ? ",\n  command : \"" : "",
          command != NULL && *command != '\0' ? command : "",
          command != NULL && *command != '\0' ? "\"" : "",
          name);
    }
    else {
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\nextensions.load(\"%s\", {\n"
              "//<%s"TMPL_CONFIG"%s//>%s"TMPL_CONFIG"\n});\n//>%s"TMPL_SCRIPT"\n", 
          name, name, name, config, name, name);
    }
  }
  if (! g_file_test(m_loader, G_FILE_TEST_EXISTS) ) {
    char *file_content = g_strdup_printf("#!javascript\n%s", script);
    g_file_set_contents(m_loader, file_content, -1, NULL);
    g_free(file_content);
  }
  else {
    char *regex = REGEX_REPLACE(SCRIPT, name);
    regex_replace_file(m_loader, regex, script);
    g_free(regex);
  }
  g_free(shortcut);
  g_free(command);
  g_free(script);
}
static int 
add_to_loader(const char *name, const char *content, int flags) {
  g_return_val_if_fail(content != NULL, -1);
  char *config = NULL;
  const char *new_config = NULL;
  char *data = NULL;
  gchar **matches = g_regex_split_simple("//<DEFAULT_CONFIG|//>DEFAULT_CONFIG", content, G_REGEX_DOTALL, 0);

  if (matches[1] == NULL) {
    notify("No default configuration found");
  }
  else if (flags & F_UPDATE) {
    data = get_data(name, TMPL_CONFIG, false);
    if (diff(data, matches[1], &config) == 0) {
      notify("Config is up to date");
    }
    else 
      new_config = config;
  }
  else if (!(flags & F_NO_CONFIG) && yes_no(1, "Edit configuration") == 1) {
    new_config = config = edit(matches[1]);
  }
  else 
    new_config = matches[1];

  set_loader(name, new_config, flags);
  g_strfreev(matches);
  g_free(config);
  g_free(data);
  return 0;
}
static gboolean 
check_installed(const char *name) {
  char meta[128];
  if (grep(m_installed, name, meta, 128) == -1) 
    return false;
  return true;
}
static int 
install_extension(const char *name, int flags) {
  int ret = -1;
  char buffer[512];
  char meta[128];
  GError *e = NULL;
  if (grep(m_meta_data, name, meta, 128) == -1) 
    die(1, "extension %s not found", name);
  snprintf(buffer, 512, "%s/%s", m_system_dir, name);
  if (g_file_test(buffer, G_FILE_TEST_EXISTS)) {
    notify("Using %s", buffer);
    char *content = NULL;
    if (g_file_get_contents(buffer, &content, NULL, NULL) ) {
      if (add_to_loader(name, content, flags) == 0) {
        update_installed(name, meta);
        ret = 0;
      }
      g_free(content);
    }
    else {
      print_error("Opening %s failed", buffer);
    }
  }
  else {
    notify("Downloading "EXT(%s), name);
    snprintf(buffer, 512, "%s/%s", REPO_BASE, name);
    SoupMessage *msg = soup_message_new("GET", buffer);
    int status = soup_session_send_message(session, msg);
    if (status == 200 && msg->response_body->data) {
      snprintf(buffer, 512, "%s/%s", m_user_dir, name);
      if (g_file_set_contents(buffer, msg->response_body->data, -1, &e)) {
        if (add_to_loader(name, msg->response_body->data, flags) == 0) {
          update_installed(name, meta);
          ret = 0;
        }
      }
      else 
        print_error("Saving %s failed: %s", name, e->message);
    }
    else {
      print_error("Download failed with status %d", status);
    }
    g_object_unref(msg);
  }
  return ret;
}

int
sync_meta(const char *output) {
  int ret = 0;
  check_dir(m_user_dir);

  static gboolean sync = false;
  if (sync)
    return ret;
  sync = true;

  notify("Syncing metadata");
  SoupMessage *msg = soup_message_new("GET", API_BASE);
  int status = soup_session_send_message(session, msg);

  if (status == 200) {
    if (msg->response_body && msg->response_body->data) {
      FILE *file = fopen(output, "w");
      if (file == NULL) {
        ret = -1;
        print_error("Cannot open %s for writing", m_meta_data);
        goto unwind;
      }
      const char *response = msg->response_body->data;
      while(*response) {
        while(isspace((int)*response))
          response++;
        while (*response && *response != '-') {
          SKIP(response, '\n');
        }
        if (*response == '\n')
          continue;
        if (*response == '\0')
          break;
        SKIP(response, '/');
        while (*response && *response != ',') {
          fputc(*(response++), file);
        }
        fputc(' ', file);
        SKIP(response, ':');
        response += 2;
        while (*response && *response != ',') {
          fputc(*(response++), file);
        }
        fputc('\n', file);
        SKIP(response, '\n');
      }

      GDir *d = g_dir_open(m_system_dir, 0, NULL);
      if (d == NULL) 
        die(1, "Cannot open %s", m_system_dir);
      const char *name;
      struct stat st;
      char buffer[1024];
      while((name = g_dir_read_name(d)) != NULL) {
        snprintf(buffer, 1024, "%s/%s", m_system_dir, name);
        if (stat(buffer, &st) != -1) {
          fprintf(file, "%s %lu\n", name, st.st_mtime);
        }
      }
      g_dir_close(d);
      fclose(file);
    }
  }
  else {
    ret = -1;
    print_error("Syncing failed, request returned with status %s\n", status);
  }
unwind:
  g_object_unref(msg);
  return ret;

}

static int 
set_data(const char *name, const char *template, const char *data, gboolean multiline) {
  char *format;
  if (multiline) 
    format = "(?<=^|\n)/\\*<%s%s.*%s%s>\\*/\\s*(?=\n|$)";
  else 
    format = "(?<=^|\n)//<%s%s.*//>%s%s\\s*(?=\n|$)";
  char *regex = g_strdup_printf(format, name, template, name, template);
  char *content = NULL;
  int ret = -1;
  if (g_file_get_contents(m_loader, &content, NULL, NULL)) {
    char **matches = g_regex_split_simple(regex, content, G_REGEX_DOTALL, 0);
    GString *buffer = g_string_new(matches[0]);
    if (multiline) {
      g_string_append_printf(buffer, "/*<%s%s%s%s%s>*/\n", name, template, data ? data : "\n", name, template);
    }
    else {
      g_string_append_printf(buffer, "//<%s%s%s//>%s%s\n", name, template, data ? data : "\n", name, template);
    }
    if (matches[1]) 
      g_string_append(buffer, matches[1]);
    if (g_file_set_contents(m_loader, buffer->str, -1, NULL))
     ret = 0;
    g_string_free(buffer, true);
  }
  g_free(regex);
  g_free(content);
  return ret;

}
static void
change_config(const char *name, int flags) {
  if (!check_installed(name))
    die(1, "Extension "EXT(%s)" is not installed", name);
  char *config = get_data(name, TMPL_CONFIG, false);
  char *new_config = NULL;
  if (config) {
    if (!(flags & F_NO_CONFIG)) {
      new_config = edit(config);
      g_free(config);
    }
    else 
      new_config = config;
    set_loader(name, new_config, flags);
    g_free(new_config);
  }
  else {
    notify("No config found, you can use extensionrc instead");
    set_loader(name, NULL, flags);
  }
}
static int 
cl_install(const char *name, int flags) {
  if (!(flags & F_FORCE) 
      && check_installed(name) 
      && !yes_no(0, EXT(%s)" is already installed, continue anyway", name))
    return -1;
  notify("Installing "EXT(%s), name);
  if (sync_meta(m_meta_data) == 0) {
    return install_extension(name, flags);
  }
  return -1;
}
static void
cl_uninstall(const char *name) {
  if (!check_installed(name)) 
    die(1, "extension %s is not installed", name);
  char *path = g_build_filename(m_user_dir, name, NULL);
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    notify("Removing %s", name);
    unlink(path);
  }
  g_free(path);
  char *regex = REGEX_REPLACE(SCRIPT, name);
  if (regex_replace_file(m_loader, regex, NULL) != -1) {
    notify("Updating %s", m_loader);
  }
  g_free(regex);
  regex = g_strdup_printf("(?<=^|\n)%s\\s\\w+\n", name);
  if (regex_replace_file(m_installed, regex, NULL) != -1) {
    notify("Updating metadata");
  }
  g_free(regex);
}

static void
cl_disable(const char *name) {
  if (!check_installed(name))
    die(1, "Extension "EXT(%s)" is not installed", name);
  char *regex = g_strdup_printf("(?<=^|\n)/\\*<%s"TMPL_DISABLED".*%s"TMPL_DISABLED">\\*/\\s*(?=$|\n)", name, name);
  char *data = get_data(name, TMPL_SCRIPT, false);
  if (!g_regex_match_simple(regex, data, G_REGEX_DOTALL, 0)) {
    char *new_data = g_strdup_printf("\n/*<%s"TMPL_DISABLED"%s%s"TMPL_DISABLED">*/\n", name, data ? data : "\n", name);
    if (set_data(name, TMPL_SCRIPT, new_data, false) != -1) 
      notify(EXT(%s)" disabled", name);
    else
      print_error("Unable to disable "EXT(%s), name);
    g_free(new_data);
  }
  else 
    print_error(EXT(%s)" is already disabled", name);
  g_free(data);
  g_free(regex);
}
static void
cl_enable(const char *name) {
  char *data = get_data(name, TMPL_DISABLED, true);
  if (data != NULL) {
    if (set_data(name, TMPL_SCRIPT, data, false) != -1) 
      notify(EXT(%s)" enabled", name);
    else
      print_error("Unable to enable "EXT(%s), name);
    g_free(data);
  }
  else 
    print_error(EXT(%s)" is already enabled", name);
}
static void
do_upate(const char *meta, int flags) {
  char buffer[128];
  char *space = strchr(meta, ' ');
  if (space != NULL) {
    snprintf(buffer, MIN(128, space - meta + 1), meta);
    if (yes_no(1, "Update "EXT(%s), buffer))
      if (cl_install(buffer, flags | F_FORCE | F_UPDATE)) 
        notify(EXT(%s)" successfully updated", buffer);
  }
}
static void
cl_update(int flags) {
  sync_meta(m_meta_data);
  char *meta = NULL;
  char *installed = NULL;
  size_t l_inst = 0, l_meta = 0;

  if (!g_file_get_contents(m_installed, &installed, &l_inst, NULL) || l_inst == 0) 
    die(1, "No installed extensions found");
  if (!g_file_get_contents(m_meta_data, &meta, &l_meta, NULL) || l_meta == 0) 
    die(1, "Cannot open metadata");

  char **lines_inst = g_strsplit(installed, "\n", -1);
  g_free(installed);

  int updates = 0;
  for (int i=0; lines_inst[i]; i++) {
    if (*(lines_inst[i]) == '\0')
      continue;
    char *regex = g_strdup_printf("(?<=^|\n)%s(?=$|\n)", lines_inst[i]);
    if (!g_regex_match_simple(regex, meta, G_REGEX_DOTALL, 0)) {
      do_upate(lines_inst[i], flags);
      updates++;
    }
  }
  g_strfreev(lines_inst);
  if (updates == 0)
    notify("Up to date");
  else 
    notify("Done");
}

int 
main(int argc, char **argv) {
  g_type_init();
  gboolean missing = argc == 1;
  char **o_install = NULL;
  char **o_remove = NULL;
  gboolean o_bind = false;
  char **o_setbind = NULL;
  char **o_setload = NULL;
  char **o_disable = NULL;
  char **o_enable = NULL;
  gboolean o_noconfig = false;
  gboolean o_update = false;
  int flags = 0;
  GOptionEntry options[] = {
    { "install",  'i', 0, G_OPTION_ARG_STRING_ARRAY, &o_install, "Install extension",  "extension" },
    { "remove",   'r', 0, G_OPTION_ARG_STRING_ARRAY, &o_remove, "Remove extension", "extension" },
    { "bind",     'b', 0, G_OPTION_ARG_NONE, &o_bind, "When installing an extension use extensions.bind instead of extensions.load", NULL },
    { "setbind",  'B', 0, G_OPTION_ARG_STRING_ARRAY, &o_setbind, "Edit configuration, use extensions.bind", "extension" },
    { "setload",  'L', 0, G_OPTION_ARG_STRING_ARRAY, &o_setload, "Edit configuration, use exensions.load", "extension" },
    { "noconfig", 'n', 0, G_OPTION_ARG_NONE, &o_noconfig, "Don't use config in loader script, use extensionrc instead", NULL },
    { "disable",  'd', 0, G_OPTION_ARG_STRING_ARRAY, &o_disable, "Disable extension", "extension" },
    { "enable",   'e', 0, G_OPTION_ARG_STRING_ARRAY, &o_enable,  "Enable extension", "extension" },
    { "update",   'u', 0, G_OPTION_ARG_NONE, &o_update,  "Update extensions", "extension" },
    { NULL },
  };
  GError *e = NULL;
  GOptionContext *ctx = g_option_context_new(NULL);
  g_option_context_add_main_entries(ctx, options, NULL);
  if (!g_option_context_parse(ctx, &argc, &argv, &e) || missing) {
    if (missing) 
      fprintf(stderr, "Missing argument\n");
    else 
      fprintf(stderr, "%s\n", missing ? "" : e->message);
    fprintf(stderr, g_option_context_get_help(ctx, false, NULL));
    return 1;
  }

  session = soup_session_sync_new();
  m_user_dir = g_build_filename(g_get_user_data_dir(), "dwb", "extensions", NULL);
  check_dir(m_user_dir);
  char *config_dir = g_build_filename(g_get_user_config_dir(), "dwb", "userscripts", NULL);
  check_dir(config_dir);
  m_loader = g_build_filename(config_dir, "extension_loader.js", NULL);
  g_free(config_dir);

  m_meta_data = g_build_filename(m_user_dir, ".metadata", NULL);
  m_installed = g_build_filename(m_user_dir, ".installed", NULL);
  m_system_dir = SYSTEM_EXTENSION_DIR;
  if (!g_file_test(m_system_dir, G_FILE_TEST_EXISTS))
    die(1, "FATAL: %s not found, check your installation", m_system_dir);

  m_editor = g_getenv("EDITOR");
  if (m_editor == NULL)
    m_editor = "vim";
  m_diff = g_getenv("DIFF_VIEWER");
  if (m_diff == NULL)
    m_diff = "vimdiff";

  if (o_bind)
    flags |= F_BIND;
  if (o_noconfig)
    flags |= F_NO_CONFIG;

  if (o_update) {
    cl_update(flags);
  }
  if (o_setbind != NULL) {
    for (int i=0; o_setbind[i]; i++) 
      change_config(o_setbind[i], flags | F_BIND);
  }
  if (o_disable != NULL) {
    for (int i=0; o_disable[i]; i++) 
      cl_disable(o_disable[i]);
  }
  if (o_enable != NULL) {
    for (int i=0; o_enable[i]; i++) 
      cl_enable(o_enable[i]);
  }
  if (o_setload != NULL) {
    for (int i=0; o_setload[i]; i++) 
      change_config(o_setload[i], flags & ~F_BIND);
  }
  if (o_install != NULL) {
    for (int i=0; o_install[i]; i++) 
      cl_install(o_install[i], flags);
  }
  if (o_remove != NULL) {
    for (int i=0; o_remove[i]; i++) 
      cl_uninstall(o_remove[i]);
  }
  return 0;
}