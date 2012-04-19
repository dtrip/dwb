/*
 * Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef SCRIPTS_H
#define SCRIPTS_H

enum SIGNALS {
  SCRIPT_SIG_FIRST        = 0,
  SCRIPT_SIG_NAVIGATION   = 0,
  SCRIPT_SIG_LOAD_STATUS,
  SCRIPT_SIG_MIME_TYPE,
  SCRIPT_SIG_DOWNLOAD,
  SCRIPT_SIG_RESOURCE,
  SCRIPT_SIG_KEY_PRESS,
  SCRIPT_SIG_KEY_RELEASE,
  SCRIPT_SIG_BUTTON_PRESS,
  SCRIPT_SIG_BUTTON_RELEASE,
  SCRIPT_SIG_TAB_FOCUS,
  SCRIPT_SIG_LAST, 
};
gboolean scripts_emit(JSObjectRef , int , const char *);
void scripts_create_tab(GList *gl);
void scripts_end(void);
void scripts_init_script(const char *);
void scripts_init(void);

#define EMIT_SCRIPT(gl, sig)  (VIEW(gl)->script != NULL && (dwb.misc.script_signals & (1<<SCRIPT_SIG_##sig)))
#define SCRIPTS_EMIT(gl, sig, json)  scripts_emit(VIEW(gl)->script, SCRIPT_SIG_##sig, json)

#define SCRIPTS_EMIT_NO_RETURN(gl, sig, ...) G_STMT_START  \
if (EMIT_SCRIPT(gl, sig))  {  \
  char *__json_##sig = util_create_json(__VA_ARGS__); \
  SCRIPTS_EMIT(gl, sig, __json_##sig); \
  g_free(__json_##sig); \
} G_STMT_END
 
#define SCRIPTS_EMIT_RETURN(gl, sig, ...) G_STMT_START  \
if (VIEW(gl)->script != NULL && (dwb.misc.script_signals & (1<<SCRIPT_SIG_##sig)))  {  \
  char *__json_##sig = util_create_json(__VA_ARGS__); \
  if (SCRIPTS_EMIT(gl, sig, __json_##sig)) { g_free(__json_##sig); return true; } \
  else { g_free(__json_##sig); }  \
} G_STMT_END

#endif
