/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-week-view-cell.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 * Author: Yang Wu <Yang.Wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#include "ea-week-view-cell.h"
#include "ea-week-view-main-item.h"
#include "ea-factory.h"

/* EWeekViewCell */

static void e_week_view_cell_class_init (EWeekViewCellClass *class);

EA_FACTORY_GOBJECT (EA_TYPE_WEEK_VIEW_CELL, ea_week_view_cell, ea_week_view_cell_new)

GType
e_week_view_cell_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EWeekViewCellClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) e_week_view_cell_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EWeekViewCell), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EWeekViewCell", &tinfo, 0);
	}

	return type;
}

static void
e_week_view_cell_class_init (EWeekViewCellClass *class)
{
    EA_SET_FACTORY (e_week_view_cell_get_type (), ea_week_view_cell);
}

EWeekViewCell *
e_week_view_cell_new (EWeekView *week_view, gint row, gint column)
{
	GObject *object;
	EWeekViewCell *cell;

	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), NULL);

	object = g_object_new (E_TYPE_WEEK_VIEW_CELL, NULL);
	cell = E_WEEK_VIEW_CELL (object);
	cell->week_view = week_view;
	cell->row = row;
	cell->column = column;

#ifdef ACC_DEBUG
	printf ("EvoAcc: e_week_view_cell created %p\n", (void *)cell);
#endif

	return cell;
}

/* EaWeekViewCell */

static void ea_week_view_cell_class_init (EaWeekViewCellClass *klass);

static G_CONST_RETURN gchar* ea_week_view_cell_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_week_view_cell_get_description (AtkObject *accessible);
static AtkStateSet* ea_week_view_cell_ref_state_set (AtkObject *obj);
static AtkObject * ea_week_view_cell_get_parent (AtkObject *accessible);
static gint ea_week_view_cell_get_index_in_parent (AtkObject *accessible);

/* component interface */
static void atk_component_interface_init (AtkComponentIface *iface);
static void component_interface_get_extents (AtkComponent *component,
					     gint *x, gint *y,
					     gint *width, gint *height,
					     AtkCoordType coord_type);
static gpointer parent_class = NULL;

#ifdef ACC_DEBUG
static gint n_ea_week_view_cell_created = 0, n_ea_week_view_cell_destroyed = 0;
static void ea_week_view_cell_finalize (GObject *object);
#endif

GType
ea_week_view_cell_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaWeekViewCellClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_week_view_cell_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaWeekViewCell), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		type = g_type_register_static (ATK_TYPE_GOBJECT_ACCESSIBLE,
					       "EaWeekViewCell", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
					     &atk_component_info);

	}

	return type;
}

static void
ea_week_view_cell_class_init (EaWeekViewCellClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

#ifdef ACC_DEBUG
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = ea_week_view_cell_finalize;
#endif

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_week_view_cell_get_name;
	class->get_description = ea_week_view_cell_get_description;
	class->ref_state_set = ea_week_view_cell_ref_state_set;

	class->get_parent = ea_week_view_cell_get_parent;
	class->get_index_in_parent = ea_week_view_cell_get_index_in_parent;

}

AtkObject* 
ea_week_view_cell_new (GObject *obj)
{
	gpointer object;
	AtkObject *atk_object;

	g_return_val_if_fail (E_IS_WEEK_VIEW_CELL (obj), NULL);
	object = g_object_new (EA_TYPE_WEEK_VIEW_CELL, NULL);
	atk_object = ATK_OBJECT (object);
	atk_object_initialize (atk_object, obj);
	atk_object->role = ATK_ROLE_UNKNOWN;

#ifdef ACC_DEBUG
	++n_ea_week_view_cell_created;
	printf ("ACC_DEBUG: n_ea_week_view_cell_created = %d\n",
		n_ea_week_view_cell_created);
#endif
	return atk_object;
}

#ifdef ACC_DEBUG
static void ea_week_view_cell_finalize (GObject *object)
{
	++n_ea_week_view_cell_destroyed;
	printf ("ACC_DEBUG: n_ea_week_view_cell_destroyed = %d\n",
		n_ea_week_view_cell_destroyed);
}
#endif

static G_CONST_RETURN gchar*
ea_week_view_cell_get_name (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewCell *cell;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_CELL (accessible), NULL);

	if (!accessible->name) {
		AtkObject *ea_main_item;
		GnomeCanvasItem *main_item;
		gchar *new_name = g_strdup ("");
		const gchar *row_label, *column_label;

		atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
		g_obj = atk_gobject_accessible_get_object (atk_gobj);
		if (!g_obj)
			return NULL;

		cell = E_WEEK_VIEW_CELL (g_obj);
		main_item = cell->week_view->main_canvas_item;
		ea_main_item = atk_gobject_accessible_for_object (G_OBJECT (main_item));
		column_label = atk_table_get_column_description (ATK_TABLE (ea_main_item),
								 cell->column);
		row_label = atk_table_get_row_description (ATK_TABLE (ea_main_item),
							   cell->row);
		new_name = g_strconcat (column_label, " ", row_label, NULL);
		ATK_OBJECT_CLASS (parent_class)->set_name (accessible, new_name);
		g_free (new_name);
	}
	return accessible->name;
}

static G_CONST_RETURN gchar*
ea_week_view_cell_get_description (AtkObject *accessible)
{
	return ea_week_view_cell_get_name (accessible);
}

static AtkStateSet*
ea_week_view_cell_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GObject *g_obj;
  AtkObject *parent;
  gint x, y, width, height;
  gint parent_x, parent_y, parent_width, parent_height;

  state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (obj);
  g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(obj));
  if (!g_obj)
	  return state_set;

  atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

  parent = atk_object_get_parent (obj);
  atk_component_get_extents (ATK_COMPONENT (obj), &x, &y,
			     &width, &height, ATK_XY_WINDOW);
  atk_component_get_extents (ATK_COMPONENT (parent), &parent_x, &parent_y,
			     &parent_width, &parent_height, ATK_XY_WINDOW);


  if (x + width < parent_x || x > parent_x + parent_width ||
      y + height < parent_y || y > parent_y + parent_height)
	  /* the cell is out of the main canvas */	  
	  ;
  else
	  atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

  return state_set;
}

static AtkObject *
ea_week_view_cell_get_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewCell *cell;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_CELL (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	cell = E_WEEK_VIEW_CELL (g_obj);
	return atk_gobject_accessible_for_object (G_OBJECT (cell->week_view->main_canvas_item));
}

static gint
ea_week_view_cell_get_index_in_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewCell *cell;
	AtkObject *parent;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_CELL (accessible), -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	cell = E_WEEK_VIEW_CELL (g_obj);
	parent = atk_object_get_parent (accessible);
	return atk_table_get_index_at (ATK_TABLE (parent),
				       cell->row, cell->column);
}

/* Atk Component Interface */

static void 
atk_component_interface_init (AtkComponentIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_extents = component_interface_get_extents;
}

static void 
component_interface_get_extents (AtkComponent *component,
				 gint *x, gint *y, gint *width, gint *height,
				 AtkCoordType coord_type)
{
	GObject *g_obj;
	AtkObject *atk_obj;
	EWeekViewCell *cell;
	EWeekView *week_view;
	GtkWidget *main_canvas;
	gint week_view_width, week_view_height;
	gint scroll_x, scroll_y;

	*x = *y = *width = *height = 0;

	g_return_if_fail (EA_IS_WEEK_VIEW_CELL (component));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(component));
	if (!g_obj)
		/* defunct object*/
		return;

	cell = E_WEEK_VIEW_CELL (g_obj);
	week_view = cell->week_view;
	main_canvas = cell->week_view->main_canvas;

	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (main_canvas));
	atk_component_get_extents (ATK_COMPONENT (atk_obj),
				   x, y,
				   &week_view_width, &week_view_height,
				   coord_type);
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (week_view->main_canvas),
					 &scroll_x, &scroll_y);
	*x += week_view->col_offsets[cell->column] - scroll_x;
	*y += week_view->row_offsets[cell->row*2]- scroll_y;
	*width = week_view->col_widths[cell->column];
	*height = week_view->row_heights[ cell->row*2]*2;
}
