/*
 * Copyright (c) 2010-2011 Stefan Bolte <portix@gmx.net>
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
#ifdef DWB_DOMAIN_SERVICE
#include <string.h>
#include "dwb.h"
#include "util.h"
#include "domain.h"
#include "tlds.h"

static GHashTable *_tld_table;

gboolean 
domain_match(char **domains, const char *host, const char *base_domain) {
  g_return_val_if_fail(domains != NULL, false);
  g_return_val_if_fail(host != NULL, false);
  g_return_val_if_fail(base_domain != NULL, false);
  g_return_val_if_fail(g_str_has_suffix(host, base_domain), false);

  const char *subdomains[SUBDOMAIN_MAX];
  int sdc = 0;

  gboolean domain_exc = false;
  gboolean has_positive = false;
  gboolean has_exception = false;
  gboolean found_positive = false;
  gboolean found_exception = false;

  char *real_domain;
  char *nextdot;
  /* extract subdomains */
  subdomains[sdc++] = host;
  while (g_strcmp0(host, base_domain)) {
    nextdot = strchr(host, '.');
    host = nextdot + 1;
    subdomains[sdc++] = host;
    if (sdc == SUBDOMAIN_MAX-1)
      break;
  }
  subdomains[sdc++] = NULL;

  /* TODO Maybe replace this with a hashtable 
   * in most cases the loop runs at most 9 times, 3 times each
   * */
  for (int k=0; domains[k]; k++) {
    for (int j=0; subdomains[j]; j++) {
      real_domain = domains[k];
      if (*real_domain == '~') {
        domain_exc = true;
        real_domain++;
        has_exception = true;
      }
      else {
        domain_exc = false;
        has_positive = true;
      }

      if (!g_strcmp0(subdomains[j], real_domain)) {
        if (domain_exc) {
          found_exception = true;
        }
        else {
          found_positive = true;
        }
      }
    }
  }
  if ((has_positive && found_positive && !found_exception) || (has_exception && !has_positive && !found_exception))
    return true;

  return false;
}/*}}}*/

const char *
domain_get_base_for_host(const char *host) {
  if (host == NULL)
    return NULL;
  g_return_val_if_fail(_tld_table != NULL, NULL);

  const char *cur_domain = host;
  const char *prev_domain = host;
  const char *pprev_domain = host;
  const char *ret = NULL;
  char *nextdot = strchr(cur_domain, '.');
  char *entry = NULL;
  while (1) {
    entry = g_hash_table_lookup(_tld_table, cur_domain);
    if (entry != NULL) {
      if (*entry == '*') {
        ret = pprev_domain;
        break;
      }
      else if (*entry == '!' && nextdot) {
        ret = nextdot + 1;
        break;
      }
      else {
        ret = prev_domain;
        break;
      }
    }
    if (nextdot == NULL)
      break;
    pprev_domain = prev_domain;
    prev_domain = cur_domain;
    cur_domain = nextdot + 1;
    nextdot = strchr(cur_domain, '.');
  }
  if (ret == NULL) 
    ret = host;
  return ret;
}
void
domain_end() {
  if (_tld_table)
    g_hash_table_unref(_tld_table);
}

void 
domain_init() {
  _tld_table = g_hash_table_new((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal);
  char *eff_tld;
  for (int i=0; (eff_tld = TLDS_EFFECTIVE[i]); i++) {
    if (*eff_tld == '*' || *eff_tld == '!') 
      eff_tld++;
    if (*eff_tld == '.')
      eff_tld++;
    g_hash_table_insert(_tld_table, eff_tld, TLDS_EFFECTIVE[i]);
  }
}
#endif