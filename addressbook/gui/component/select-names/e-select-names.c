/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-select-names.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-without.h>
#include <gal/widgets/e-font.h>
#include <gal/widgets/e-popup-menu.h>

#include <addressbook/gui/widgets/e-addressbook-model.h>
#include <addressbook/gui/widgets/e-addressbook-table-adapter.h>
#include <addressbook/gui/component/e-cardlist-model.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/gui/component/addressbook-component.h>
#include <addressbook/gui/component/addressbook-storage.h>
#include <addressbook/gui/component/addressbook.h>
#include <shell/evolution-shell-client.h>
#include <shell/evolution-folder-selector-button.h>

#include "e-select-names.h"
#include <addressbook/backend/ebook/e-card-simple.h>
#include "e-select-names-text-model.h"
#include <gal/widgets/e-categories-master-list-option-menu.h>
#include <gal/widgets/e-unicode.h>
#include <gal/e-text/e-entry.h>
#include <e-util/e-categories-master-list-wombat.h>

static void e_select_names_init		(ESelectNames		 *card);
static void e_select_names_class_init	(ESelectNamesClass	 *klass);
static void e_select_names_dispose (GObject *object);
static void update_query (GtkWidget *widget, ESelectNames *e_select_names);

extern EvolutionShellClient *global_shell_client;

static GnomeDialogClass *parent_class = NULL;
#define PARENT_TYPE gnome_dialog_get_type()

/* The arguments we take */
enum {
	ARG_0,
};

typedef struct {
	char                  *title;
	ESelectNamesModel     *source;
	ESelectNamesTextModel *text_model;
	ESelectNames          *names;
	GtkWidget             *label;
	GtkWidget             *button;
} ESelectNamesChild;

GType
e_select_names_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESelectNamesClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNames),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_init,
		};

		type = g_type_register_static (PARENT_TYPE, "ESelectNames", &info, 0);
	}

	return type;
}

static void
e_select_names_class_init (ESelectNamesClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_select_names_dispose;
}

GtkWidget *e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2);
GtkWidget *e_addressbook_create_folder_selector(char *name, char *string1, char *string2, int num1, int num2);

static void
set_book(EBook *book, EBookStatus status, ESelectNames *esn)
{
	g_object_set(esn->model,
		     "book", book,
		     NULL);
	update_query (NULL, esn);
	g_object_unref(book);
	g_object_unref(esn->model);
	g_object_unref(esn);
}

static void
addressbook_model_set_uri(ESelectNames *e_select_names, EAddressbookModel *model, const char *uri)
{
	EBook *book;
	char *book_uri;

	book_uri = e_book_expand_uri (uri);

	/* If uri == the current uri, then we don't have to do anything */
	book = e_addressbook_model_get_ebook (model);
	if (book) {
		const gchar *current_uri = e_book_get_uri (book);
		if (current_uri && !strcmp (book_uri, current_uri)) {
			g_free (book_uri);
			return;
		}
	}

	book = e_book_new();

	g_object_ref(e_select_names);
	g_object_ref(model);
	addressbook_load_uri(book, book_uri, (EBookCallback) set_book, e_select_names);

	g_free (book_uri);
}

static void *
card_key (ECard *card)
{
	EBook *book;
	const gchar *book_uri;
	
	if (card == NULL)
		return NULL;

	g_assert (E_IS_CARD (card));

	book = e_card_get_book (card);
	book_uri = book ? e_book_get_uri (book) : "NoBook";
	return g_strdup_printf ("%s|%s", book_uri ? book_uri : "NoURI", e_card_get_id (card));
}

static void
sync_one_model (gpointer k, gpointer val, gpointer closure)
{
	ETableWithout *etw = E_TABLE_WITHOUT (closure);
	ESelectNamesChild *child = val;
	ESelectNamesModel *model = child->source;
	gint i, count;
	ECard *card;
	void *key;
	
	count = e_select_names_model_count (model);
	for (i = 0; i < count; ++i) {
		card = e_select_names_model_get_card (model, i);
		if (card) {
			key = card_key (card);
			e_table_without_hide (etw, key);
			g_free (key);
		}
	}
}

static void
sync_table_and_models (ESelectNamesModel *triggering_model, ESelectNames *esl)
{
	e_table_without_show_all (E_TABLE_WITHOUT (esl->without));
	g_hash_table_foreach (esl->children, sync_one_model, esl->without);
}

static void
real_add_address_cb (int model_row, gpointer closure)
{
	ESelectNamesChild *child = closure;
	ESelectNames *names = child->names;
	ECard *card;
	EDestination *dest = e_destination_new ();
	gint mapped_row;

	mapped_row = e_table_subset_view_to_model_row (E_TABLE_SUBSET (names->without), model_row);

	card = e_addressbook_model_get_card (E_ADDRESSBOOK_MODEL(names->model), mapped_row);
	
	if (card != NULL) {
		e_destination_set_card (dest, card, 0);

		e_select_names_model_append (child->source, dest);
		e_select_names_model_clean (child->source, FALSE);

		g_object_unref(card);
	}
}

static void
real_add_address(ESelectNames *names, ESelectNamesChild *child)
{
	e_select_names_model_freeze (child->source);
	e_table_selected_row_foreach(e_table_scrolled_get_table(names->table),
				     real_add_address_cb, child);
	e_select_names_model_thaw (child->source);
}

static void
add_address(ETable *table, int row, int col, GdkEvent *event, ESelectNames *names)
{
	ESelectNamesChild *child;

	child = g_hash_table_lookup(names->children, names->def);
	if (child) {
		real_add_address(names, child);
	}
}

static void
sensitize_button (gpointer key, gpointer data, gpointer user_data)
{
	gboolean *sensitive = user_data;
	ESelectNamesChild *child = data;

	gtk_widget_set_sensitive (child->button, *sensitive);
}

static void
selection_change (ETable *table, ESelectNames *names)
{
	gboolean sensitive;

	sensitive = e_table_selected_count (table) > 0;

	g_hash_table_foreach (names->children, sensitize_button, &sensitive);
}

static void *
esn_get_key_fn (ETableModel *source, int row, void *closure)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL (closure);
	ECard *card = e_addressbook_model_get_card (model, row);
	void *key = card_key (card);
	g_object_unref (card);
	return key;
}

static void *
esn_dup_key_fn (const void *key, void *closure)
{
	void *dup = (void *) g_strdup ((const gchar *) key);
	return dup;
}

static void
esn_free_gotten_key_fn (void *key, void *closure)
{
	g_free (key);
}

static void
esn_free_duped_key_fn (void *key, void *closure)
{
	g_free (key);
}

GtkWidget *
e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2)
{
	ETableModel *adapter;
	ETableModel *without;
	EAddressbookModel *model;
	GtkWidget *table;
	char *spec;

	model = e_addressbook_model_new ();
	adapter = E_TABLE_MODEL (e_addressbook_table_adapter_new (model));

	g_object_set(model,
		     "editable", FALSE,
		     NULL);

	without = e_table_without_new (adapter,
				       g_str_hash,
				       g_str_equal,
				       esn_get_key_fn,
				       esn_dup_key_fn,
				       esn_free_gotten_key_fn,
				       esn_free_duped_key_fn,
				       model);

	table = e_table_scrolled_new_from_spec_file (without,
						     NULL,
						     EVOLUTION_ETSPECDIR "/e-select-names.etspec",
						     NULL);

	g_object_set_data(G_OBJECT(table), "adapter", adapter);
	g_object_set_data(G_OBJECT(table), "without", without);
	g_object_set_data(G_OBJECT(table), "model", model);

	return table;
}

GtkWidget *
e_addressbook_create_folder_selector(char *name, char *string1, char *string2, int num1, int num2)
{
	return g_object_new (EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON, NULL);
}

static void
folder_selected (EvolutionFolderSelectorButton *button, GNOME_Evolution_Folder *folder,
		 ESelectNames *e_select_names)
{
	addressbook_model_set_uri(e_select_names, e_select_names->model, folder->physicalUri);

	e_config_listener_set_string (e_book_get_config_database(),
				      "/Addressbook/select_names_uri", folder->physicalUri);
}

static void
update_query (GtkWidget *widget, ESelectNames *e_select_names)
{
	char *category = "";
	const char *search = "";
	char *query;
	char *q_array[4];
	int i;
	if (e_select_names->categories) {
		category = e_categories_master_list_option_menu_get_category (E_CATEGORIES_MASTER_LIST_OPTION_MENU (e_select_names->categories));
	}
	if (e_select_names->select_entry) {
		search = gtk_entry_get_text (GTK_ENTRY (e_select_names->select_entry));
	}
	i = 0;
	q_array[i++] = "(contains \"email\" \"\")";
	if (category && *category)
		q_array[i++] = g_strdup_printf ("(is \"category\" \"%s\")", category);
	if (search && *search)
		q_array[i++] = g_strdup_printf ("(or (beginswith \"email\" \"%s\") "
						"    (beginswith \"full_name\" \"%s\") "
						"    (beginswith \"nickname\" \"%s\")"
						"    (beginswith \"file_as\" \"%s\"))",
						search, search, search, search);
	q_array[i++] = NULL;
	if (i > 2) {
		char *temp = g_strjoinv (" ", q_array);
		query = g_strdup_printf ("(and %s)", temp);
		g_free (temp);
	} else {
		query = g_strdup (q_array[0]);
	}
	g_object_set (e_select_names->model,
		      "query", query,
		      NULL);
	for (i = 1; q_array[i]; i++) {
		g_free (q_array[i]);
	}
	g_free (query);
}

static void
status_message (EAddressbookModel *model, const gchar *message, ESelectNames *e_select_names)
{
	if (message == NULL)
		gtk_label_set_text (GTK_LABEL (e_select_names->status_message), "");
	else
		gtk_label_set_text (GTK_LABEL (e_select_names->status_message), message);
}

static void
categories_changed (GtkWidget *widget, gint value, ESelectNames *e_select_names)
{
	update_query (widget, e_select_names);
}

static void
select_entry_changed (GtkWidget *widget, ESelectNames *e_select_names)
{
	if (e_select_names->select_entry) {
		const char *select_string = gtk_entry_get_text (GTK_ENTRY (e_select_names->select_entry));
		char *select_strcoll_string = g_utf8_collate_key (select_string, -1);
		int count;
		ETable *table;
		int i;

		table = e_table_scrolled_get_table (e_select_names->table);

		count = e_table_model_row_count (e_select_names->without);

		for (i = 0; i < count; i++) {
			int model_row = e_table_view_to_model_row (table, i);
			char *row_strcoll_string =
				g_utf8_collate_key (e_table_model_value_at (e_select_names->without,
									    E_CARD_SIMPLE_FIELD_NAME_OR_ORG,
									    model_row),
						    -1);
			if (strcmp (select_strcoll_string, row_strcoll_string) <= 0) {
				g_free (row_strcoll_string);
				break;
			}
			g_free (row_strcoll_string);
		}
		g_free (select_strcoll_string);
		if (i == count)
			i --;

		if (count > 0) {
			i = e_table_view_to_model_row (table, i);
			e_table_set_cursor_row (table, i);
		}
	}
}

GtkWidget *e_select_names_create_categories (gchar *name,
					     gchar *string1, gchar *string2,
					     gint int1, gint int2);

GtkWidget *
e_select_names_create_categories (gchar *name,
				  gchar *string1, gchar *string2,
				  gint int1, gint int2)
{
#ifdef PENDING_PORT_WORK
	ECategoriesMasterList *ecml;
	GtkWidget *option_menu;

	ecml = e_categories_master_list_wombat_new ();
	option_menu = e_categories_master_list_option_menu_new (ecml);
	g_object_unref (ecml);

	return option_menu;
#else
	return gtk_label_new ("e_select_names_create_categories\nneeds work");
#endif
}

static void
clear_widget (GtkWidget *w, gpointer user_data)
{
	GtkWidget **widget_ref = user_data;
	*widget_ref = NULL;
}

static void
e_select_names_init (ESelectNames *e_select_names)
{
	GladeXML *gui;
	GtkWidget *widget, *button;
	const char *selector_types[] = { "contacts/*", NULL };
	char *filename;
	char *contacts_uri;
	EConfigListener *db;

	db = e_book_get_config_database();

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/select-names.glade", NULL, NULL);
	e_select_names->gui = gui;

	e_select_names->children = g_hash_table_new(g_str_hash, g_str_equal);
	e_select_names->child_count = 0;
	e_select_names->def = NULL;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(e_select_names)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gnome_dialog_append_buttons(GNOME_DIALOG(e_select_names),
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL,
				    NULL);
	gnome_dialog_set_default(GNOME_DIALOG(e_select_names), 0);

	gtk_window_set_title(GTK_WINDOW(e_select_names), _("Select Contacts from Addressbook")); 
	gtk_window_set_policy(GTK_WINDOW(e_select_names), FALSE, TRUE, FALSE);

	e_select_names->table = E_TABLE_SCROLLED(glade_xml_get_widget(gui, "table-source"));
	e_select_names->model = g_object_get_data(G_OBJECT(e_select_names->table), "model");
	e_select_names->adapter = g_object_get_data(G_OBJECT(e_select_names->table), "adapter");
	e_select_names->without = g_object_get_data(G_OBJECT(e_select_names->table), "without");

	e_select_names->status_message = glade_xml_get_widget (gui, "status-message");
	if (e_select_names->status_message && !GTK_IS_LABEL (e_select_names->status_message))
		e_select_names->status_message = NULL;
	if (e_select_names->status_message) {
		g_signal_connect (e_select_names->model, "status_message",
				    G_CALLBACK (status_message), e_select_names);
		g_signal_connect(e_select_names->status_message, "destroy",
				   G_CALLBACK(clear_widget), &e_select_names->status_message);
	}

	e_select_names->categories = glade_xml_get_widget (gui, "custom-categories");
	if (e_select_names->categories && !E_IS_CATEGORIES_MASTER_LIST_OPTION_MENU (e_select_names->categories))
		e_select_names->categories = NULL;
	if (e_select_names->categories) {
		g_signal_connect(e_select_names->categories, "changed",
				   G_CALLBACK(categories_changed), e_select_names);
		g_signal_connect(e_select_names->categories, "destroy",
				   G_CALLBACK(clear_widget), &e_select_names->categories);
	}

	e_select_names->select_entry = glade_xml_get_widget (gui, "entry-select");
	if (e_select_names->select_entry && !GTK_IS_ENTRY (e_select_names->select_entry))
		e_select_names->select_entry = NULL;
	if (e_select_names->select_entry) {
		g_signal_connect(e_select_names->select_entry, "changed",
				   G_CALLBACK(select_entry_changed), e_select_names);
		g_signal_connect(e_select_names->select_entry, "activate",
				   G_CALLBACK(update_query), e_select_names);
		g_signal_connect(e_select_names->select_entry, "destroy",
				   G_CALLBACK(clear_widget), &e_select_names->select_entry);
	}

	button  = glade_xml_get_widget (gui, "button-find");
	if (button && GTK_IS_BUTTON (button))
		g_signal_connect(button, "clicked",
				   G_CALLBACK(update_query), e_select_names);

	contacts_uri = e_config_listener_get_string_with_default (db, "/Addressbook/select_names_uri", NULL, NULL);
	if (!contacts_uri) {
		contacts_uri = e_config_listener_get_string_with_default (db, "/DefaultFolders/contacts_uri",
									  NULL, NULL);
	}
	if (!contacts_uri) {
		filename = gnome_util_prepend_user_home("evolution/local/Contacts");
		contacts_uri = g_strdup_printf("file://%s", filename);
		g_free (filename);
	}

	button = glade_xml_get_widget (gui, "folder-selector");
	evolution_folder_selector_button_construct (EVOLUTION_FOLDER_SELECTOR_BUTTON (button),
						    global_shell_client,
						    _("Find contact in"),
						    contacts_uri,
						    selector_types);
	if (button && EVOLUTION_IS_FOLDER_SELECTOR_BUTTON (button))
		g_signal_connect(button, "selected",
				   G_CALLBACK(folder_selected), e_select_names);

	g_signal_connect (e_table_scrolled_get_table (e_select_names->table), "double_click",
			    G_CALLBACK (add_address), e_select_names);
	g_signal_connect (e_table_scrolled_get_table (e_select_names->table), "selection_change",
			    G_CALLBACK (selection_change), e_select_names);
	selection_change (e_table_scrolled_get_table (e_select_names->table), e_select_names);

	addressbook_model_set_uri(e_select_names, e_select_names->model, contacts_uri);

	g_free (contacts_uri);
}

static void e_select_names_child_free(char *key, ESelectNamesChild *child, ESelectNames *e_select_names)
{
	g_signal_handlers_disconnect_matched (child->source,
					      (GSignalMatchType) (G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA),
					      0, 0, NULL,
					      G_CALLBACK (sync_table_and_models), e_select_names);
	g_free(child->title);
	g_object_unref(child->text_model);
	g_object_unref(child->source);
	g_free(key);
	g_free(child);
}

static void
e_select_names_dispose (GObject *object)
{
	ESelectNames *e_select_names = E_SELECT_NAMES(object);

	g_object_unref(e_select_names->gui);
	g_hash_table_foreach(e_select_names->children, (GHFunc) e_select_names_child_free, e_select_names);
	g_hash_table_destroy(e_select_names->children);
	g_object_unref(e_select_names->without);
	g_object_unref(e_select_names->adapter);
	g_object_unref(e_select_names->model);

	g_free(e_select_names->def);

	(*(G_OBJECT_CLASS(parent_class))->dispose)(object);
}

GtkWidget*
e_select_names_new (void)
{
	GtkWidget *widget = g_object_new (E_TYPE_SELECT_NAMES, NULL);
	return widget;
}

static void
button_clicked(GtkWidget *button, ESelectNamesChild *child)
{
	real_add_address(child->names, child);
}

#if 0
static void
remove_address(ETable *table, int row, int col, GdkEvent *event, ESelectNamesChild *child)
{
	e_select_names_model_delete (child->source, row);
}
#endif

struct _RightClickData {
	ESelectNamesChild *child;
	int index;
};
typedef struct _RightClickData RightClickData;

#if 0
static GSList *selected_rows = NULL;

static void
etable_selection_foreach_cb (int row, void *data)
{
	/* Build a list of rows in reverse order, then remove them,
           necessary because otherwise it'll start trying to delete
           rows out of index in ETableModel */
	selected_rows = g_slist_prepend (selected_rows, GINT_TO_POINTER (row));
}

static void
selected_rows_foreach_cb (void *row, void *data)
{
	ESelectNamesChild *child = data;

	remove_address (NULL, GPOINTER_TO_INT (row), 0, NULL, child);
}
#endif

static void
remove_cb (GtkWidget *widget, void *data)
{
	RightClickData *rcdata = (RightClickData *)data;

	e_select_names_model_delete (rcdata->child->source, rcdata->index);

	/* Free everything we've created */
	g_free (rcdata);
}

static void
section_right_click_cb (EText *text, GdkEventButton *ev, gint pos, ESelectNamesChild *child)
{
	EPopupMenu right_click_menu[] = {
		E_POPUP_ITEM (N_("Remove"), G_CALLBACK (remove_cb), 0),
		E_POPUP_TERMINATOR
	};
	gint index;

	e_select_names_model_text_pos (child->source, child->text_model->seplen, pos, &index, NULL, NULL);

	if (index != -1) {
		RightClickData *rcdata = g_new0 (RightClickData, 1);
		rcdata->index = index;
		rcdata->child = child;

		e_popup_menu_run (right_click_menu, (GdkEvent *)ev, 0, 0, rcdata);
	}
}

void
e_select_names_add_section(ESelectNames *e_select_names, char *name, char *id, ESelectNamesModel *source)
{
	ESelectNamesChild *child;
	GtkWidget *button;
	GtkWidget *alignment;
	GtkWidget *label;
	GtkTable *table;
	char *label_text;
	ETable *etable;

	GtkWidget *sw;
	GtkWidget *recipient_table;

	if (g_hash_table_lookup(e_select_names->children, id)) {
		return;
	}

	table = GTK_TABLE(glade_xml_get_widget (e_select_names->gui, "table-recipients"));

	child = g_new(ESelectNamesChild, 1);

	child->names = e_select_names;
	child->title = e_utf8_from_locale_string(_(name));

	child->text_model = (ESelectNamesTextModel *) e_select_names_text_model_new (source);
	e_select_names_text_model_set_separator (child->text_model, "\n");

	child->source = source;
	g_object_ref(child->source);

	e_select_names->child_count++;

	alignment = gtk_alignment_new(0, 0, 1, 0);

	button = gtk_button_new ();

	label = e_entry_new ();
	g_object_set(label,
		     "draw_background", FALSE,
		     "draw_borders", FALSE,
		     "draw_button", TRUE,
		     "editable", FALSE,
		     "text", "",
		     "use_ellipsis", FALSE,
		     "justification", GTK_JUSTIFY_CENTER,
		     NULL);

	label_text = g_strconcat (child->title, " ->", NULL);
	g_object_set (label,
		      "text", label_text,
		      "emulate_label_resize", TRUE,
		      NULL);
	g_free (label_text);
	gtk_container_add (GTK_CONTAINER (button), label);
	child->label = label;
	child->button = button;

	gtk_container_add(GTK_CONTAINER(alignment), button);
	gtk_widget_show_all(alignment);
	g_signal_connect(button, "clicked",
			   G_CALLBACK(button_clicked), child);
	gtk_table_attach(table, alignment,
			 0, 1,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL, GTK_FILL,
			 0, 0);

	etable = e_table_scrolled_get_table (e_select_names->table);
	gtk_widget_set_sensitive (button, e_table_selected_count (etable) > 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	recipient_table = e_entry_new ();
	g_object_set (recipient_table,
		      "model", child->text_model,
		      "allow_newlines", TRUE,
		      NULL);

	g_signal_connect (recipient_table,
			    "popup",
			    G_CALLBACK (section_right_click_cb),
			    child);

	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), recipient_table);
	
#if 0
	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(etable)), "right_click",
			   G_CALLBACK(section_right_click_cb), child);
	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(etable)), "double_click",
			   G_CALLBACK(remove_address), child);
#endif


	g_signal_connect (child->source,
			    "changed",
			    G_CALLBACK (sync_table_and_models),
			    e_select_names);
	
	gtk_widget_show_all (sw);
	
	gtk_table_attach(table, sw,
			 1, 2,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	g_hash_table_insert(e_select_names->children, g_strdup(id), child);

	sync_table_and_models (child->source, e_select_names);
}

static void *
card_copy(const void *value, void *closure)
{
	g_object_ref((gpointer)value);
	return (void *)value;
}

static void
card_free(void *value, void *closure)
{
	g_object_unref((gpointer)value);
}

EList *
e_select_names_get_section(ESelectNames *e_select_names, char *id)
{
	ESelectNamesChild *child;
	int i;
	int rows;
	EList *list;
	
	child = g_hash_table_lookup(e_select_names->children, id);
	if (!child)
		return NULL;
	rows = e_select_names_model_count (child->source);
	
	list = e_list_new(card_copy, card_free, NULL);
	for (i = 0; i < rows; i++) {
		ECard *card = e_select_names_model_get_card (child->source, i);
		e_list_append(list, card);
		g_object_unref(card);
	}
	return list;
}

ESelectNamesModel *
e_select_names_get_source(ESelectNames *e_select_names,
			  char *id)
{
	ESelectNamesChild *child = g_hash_table_lookup(e_select_names->children, id);
	if (child) {
		if (child->source)
			g_object_ref(child->source);
		return child->source;
	} else
		return NULL;
}

void
e_select_names_set_default (ESelectNames *e_select_names,
			    const char *id)
{
	ESelectNamesChild *child;

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child)
			g_object_set (E_ENTRY (child->label)->item,
				      "bold", FALSE,
				      NULL);
	}

	g_free(e_select_names->def);
	e_select_names->def = g_strdup(id);

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child)
			g_object_set (E_ENTRY (child->label)->item,
				      "bold", TRUE,
				      NULL);
	}
}
