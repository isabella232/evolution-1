/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mail-composer-prefs.h"
#include "composer/e-msg-composer.h"

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-object-client.h>

#include <gal/widgets/e-gui-utils.h>

#include "widgets/misc/e-charset-picker.h"

#include "mail-config.h"

static void mail_composer_prefs_class_init (MailComposerPrefsClass *class);
static void mail_composer_prefs_init       (MailComposerPrefs *dialog);
static void mail_composer_prefs_destroy    (GtkObject *obj);
static void mail_composer_prefs_finalise   (GtkObject *obj);

static void sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, MailComposerPrefs *prefs);

static GtkVBoxClass *parent_class = NULL;


GtkType
mail_composer_prefs_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailComposerPrefs",
			sizeof (MailComposerPrefs),
			sizeof (MailComposerPrefsClass),
			(GtkClassInitFunc) mail_composer_prefs_class_init,
			(GtkObjectInitFunc) mail_composer_prefs_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_vbox_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_composer_prefs_class_init (MailComposerPrefsClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gtk_vbox_get_type ());
	
	object_class->destroy = mail_composer_prefs_destroy;
	object_class->finalize = mail_composer_prefs_finalise;
	/* override methods */
	
}

static void
mail_composer_prefs_init (MailComposerPrefs *composer_prefs)
{
	;
}

static void
mail_composer_prefs_finalise (GtkObject *obj)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) obj;
	
	gtk_object_unref (GTK_OBJECT (prefs->gui));
	gtk_object_unref (GTK_OBJECT (prefs->pman));

        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static void
mail_composer_prefs_destroy (GtkObject *obj)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) obj;
	
	mail_config_signature_unregister_client ((MailConfigSignatureClient) sig_event_client, prefs);
	
	if (GTK_OBJECT_CLASS (parent_class))
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (obj);
}

static void
attach_style_info (GtkWidget *item, gpointer user_data)
{
	int *style = user_data;
	
	gtk_object_set_data (GTK_OBJECT (item), "style", GINT_TO_POINTER (*style));
	
	(*style)++;
}

static void
toggle_button_toggled (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
menu_changed (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
option_menu_connect (GtkOptionMenu *omenu, gpointer user_data)
{
	GtkWidget *menu, *item;
	GList *items;
	
	menu = gtk_option_menu_get_menu (omenu);
	
	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    menu_changed, user_data);
		items = items->next;
	}
}

static void
run_script (char *script)
{
	struct stat st;
	
	if (stat (script, &st))
		return;
	
	if (!S_ISREG (st.st_mode) || !(st.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
		return;
	
	mail_config_signature_run_script (script);
}

static void
sig_load_preview (MailComposerPrefs *prefs, MailConfigSignature *sig)
{
	char *str;
	
	if (!sig) {
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), " ", 1);
		return;
	}
	
	str = e_msg_composer_get_sig_file_content (sig->filename, sig->html);
	if (!str)
		str = g_strdup (" ");
	
	/* printf ("HTML: %s\n", str); */
	if (sig->html)
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), str, strlen (str));
	else {
		GtkHTMLStream *stream;
		int len;
		
		len = strlen (str);
		stream = gtk_html_begin (GTK_HTML (prefs->sig_preview));
		gtk_html_write (GTK_HTML (prefs->sig_preview), stream, "<PRE>", 5);
		if (len)
			gtk_html_write (GTK_HTML (prefs->sig_preview), stream, str, len);
		gtk_html_write (GTK_HTML (prefs->sig_preview), stream, "</PRE>", 6);
		gtk_html_end (GTK_HTML (prefs->sig_preview), stream, GTK_HTML_STREAM_OK);
	}
	
	g_free (str);
}

static void
sig_write_and_update_preview (MailComposerPrefs *prefs, MailConfigSignature *sig)
{
	sig_load_preview (prefs, sig);
	mail_config_signature_write (sig);
}

static MailConfigSignature *
sig_current_sig (MailComposerPrefs *prefs)
{
	return gtk_clist_get_row_data (GTK_CLIST (prefs->sig_clist), prefs->sig_row);
}

static void
sig_script_activate (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (sig && sig->script && *sig->script) {
		run_script (sig->script);
		sig_write_and_update_preview (prefs, sig);
	}
}

static void
sig_edit (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (sig->filename && *sig->filename)
		mail_signature_editor (sig);
	else
		e_notice (GTK_WINDOW (prefs), GNOME_MESSAGE_BOX_ERROR,
			  _("Please specify signature filename\nin Andvanced section of signature settings."));
}

MailConfigSignature *
mail_composer_prefs_new_signature (MailComposerPrefs *prefs, gboolean html)
{
	MailConfigSignature *sig;
	char *name[2];
	int row;
	
	sig = mail_config_signature_add (html);
	
	if (prefs) {
		name[0] = sig->name;
		name[1] = sig->random ? _("yes") : _("no");
		row = gtk_clist_append (GTK_CLIST (prefs->sig_clist), name);
		gtk_clist_set_row_data (GTK_CLIST (prefs->sig_clist), row, sig);
		gtk_clist_select_row (GTK_CLIST (prefs->sig_clist), row, 0);
		/*gtk_widget_grab_focus (prefs->sig_name);*/
	}
	
	if (sig->filename && *sig->filename)
		mail_signature_editor (sig);
	
	return sig;
}

static void sig_row_unselect (GtkCList *clist, int row, int col, GdkEvent *event, MailComposerPrefs *prefs);

static void
sig_delete (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	gtk_clist_remove (prefs->sig_clist, prefs->sig_row);
	mail_config_signature_delete (sig);
	if (prefs->sig_row < prefs->sig_clist->rows)
		gtk_clist_select_row (prefs->sig_clist, prefs->sig_row, 0);
	else if (prefs->sig_row)
		gtk_clist_select_row (prefs->sig_clist, prefs->sig_row - 1, 0);
	else
		sig_row_unselect (prefs->sig_clist, prefs->sig_row, 0, NULL, prefs);
}

static void
sig_add (GtkWidget *widget, MailComposerPrefs *prefs)
{
	mail_composer_prefs_new_signature (prefs, FALSE);
}

static void
sig_row_select (GtkCList *clist, int row, int col, GdkEvent *event, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig;
	
	printf ("sig_row_select\n");
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, TRUE);
	/*gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_name, TRUE);*/
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_random, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_html, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_filename, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_script, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_html, TRUE);
	
	prefs->sig_switch = TRUE;
	sig = gtk_clist_get_row_data (prefs->sig_clist, row);
	if (sig) {
		/*if (sig->name)
		  e_utf8_gtk_entry_set_text (GTK_ENTRY (prefs->sig_name), sig->name);*/
		gtk_toggle_button_set_active (prefs->sig_random, sig->random);
		gtk_toggle_button_set_active (prefs->sig_html, sig->html);
		if (sig->filename)
			gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_filename)),
					    sig->filename);
		if (sig->script)
			gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_script)),
					    sig->script);
	}
	prefs->sig_switch = FALSE;
	prefs->sig_row = row;
	
	sig_load_preview (prefs, sig);
}

static void
sig_row_unselect (GtkCList *clist, int row, int col, GdkEvent *event, MailComposerPrefs *prefs)
{
	printf ("sig_row_unselect\n");
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, FALSE);
	/*gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_name, FALSE);*/
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_random, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_html, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_filename, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_script, FALSE);
	
	prefs->sig_switch = TRUE;
	/*gtk_entry_set_text (GTK_ENTRY (prefs->sig_name), "");*/
	gtk_toggle_button_set_active (prefs->sig_random, FALSE);
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_filename)), "");
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_script)), "");
	prefs->sig_switch = FALSE;
}

static void
sig_fill_clist (GtkCList *clist)
{
	GList *l;
	char *name[2];
	int row;
	
	gtk_clist_freeze (clist);
	for (l = mail_config_get_signature_list (); l; l = l->next) {
		name[0] = ((MailConfigSignature *) l->data)->name;
		name[1] = ((MailConfigSignature *) l->data)->random ? _("yes") : _("no");
		row = gtk_clist_append (clist, name);
		gtk_clist_set_row_data (clist, row, l->data);
	}
	gtk_clist_thaw (clist);
}

#if 0
static void
sig_name_changed (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	/*mail_config_signature_set_name (sig, e_utf8_gtk_entry_get_text (GTK_ENTRY (prefs->sig_name)));*/
	gtk_clist_set_text (GTK_CLIST (prefs->sig_clist), prefs->sig_row, 0, sig->name);
	
	sig_write_and_update_preview (prefs, sig);
}
#endif

static void
sig_random_toggled (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	mail_config_signature_set_random (sig, gtk_toggle_button_get_active (prefs->sig_random));
	
	gtk_clist_set_text (prefs->sig_clist, prefs->sig_row, 1, sig->random ? _("yes") : _("no"));
	
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_advanced_toggled (GtkWidget *widget, MailComposerPrefs *prefs)
{
	GtkWidget *advanced_frame;
	
	advanced_frame = glade_xml_get_widget (prefs->gui, "frameAdvancedOptions");
	gtk_widget_set_sensitive (advanced_frame, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
sig_html_toggled (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	sig->html = gtk_toggle_button_get_active (prefs->sig_html);
	
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_filename_changed (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	mail_config_signature_set_filename (sig, gnome_file_entry_get_full_path (prefs->sig_filename, FALSE));
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_script_changed (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	g_free (sig->script);
	sig->script = g_strdup (gnome_file_entry_get_full_path (prefs->sig_script, FALSE));
	
	sig_write_and_update_preview (prefs, sig);
}

static void
url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle)
{
	GtkHTMLStreamStatus status;
	int fd;
	
	if (!strncmp (url, "file:", 5))
		url += 5;
	
	fd = open (url, O_RDONLY);
	status = GTK_HTML_STREAM_OK;
	if (fd != -1) {
		ssize_t size;
		void *buf = alloca (1 << 7);
		while ((size = read (fd, buf, 1 << 7))) {
			if (size == -1) {
				status = GTK_HTML_STREAM_ERROR;
				break;
			} else
				gtk_html_write (html, handle, (const gchar *) buf, size);
		}
	} else
		status = GTK_HTML_STREAM_ERROR;
	
	gtk_html_end (html, handle, status);
}

static void
sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, MailComposerPrefs *prefs)
{
	switch (event) {
	case MAIL_CONFIG_SIG_EVENT_NAME_CHANGED:
		printf ("accounts NAME CHANGED\n");
		gtk_clist_set_text (GTK_CLIST (prefs->sig_clist), sig->id, 0, sig->name);
		if (sig == sig_current_sig (prefs)) {
			prefs->sig_switch = TRUE;
			/*e_utf8_gtk_entry_set_text (GTK_ENTRY (prefs->sig_name), sig->name);*/
			prefs->sig_switch = FALSE;
		}
		break;
	case MAIL_CONFIG_SIG_EVENT_CONTENT_CHANGED:
		printf ("accounts CONTENT CHANGED\n");
		if (sig == sig_current_sig (prefs))
			sig_load_preview (prefs, sig);
		break;
	case MAIL_CONFIG_SIG_EVENT_HTML_CHANGED:
		printf ("accounts HTML CHANGED\n");
		if (sig == sig_current_sig (prefs))
			gtk_toggle_button_set_active (prefs->sig_html, sig->html);
		break;
	default:
		;
	}
}

/*
 *
 * Spell checking cut'n'pasted from gnome-spell/capplet/main.c
 *
 */

#include "Spell.h"

#define GNOME_SPELL_GCONF_DIR "/GNOME/Spell"
#define SPELL_API_VERSION "0.2"

static void
spell_select_lang (MailComposerPrefs *prefs, const gchar *abrev)
{
	gint i;

	for (i = 0; i < prefs->language_seq->_length; i ++) {
		if (!strcasecmp (abrev, prefs->language_seq->_buffer [i].abrev)) {
			gtk_clist_select_row (GTK_CLIST (prefs->language), i, 0);
		}
	}
}

static void
spell_set_ui_language (MailComposerPrefs *prefs)
{
	gchar *l, *last, *lang;

	gtk_clist_freeze (GTK_CLIST (prefs->language));
	gtk_clist_unselect_all (GTK_CLIST (prefs->language));
	last = prefs->language_str;
	while ((l = strchr (last, ' '))) {
		if (l != last) {
			lang = g_strndup (last, l - last);
			spell_select_lang (prefs, lang);
			g_free (lang);
		}

		last = l + 1;
	}
	if (last)
		spell_select_lang (prefs, last);
	gtk_clist_thaw (GTK_CLIST (prefs->language));
}

static void
spell_set_ui (MailComposerPrefs *prefs)
{
	prefs->spell_active = FALSE;

	spell_set_ui_language (prefs);
	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (prefs->colour),
				    prefs->spell_error_color.red, prefs->spell_error_color.green, prefs->spell_error_color.blue, 0xffff);

	prefs->spell_active = TRUE;
}

static gchar *
spell_get_language_str (MailComposerPrefs *prefs)
{
	GList *selection = GTK_CLIST (prefs->language)->selection;
	GString *str = g_string_new (NULL);
	gchar *rv;

	for (; selection; selection = selection->next) {
		g_string_append (str, gtk_clist_get_row_data (GTK_CLIST (prefs->language),
							      GPOINTER_TO_INT (selection->data)));
		if (selection->next)
			g_string_append_c (str, ' ');
	}

	rv = str->str;
	g_string_free (str, FALSE);

	return rv ? rv : g_strdup ("");
}

static void
spell_get_ui (MailComposerPrefs *prefs)
{
	gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (prefs->colour),
				    &prefs->spell_error_color.red,
				    &prefs->spell_error_color.green,
				    &prefs->spell_error_color.blue, NULL);
	g_free (prefs->language_str);
	prefs->language_str = spell_get_language_str (prefs);
}

#define GET(t,x,prop,f,c) \
        val = gconf_client_get_without_default (prefs->gconf, GNOME_SPELL_GCONF_DIR x, NULL); \
        if (val) { f; prop = c (gconf_value_get_ ## t (val)); \
        gconf_value_free (val); }

static void
spell_save_orig (MailComposerPrefs *prefs)
{
	g_free (prefs->language_str_orig);
	prefs->language_str_orig = g_strdup (prefs->language_str ? prefs->language_str : "");
	prefs->spell_error_color_orig = prefs->spell_error_color;
}

/* static void
spell_load_orig (MailComposerPrefs *prefs)
{
	g_free (prefs->language_str);
	prefs->language_str = g_strdup (prefs->language_str_orig);
	prefs->spell_error_color = prefs->spell_error_color_orig;
} */

static void
spell_load_values (MailComposerPrefs *prefs)
{
	GConfValue *val;
	gchar *def_lang;

	def_lang = g_strdup (_("en"));
	g_free (prefs->language_str);
	prefs->language_str = g_strdup (def_lang);
	prefs->spell_error_color.red   = 0xffff;
	prefs->spell_error_color.green = 0;
	prefs->spell_error_color.blue  = 0;

	GET (int, "/spell_error_color_red",   prefs->spell_error_color.red,,);
	GET (int, "/spell_error_color_green", prefs->spell_error_color.green,,);
	GET (int, "/spell_error_color_blue",  prefs->spell_error_color.blue,,);
	GET (string, "/language", prefs->language_str, g_free (prefs->language_str), g_strdup);

	if (prefs->language_str == NULL)
		prefs->language_str = g_strdup (def_lang);

	spell_save_orig (prefs);

	g_free (def_lang);
}

#define SET(t,x,prop) \
        gconf_client_set_ ## t (prefs->gconf, GNOME_SPELL_GCONF_DIR x, prop, NULL);

#define STR_EQUAL(str1, str2) ((str1 == NULL && str2 == NULL) || (str1 && str2 && !strcmp (str1, str2)))

static void
spell_save_values (MailComposerPrefs *prefs, gboolean force)
{
	if (force || !gdk_color_equal (&prefs->spell_error_color, &prefs->spell_error_color_orig)) {
		SET (int, "/spell_error_color_red",   prefs->spell_error_color.red);
		SET (int, "/spell_error_color_green", prefs->spell_error_color.green);
		SET (int, "/spell_error_color_blue",  prefs->spell_error_color.blue);
	}

	if (force || !STR_EQUAL (prefs->language_str, prefs->language_str_orig)) {
		SET (string, "/language", prefs->language_str ? prefs->language_str : "");
	}

	gconf_client_suggest_sync (prefs->gconf, NULL);
}

static void
spell_apply (MailComposerPrefs *prefs)
{
	spell_get_ui (prefs);
	spell_save_values (prefs, FALSE);
}

/* static void
spell_revert (MailComposerPrefs *prefs)
{
	spell_load_orig (prefs);
	spell_set_ui (prefs);
	spell_save_values (prefs, TRUE);
} */

static void
spell_changed (gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
spell_color_set (GtkWidget *widget, guint r, guint g, guint b, guint a, gpointer user_data)
{
	spell_changed (user_data);
}

static void
spell_language_changed (GtkWidget *widget, gint row, gint column, GdkEvent *event, gpointer user_data)
{
	spell_changed (user_data);
}

static void
spell_setup (MailComposerPrefs *prefs)
{
	gint i;

	gtk_clist_freeze (GTK_CLIST (prefs->language));
	for (i = 0; i < prefs->language_seq->_length; i ++) {
		gchar *texts [1];

		texts [0] = _(prefs->language_seq->_buffer [i].name);
		gtk_clist_append (GTK_CLIST (prefs->language), texts);
		gtk_clist_set_row_data (GTK_CLIST (prefs->language), i, prefs->language_seq->_buffer [i].abrev);
	}
	gtk_clist_thaw (GTK_CLIST (prefs->language));

	spell_load_values (prefs);
	spell_set_ui (prefs);

	glade_xml_signal_connect_data (prefs->gui, "spellColorSet", GTK_SIGNAL_FUNC (spell_color_set), prefs);
	glade_xml_signal_connect_data (prefs->gui, "spellLanguageChanged", GTK_SIGNAL_FUNC (spell_language_changed), prefs);
}

static gboolean
spell_setup_check_options (MailComposerPrefs *prefs)
{
	BonoboObjectClient *dictionary_client;
	GNOME_Spell_Dictionary dict;
	CORBA_Environment ev;
	gchar *dictionary_id;

	dictionary_id = "OAFIID:GNOME_Spell_Dictionary:" SPELL_API_VERSION;
	dictionary_client = bonobo_object_activate (dictionary_id, 0);

	if (!dictionary_client) {
		g_warning ("Cannot activate %s", dictionary_id);

		return FALSE;
	}
	dict = bonobo_object_corba_objref (BONOBO_OBJECT (dictionary_client));

	CORBA_exception_init (&ev);
	prefs->language_seq = GNOME_Spell_Dictionary_getLanguages (dict, &ev);
	CORBA_exception_free (&ev);

	gconf_client_add_dir (prefs->gconf, GNOME_SPELL_GCONF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);

        spell_setup (prefs);

	return TRUE;
}

/*
 * End of Spell checking
 */

static void
mail_composer_prefs_construct (MailComposerPrefs *prefs)
{
	GtkWidget *toplevel, *widget, *menu;
	GladeXML *gui;
	int style;
	char *names[][2] = {{"live_spell_check", "chkEnableSpellChecking"},
			    {"gtk_html_prop_keymap_option", "omenuShortcutsType"},
			    {NULL, NULL}};

	prefs->gconf = gconf_client_get_default ();

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "composer_tab");
	prefs->gui = gui;

	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);
	
	/* General tab */
	
	/* Default Behavior */
	prefs->send_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSendHTML"));
	gtk_toggle_button_set_active (prefs->send_html, mail_config_get_send_html ());
	gtk_signal_connect (GTK_OBJECT (prefs->send_html), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->prompt_empty_subject = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptEmptySubject"));
	gtk_toggle_button_set_active (prefs->prompt_empty_subject, mail_config_get_prompt_empty_subject ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_empty_subject), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->prompt_bcc_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptBccOnly"));
	gtk_toggle_button_set_active (prefs->prompt_bcc_only, mail_config_get_prompt_only_bcc ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_bcc_only), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	menu = e_charset_picker_new (mail_config_get_default_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs->charset, prefs);
	
	/* Spell Checking: GtkHTML part */
	prefs->pman = GTK_HTML_PROPMANAGER (gtk_html_propmanager_new (NULL));
	gtk_signal_connect (GTK_OBJECT (prefs->pman), "changed", toggle_button_toggled, prefs);
	gtk_object_ref (GTK_OBJECT (prefs->pman));

	gtk_html_propmanager_set_names (prefs->pman, names);
	gtk_html_propmanager_set_gui (prefs->pman, gui, NULL);

	/* Spell Checking: GNOME Spell part */
	prefs->colour = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerSpellCheckColor"));
	prefs->language = GTK_CLIST (glade_xml_get_widget (gui, "clistSpellCheckLanguage"));

	if (!spell_setup_check_options (prefs)) {
		gtk_widget_hide (GTK_WIDGET (prefs->colour));
		gtk_widget_hide (GTK_WIDGET (prefs->language));
	}

	/* Forwards and Replies */
	prefs->forward_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuForwardStyle"));
	gtk_option_menu_set_history (prefs->forward_style, mail_config_get_default_forward_style ());
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->forward_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->forward_style, prefs);
	
	prefs->reply_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuReplyStyle"));
	gtk_option_menu_set_history (prefs->reply_style, mail_config_get_default_reply_style ());
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->reply_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->reply_style, prefs);
	
	/* Signatures */
	prefs->sig_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureAdd"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_add), "clicked",
			    GTK_SIGNAL_FUNC (sig_add), prefs);
	
	prefs->sig_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureEdit"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_edit), "clicked",
			    GTK_SIGNAL_FUNC (sig_edit), prefs);
	
	prefs->sig_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureDelete"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_delete), "clicked",
			    GTK_SIGNAL_FUNC (sig_delete), prefs);
	
	prefs->sig_random = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkRandomSignature"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_random), "toggled",
			    GTK_SIGNAL_FUNC (sig_random_toggled), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_random), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->sig_clist = GTK_CLIST (glade_xml_get_widget (gui, "clistSignatures"));
	sig_fill_clist (prefs->sig_clist);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_clist), "select_row",
			    GTK_SIGNAL_FUNC (sig_row_select), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_clist), "unselect_row",
			    GTK_SIGNAL_FUNC (sig_row_unselect), prefs);
	
	prefs->sig_advanced = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkAdvancedSignature"));
	gtk_toggle_button_set_active (prefs->sig_advanced, FALSE);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_advanced), "toggled",
			    GTK_SIGNAL_FUNC (sig_advanced_toggled), prefs);
	
	widget = glade_xml_get_widget (gui, "frameAdvancedOptions");
	gtk_widget_set_sensitive (widget, FALSE);
	
	prefs->sig_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkHtmlSignature"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_html), "toggled",
			    GTK_SIGNAL_FUNC (sig_html_toggled), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_html), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->sig_filename = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignatureFilename"));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->sig_filename)),
			    "changed", GTK_SIGNAL_FUNC (sig_filename_changed), prefs);
	
	prefs->sig_script = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignatureScript"));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->sig_script)),
			    "changed", GTK_SIGNAL_FUNC (sig_script_changed), prefs);
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->sig_script)),
			    "activate", GTK_SIGNAL_FUNC (sig_script_activate), prefs);
	
	/* preview GtkHTML widget */
	widget = glade_xml_get_widget (gui, "scrolled-sig");
	prefs->sig_preview = (GtkHTML *) gtk_html_new ();
	gtk_signal_connect (GTK_OBJECT (prefs->sig_preview), "url_requested", GTK_SIGNAL_FUNC (url_requested), NULL);
	gtk_widget_show (GTK_WIDGET (prefs->sig_preview));
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (prefs->sig_preview));
	
	if (GTK_CLIST (prefs->sig_clist)->rows)
		gtk_clist_select_row (GTK_CLIST (prefs->sig_clist), 0, 0);
	
	mail_config_signature_register_client ((MailConfigSignatureClient) sig_event_client, prefs);
}


GtkWidget *
mail_composer_prefs_new (void)
{
	MailComposerPrefs *new;
	
	new = (MailComposerPrefs *) gtk_type_new (mail_composer_prefs_get_type ());
	mail_composer_prefs_construct (new);
	
	return (GtkWidget *) new;
}


void
mail_composer_prefs_apply (MailComposerPrefs *prefs)
{
	GtkWidget *menu, *item;
	char *string;
	int val;
	
	/* General tab */
	
	/* Default Behavior */
	mail_config_set_send_html (gtk_toggle_button_get_active (prefs->send_html));
	mail_config_set_prompt_empty_subject (gtk_toggle_button_get_active (prefs->prompt_empty_subject));
	mail_config_set_prompt_only_bcc (gtk_toggle_button_get_active (prefs->prompt_bcc_only));
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	string = e_charset_picker_get_charset (menu);
	if (string) {
		mail_config_set_default_charset (string);
		g_free (string);
	}
	
	/* Spell Checking */
	gtk_html_propmanager_apply (prefs->pman);
	spell_apply (prefs);
	
	/* Forwards and Replies */
	menu = gtk_option_menu_get_menu (prefs->forward_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "style"));
	mail_config_set_default_forward_style (val);
	
	menu = gtk_option_menu_get_menu (prefs->reply_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "style"));
	mail_config_set_default_reply_style (val);
	
	/* Keyboard Shortcuts */
	/* FIXME: implement me */
	
	/* Signatures */
	/* FIXME: implement me */
}
