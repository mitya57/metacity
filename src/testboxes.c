/* Metacity box operation testing program */

/* 
 * Copyright (C) 2005 Elijah Newren
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* Note that you can compile this function with the following very simple
 * command line:
 *   gcc testboxes.c boxes.c -o testboxes $(pkg-config --cflags --libs glib-2.0)
 * IF you first add the line
 *   #define meta_warning(s)
 * near the beginning of boxes.c.
 */

#include <glib.h>
#include "boxes.h"

#define NUM_RANDOM_RUNS 100

static void
init_random_ness ()
{
  srand(time(NULL));
}

static void
get_random_rect (MetaRectangle *rect)
{
  rect->x = rand () % 1600;
  rect->y = rand () % 1200;
  rect->width  = rand () % 1600;
  rect->height = rand () % 1200;
}

static void
print_rect (const MetaRectangle *rect)
{
  printf ("%d,%d +%d,%d", rect->x, rect->y, rect->width, rect->height);
}

static void
print_rects (const MetaRectangle *rect1, const MetaRectangle *rect2)
{
  print_rect (rect1);
  printf ("   ");
  print_rect (rect2);
}

static void
alt_print_rect (const MetaRectangle *rect)
{
  printf ("%d,%d %d,%d", 
          rect->x,                   rect->y, 
          rect->x + rect->width - 1, rect->y + rect->height - 1);
}

static void
alt_print_rects (const MetaRectangle *rect1, const MetaRectangle *rect2)
{
  alt_print_rect (rect1);
  printf ("   ");
  alt_print_rect (rect2);
}

static MetaRectangle
meta_rect (int x, int y, int width, int height)
{
  MetaRectangle temporary;
  temporary.x = x;
  temporary.y = y;
  temporary.width  = width;
  temporary.height = height;

  return temporary;
}

static MetaRectangle*
new_meta_rect (int x, int y, int width, int height)
{
  MetaRectangle* temporary;
  temporary = g_new (MetaRectangle, 1);
  temporary->x = x;
  temporary->y = y;
  temporary->width  = width;
  temporary->height = height;

  return temporary;
}

void
test_area ()
{
  MetaRectangle temp;
  int i;
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp);
      g_assert (meta_rectangle_area (&temp) == temp.width * temp.height);
    }

  temp = meta_rect (0, 0, 5, 7);
  g_assert (meta_rectangle_area (&temp) == 35);

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

void
test_intersect ()
{
  MetaRectangle a = {100, 200,  50,  40};
  MetaRectangle b = {  0,  50, 110, 152};
  MetaRectangle c = {  0,   0,  10,  10};
  MetaRectangle d = {100, 100,  50,  50};
  MetaRectangle temp;
  MetaRectangle temp2;

  meta_rectangle_intersect (&a, &b, &temp);
  temp2 = meta_rect (100, 200, 10, 2);
  g_assert (meta_rectangle_equal (&temp, &temp2));
  g_assert (meta_rectangle_area (&temp) == 20);

  meta_rectangle_intersect (&a, &c, &temp);
  g_assert (meta_rectangle_area (&temp) == 0);

  meta_rectangle_intersect (&a, &d, &temp);
  g_assert (meta_rectangle_area (&temp) == 0);

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

void
test_equal ()
{
  MetaRectangle a = {10, 12, 4, 18};
  MetaRectangle b = a;
  MetaRectangle c = {10, 12, 4, 19};
  MetaRectangle d = {10, 12, 7, 18};
  MetaRectangle e = {10, 62, 4, 18};
  MetaRectangle f = {27, 12, 4, 18};

  g_assert ( meta_rectangle_equal (&a, &b));
  g_assert (!meta_rectangle_equal (&a, &c));
  g_assert (!meta_rectangle_equal (&a, &d));
  g_assert (!meta_rectangle_equal (&a, &e));
  g_assert (!meta_rectangle_equal (&a, &f));

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

void
test_overlap_funcs ()
{
  MetaRectangle temp1, temp2;
  int i;
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp1);
      get_random_rect (&temp2);
      g_assert (meta_rectangle_overlap (&temp1, &temp2) ==
                (meta_rectangle_horiz_overlap (&temp1, &temp2) &&
                 meta_rectangle_vert_overlap (&temp1, &temp2)));
    }

  temp1 = meta_rect ( 0, 0, 10, 10);
  temp2 = meta_rect (20, 0, 10,  5);
  g_assert (!meta_rectangle_overlap (&temp1, &temp2));
  g_assert (!meta_rectangle_horiz_overlap (&temp1, &temp2));
  g_assert ( meta_rectangle_vert_overlap (&temp1, &temp2));

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

void
test_basic_fitting ()
{
  MetaRectangle temp1, temp2, temp3;
  int i;
  /* Four cases:
   *   case   temp1 fits temp2    temp1 could fit temp2
   *     1           Y                      Y
   *     2           N                      Y
   *     3           Y                      N
   *     4           N                      N
   * Of the four cases, case 3 is impossible.  An alternate way of looking
   * at this table is that either the middle column must be no, or the last
   * column must be yes.  So we test that--plus the same reversing temp1
   * and temp2.
   */
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp1);
      get_random_rect (&temp2);
      g_assert (meta_rectangle_contains_rect (&temp1, &temp2) == FALSE ||
                meta_rectangle_contains_rect (&temp1, &temp2) == TRUE);
      g_assert (meta_rectangle_contains_rect (&temp2, &temp1) == FALSE ||
                meta_rectangle_contains_rect (&temp2, &temp1) == TRUE);
    }

  temp1 = meta_rect ( 0, 0, 10, 10);
  temp2 = meta_rect ( 5, 5,  5,  5);
  temp3 = meta_rect ( 8, 2,  3,  7);
  g_assert ( meta_rectangle_contains_rect (&temp1, &temp2));
  g_assert (!meta_rectangle_contains_rect (&temp2, &temp1));
  g_assert (!meta_rectangle_contains_rect (&temp1, &temp3));
  g_assert ( meta_rectangle_could_fit_rect (&temp1, &temp3));
  g_assert (!meta_rectangle_could_fit_rect (&temp3, &temp2));

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

static GList*
get_screen_region (int which)
{
  GList *ret;
  GSList *struts;
  MetaRectangle basic_rect;

  basic_rect = meta_rect (0, 0, 1600, 1200);
  ret = NULL;
  struts = NULL;

  g_assert (which >=0 && which <= 2);
  switch (which)
    {
    case 0:
      break;
    case 1:
      struts = g_slist_prepend (struts, new_meta_rect (   0,    0, 1600,   20));
      struts = g_slist_prepend (struts, new_meta_rect ( 400, 1160, 1600,   40));
      break;
    case 2:
      struts = g_slist_prepend (struts, new_meta_rect (   0,    0, 1600,   20));
      struts = g_slist_prepend (struts, new_meta_rect ( 800, 1100,  400,  100));
      struts = g_slist_prepend (struts, new_meta_rect ( 300, 1150,  100,   50));
      break;
    case 3:
      struts = g_slist_prepend (struts, new_meta_rect (   0,    0, 1600,   20));
      struts = g_slist_prepend (struts, new_meta_rect ( 800, 1100,  400,  100));
      struts = g_slist_prepend (struts, new_meta_rect ( 300, 1150,  100,   50));
      struts = g_slist_prepend (struts, new_meta_rect ( 700,  525,  200,  150));
      break;
    }

  ret = meta_rectangle_get_minimal_spanning_set_for_region (&basic_rect, struts,
                                                            0, 0, 0, 0);

  return ret;
}

void
test_regions_okay ()
{
  GList* region;
  GList* tmp;
  MetaRectangle answer;

  /* Make sure region 0 has the right spanning rectangles */
  region = get_screen_region (0);
  g_assert (region);
  answer = meta_rect (0, 0, 1600, 1200);
  g_assert (meta_rectangle_equal (region->data, &answer));
  meta_rectangle_free_spanning_set (region);
  
  /* Make sure region 1 has the right spanning rectangles */
  tmp = region = get_screen_region (1);
  g_assert (region);

  answer = meta_rect (0, 20, 1600, 1140);
  g_assert (meta_rectangle_equal (tmp->data, &answer));
  g_assert (tmp->next);

  tmp = tmp->next;
  answer = meta_rect (0, 20, 400, 1180);
  g_assert (meta_rectangle_equal (tmp->data, &answer));

  meta_rectangle_free_spanning_set (region);

  /* Make sure region 2 has the right spanning rectangles */
  tmp = region = get_screen_region (2);
  g_assert (region);

  printf ("All rects in rection 2\n");
  while (tmp)
    {
      printf("  ");
      print_rect (tmp->data);
      printf("\n");
      tmp = tmp->next;
    }
  tmp = region;

  answer = meta_rect (   0,   20, 1600, 1080);
  g_assert (meta_rectangle_equal (tmp->data, &answer));
  g_assert (tmp->next);

  tmp = tmp->next;
  answer = meta_rect (   0,   20,  800, 1130);
  print_rect (tmp->data); printf("\n");
  g_assert (meta_rectangle_equal (tmp->data, &answer));
  g_assert (tmp->next);

  tmp = tmp->next;
  answer = meta_rect (1200,   20,  400, 1180);
  g_assert (meta_rectangle_equal (tmp->data, &answer));
  g_assert (tmp->next);

  tmp = tmp->next;
  answer = meta_rect ( 400,   20,  400, 1140);
  g_assert (meta_rectangle_equal (tmp->data, &answer));
  g_assert (tmp->next);

  tmp = tmp->next;
  answer = meta_rect (   0,   20,  300, 1140);
  g_assert (meta_rectangle_equal (tmp->data, &answer));

  meta_rectangle_free_spanning_set (region);

  /*
      struts = g_slist_prepend (struts, new_meta_rect (   0,    0, 1600,   20));
      struts = g_slist_prepend (struts, new_meta_rect ( 800, 1100,  400,  100));
      struts = g_slist_prepend (struts, new_meta_rect ( 300, 1150,  100,   50));
      break;
    case 3:
      struts = g_slist_prepend (struts, new_meta_rect (   0,    0, 1600,   20));
      struts = g_slist_prepend (struts, new_meta_rect ( 800, 1100,  400,  100));
      struts = g_slist_prepend (struts, new_meta_rect ( 300, 1150,  100,   50));
      struts = g_slist_prepend (struts, new_meta_rect ( 700,  525,  200,  150));
  */


  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

void
test_region_fitting ()
{
  MetaRectangle temp1, temp2, temp3;
  int i;
  /* Four cases:
   *   case   temp1 fits temp2    temp1 could fit temp2
   *     1           Y                      Y
   *     2           N                      Y
   *     3           Y                      N
   *     4           N                      N
   * Of the four cases, case 3 is impossible.  An alternate way of looking
   * at this table is that either the middle column must be no, or the last
   * column must be yes.  So we test that--plus the same reversing temp1
   * and temp2.
   */
  for (i = 0; i < NUM_RANDOM_RUNS; i++)
    {
      get_random_rect (&temp1);
      get_random_rect (&temp2);
      g_assert (meta_rectangle_contains_rect (&temp1, &temp2) == FALSE ||
                meta_rectangle_contains_rect (&temp1, &temp2) == TRUE);
      g_assert (meta_rectangle_contains_rect (&temp2, &temp1) == FALSE ||
                meta_rectangle_contains_rect (&temp2, &temp1) == TRUE);
    }

  temp1 = meta_rect ( 0, 0, 10, 10);
  temp2 = meta_rect ( 5, 5,  5,  5);
  temp3 = meta_rect ( 8, 2,  3,  7);
  g_assert ( meta_rectangle_contains_rect (&temp1, &temp2));
  g_assert (!meta_rectangle_contains_rect (&temp2, &temp1));
  g_assert (!meta_rectangle_contains_rect (&temp1, &temp3));
  g_assert ( meta_rectangle_could_fit_rect (&temp1, &temp3));
  g_assert (!meta_rectangle_could_fit_rect (&temp3, &temp2));

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}


int
main()
{
  init_random_ness ();
  test_area ();
  test_intersect ();
  test_equal ();
  test_overlap_funcs ();
  test_basic_fitting ();

  test_regions_okay ();
  test_region_fitting ();

  printf ("All tests passed.\n");
  return 0;
}
