/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A client-side GtkObject which exposes the
 * Evolution:BookListener interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_LISTENER_H__
#define __E_BOOK_LISTENER_H__

#include <libgnome/gnome-defs.h>
#include <bonobo/bonobo-object.h>
#include <ebook/addressbook.h>
#include <ebook/e-book-types.h>

BEGIN_GNOME_DECLS

typedef struct _EBookListener EBookListener;
typedef struct _EBookListenerClass EBookListenerClass;
typedef struct _EBookListenerPrivate EBookListenerPrivate;

struct _EBookListener {
	BonoboObject           parent;
	EBookListenerPrivate *priv;
};

struct _EBookListenerClass {
	BonoboObjectClass parent;

	/*
	 * Signals
	 */
	void (*responses_queued) (void);
};

typedef enum {
	/* Async responses */
	OpenBookResponse,
	CreateCardResponse,
	RemoveCardResponse,
	ModifyCardResponse,
	GetCursorResponse,
	GetBookViewResponse,
	GetChangesResponse,
	AuthenticationResponse,
	GetSupportedFieldsResponse,

	/* Async events */
	LinkStatusEvent,
	OpenProgressEvent,
} EBookListenerOperation;

typedef struct {
	EBookListenerOperation  op;

	/* For most Response notifications */
	EBookStatus             status;

	/* For OpenBookResponse */
	GNOME_Evolution_Addressbook_Book          book;

	/* For GetCursorResponse */
	GNOME_Evolution_Addressbook_CardCursor    cursor;

	/* For GetBookViewReponse */
	GNOME_Evolution_Addressbook_BookView      book_view;

	/* For GetSupportedFields */
	const GNOME_Evolution_Addressbook_stringlist   *fields;

	/* For OpenProgressEvent */
	char                   *msg;
	short                   percent;

	/* For LinkStatusEvent */
	gboolean                connected;

	/* For Card[Added|Removed|Modified]Event */
	char                   *id;
} EBookListenerResponse;

EBookListener         *e_book_listener_new            (void);
int                    e_book_listener_check_pending  (EBookListener *listener);
EBookListenerResponse *e_book_listener_pop_response   (EBookListener *listener);
GtkType                e_book_listener_get_type       (void);

POA_GNOME_Evolution_Addressbook_BookListener__epv *e_book_listener_get_epv (void);

#define E_BOOK_LISTENER_TYPE        (e_book_listener_get_type ())
#define E_BOOK_LISTENER(o)          (GTK_CHECK_CAST ((o), E_BOOK_LISTENER_TYPE, EBookListener))
#define E_BOOK_LISTENER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_LISTENER_TYPE, EBookListenerClass))
#define E_IS_BOOK_LISTENER(o)       (GTK_CHECK_TYPE ((o), E_BOOK_LISTENER_TYPE))
#define E_IS_BOOK_LISTENER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_LISTENER_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_LISTENER_H__ */
