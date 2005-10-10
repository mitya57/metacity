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
 *   gcc -g testboxes.c boxes.c -o testboxes \
 *     $(pkg-config --cflags --libs glib-2.0)
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

static void
print_rect_list (GList *rects, const char *prefix)
{
  while (rects)
    {
      printf("%s", prefix);
      print_rect (rects->data);
      printf("\n");
      rects = rects->next;
    }
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

static void
free_strut_list (GSList *struts)
{
  GSList *tmp = struts;
  while (tmp)
    {
      g_free (tmp->data);
      tmp = tmp->next;
    }
  g_slist_free (struts);
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

  g_assert (which >=0 && which <= 3);
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
      struts = g_slist_prepend (struts, new_meta_rect ( 300, 1150,  150,   50));
      break;
    case 3:
      struts = g_slist_prepend (struts, new_meta_rect (   0,    0, 1600,   20));
      struts = g_slist_prepend (struts, new_meta_rect ( 800, 1100,  400,  100));
      struts = g_slist_prepend (struts, new_meta_rect ( 300, 1150,   80,   50));
      struts = g_slist_prepend (struts, new_meta_rect ( 700,  525,  200,  150));
      break;
    }

  ret = meta_rectangle_get_minimal_spanning_set_for_region (&basic_rect, struts,
                                                            0, 0, 0, 0);

  free_strut_list (struts);
  return ret;
}

#if 0
void
test_merge_regions ()
{
  /* logarithmically distributed random number of struts (range?)
   * logarithmically distributed random size of struts (up to screen size???)
   * uniformly distributed location of center of struts (within screen)
   * merge all regions that are possible
   * print stats on problem setup
   *   number of (non-completely-occluded?) struts 
   *   percentage of screen covered
   *   length of resulting non-minimal spanning set
   *   length of resulting minimal spanning set
   * print stats on merged regions:
   *   number boxes merged
   *   number of those merges that were of the form A contains B
   *   number of those merges that were of the form A partially contains B
   *   number of those merges that were of the form A is adjacent to B
   */

  GList* region;
  GList* compare;
  MetaRectangle answer;
  int num_contains, num_merged, num_part_contains, num_adjacent;

  num_contains = num_merged = num_part_contains = num_adjacent = 0;
  compare = region = get_screen_region (2);
  g_assert (region);

  printf ("Merging stats:\n");
  printf ("  Length of initial list: %d\n", g_list_length (region));
#ifdef PRINT_DEBUG
  printf ("  Initial rectangles:\n");
  print_rect_list (region, "    ");
#endif

  while (compare && compare->next)
    {
      MetaRectangle *a = compare->data;
      GList *other = compare->next;

      g_assert (a->width > 0 && a->height > 0);

      while (other)
        {
          MetaRectangle *b = other->data;
          GList *delete_me = NULL;

          g_assert (b->width > 0 && b->height > 0);

#ifdef PRINT_DEBUG
          printf ("    -- Comparing %d,%d +%d,%d  to  %d,%d + %d,%d --\n",
                  a->x, a->y, a->width, a->height,
                  b->x, b->y, b->width, b->height);
#endif

          /* If a contains b, just remove b */
          if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = other;
              num_contains++;
              num_merged++;
            }
          /* If b contains a, just remove a */
          else if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = compare;
              num_contains++;
              num_merged++;
            }
          /* If a and b might be mergeable horizontally */
          else if (a->y == b->y && a->height == b->height)
            {
              /* If a and b overlap */
              if (meta_rectangle_overlap (a, b))
                {
                  int new_x = MIN (a->x, b->x);
                  a->width = MAX (a->x + a->width, b->x + b->width) - new_x;
                  a->x = new_x;
                  delete_me = other;
                  num_part_contains++;
                  num_merged++;
                }
              /* If a and b are adjacent */
              else if (a->x + a->width == b->x || a->x == b->x + b->width)
                {
                  int new_x = MIN (a->x, b->x);
                  a->width = MAX (a->x + a->width, b->x + b->width) - new_x;
                  a->x = new_x;
                  delete_me = other;
                  num_adjacent++;
                  num_merged++;
                }
            }
          /* If a and b might be mergeable vertically */
          else if (a->x == b->x && a->width == b->width)
            {
              /* If a and b overlap */
              if (meta_rectangle_overlap (a, b))
                {
                  int new_y = MIN (a->y, b->y);
                  a->height = MAX (a->y + a->height, b->y + b->height) - new_y;
                  a->y = new_y;
                  delete_me = other;
                  num_part_contains++;
                  num_merged++;
                }
              /* If a and b are adjacent */
              else if (a->y + a->height == b->y || a->y == b->y + b->height)
                {
                  int new_y = MIN (a->y, b->y);
                  a->height = MAX (a->y + a->height, b->y + b->height) - new_y;
                  a->y = new_y;
                  delete_me = other;
                  num_adjacent++;
                  num_merged++;
                }
            }

          other = other->next;

          /* Delete any rectangle in the list that is no longer wanted */
          if (delete_me != NULL)
            {
              MetaRectangle *bla = delete_me->data;
#ifdef PRINT_DEBUG
              printf ("    Deleting rect %d,%d +%d,%d\n",
                      bla->x, bla->y, bla->width, bla->height);
#endif

              /* Deleting the rect we're compare others to is a little tricker */
              if (compare == delete_me)
                {
                  compare = compare->next;
                  other = compare->next;
                  a = compare->data;
                }

              /* Okay, we can free it now */
              g_free (delete_me->data);
              region = g_list_delete_link (region, delete_me);
            }

#ifdef PRINT_DEBUG
          printf ("    After comparison, new list is:\n");
          print_rect_list (region, "      ");
#endif
        }

      compare = compare->next;
    }

  printf ("  Num rectangles contained in others          : %d\n", 
          num_contains);
  printf ("  Num rectangles partially contained in others: %d\n", 
          num_part_contains);
  printf ("  Num rectangles adjacent to others           : %d\n", 
          num_adjacent);
  printf ("  Num rectangles merged with others           : %d\n",
          num_merged);
#ifdef PRINT_DEBUG
  printf ("  Final rectangles:\n");
  print_rect_list (region, "    ");
#endif

  meta_rectangle_free_spanning_set (region);
  region = NULL;

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}
#endif

static void
verify_lists_are_equal (GList *code, GList *answer)
{
  int which = 0;

  while (code && answer)
    {
      MetaRectangle *a = code->data;
      MetaRectangle *b = answer->data;

      if (a->x      != b->x     ||
          a->y      != b->y     ||
          a->width  != b->width ||
          a->height != b->height)
        {
          g_error ("%dth item in code answer answer lists do not match; "
                   "code rect: %d,%d + %d,%d; answer rect: %d,%d + %d,%d\n",
                   which,
                   a->x, a->y, a->width, a->height,
                   b->x, b->y, b->width, b->height);
        }

      code = code->next;
      answer = answer->next;

      which++;
    }

  /* Ought to be at the end of both lists; check if we aren't */
  if (code)
    {
      MetaRectangle *tmp = code->data;
      g_error ("code list longer than answer list by %d items; "
               "first extra item: %d,%d +%d,%d\n",
               g_list_length (code),
               tmp->x, tmp->y, tmp->width, tmp->height);
    }

  if (answer)
    {
      MetaRectangle *tmp = answer->data;
      g_error ("answer list longer than code list by %d items; "
               "first extra item: %d,%d +%d,%d\n",
               g_list_length (answer),
               tmp->x, tmp->y, tmp->width, tmp->height);
    }
}

void
test_regions_okay ()
{
  GList* region;
  GList* tmp;
  MetaRectangle answer;

  /* FIXME!!!!!!! I'm pretty sure this function leaks like a sieve!! */

  /*************************************************************/  
  /* Make sure test region 0 has the right spanning rectangles */
  /*************************************************************/  
  region = get_screen_region (0);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect (0, 0, 1600, 1200));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_spanning_set (tmp);
  meta_rectangle_free_spanning_set (region);

  /*************************************************************/  
  /* Make sure test region 1 has the right spanning rectangles */
  /*************************************************************/  
  region = get_screen_region (1);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect (0, 20,  400, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect (0, 20, 1600, 1140));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_spanning_set (tmp);
  meta_rectangle_free_spanning_set (region);

  /*************************************************************/
  /* Make sure test region 2 has the right spanning rectangles */
  /*************************************************************/  
  tmp = region = get_screen_region (2);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  300, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect ( 450,   20,  350, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect (1200,   20,  400, 1180));
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  800, 1130));
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20, 1600, 1080));
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_spanning_set (tmp);
  meta_rectangle_free_spanning_set (region);

  /*************************************************************/
  /* Make sure test region 3 has the right spanning rectangles */
  /*************************************************************/  
  tmp = region = get_screen_region (3);
  tmp = NULL;
  tmp = g_list_prepend (tmp, new_meta_rect ( 380,  675,  420,  525)); // 220500
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  300, 1180)); // 354000
  tmp = g_list_prepend (tmp, new_meta_rect ( 380,   20,  320, 1180)); // 377600
  tmp = g_list_prepend (tmp, new_meta_rect (   0,  675,  800,  475)); // 380000
  tmp = g_list_prepend (tmp, new_meta_rect (1200,   20,  400, 1180)); // 472000
  tmp = g_list_prepend (tmp, new_meta_rect (   0,  675, 1600,  425)); // 680000
  tmp = g_list_prepend (tmp, new_meta_rect ( 900,   20,  700, 1080)); // 756000
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20,  700, 1130)); // 791000
  tmp = g_list_prepend (tmp, new_meta_rect (   0,   20, 1600,  505)); // 808000
#if 0
  printf ("Got to here...\n");
  print_rect_list (region, "  ");
  printf (" vs. \n");
  print_rect_list (tmp, "  ");
#endif
  verify_lists_are_equal (region, tmp);
  meta_rectangle_free_spanning_set (tmp);
  meta_rectangle_free_spanning_set (region);

  /* FIXME: Still to do:
   *   - Create random struts and check the regions somehow
   *     - Don't forget to test for empty rects
   *     - Don't forget to test if I ever get an empty region if the
   *       struts cover everything; should I just ignore all struts in
   *       such a case???
   */

  printf ("%s passed.\n", __PRETTY_FUNCTION__);
}

void
test_region_fitting ()
{
  /* FIXME!!!!!  I merely copied and pasted this; this function really hasn't
   * been written yet.
   */

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
