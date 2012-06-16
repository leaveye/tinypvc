/*
 * =====================================================================================
 *
 *       Filename:  data.c
 *
 *    Description:  Elemental data structure support
 *                  *Copied from cbase library*
 *
 *        Version:  1.0
 *        Created:  2012/06/12 21时30分22秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Levi G (yxguo), yxguo@wisvideo.com.cn
 *   Organization:  WisVideo
 *
 * =====================================================================================
 */

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#include "data.h"

/* Macros */

#define C_DARRAY_NBBY 8
#define C_DARRAY_USED_MASK(B) ~(1 << (B))
#define C_DARRAY_FREE_MASK(B) (1 << (B))

/* Functions */

c_darray_t *C_darray_create(uint_t resize, size_t elemsz)
{
  c_darray_t *a;
  char *b;
  uint_t i;

  if(resize < 1 || resize > C_DARRAY_MAX_RESIZE || !elemsz)
    return(NULL);

  a = C_new(c_darray_t);
  a->resize = resize * C_DARRAY_NBBY;
  a->iresize = resize;
  a->elemsz = elemsz;
  a->mem = (void *)C_malloc(a->resize * a->elemsz, char);
  a->size = a->resize;
  a->isize = a->iresize;  
  a->bot = a->del_count = 0;
  a->free_list = C_malloc(a->iresize, char);
  for(i = a->iresize, b = a->free_list; i--; b++)
    *b = ~0;

  return(a);
}

/*
 */

void C_darray_destroy(c_darray_t *a)
{

  if(!a)
    return;

  C_free(a->free_list);
  C_free(a->mem);
  C_free(a);
}

/*
 */

void *C_darray_store(c_darray_t *a, const void *data, uint_t *index)
{
  uint_t e, bit, byte, o;
  char b, *bp;
  div_t dv;

  if(!a)
    return(NULL);
  
  if(!a->del_count)
  {
    if(a->bot == a->size)
    {
      if((a->size / C_DARRAY_NBBY) == a->isize)
      {
        o = a->isize;
        a->free_list = C_realloc(a->free_list, (a->isize += a->iresize), char);
        memset((void *)(a->free_list + o), ~0, a->iresize);
        
      }
      a->mem = (void *)C_realloc(a->mem, (a->size += a->resize) * a->elemsz,
                                 char);
    }
    dv = div((e = (a->bot)++), C_DARRAY_NBBY);
    byte = dv.quot, bit = dv.rem;
  }
  else
  {
    a->del_count--;
    for(byte = 0, bp = a->free_list; !*bp; byte++, bp++);
    b = *bp;
    for(bit = 0; bit < C_DARRAY_NBBY; bit++)
    {
      if(b & 1)
        break;
      b >>= 1;
    }
    e = C_DARRAY_NBBY * byte + bit;
  }
  if(index) *index = e;
  *(a->free_list + byte) &= C_DARRAY_USED_MASK(bit);

  return(memcpy((void *)(a->mem + (e * a->elemsz)), data, a->elemsz));
}

/*
 */

void *C_darray_restore(c_darray_t *a, uint_t index)
{
  div_t dv;

  if(!a)
    return(NULL);
  if(index >= a->bot)
    return(NULL);

  dv = div(index, C_DARRAY_NBBY);

  return((*(a->free_list + dv.quot) & C_DARRAY_FREE_MASK(dv.rem))
         ? NULL : (void *)(a->mem + (index * a->elemsz)));
}

/*
 */

c_bool_t C_darray_delete(c_darray_t *a, uint_t index)
{
  div_t dv;
  char *b;

  if(!a)
    return(FALSE);
  if(index >= a->bot)
    return(FALSE);
  dv = div(index, C_DARRAY_NBBY);
  b = (a->free_list + dv.quot);
  if(*b & C_DARRAY_FREE_MASK(dv.rem))
    return(FALSE);

  *b |= C_DARRAY_FREE_MASK(dv.rem);
  a->del_count++;

  return(TRUE);
}

/*
 */

c_darray_t *C_darray_load(const char *path)
{
  FILE *fp;
  c_darray_t *a;

  if(!path)
    return(NULL);
  if(!*path)
    return(NULL);
  if(!(fp = fopen(path, "r")))
    return(NULL);

  a = C_new(c_darray_t);
  if(fread((void *)a, sizeof(c_darray_t), (size_t)1, fp) != 1)
  {
    C_free(a);
    fclose(fp);
    return(NULL);
  }

  a->free_list = C_newstr(a->isize);
  if(fread((void *)a->free_list, sizeof(char), a->isize, fp) != a->isize)
  {
    fclose(fp);
    C_free(a->free_list);
    C_free(a);
    return(NULL);
  }

  a->mem = (void *)C_malloc(a->size * a->elemsz, char);
  if(fread(a->mem, a->elemsz, a->size, fp) != a->size)
  {
    fclose(fp);
    C_free(a->free_list);
    C_free(a->mem);
    C_free(a);
    return(NULL);
  }
  
  fclose(fp);
  return(a);
}

/*
 */

c_bool_t C_darray_save(c_darray_t *a, const char *path)
{
  FILE *fp;

  if(!a || !path)
    return(FALSE);
  if(!*path)
    return(FALSE);
  if(!(fp = fopen(path, "w")))
    return(FALSE);

  if(fwrite((void *)a, sizeof(c_darray_t), (size_t)1, fp) != 1)
  {
    fclose(fp);
    return(FALSE);
  }

  if(fwrite((void *)a->free_list, sizeof(char), a->isize, fp) != a->isize)
  {
    fclose(fp);
    return(FALSE);
  }

  if(fwrite(a->mem, a->elemsz, a->size, fp) != a->size)
  {
    fclose(fp);
    return(FALSE);
  }

  fclose(fp);
  return(TRUE);
}

/*
 */

c_darray_t *C_darray_defragment(c_darray_t *a)
{
  uint_t i;
  c_darray_t *n;
  void *e;

  if(!a) return(NULL);
  if(!a->del_count) return(a);

  n = C_darray_create(a->resize, a->elemsz);
  for(i = 0; i < a->bot; i++)
    if((e = C_darray_restore(a, i)))
      C_darray_store(n, e, NULL);
  C_darray_destroy(a);

  return(n);
}

/*
 */

c_bool_t C_darray_iterate(c_darray_t *a,
                          c_bool_t (*iter)(void *elem, uint_t index,
                                           void *hook),
                          uint_t index, void *hook)
{
  char *byp, by, bil;
  div_t dv;
  uint_t i, left;
  void *e;

  if(!a || !iter)
    return(FALSE);
  if(a->bot < index)
    return(TRUE);
  dv = div(index, C_DARRAY_NBBY);

  for(e = a->mem + (a->elemsz * index),
        byp = a->free_list + dv.quot,
        i = 0,
        left = a->bot - index,
        bil = C_DARRAY_NBBY - dv.rem,
        by = *byp >> dv.rem;
      left--;
      i++, e += a->elemsz)
  {
    if(!(by & 1))
      if(!iter(e, i, hook))
        return(FALSE);

    if(!--bil)
      bil = C_DARRAY_NBBY, by = *(byp++);
  }
  
  return(TRUE);
}

/* end of darray */

static c_link_t *__C_linklist_unlink_r(c_linklist_t *l, c_link_t **p)
{
  c_link_t *r;

  r = *p;
  if(l->head == r)
    l->head = r->next;
  if(l->tail == r)
    l->tail = r->prev;
  if((*p)->prev)
    (*p)->prev->next = (*p)->next;
  if((*p)->next)
    (*p)->next->prev = (*p)->prev;
  (*p) = (*p)->next;
  l->size--;

  return(r);
}

/*
 */

c_linklist_t *C_linklist_create(void)
{
  c_linklist_t *l = C_new(c_linklist_t);

  l->head = l->tail = l->p = NULL;
  l->size = 0L;

  return(l);
}

/*
 */

void C_linklist_destroy(c_linklist_t *l)
{
  c_link_t *p, *q;

  if(!l)
    return;

  for(p = l->head; p;)
  {
    q = p;
    p = p->next;

    if(l->destructor)
      l->destructor(q->data);
    
    C_free(q);
  }
  C_free(l);
}

/*
 */

c_bool_t C_linklist_set_destructor(c_linklist_t *l,
                                   void (*destructor)(void *data))
{
  if(!l)
    return(FALSE);
  
  l->destructor = destructor;
  return(TRUE);
}

/*
 */

c_bool_t C_linklist_store(c_linklist_t *l, const void *data)
{
  if(!l)
    return(FALSE);

  return(C_linklist_store_r(l, data, &(l->p)));
}
  
/*
 */

c_bool_t C_linklist_store_r(c_linklist_t *l, const void *data, c_link_t **p)
{
  c_link_t *q;

  if(!l || !data)
    return(FALSE);

  q = C_new(c_link_t);
  q->data = (void *)data;

  if(*p == l->head) /* new head? */
  {
    q->next = l->head;
    q->prev = NULL;
    if(l->head)
      l->head->prev = q;
    l->head = q;
    if(!l->tail)
      l->tail = q;
  }
  else if(!(*p)) /* new tail? */
  {
    q->next = NULL;
    q->prev = l->tail;
    if(l->tail)
      l->tail->next = q;
    l->tail = q;
  }
  else
  {
    q->next = *p;
    q->prev = (*p)->prev;
    if((*p)->prev)
      (*p)->prev->next = q;
    (*p)->prev = q;
  }
  *p = q;
  l->size++;
  
  return(TRUE);
}

/*
 */

void *C_linklist_restore(c_linklist_t *l)
{
  if(!l)
    return(NULL);
  
  return(C_linklist_restore_r(l, &(l->p)));
}

/*
 */

void *C_linklist_restore_r(c_linklist_t *l, c_link_t **p)
{
  
  if(!l)
    return(NULL);

  return(*p ? (*p)->data : NULL);
}

/*
 */

c_bool_t C_linklist_search(c_linklist_t *l, const void *data)
{
  if(!l)
    return(FALSE);

  return(C_linklist_search_r(l, data, &(l->p)));
}

/*
 */

c_bool_t C_linklist_search_r(c_linklist_t *l, const void *data, c_link_t **p)
{
  void *d;

  for(C_linklist_move_head_r(l, p);
      (d = C_linklist_restore_r(l, p)) != NULL;
      C_linklist_move_next_r(l, p))
  {
    if(d == data)
      return(TRUE);
  }

  return(FALSE);
}

/*
 */

c_bool_t C_linklist_prepend(c_linklist_t *l, const void *data)
{
  c_link_t *p;
  
  if(!l || !data)
    return(FALSE);

  C_linklist_move_head_r(l, &p);
  return(C_linklist_store_r(l, data, &p));
}

/*
 */

void *C_linklist_pop(c_linklist_t *l)
{
  void *r;
  c_link_t *p, *q;

  if(!l)
    return(NULL);
  if(!l->size)
    return(NULL);

  C_linklist_move_head_r(l, &p);
  q = __C_linklist_unlink_r(l, &p);
  r = q->data;
  C_free(q);

  return(r);
}

/*
 */

void *C_linklist_peek(c_linklist_t *l)
{
  c_link_t *p;
  
  if(!l)
    return(NULL);
  if(!l->size)
    return(NULL);

  C_linklist_move_head_r(l, &p);
  return(C_linklist_restore_r(l, &p));
}

/*
 */

c_bool_t C_linklist_append(c_linklist_t *l, const void *data)
{
  c_link_t *p;

  if(!l || !data)
    return(FALSE);

  C_linklist_move_end_r(l, &p);
  return(C_linklist_store_r(l, data, &p));
}

/*
 */

c_bool_t C_linklist_delete(c_linklist_t *l)
{
  if(!l)
    return(FALSE);
  if(!l->p)
    return(FALSE);

  return(C_linklist_delete_r(l, &(l->p)));
}

/*
 */

c_bool_t C_linklist_delete_r(c_linklist_t *l, c_link_t **p)
{
  c_link_t *r;

  if(!l)
    return(FALSE);
  if(!*p)
    return(FALSE);

  r = __C_linklist_unlink_r(l, p);

  if(l->destructor)
    l->destructor(r->data);
  
  C_free(r);

  return(TRUE);
}

/*
 */

c_bool_t C_linklist_move(c_linklist_t *l, int where)
{
  if(!l)
    return(FALSE);
  
  return(C_linklist_move_r(l, where, &(l->p)));
}

/*
 */

c_bool_t C_linklist_move_r(c_linklist_t *l, int where, c_link_t **p)
{

  if(!l)
    return(FALSE);

  switch(where)
  {
    case C_LINKLIST_HEAD:
      *p = l->head;
      return(TRUE);

    case C_LINKLIST_TAIL:
      *p = l->tail;
      return(TRUE);

    case C_LINKLIST_NEXT:
      if(*p)
      {
        *p = (*p)->next;
        return(TRUE);
      }
      break;

    case C_LINKLIST_PREV:
      if(*p)
        if((*p)->prev)
        {
          *p = (*p)->prev;
          return(TRUE);
        }
      break;

    case C_LINKLIST_END:
      *p = NULL;
      return(TRUE);
      break;
  }

  return(FALSE);
}

/* end of linklist */

