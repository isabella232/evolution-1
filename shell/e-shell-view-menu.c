/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Miguel de Icaza
 *   Ettore Perazzoli
 */

#include <config.h>
#include <gnome.h>

#include "e-shell-folder-creation-dialog.h"
#include "e-shell-folder-selection-dialog.h"

#include "e-shell-constants.h"

#include "e-shell-view-menu.h"
#include <bonobo.h>


/* EShellView callbacks.  */

static void
shortcut_bar_mode_changed_cb (EShellView *shell_view,
			      EShellViewSubwindowMode new_mode,
			      void *data)
{
	BonoboUIHandler *uih;
	const char *path;
	gboolean toggle_state;

	if (new_mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN)
		toggle_state = FALSE;
	else
		toggle_state = TRUE;

	path = (const char *) data;
	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	bonobo_ui_handler_menu_set_toggle_state (uih, path, toggle_state);
}

static void
folder_bar_mode_changed_cb (EShellView *shell_view,
			    EShellViewSubwindowMode new_mode,
			    void *data)
{
	BonoboUIHandler *uih;
	const char *path;
	gboolean toggle_state;

	if (new_mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN)
		toggle_state = FALSE;
	else
		toggle_state = TRUE;

	path = (const char *) data;
	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	bonobo_ui_handler_menu_set_toggle_state (uih, path, toggle_state);
}


/* Command callbacks.  */
static void
command_quit (BonoboUIHandler *uih,
	      void *data,
	      const char *path)
{
	EShellView *shell_view;
	EShell *shell;

	shell_view = E_SHELL_VIEW (data);

	shell = e_shell_view_get_shell (shell_view);
	e_shell_quit (shell);
}

static void
command_run_bugbuddy (BonoboUIHandler *uih,
		      void *data,
		      const char *path)
{
        int pid;
        char *args[] = {
                "bug-buddy",
                "--sm-disable",
                "--package=evolution",
                "--package-ver="VERSION,
                NULL
        };
        args[0] = gnome_is_program_in_path ("bug-buddy");
        if (!args[0]) {
                /* you might have to call gnome_dialog_run() on the
                 * dialog returned here, I don't remember...
                 */
                gnome_error_dialog (_("Bug buddy was not found in your $PATH."));
        }
        pid = gnome_execute_async (NULL, 4, args);
        g_free (args[0]);
        if (pid == -1) {
                /* same as above */
                gnome_error_dialog (_("Bug buddy could not be run."));
        }
}

static void
zero_pointer(GtkObject *object, void **pointer)
{
	*pointer = NULL;
}

static void
command_about_box (BonoboUIHandler *uih,
		   void *data,
		   const char *path)
{
	static GtkWidget *about_box = NULL;

	if (about_box)
		gdk_window_raise(GTK_WIDGET(about_box)->window);
	else {
		const gchar *authors[] = {
			"Seth Alves",
			"Anders Carlsson",
			"Damon Chaplin",
			"Clifford R. Conover",
			"Miguel de Icaza",
			"Radek Doulik",
			"Arturo Espinoza",
			"Larry Ewing",
			"Nat Friedman",
			"Bertrand Guiheneuf",
			"Tuomas Kuosmanen",
			"Christopher J. Lahey",
			"Matthew Loper",
			"Federico Mena",
			"Eskil Heyn Olsen",
			"Ettore Perazzoli",
			"Jeffrey Stedfast",
			"Russell Steinthal",
			"Peter Teichman",
			"Chris Toshok",
			"Peter Williams",
			"Dan Winship",
			"Michael Zucchi",
			NULL};

		about_box = gnome_about_new(_("Evolution"),
					    VERSION,
					    _("Copyright 1999, 2000 Helix Code, Inc."),
					    authors,
					    _("Evolution is a suite of groupware applications\n"
					      "for mail, calendaring, and contact management\n"
					      "within the GNOME desktop environment."),
					    NULL);
		gtk_signal_connect(GTK_OBJECT(about_box), "destroy",
				   GTK_SIGNAL_FUNC(zero_pointer), &about_box);
		gtk_widget_show(about_box);
	}
}

static void
command_help (BonoboUIHandler *uih,
	      void *data,
	      const char *path)
{
	char *url;

	url = g_strdup_printf ("ghelp:%s/gnome/help/evolution/C/%s",
			       EVOLUTION_DATADIR, (char *)data);
	gnome_url_show (url);
}

static void
command_toggle_folder_bar (BonoboUIComponent           *component,
			   const char                  *path,
			   Bonobo_UIComponent_EventType type,
			   const char                  *state,
			   gpointer                     user_data)
{
	EShellView *shell_view;
	EShellViewSubwindowMode mode;
	gboolean show;

	shell_view = E_SHELL_VIEW (user_data);

	show = atoi (state);
	if (show)
		mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	else
		mode = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	e_shell_view_set_folder_bar_mode (shell_view, mode);
}

static void
command_toggle_shortcut_bar (BonoboUIComponent           *component,
			     const char                  *path,
			     Bonobo_UIComponent_EventType type,
			     const char                  *state,
			     gpointer                     user_data)

{
	EShellView *shell_view;
	EShellViewSubwindowMode mode;
	gboolean show;

	shell_view = E_SHELL_VIEW (user_data);

	show = atoi (state);

	if (show)
		mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	else
		mode = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	e_shell_view_set_shortcut_bar_mode (shell_view, mode);
}


static void
command_new_folder (BonoboUIHandler *uih,
		    void *data,
		    const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;
	const char *default_parent_folder;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);
	current_uri = e_shell_view_get_current_uri (shell_view);

	if (strncmp (current_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0)
		default_parent_folder = current_uri + E_SHELL_URI_PREFIX_LEN;
	else
		default_parent_folder = NULL;

	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view),
					     default_parent_folder);
}

static void
command_new_view (BonoboUIHandler *uih,
		  void *data,
		  const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);
	current_uri = e_shell_view_get_current_uri (shell_view);

	e_shell_new_view (shell, current_uri);
}


/* Going to a folder.  */

static void
folder_selection_dialog_cancelled_cb (EShellFolderSelectionDialog *folder_selection_dialog,
				      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
folder_selection_dialog_folder_selected_cb (EShellFolderSelectionDialog *folder_selection_dialog,
					    const char *path,
					    void *data)
{
	if (path != NULL) {
		EShellView *shell_view;
		char *uri;

		shell_view = E_SHELL_VIEW (data);

		uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);
		e_shell_view_display_uri (shell_view, uri);
		g_free (uri);
	}
}

static void
command_goto_folder (BonoboUIHandler *uih,
		     void *data,
		     const char *path)
{
	GtkWidget *folder_selection_dialog;
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	current_uri = e_shell_view_get_current_uri (shell_view);

	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell,
								       _("Go to folder..."),
								       current_uri,
								       NULL);

	gtk_window_set_transient_for (GTK_WINDOW (folder_selection_dialog), GTK_WINDOW (shell_view));

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "folder_selected",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_folder_selected_cb), shell_view);
	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "cancelled",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_cancelled_cb), shell_view);

	gtk_widget_show (folder_selection_dialog);
}

static void
command_create_folder (BonoboUIHandler *uih,
		       void *data,
		       const char *path)
{
	EShellView *shell_view;
	EShell *shell;
	const char *current_uri;
	const char *default_folder;

	shell_view = E_SHELL_VIEW (data);
	shell = e_shell_view_get_shell (shell_view);

	current_uri = e_shell_view_get_current_uri (shell_view);

	if (strncmp (current_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0)
		default_folder = current_uri + E_SHELL_URI_PREFIX_LEN;
	else
		default_folder = NULL;

	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view), default_folder);
}

static void
command_xml_dump (gpointer         dummy,
		  EShellView      *view)
{
	BonoboUIHandler *uih;
	BonoboWin *win;

	uih = e_shell_view_get_bonobo_ui_handler (view);
	
	win = bonobo_ui_handler_get_app (uih);
       
	bonobo_win_dump (win, "On demand");
}


/* Unimplemented commands.  */

#define DEFINE_UNIMPLEMENTED(func)					\
static void								\
func (BonoboUIHandler *uih, void *data, const char *path)		\
{									\
	g_warning ("EShellView: %s: not implemented.", __FUNCTION__);	\
}									\

DEFINE_UNIMPLEMENTED (command_new_shortcut)
DEFINE_UNIMPLEMENTED (command_new_mail_message)
DEFINE_UNIMPLEMENTED (command_new_contact)
DEFINE_UNIMPLEMENTED (command_new_task_request)

BonoboUIVerb new_verbs [] = {
	BONOBO_UI_VERB ("NewView", command_new_view),
	BONOBO_UI_VERB ("NewFolder", command_new_folder),
	BONOBO_UI_VERB ("NewShortcut", command_new_shortcut),
	BONOBO_UI_VERB ("NewMailMessage", command_new_mail_message),

	BONOBO_UI_VERB ("NewAppointment", command_new_shortcut),
	BONOBO_UI_VERB ("NewContact", command_new_contact),
	BONOBO_UI_VERB ("NewTask", command_new_task_request),

	BONOBO_UI_VERB_END
};

BonoboUIVerb file_verbs [] = {
	BONOBO_UI_VERB ("FileGoToFolder", command_goto_folder),
	BONOBO_UI_VERB ("FileCreateFolder", command_create_folder),
	BONOBO_UI_VERB ("FileExit", command_quit),

	BONOBO_UI_VERB_END
};

BonoboUIVerb help_verbs [] = {
	BONOBO_UI_VERB_DATA ("HelpIndex", command_help, "index.html"),
	BONOBO_UI_VERB_DATA ("HelpGetStarted", command_help, "usage-mainwindow.html"),
	BONOBO_UI_VERB_DATA ("HelpUsingMail", command_help, "usage-mail.html"),
	BONOBO_UI_VERB_DATA ("HelpUsingCalendar", command_help, "usage-calendar.html"),
	BONOBO_UI_VERB_DATA ("HelpUsingContact", command_help, "usage-contact.html"),
	BONOBO_UI_VERB ("DumpXML", command_xml_dump),

	BONOBO_UI_VERB_END
};

static void
menu_do_misc (BonoboUIComponent *component,
	      EShellView        *shell_view)
{
	bonobo_ui_component_add_listener (
		component, "ViewShortcutBar",
		command_toggle_shortcut_bar, shell_view);
	bonobo_ui_component_add_listener (
		component, "ViewFolderBar",
		command_toggle_folder_bar, shell_view);
	bonobo_ui_component_add_verb (
		component, "HelpSubmitBug",
		(BonoboUIVerbFn) command_run_bugbuddy, shell_view);
	bonobo_ui_component_add_verb (
		component, "HelpAbout",
		(BonoboUIVerbFn) command_about_box, shell_view);
}


#define SHORTCUT_BAR_TOGGLE_PATH "/View/ShortcutBar"
#define FOLDER_BAR_TOGGLE_PATH "/View/FolderBar"

void
e_shell_view_menu_setup (EShellView *shell_view)
{
	BonoboUIHandler *uih;
	BonoboUIComponent *component;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	uih = e_shell_view_get_bonobo_ui_handler (shell_view);

	component = bonobo_ui_compat_get_component (uih);

	bonobo_ui_component_add_verb_list_with_data (
		component, file_verbs, shell_view);

	bonobo_ui_component_add_verb_list_with_data (
		component, new_verbs, shell_view);

	bonobo_ui_component_add_verb_list_with_data (
		component, help_verbs, shell_view);

	menu_do_misc (component, shell_view);

	gtk_signal_connect (GTK_OBJECT (shell_view), "shortcut_bar_mode_changed",
			    GTK_SIGNAL_FUNC (shortcut_bar_mode_changed_cb),
			    SHORTCUT_BAR_TOGGLE_PATH);
	gtk_signal_connect (GTK_OBJECT (shell_view), "folder_bar_mode_changed",
			    GTK_SIGNAL_FUNC (folder_bar_mode_changed_cb),
			    FOLDER_BAR_TOGGLE_PATH);

	/* Initialize the toggles.  Yeah, this is, well, yuck.  */
	folder_bar_mode_changed_cb   (shell_view, e_shell_view_get_folder_bar_mode (shell_view),
				      FOLDER_BAR_TOGGLE_PATH);
	shortcut_bar_mode_changed_cb (shell_view, e_shell_view_get_shortcut_bar_mode (shell_view),
				      SHORTCUT_BAR_TOGGLE_PATH);
}
