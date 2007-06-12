/* Copyright (C) 2006 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@thkukuk.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "pam-config.h"

static void
parse_pwcheck_options (config_file_t *conf, char *args)
{
  while (args && strlen (args) > 0)
    {
      char *cp = strsep (&args, " \t");
      if (args)
	while (isspace ((int)*args))
        ++args;

      if (strcmp (cp, "debug") == 0)
	conf->pwcheck_debug = 1;
      else if (strcmp (cp, "nullok") == 0)
	conf->pwcheck_nullok = 1;
      else if (strcmp (cp, "cracklib") == 0)
	conf->pwcheck_cracklib = 1;
      else if (strncmp (cp, "cracklib=", 9) == 0)
	{
	  conf->pwcheck_cracklib = 1;
	  conf->pwcheck_cracklib_path = strdup (&cp[9]);
	}
      else if (strncmp (cp, "maxlen=", 7) == 0)
	conf->pwcheck_maxlen = atoi (&cp[7]);
      else if (strncmp (cp, "minlen=", 7) == 0)
	conf->pwcheck_minlen = atoi (&cp[7]);
      else if (strncmp (cp, "tries=", 6) == 0)
	conf->pwcheck_tries = atoi (&cp[6]);
      else if (strncmp (cp, "remember=", 9) == 0)
	conf->pwcheck_remember = atoi (&cp[9]);
      else if (strncmp (cp, "nisdir=", 7) == 0)
	conf->pwcheck_nisdir = strdup (&cp[7]);
      else if (strcmp (cp, "use_first_pass") == 0)
	{ /* will be ignored */ }
      else if (strcmp (cp, "use_authtok") == 0)
	{ /* will be ignored */ }
      else
	fprintf (stderr,
		 _("Unknown option for pam_pwcheck.so, ignored: '%s'\n"),
		 cp);
    }
  return;
}

static void
parse_unix2_options (config_file_t *conf, char *args)
{
  while (args && strlen (args) > 0)
    {
      char *cp = strsep (&args, " \t");
      if (args)
	while (isspace ((int)*args))
        ++args;

      if (strcmp (cp, "debug") == 0)
	conf->unix2_debug = 1;
      else if (strcmp (cp, "nullok") == 0)
	conf->unix2_nullok = 1;
      else if (strcmp (cp, "trace") == 0)
	conf->unix2_trace = 1;
      else if (strcmp (cp, "use_first_pass") == 0)
	{ /* will be ignored */ }
      else if (strcmp (cp, "use_authtok") == 0)
	{ /* will be ignored */ }
      else if (strncmp (cp, "call_modules=", 13) == 0)
	/* XXX strip krb5 and ldap modules from it */
	conf->unix2_call_modules = strdup (&cp[13]);
      else
	fprintf (stderr,
		 _("Unknown option for pam_unix2.so, ignored: '%s'\n"),
		 cp);
    }
  return;
}

static void
parse_krb5_options (config_file_t *conf, char *args)
{
  while (args && strlen (args) > 0)
    {
      char *cp = strsep (&args, " \t");
      if (args)
	while (isspace ((int)*args))
        ++args;

      if (strcmp (cp, "debug") == 0)
	conf->krb5_debug = 1;
      else if (strcmp (cp, "use_first_pass") == 0)
	{ /* will be ignored */ }
      else if (strcmp (cp, "use_authtok") == 0)
	{ /* will be ignored */ }
      else
	fprintf (stderr,
		 _("Unknown option for pam_krb5.so, ignored: '%s'\n"),
		 cp);
    }
  return;
}

static void
parse_ldap_options (config_file_t *conf, char *args)
{
  while (args && strlen (args) > 0)
    {
      char *cp = strsep (&args, " \t");
      if (args)
	while (isspace ((int)*args))
        ++args;

      if (strcmp (cp, "debug") == 0)
	conf->ldap_debug = 1;
      else if (strcmp (cp, "try_first_pass") == 0)
	{ /* will be ignored */ }
      else if (strcmp (cp, "use_authtok") == 0)
	{ /* will be ignored */ }
      else if (strcmp (cp, "use_first_pass") == 0)
	{ /* will be ignored */ }
      else
	fprintf (stderr,
		 _("Unknown option for pam_ldap.so, ignored: '%s'\n"),
		 cp);
    }
  return;
}

int
load_config (const char *file, const char *wanted,
	     config_file_t *conf)
{
  FILE *fp;
  char *buf = NULL;
  size_t buflen = 0;

  if (debug)
    printf ("*** load_config (%s, %s, ...)\n", file, wanted);

  fp = fopen(file, "r");
  if (fp == NULL)
    {
      if (errno == ENOENT)
	return 0;
      else
	return -1;
    }

  while (!feof (fp))
    {
      char *cp, *tmp, *type, *control, *module, *arguments;
      ssize_t n = getline (&buf, &buflen, fp);

      cp = buf;

      if (n < 1)
        break;

      tmp = strchr (cp, '#');  /* remove comments */
      if (tmp)
        *tmp = '\0';
      while (isspace ((int)*cp))    /* remove spaces and tabs */
        ++cp;
      if (*cp == '\0')        /* ignore empty lines */
        continue;

      if (cp[strlen (cp) - 1] == '\n')
        cp[strlen (cp) - 1] = '\0';

      type = strsep (&cp, " \t");
      if (cp == NULL)
	{
	  fprintf (stderr, "%s: broken line: %s\n", file, buf);
	  fclose (fp);
	  free (buf);
	  return -1;
	}
      while (isspace ((int)*cp))
	++cp;

      if (*cp == '[')
	{
	  control = cp;
	  cp = strchr (cp, ']');
	  if (cp)
	    {
	      cp++;
	      *cp++ = '\0';
	    }
	}
      else
	control = strsep (&cp, " \t");
      if (cp == NULL)
	{
	  fprintf (stderr, "%s: broken line: '%s'\n", file, buf);
	  fclose (fp);
	  free (buf);
	  return -1;
	}
      while (isspace ((int)*cp))
	++cp;
      module = strsep (&cp, " \t");
      if (cp)
	{
	  while (isspace ((int)*cp))
	    ++cp;
	  if (strlen (cp) > 0)
	    arguments = cp;
	  else
	    arguments = NULL;
	}
      else
	arguments = NULL;

      if (debug)
	printf ("**** [%s, %s, %s, %s]\n", type, control, module,
		arguments?arguments:"");

      if (strcmp (type, wanted) == 0)
	{
	  if (strcmp (module, "pam_pwcheck.so") == 0)
	    {
	      conf->use_pwcheck = 1;
	      if (arguments)
		parse_pwcheck_options (conf, arguments);
	    }
	  else if (strcmp (module, "pam_mkhomedir.so") == 0)
	    {
	      conf->use_mkhomedir = 1;
	      if (arguments)
		fprintf (stderr, _("%s (%s): Arguments will be ignored\n"),
			 file, module);
	    }
	  else if (strcmp (module, "pam_limits.so") == 0)
	    {
	      conf->use_limits = 1;
	      if (arguments)
		fprintf (stderr, _("%s (%s): Arguments will be ignored\n"),
			 file, module);
	    }
	  else if (strcmp (module, "pam_bioapi.so") == 0)
	    {
	      conf->use_bioapi = 1;
	      if (arguments)
		conf->bioapi_options = strdup (arguments);
	    }
	  else if (strcmp (module, "pam_env.so") == 0)
	    {
	      conf->use_env = 1;
	      if (arguments)
		fprintf (stderr, _("%s (%s): Arguments will be ignored\n"),
			 file, module);
	    }
	  else if (strcmp (module, "pam_xauth.so") == 0)
	    {
	      conf->use_xauth = 1;
	      if (arguments)
		fprintf (stderr, _("%s (%s): Arguments will be ignored\n"),
			 file, module);
	    }
	  else if (strcmp (module, "pam_make.so") == 0)
	    {
	      conf->use_make = 1;
	      if (arguments)
		conf->make_options = strdup (arguments);
	    }
	  else if (strcmp (module, "pam_unix2.so") == 0)
	    {
	      conf->use_unix2 = 1;
	      if (arguments)
		parse_unix2_options (conf, arguments);
	    }
	  else if (strcmp (module, "pam_krb5.so") == 0)
	    {
	      conf->use_krb5 = 1;
	      if (arguments)
		parse_krb5_options (conf, arguments);
	    }
	  else if (strcmp (module, "pam_ldap.so") == 0)
	    {
	      conf->use_ldap = 1;
	      if (arguments)
		parse_ldap_options (conf, arguments);
	    }
	  else if (strcmp (module, "pam_ccreds.so") == 0)
	    conf->use_ccreds = 1;
	  else if (strcmp (module, "pam_pkcs11.so") == 0)
	    conf->use_pkcs11 = 1;
	  else
	    {
	      fprintf (stderr, _("%s: Unknown module %s, ignored!\n"),
		       file, module);
	    }
	}
    }

  fclose (fp);

  if (buf)
    free (buf);

  return 0;
}
