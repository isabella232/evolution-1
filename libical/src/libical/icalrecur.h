/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalrecur.h
 CREATOR: eric 20 March 2000


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.h

======================================================================*/

#ifndef ICALRECUR_H
#define ICALRECUR_H

#include <time.h>
#include "icaltypes.h"
#include "icalenums.h" /* for recurrence enums */

typedef void icalrecur_iterator;
void icalrecurrencetype_test();


icalrecur_iterator* icalrecur_iterator_new(struct icalrecurrencetype rule, struct icaltimetype dtstart);

struct icaltimetype icalrecur_iterator_next(icalrecur_iterator*);

int icalrecur_iterator_count(icalrecur_iterator*);

void icalrecur_iterator_free(icalrecur_iterator*);


#endif
