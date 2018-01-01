/**	@file eidl.c
 *	@brief ldap bdb back-end ID List functions */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2000-2016 The OpenLDAP Foundation.
 * Portions Copyright 2001-2017 Howard Chu, Symas Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include "eidl.h"

/** @defgroup internal	NDRXDB Internals
 *	@{
 */
/** @defgroup idls	ID List Management
 *	@{
 */
#define CMP(x,y)	 ( (x) < (y) ? -1 : (x) > (y) )

unsigned exdb_eidl_search( EXDB_IDL ids, EXDB_ID id )
{
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first position greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 1;
	int val = 0;
	unsigned n = ids[0];

	while( 0 < n ) {
		unsigned pivot = n >> 1;
		cursor = base + pivot + 1;
		val = CMP( ids[cursor], id );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor;
			n -= pivot + 1;

		} else {
			return cursor;
		}
	}

	if( val > 0 ) {
		++cursor;
	}
	return cursor;
}

#if 0	/* superseded by append/sort */
int exdb_eidl_insert( EXDB_IDL ids, EXDB_ID id )
{
	unsigned x, i;

	x = exdb_eidl_search( ids, id );
	assert( x > 0 );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0] && ids[x] == id ) {
		/* duplicate */
		assert(0);
		return -1;
	}

	if ( ++ids[0] >= EXDB_IDL_DB_MAX ) {
		/* no room */
		--ids[0];
		return -2;

	} else {
		/* insert id */
		for (i=ids[0]; i>x; i--)
			ids[i] = ids[i-1];
		ids[x] = id;
	}

	return 0;
}
#endif

EXDB_IDL exdb_eidl_alloc(int num)
{
	EXDB_IDL ids = malloc((num+2) * sizeof(EXDB_ID));
	if (ids) {
		*ids++ = num;
		*ids = 0;
	}
	return ids;
}

void exdb_eidl_free(EXDB_IDL ids)
{
	if (ids)
		free(ids-1);
}

void exdb_eidl_shrink( EXDB_IDL *idp )
{
	EXDB_IDL ids = *idp;
	if (*(--ids) > EXDB_IDL_UM_MAX &&
		(ids = realloc(ids, (EXDB_IDL_UM_MAX+2) * sizeof(EXDB_ID))))
	{
		*ids++ = EXDB_IDL_UM_MAX;
		*idp = ids;
	}
}

static int exdb_eidl_grow( EXDB_IDL *idp, int num )
{
	EXDB_IDL idn = *idp-1;
	/* grow it */
	idn = realloc(idn, (*idn + num + 2) * sizeof(EXDB_ID));
	if (!idn)
		return ENOMEM;
	*idn++ += num;
	*idp = idn;
	return 0;
}

int exdb_eidl_need( EXDB_IDL *idp, unsigned num )
{
	EXDB_IDL ids = *idp;
	num += ids[0];
	if (num > ids[-1]) {
		num = (num + num/4 + (256 + 2)) & -256;
		if (!(ids = realloc(ids-1, num * sizeof(EXDB_ID))))
			return ENOMEM;
		*ids++ = num - 2;
		*idp = ids;
	}
	return 0;
}

int exdb_eidl_append( EXDB_IDL *idp, EXDB_ID id )
{
	EXDB_IDL ids = *idp;
	/* Too big? */
	if (ids[0] >= ids[-1]) {
		if (exdb_eidl_grow(idp, EXDB_IDL_UM_MAX))
			return ENOMEM;
		ids = *idp;
	}
	ids[0]++;
	ids[ids[0]] = id;
	return 0;
}

int exdb_eidl_append_list( EXDB_IDL *idp, EXDB_IDL app )
{
	EXDB_IDL ids = *idp;
	/* Too big? */
	if (ids[0] + app[0] >= ids[-1]) {
		if (exdb_eidl_grow(idp, app[0]))
			return ENOMEM;
		ids = *idp;
	}
	memcpy(&ids[ids[0]+1], &app[1], app[0] * sizeof(EXDB_ID));
	ids[0] += app[0];
	return 0;
}

int exdb_eidl_append_range( EXDB_IDL *idp, EXDB_ID id, unsigned n )
{
	EXDB_ID *ids = *idp, len = ids[0];
	/* Too big? */
	if (len + n > ids[-1]) {
		if (exdb_eidl_grow(idp, n | EXDB_IDL_UM_MAX))
			return ENOMEM;
		ids = *idp;
	}
	ids[0] = len + n;
	ids += len;
	while (n)
		ids[n--] = id++;
	return 0;
}

void exdb_eidl_xmerge( EXDB_IDL idl, EXDB_IDL merge )
{
	EXDB_ID old_id, merge_id, i = merge[0], j = idl[0], k = i+j, total = k;
	idl[0] = (EXDB_ID)-1;		/* delimiter for idl scan below */
	old_id = idl[j];
	while (i) {
		merge_id = merge[i--];
		for (; old_id < merge_id; old_id = idl[--j])
			idl[k--] = old_id;
		idl[k--] = merge_id;
	}
	idl[0] = total;
}

/* Quicksort + Insertion sort for small arrays */

#define SMALL	8
#define	EIDL_SWAP(a,b)	{ itmp=(a); (a)=(b); (b)=itmp; }

void
exdb_eidl_sort( EXDB_IDL ids )
{
	/* Max possible depth of int-indexed tree * 2 items/level */
	int istack[sizeof(int)*CHAR_BIT * 2];
	int i,j,k,l,ir,jstack;
	EXDB_ID a, itmp;

	ir = (int)ids[0];
	l = 1;
	jstack = 0;
	for(;;) {
		if (ir - l < SMALL) {	/* Insertion sort */
			for (j=l+1;j<=ir;j++) {
				a = ids[j];
				for (i=j-1;i>=1;i--) {
					if (ids[i] >= a) break;
					ids[i+1] = ids[i];
				}
				ids[i+1] = a;
			}
			if (jstack == 0) break;
			ir = istack[jstack--];
			l = istack[jstack--];
		} else {
			k = (l + ir) >> 1;	/* Choose median of left, center, right */
			EIDL_SWAP(ids[k], ids[l+1]);
			if (ids[l] < ids[ir]) {
				EIDL_SWAP(ids[l], ids[ir]);
			}
			if (ids[l+1] < ids[ir]) {
				EIDL_SWAP(ids[l+1], ids[ir]);
			}
			if (ids[l] < ids[l+1]) {
				EIDL_SWAP(ids[l], ids[l+1]);
			}
			i = l+1;
			j = ir;
			a = ids[l+1];
			for(;;) {
				do i++; while(ids[i] > a);
				do j--; while(ids[j] < a);
				if (j < i) break;
				EIDL_SWAP(ids[i],ids[j]);
			}
			ids[l+1] = ids[j];
			ids[j] = a;
			jstack += 2;
			if (ir-i+1 >= j-l) {
				istack[jstack] = ir;
				istack[jstack-1] = i;
				ir = j-1;
			} else {
				istack[jstack] = j-1;
				istack[jstack-1] = l;
				l = i;
			}
		}
	}
}

unsigned exdb_mid2l_search( EXDB_ID2L ids, EXDB_ID id )
{
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first position greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 1;
	int val = 0;
	unsigned n = (unsigned)ids[0].mid;

	while( 0 < n ) {
		unsigned pivot = n >> 1;
		cursor = base + pivot + 1;
		val = CMP( id, ids[cursor].mid );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor;
			n -= pivot + 1;

		} else {
			return cursor;
		}
	}

	if( val > 0 ) {
		++cursor;
	}
	return cursor;
}

int exdb_mid2l_insert( EXDB_ID2L ids, EXDB_ID2 *id )
{
	unsigned x, i;

	x = exdb_mid2l_search( ids, id->mid );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0].mid && ids[x].mid == id->mid ) {
		/* duplicate */
		return -1;
	}

	if ( ids[0].mid >= EXDB_IDL_UM_MAX ) {
		/* too big */
		return -2;

	} else {
		/* insert id */
		ids[0].mid++;
		for (i=(unsigned)ids[0].mid; i>x; i--)
			ids[i] = ids[i-1];
		ids[x] = *id;
	}

	return 0;
}

int exdb_mid2l_append( EXDB_ID2L ids, EXDB_ID2 *id )
{
	/* Too big? */
	if (ids[0].mid >= EXDB_IDL_UM_MAX) {
		return -2;
	}
	ids[0].mid++;
	ids[ids[0].mid] = *id;
	return 0;
}

#ifdef EXDB_VL32
unsigned exdb_mid3l_search( EXDB_ID3L ids, EXDB_ID id )
{
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first position greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 1;
	int val = 0;
	unsigned n = (unsigned)ids[0].mid;

	while( 0 < n ) {
		unsigned pivot = n >> 1;
		cursor = base + pivot + 1;
		val = CMP( id, ids[cursor].mid );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor;
			n -= pivot + 1;

		} else {
			return cursor;
		}
	}

	if( val > 0 ) {
		++cursor;
	}
	return cursor;
}

int exdb_mid3l_insert( EXDB_ID3L ids, EXDB_ID3 *id )
{
	unsigned x, i;

	x = exdb_mid3l_search( ids, id->mid );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0].mid && ids[x].mid == id->mid ) {
		/* duplicate */
		return -1;
	}

	/* insert id */
	ids[0].mid++;
	for (i=(unsigned)ids[0].mid; i>x; i--)
		ids[i] = ids[i-1];
	ids[x] = *id;

	return 0;
}
#endif /* EXDB_VL32 */

/** @} */
/** @} */
