/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <e-util/e-mktemp.h>

#include <camel/camel-file-utils.h>

#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-mt.h"

#include "em-utils.h"

#include "em-marshal.h"
#include "em-folder-tree-model.h"


#define d(x) x

static GType col_types[] = {
	G_TYPE_STRING,   /* display name */
	G_TYPE_POINTER,  /* store object */
	G_TYPE_STRING,   /* path */
	G_TYPE_STRING,   /* uri */
	G_TYPE_UINT,     /* unread count */
	G_TYPE_BOOLEAN,  /* is a store node */
	G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
};


/* Drag & Drop types */
enum DndDragType {
	DND_DRAG_TYPE_FOLDER,          /* drag an evo folder */
	DND_DRAG_TYPE_TEXT_URI_LIST,   /* drag to an mbox file */
	NUM_DRAG_TYPES
};

enum DndDropType {
	DND_DROP_TYPE_UID_LIST,        /* drop a list of message uids */
	DND_DROP_TYPE_FOLDER,          /* drop an evo folder */
	DND_DROP_TYPE_MESSAGE_RFC822,  /* drop a message/rfc822 stream */
	DND_DROP_TYPE_TEXT_URI_LIST,   /* drop an mbox file */
	NUM_DROP_TYPES
};

static GtkTargetEntry drag_types[] = {
	{ "x-folder",         0, DND_DRAG_TYPE_FOLDER         },
	{ "text/uri-list",    0, DND_DRAG_TYPE_TEXT_URI_LIST  },
};

static GtkTargetEntry drop_types[] = {
	{ "x-uid-list" ,      0, DND_DROP_TYPE_UID_LIST       },
	{ "x-folder",         0, DND_DROP_TYPE_FOLDER         },
	{ "message/rfc822",   0, DND_DROP_TYPE_MESSAGE_RFC822 },
	{ "text/uri-list",    0, DND_DROP_TYPE_TEXT_URI_LIST  },
};

static GdkAtom drag_atoms[NUM_DRAG_TYPES];
static GdkAtom drop_atoms[NUM_DROP_TYPES];


/* GObject virtual method overrides */
static void em_folder_tree_model_class_init (EMFolderTreeModelClass *klass);
static void em_folder_tree_model_init (EMFolderTreeModel *model);
static void em_folder_tree_model_finalize (GObject *obj);

/* interface init methods */
static void tree_model_iface_init (GtkTreeModelIface *iface);
static void tree_sortable_iface_init (GtkTreeSortableIface *iface);


enum {
	LOADING_ROW,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };


static GtkTreeStoreClass *parent_class = NULL;


GType
em_folder_tree_model_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeModelClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_model_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTreeModel),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_model_init,
		};
		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) tree_model_iface_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo sortable_info = {
			(GInterfaceInitFunc) tree_sortable_iface_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GTK_TYPE_TREE_STORE, "EMFolderTreeModel", &info, 0);
		
		g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_SORTABLE,
					     &sortable_info);
	}
	
	return type;
}


static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	int i;
	
	parent_class = g_type_class_ref (GTK_TYPE_TREE_STORE);
	
	object_class->finalize = em_folder_tree_model_finalize;
	
	/* signals */
	signals[LOADING_ROW] =
		g_signal_new ("loading-row",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, loading_row),
			      NULL, NULL,
			      em_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	for (i = 0; i < NUM_DRAG_TYPES; i++)
		drag_atoms[i] = gdk_atom_intern (drag_types[i].target, FALSE);
	
	for (i = 0; i < NUM_DROP_TYPES; i++)
		drop_atoms[i] = gdk_atom_intern (drop_types[i].target, FALSE);
}

static int
sort_cb (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	extern CamelStore *vfolder_store;
	char *aname, *bname;
	CamelStore *store;
	gboolean is_store;
	
	gtk_tree_model_get (model, a, COL_BOOL_IS_STORE, &is_store,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_DISPLAY_NAME, &aname, -1);
	gtk_tree_model_get (model, b, COL_STRING_DISPLAY_NAME, &bname, -1);
	
	if (is_store) {
		/* On This Computer is always first and VFolders is always last */
		if (!strcmp (aname, _("On this Computer")))
			return -1;
		if (!strcmp (bname, _("On this Computer")))
			return 1;
		if (!strcmp (aname, _("VFolders")))
			return 1;
		if (!strcmp (bname, _("VFolders")))
			return -1;
	} else if (store == vfolder_store) {
		/* perform no sorting, we want to display in the same
		 * order as they appear in the VFolder editor - UNMATCHED is always last */
		GtkTreePath *path;
		int ret;
		
		if (aname && !strcmp (aname, _("UNMATCHED")))
			return 1;
		if (bname && !strcmp (bname, _("UNMATCHED")))
			return -1;
		
		path = gtk_tree_model_get_path (model, a);
		if (path) {
			aname = gtk_tree_path_to_string (path);
			gtk_tree_path_free (path);
		} else {
			aname = g_strdup("");
		}
		
		path = gtk_tree_model_get_path (model, b);
		if (path) {
			bname = gtk_tree_path_to_string (path);
			gtk_tree_path_free (path);
		} else {
			bname = g_strdup("");
		}
		
		ret = strcmp (aname, bname);
		g_free (aname);
		g_free (bname);
		
		return ret;
	} else {
		/* Inbox is always first */
		if (aname && (!strcmp (aname, "INBOX") || !strcmp (aname, _("Inbox"))))
			return -1;
		if (bname && (!strcmp (bname, "INBOX") || !strcmp (bname, _("Inbox"))))
			return 1;
	}
	
	if (aname == NULL) {
		if (bname == NULL)
			return 0;
	} else if (bname == NULL)
		return 1;
	
	return g_utf8_collate (aname, bname);
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	model->store_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->uri_hash = g_hash_table_new (g_str_hash, g_str_equal);
	model->expanded = g_hash_table_new (g_str_hash, g_str_equal);
	
	gtk_tree_sortable_set_default_sort_func ((GtkTreeSortable *) model, sort_cb, NULL, NULL);
}

static void
path_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
}

static void
store_info_free (struct _EMFolderTreeModelStoreInfo *si)
{
	camel_object_remove_event (si->store, si->created_id);
	camel_object_remove_event (si->store, si->deleted_id);
	camel_object_remove_event (si->store, si->renamed_id);
	camel_object_remove_event (si->store, si->subscribed_id);
	camel_object_remove_event (si->store, si->unsubscribed_id);
	
	g_free (si->display_name);
	camel_object_unref (si->store);
	gtk_tree_row_reference_free (si->row);
	g_hash_table_foreach (si->path_hash, path_hash_free, NULL);
	g_free (si);
}

static void
store_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	struct _EMFolderTreeModelStoreInfo *si = value;
	
	store_info_free (si);
}

static void
uri_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
}

static gboolean
expanded_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	return TRUE;
}

static void
em_folder_tree_model_finalize (GObject *obj)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) obj;
	
	g_hash_table_foreach (model->store_hash, store_hash_free, NULL);
	g_hash_table_destroy (model->store_hash);
	
	g_hash_table_foreach (model->uri_hash, uri_hash_free, NULL);
	g_hash_table_destroy (model->uri_hash);
	
	g_hash_table_foreach (model->expanded, (GHFunc) expanded_free, NULL);
	g_hash_table_destroy (model->expanded);
	
	g_free (model->filename);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
	;
}

static void
tree_sortable_iface_init (GtkTreeSortableIface *iface)
{
	;
}


static void
em_folder_tree_model_load_state (EMFolderTreeModel *model, const char *filename)
{
	char *node;
	FILE *fp;
	
	g_hash_table_foreach_remove (model->expanded, expanded_free, NULL);
	
	if ((fp = fopen (filename, "r")) == NULL)
		return;
	
	while (camel_file_util_decode_string (fp, &node) != -1)
		g_hash_table_insert (model->expanded, node, GINT_TO_POINTER (TRUE));
	
	fclose (fp);
}


EMFolderTreeModel *
em_folder_tree_model_new (const char *evolution_dir)
{
	EMFolderTreeModel *model;
	char *filename;
	
	model = g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
	gtk_tree_store_set_column_types ((GtkTreeStore *) model, NUM_COLUMNS, col_types);
	gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *) model,
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
	
	filename = g_build_filename (evolution_dir, "mail", "config", "folder-tree.state", NULL);
	em_folder_tree_model_load_state (model, filename);
	model->filename = filename;
	
	return model;
}


void
em_folder_tree_model_set_folder_info (EMFolderTreeModel *model, GtkTreeIter *iter,
				      struct _EMFolderTreeModelStoreInfo *si,
				      CamelFolderInfo *fi)
{
	GtkTreeRowReference *uri_row, *path_row;
	unsigned int unread;
	GtkTreePath *path;
	GtkTreeIter sub;
	gboolean load;
	
	load = fi->child == NULL && !(fi->flags & (CAMEL_FOLDER_NOCHILDREN | CAMEL_FOLDER_NOINFERIORS));
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
	uri_row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	path_row = gtk_tree_row_reference_copy (uri_row);
	gtk_tree_path_free (path);
	
	g_hash_table_insert (model->uri_hash, g_strdup (fi->url), uri_row);
	g_hash_table_insert (si->path_hash, g_strdup (fi->path), path_row);
	
	unread = fi->unread_message_count == -1 ? 0 : fi->unread_message_count;
	
	gtk_tree_store_set ((GtkTreeStore *) model, iter,
			    COL_STRING_DISPLAY_NAME, fi->name,
			    COL_POINTER_CAMEL_STORE, si->store,
			    COL_STRING_FOLDER_PATH, fi->path,
			    COL_STRING_URI, fi->url,
			    COL_UINT_UNREAD, unread,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_BOOL_LOAD_SUBDIRS, load,
			    -1);
	
	if (fi->child) {
		fi = fi->child;
		
		do {
			gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
			em_folder_tree_model_set_folder_info (model, &sub, si, fi);
			fi = fi->sibling;
		} while (fi);
	} else if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
		gtk_tree_store_set ((GtkTreeStore *) model, &sub,
				    COL_STRING_DISPLAY_NAME, _("Loading..."),
				    COL_POINTER_CAMEL_STORE, NULL,
				    COL_STRING_FOLDER_PATH, NULL,
				    COL_BOOL_LOAD_SUBDIRS, FALSE,
				    COL_BOOL_IS_STORE, FALSE,
				    COL_STRING_URI, NULL,
				    COL_UINT_UNREAD, 0,
				    -1);
		
		path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
		g_signal_emit (model, signals[LOADING_ROW], 0, path, iter);
		gtk_tree_path_free (path);
	}
}


static void
folder_subscribed (CamelStore *store, CamelFolderInfo *fi, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter parent, iter;
	GtkTreePath *path;
	gboolean load;
	char *dirname;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;
	
	/* make sure we don't already know about it? */
	if (g_hash_table_lookup (si->path_hash, fi->path))
		goto done;
	
	/* get our parent folder's path */
	if (!(dirname = g_path_get_dirname (fi->path)))
		goto done;
	
	if (!strcmp (dirname, "/")) {
		/* user subscribed to a toplevel folder */
		row = si->row;
		g_free (dirname);
	} else {
		row = g_hash_table_lookup (si->path_hash, dirname);
		g_free (dirname);
		
		/* if row is NULL, don't bother adding to the tree,
		 * when the user expands enough nodes - it will be
		 * added auto-magically */
		if (row == NULL)
			goto done;
	}
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &parent, path))) {
		gtk_tree_path_free (path);
		goto done;
	}
	
	gtk_tree_path_free (path);
	
	/* make sure parent's subfolders have already been loaded */
	gtk_tree_model_get ((GtkTreeModel *) model, &parent, COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		goto done;
	
	/* append a new node */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &parent);
	
	em_folder_tree_model_set_folder_info (model, &iter, si, fi);
	
 done:
	
	camel_object_unref (store);
	camel_folder_info_free (fi);
}

static void
folder_subscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_subscribed, store, fi, model);
}

static void
folder_unsubscribed (CamelStore *store, CamelFolderInfo *fi, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;
	
	if (!(row = g_hash_table_lookup (si->path_hash, fi->path)))
		goto done;
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		goto done;
	}
	
	em_folder_tree_model_remove_folders (model, si, &iter);
	
 done:
	
	camel_object_unref (store);
	camel_folder_info_free (fi);
}

static void
folder_unsubscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_unsubscribed, store, fi, model);
}

static void
folder_created_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	/* we only want created events to do more work if we don't support subscriptions */
	if (camel_store_supports_subscriptions (store))
		return;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_subscribed_cb, store, fi, model);
}

static void
folder_deleted_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	/* we only want deleted events to do more work if we don't support subscriptions */
	if (camel_store_supports_subscriptions (store))
		return;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_unsubscribed_cb, store, fi, model);
}

static void
folder_renamed (CamelStore *store, CamelRenameInfo *info, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	char *parent, *p;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;
	
	parent = g_strdup_printf ("/%s", info->old_base);
	if (!(row = g_hash_table_lookup (si->path_hash, parent))) {
		g_free (parent);
		goto done;
	}
	g_free (parent);
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		goto done;
	}
	
	em_folder_tree_model_remove_folders (model, si, &iter);
	
	parent = g_strdup (info->new->path);
	if ((p = strrchr (parent + 1, '/')))
		*p = '\0';
	
	if (!strcmp (parent, "/")) {
		/* renamed to a toplevel folder on the store */
		path = gtk_tree_row_reference_get_path (si->row);
	} else {
		if (!(row = g_hash_table_lookup (si->path_hash, parent))) {
			/* NOTE: this should never happen, but I
			 * suppose if it does in reality, we can add
			 * code here to add the missing nodes to the
			 * tree */
			g_assert_not_reached ();
			g_free (parent);
			goto done;
		}
		
		path = gtk_tree_row_reference_get_path (row);
	}
	
	g_free (parent);
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &root, path)) {
		gtk_tree_path_free (path);
		g_assert_not_reached ();
		goto done;
	}
	
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	em_folder_tree_model_set_folder_info (model, &iter, si, info->new);
	
 done:
	
	camel_object_unref (store);
	
	g_free (info->old_base);
	camel_folder_info_free (info->new);
	g_free (info);
}

static void
folder_renamed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelRenameInfo *rinfo, *info = event_data;
	
	camel_object_ref (store);
	
	rinfo = g_new0 (CamelRenameInfo, 1);
	rinfo->old_base = g_strdup (info->old_base);
	rinfo->new = camel_folder_info_clone (info->new);
	
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_renamed, store, rinfo, model);
}

void
em_folder_tree_model_add_store (EMFolderTreeModel *model, CamelStore *store, const char *display_name)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	char *uri;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);
	
	if ((si = g_hash_table_lookup (model->store_hash, store))) {
		const char *name;
		
		path = gtk_tree_row_reference_get_path (si->row);
		gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
		gtk_tree_path_free (path);
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_STRING_DISPLAY_NAME, (char **) &name, -1);
		
		g_warning ("the store `%s' is already in the folder tree as `%s'",
			   display_name, name);
		
		return;
	}
	
	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	
	/* add the store to the tree */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, NULL);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, display_name,
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FOLDER_PATH, "/",
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, TRUE,
			    COL_STRING_URI, uri, -1);
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, &iter);
	row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	gtk_tree_path_free (path);
	
	si = g_new (struct _EMFolderTreeModelStoreInfo, 1);
	si->display_name = g_strdup (display_name);
	camel_object_ref (store);
	si->store = store;
	si->row = row;
	si->path_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (model->store_hash, store, si);
	
	/* each store has folders... but we don't load them until the user demands them */
	root = iter;
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, _("Loading..."),
			    COL_POINTER_CAMEL_STORE, NULL,
			    COL_STRING_FOLDER_PATH, NULL,
			    COL_BOOL_LOAD_SUBDIRS, FALSE,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_STRING_URI, NULL,
			    COL_UINT_UNREAD, 0,
			    -1);
	
	g_free (uri);
	
	/* listen to store events */
#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
	si->created_id = camel_object_hook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), model);
	si->deleted_id = camel_object_hook_event (store, "folder_deleted", CAMEL_CALLBACK (folder_deleted_cb), model);
	si->renamed_id = camel_object_hook_event (store, "folder_renamed", CAMEL_CALLBACK (folder_renamed_cb), model);
	si->subscribed_id = camel_object_hook_event (store, "folder_subscribed", CAMEL_CALLBACK (folder_subscribed_cb), model);
	si->unsubscribed_id = camel_object_hook_event (store, "folder_unsubscribed", CAMEL_CALLBACK (folder_unsubscribed_cb), model);
}


static void
em_folder_tree_model_remove_uri (EMFolderTreeModel *model, const char *uri)
{
	GtkTreeRowReference *row;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (uri != NULL);
	
	if ((row = g_hash_table_lookup (model->uri_hash, uri))) {
		g_hash_table_remove (model->uri_hash, uri);
		gtk_tree_row_reference_free (row);
	}
}


static void
em_folder_tree_model_remove_store_info (EMFolderTreeModel *model, CamelStore *store)
{
	struct _EMFolderTreeModelStoreInfo *si;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	g_hash_table_remove (model->store_hash, si->store);
	store_info_free (si);
}


void
em_folder_tree_model_remove_folders (EMFolderTreeModel *model, struct _EMFolderTreeModelStoreInfo *si, GtkTreeIter *toplevel)
{
	GtkTreeRowReference *row;
	char *uri, *folder_path;
	gboolean is_store, go;
	GtkTreeIter iter;
	
	if (gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, toplevel)) {
		do {
			GtkTreeIter next = iter;
			
			go = gtk_tree_model_iter_next ((GtkTreeModel *) model, &next);
			em_folder_tree_model_remove_folders (model, si, &iter);
			iter = next;
		} while (go);
	}
	
	gtk_tree_model_get ((GtkTreeModel *) model, toplevel, COL_STRING_URI, &uri,
			    COL_STRING_FOLDER_PATH, &folder_path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if (folder_path && (row = g_hash_table_lookup (si->path_hash, folder_path))) {
		g_hash_table_remove (si->path_hash, folder_path);
		gtk_tree_row_reference_free (row);
	}
	
	em_folder_tree_model_remove_uri (model, uri);
	
	gtk_tree_store_remove ((GtkTreeStore *) model, toplevel);
	
	if (is_store)
		em_folder_tree_model_remove_store_info (model, si->store);
}


void
em_folder_tree_model_remove_store (EMFolderTreeModel *model, CamelStore *store)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	if (!(si = g_hash_table_lookup (model->store_hash, store))) {
		g_warning ("the store `%s' is not in the folder tree", si->display_name);
		
		return;
	}
	
	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
	gtk_tree_path_free (path);
	
	/* recursively remove subfolders and finally the toplevel store */
	em_folder_tree_model_remove_folders (model, si, &iter);
}


gboolean
em_folder_tree_model_get_expanded (EMFolderTreeModel *model, const char *key)
{
	if (g_hash_table_lookup (model->expanded, key))
		return TRUE;
	
	return FALSE;
}


void
em_folder_tree_model_set_expanded (EMFolderTreeModel *model, const char *key, gboolean expanded)
{
	gpointer okey, oval;
	
	if (g_hash_table_lookup_extended (model->expanded, key, &okey, &oval)) {
		g_hash_table_remove (model->expanded, okey);
		g_free (okey);
	}
	
	if (expanded)
		g_hash_table_insert (model->expanded, g_strdup (key), GINT_TO_POINTER (TRUE));
}


static void
expanded_save (gpointer key, gpointer value, FILE *fp)
{
	/* FIXME: don't save stale entries */
	if (!GPOINTER_TO_INT (value))
		return;
	
	camel_file_util_encode_string (fp, key);
}

void
em_folder_tree_model_save_expanded (EMFolderTreeModel *model)
{
	char *dirname, *tmpname;
	FILE *fp;
	int fd;
	
	dirname = g_path_get_dirname (model->filename);
	if (camel_mkdir (dirname, 0777) == -1 && errno != EEXIST) {
		g_free (dirname);
		return;
	}
	
	g_free (dirname);
	tmpname = g_strdup_printf ("%s~", model->filename);
	
	if (!(fp = fopen (tmpname, "w+"))) {
		g_free (tmpname);
		return;
	}
	
	g_hash_table_foreach (model->expanded, (GHFunc) expanded_save, fp);
	
	if (fflush (fp) != 0)
		goto exception;
	
	if ((fd = fileno (fp)) == -1)
		goto exception;
	
	if (fsync (fd) == -1)
		goto exception;
	
	fclose (fp);
	fp = NULL;
	
	if (rename (tmpname, model->filename) == -1)
		goto exception;
	
	g_free (tmpname);
	
	return;
	
 exception:
	
	if (fp != NULL)
		fclose (fp);
	
	unlink (tmpname);
	g_free (tmpname);
}


void
em_folder_tree_model_set_unread_count (EMFolderTreeModel *model, CamelStore *store, const char *path, int unread)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *tree_path;
	GtkTreeIter iter;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (path != NULL);
	
	if (unread < 0)
		unread = 0;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	if (!(row = g_hash_table_lookup (si->path_hash, path)))
		return;
	
	tree_path = gtk_tree_row_reference_get_path (row);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, tree_path)) {
		gtk_tree_path_free (tree_path);
		return;
	}
	
	gtk_tree_path_free (tree_path);
	
	gtk_tree_store_set ((GtkTreeStore *) model, &iter, COL_UINT_UNREAD, unread, -1);
}


/* Drag & Drop methods */
static void
drop_uid_list (CamelFolder *dest, GtkSelectionData *selection, gboolean move, CamelException *ex)
{
	CamelFolder *src;
	GPtrArray *uids;
	char *src_uri;
	
	em_utils_selection_get_uidlist (selection, &src_uri, &uids);
	
	if (!(src = mail_tool_uri_to_folder (src_uri, 0, ex))) {
		em_utils_uids_free (uids);
		g_free (src_uri);
		return;
	}
	
	g_free (src_uri);
	
	camel_folder_transfer_messages_to (src, uids, dest, NULL, move, ex);
	em_utils_uids_free (uids);
	camel_object_unref (src);
}

static void
drop_folder (CamelFolder *dest, GtkSelectionData *selection, gboolean move, CamelException *ex)
{
	CamelFolder *src;
	
	/* get the folder being dragged */
	if (!(src = mail_tool_uri_to_folder (selection->data, 0, ex)))
		return;
	
	if (src->parent_store == dest->parent_store && move) {
		/* simple rename() action */
		char *old_name, *new_name;
		
		old_name = g_strdup (src->full_name);
		new_name = g_strdup_printf ("%s/%s", dest->full_name, src->name);
		
		camel_store_rename_folder (dest->parent_store, old_name, new_name, ex);
		
		g_free (old_name);
		g_free (new_name);
	} else {
		/* copy the folder to the new location */
		CamelFolder *folder;
		char *path;
		
		path = g_strdup_printf ("%s/%s", dest->full_name, src->name);
		if ((folder = camel_store_get_folder (dest->parent_store, path, CAMEL_STORE_FOLDER_CREATE, ex))) {
			GPtrArray *uids;
			
			uids = camel_folder_get_uids (src);
			camel_folder_transfer_messages_to (src, uids, folder, NULL, FALSE, ex);
			camel_folder_free_uids (src, uids);
			
			camel_object_unref (folder);
		}
		
		g_free (path);
	}
	
	camel_object_unref (src);
}

static gboolean
import_message_rfc822 (CamelFolder *dest, CamelStream *stream, gboolean scan_from, CamelException *ex)
{
	CamelMimeParser *mp;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, scan_from);
	camel_mime_parser_init_with_stream (mp, stream);
	
	while (camel_mime_parser_step (mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_object_unref (msg);
			camel_object_unref (mp);
			return FALSE;
		}
		
		/* append the message to the folder... */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (dest, msg, info, NULL, ex);
		camel_object_unref (msg);
		
		if (camel_exception_is_set (ex)) {
			camel_object_unref (mp);
			return FALSE;
		}
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (mp);
	
	return TRUE;
}

static void
drop_message_rfc822 (CamelFolder *dest, GtkSelectionData *selection, CamelException *ex)
{
	CamelStream *stream;
	gboolean scan_from;
	
	scan_from = selection->length > 5 && !strncmp (selection->data, "From ", 5);
	stream = camel_stream_mem_new_with_buffer (selection->data, selection->length);
	
	import_message_rfc822 (dest, stream, scan_from, ex);
	
	camel_object_unref (stream);
}

static void
drop_text_uri_list (CamelFolder *dest, GtkSelectionData *selection, CamelException *ex)
{
	char **urls, *url, *tmp;
	CamelStream *stream;
	CamelURL *uri;
	int fd, i;
	
	tmp = g_strndup (selection->data, selection->length);
	urls = g_strsplit (tmp, "\n", 0);
	g_free (tmp);
	
	for (i = 0; urls[i] != NULL; i++) {
		/* get the path component */
		url = g_strstrip (urls[i]);
		uri = camel_url_new (url, NULL);
		g_free (url);
		
		if (!uri || strcmp (uri->protocol, "file") != 0) {
			camel_url_free (uri);
			continue;
		}
		
		url = uri->path;
		uri->path = NULL;
		camel_url_free (uri);
		
		if ((fd = open (url, O_RDONLY)) == -1) {
			g_free (url);
			continue;
		}
		
		stream = camel_stream_fs_new_with_fd (fd);
		if (!import_message_rfc822 (dest, stream, TRUE, ex)) {
			/* FIXME: should we abort now? or continue? */
			/* for now lets just continue... */
			camel_exception_clear (ex);
		}
		
		camel_object_unref (stream);
		g_free (url);
	}
	
	g_free (urls);
}


gboolean
em_folder_tree_model_drag_data_received (EMFolderTreeModel *model, GtkTreePath *dest_path, GtkSelectionData *selection,
					 guint info, gboolean move, gboolean *moved)
{
	const char *full_name;
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	*moved = FALSE;
	
	/* this means we are receiving no data */
	if (!selection->data || selection->length == -1)
		return FALSE;
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, dest_path)) {
		d(printf ("\tfailed to get row\n"));
		return FALSE;
	}
	
	gtk_tree_model_get ((GtkTreeModel *) model, &iter,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_FOLDER_PATH, &path, -1);
	
	/* make sure user isn't try to drop on a placeholder row */
	if (path == NULL) {
		d(printf ("\tdropped on a placeholder row?\n"));
		return FALSE;
	}
	
	full_name = path[0] == '/' ? path + 1 : path;
	
	camel_exception_init (&ex);
	if ((folder = camel_store_get_folder (store, full_name, 0, &ex))) {
		switch (info) {
		case DND_DROP_TYPE_UID_LIST:
			/* import a list of uids from another evo folder */
			drop_uid_list (folder, selection, move, &ex);
			*moved = move;
			d(printf ("\t* dropped a x-uid-list\n"));
			break;
		case DND_DROP_TYPE_FOLDER:
			/* copy or move (aka rename) a folder */
			drop_folder (folder, selection, move, &ex);
			*moved = move;
			d(printf ("\t* dropped a x-folder ('%s' into '%s')\n", selection->data, full_name));
			break;
		case DND_DROP_TYPE_MESSAGE_RFC822:
			/* import a message/rfc822 stream */
			drop_message_rfc822 (folder, selection, &ex);
			d(printf ("\t* dropped a message/rfc822\n"));
			break;
		case DND_DROP_TYPE_TEXT_URI_LIST:
			/* import an mbox, maildir, or mh folder? */
			drop_text_uri_list (folder, selection, &ex);
			d(printf ("\t* dropped a text/uri-list\n"));
			break;
		default:
			g_assert_not_reached ();
		}
	}
	
	if (camel_exception_is_set (&ex)) {
		camel_exception_clear (&ex);
		*moved = FALSE;
		return FALSE;
	}
	
	return TRUE;
}


GdkDragAction
em_folder_tree_model_row_drop_possible (EMFolderTreeModel *model, GtkTreePath *path, GList *targets)
{
	GdkAtom target;
	int i;
	
	target = em_folder_tree_model_row_drop_target (model, path, targets);
	if (target == GDK_NONE)
		return 0;
	
	for (i = 0; i < NUM_DROP_TYPES; i++) {
		if (drop_atoms[i] == target) {
			switch (i) {
			case DND_DROP_TYPE_FOLDER:
				return GDK_ACTION_MOVE;
			default:
				return GDK_ACTION_COPY;
			}
		}
	}
	
	return 0;
}


GdkAtom
em_folder_tree_model_row_drop_target (EMFolderTreeModel *model, GtkTreePath *path, GList *targets)
{
	gboolean is_store;
	GtkTreeIter iter;
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))
		return GDK_NONE;
	
	gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_BOOL_IS_STORE, &is_store, -1);
	
	if (is_store) {
		/* can only drop x-folder into a store */
		GdkAtom xfolder;
		
		xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
		while (targets != NULL) {
			if (targets->data == (gpointer) xfolder)
				return xfolder;
			
			targets = targets->next;
		}
	} else {
		/* can drop anything into a folder */
		int i;
		
		while (targets != NULL) {
			for (i = 0; i < NUM_DROP_TYPES; i++) {
				if (targets->data == (gpointer) drop_atoms[i])
					return drop_atoms[i];
			}
			
			targets = targets->next;
		}
	}
	
	return GDK_NONE;
}


gboolean
em_folder_tree_model_row_draggable (EMFolderTreeModel *model, GtkTreePath *path)
{
	gboolean is_store;
	GtkTreeIter iter;
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))
		return FALSE;
	
	gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_BOOL_IS_STORE, &is_store, -1);
	
	return !is_store;
}


static void
drag_text_uri_list (CamelFolder *src, GtkSelectionData *selection, CamelException *ex)
{
	CamelFolder *dest;
	const char *tmpdir;
	CamelStore *store;
	GPtrArray *uids;
	GString *url;
	
	if (!(tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX"))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create temporary directory: %s"),
				      g_strerror (errno));
		return;
	}
	
	url = g_string_new ("mbox:");
	g_string_append (url, tmpdir);
	if (!(store = camel_session_get_store (session, url->str, ex))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create temporary mbox store: %s"),
				      camel_exception_get_description (ex));
		g_string_free (url, TRUE);
		
		return;
	}
	
	if (!(dest = camel_store_get_folder (store, "mbox", CAMEL_STORE_FOLDER_CREATE, ex))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create temporary mbox folder: %s"),
				      camel_exception_get_description (ex));
		
		camel_object_unref (store);
		g_string_free (url, TRUE);
		
		return;
	}
	
	camel_object_unref (store);
	uids = camel_folder_get_uids (src);
	
	camel_folder_transfer_messages_to (src, uids, dest, NULL, FALSE, ex);
	if (camel_exception_is_set (ex)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not copy messages to temporary mbox folder: %s"),
				      camel_exception_get_description (ex));
	} else {
		/* replace "mbox:" with "file:" */
		memcpy (url->str, "file", 4);
		g_string_append (url, "\r\n");
		gtk_selection_data_set (selection, drag_atoms[DND_DRAG_TYPE_TEXT_URI_LIST], 8, url->str, url->len);
	}
	
	camel_folder_free_uids (src, uids);
	camel_object_unref (dest);
	g_string_free (url, TRUE);
}


gboolean
em_folder_tree_model_drag_data_get (EMFolderTreeModel *model, GtkTreePath *src_path, GtkSelectionData *selection, guint info)
{
	const char *full_name;
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path, *uri;
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, src_path)) {
		printf ("model_drag_data_get failed to get iter\n");
		return FALSE;
	}
	
	gtk_tree_model_get ((GtkTreeModel *) model, &iter,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_STRING_URI, &uri, -1);
	
	/* make sure user isn't try to drag on a placeholder row */
	if (path == NULL) {
		printf ("model_drag_data_get failed to get path\n");
		return FALSE;
	}
	
	full_name = path[0] == '/' ? path + 1 : path;
	
	camel_exception_init (&ex);
	
	switch (info) {
	case DND_DRAG_TYPE_FOLDER:
		/* dragging to a new location in the folder tree */
		gtk_selection_data_set (selection, drag_atoms[info], 8, uri, strlen (uri) + 1);
		printf ("model_drag_data_get setting x-folder data\n");
		break;
	case DND_DRAG_TYPE_TEXT_URI_LIST:
		/* dragging to nautilus or something, probably */
		if ((folder = camel_store_get_folder (store, full_name, 0, &ex))) {
			printf ("model_drag_data_get setting text/uri-list data\n");
			drag_text_uri_list (folder, selection, &ex);
			camel_object_unref (folder);
		}
		break;
	default:
		g_assert_not_reached ();
	}
	
	if (camel_exception_is_set (&ex)) {
		printf ("model_drag_data_get failed: %s\n", ex.desc);
		camel_exception_clear (&ex);
		return FALSE;
	}
	
	return TRUE;
}


gboolean
em_folder_tree_model_drag_data_delete (EMFolderTreeModel *model, GtkTreePath *src_path)
{
	const char *full_name;
	gboolean is_store;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, src_path))
		return FALSE;
	
	gtk_tree_model_get ((GtkTreeModel *) model, &iter,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if (is_store)
		return FALSE;
	
	full_name = path[0] == '/' ? path + 1 : path;
	
	camel_exception_init (&ex);
	camel_store_delete_folder (store, full_name, &ex);
	if (camel_exception_is_set (&ex)) {
		camel_exception_clear (&ex);
		return FALSE;
	}
	
	return TRUE;
}


void
em_folder_tree_model_set_drag_drop_types (EMFolderTreeModel *model, GtkWidget *widget)
{
	gtk_drag_source_set (widget, GDK_BUTTON1_MASK, drag_types, NUM_DRAG_TYPES,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, drop_types,
			   NUM_DROP_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

