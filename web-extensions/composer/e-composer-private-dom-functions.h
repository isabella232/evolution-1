/*
 * e-composer-private-dom-functions.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_COMPOSER_PRIVATE_DOM_FUNCTIONS_H
#define E_COMPOSER_PRIVATE_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-html-editor-web-extension.h"

G_BEGIN_DECLS

gchar *		dom_remove_signatures		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean top_signature);
gchar *		dom_insert_signature		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *content,
						 gboolean is_html,
						 const gchar *id,
						 gboolean *set_signature_from_message,
						 gboolean *check_if_signature_is_changed,
						 gboolean *ignore_next_signature_change);
void		dom_save_drag_and_drop_history	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
void		dom_clean_after_drag_and_drop	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

G_END_DECLS

#endif /* E_COMPOSER_PRIVATE_DOM_FUNCTIONS_H */
