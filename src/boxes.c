/* Simple box operations */

/* 
 * Copyright (C) 2005 Elijah Newren
 * [According to the ChangeLog, Anders, Havoc, and Rob were responsible
 * for the meta_rectangle_intersect() and meta_rectangle_equal()
 * functions that I copied from display.c]
 * Copyright (C) 2002 Anders Carlsson
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
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

#include "boxes.h"
#include "util.h"

#if 0
static void   free_the_list                (GList *list);
static GList* get_optimal_locations        (GList *region);
static GList* remove_rectangle_from_region (const MetaRectangle *rect,
                                            GList               *region);
static void   clip_rectangle_away_from_spot(MetaRectangle       *rect,
                                            const MetaRectangle *bad_rect);
static void   move_rectangle_away_from_spot(MetaRectangle       *rect,
                                            const MetaRectangle *bad_rect,
                                            gboolean             shortest_path);
#endif

/* PRINT_DEBUG may be useful to define when compiling the testboxes program if
 * any issues crop up.
 */
/* #define PRINT_DEBUG */
#ifdef PRINT_DEBUG
static const char*
rect2String (const MetaRectangle *rect)
{
  static char* little_string = NULL;

  if (little_string == NULL)
    little_string = g_new(char, 100);

  sprintf(little_string, 
          "%d,%d +%d,%d",
          rect->x, rect->y, rect->width, rect->height);

  return little_string;
}
#endif

int
meta_rectangle_area (const MetaRectangle *rect)
{
  g_return_val_if_fail (rect != NULL, 0);
  return rect->width * rect->height;
}

gboolean
meta_rectangle_intersect (const MetaRectangle *src1,
			  const MetaRectangle *src2,
			  MetaRectangle *dest)
{
  int dest_x, dest_y;
  int dest_w, dest_h;
  int return_val;

  g_return_val_if_fail (src1 != NULL, FALSE);
  g_return_val_if_fail (src2 != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  return_val = FALSE;

  dest_x = MAX (src1->x, src2->x);
  dest_y = MAX (src1->y, src2->y);
  dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;
  
  if (dest_w > 0 && dest_h > 0)
    {
      dest->x = dest_x;
      dest->y = dest_y;
      dest->width = dest_w;
      dest->height = dest_h;
      return_val = TRUE;
    }
  else
    {
      dest->width = 0;
      dest->height = 0;
    }

  return return_val;
}

gboolean
meta_rectangle_equal (const MetaRectangle *src1,
                      const MetaRectangle *src2)
{
  return ((src1->x == src2->x) &&
          (src1->y == src2->y) &&
          (src1->width == src2->width) &&
          (src1->height == src2->height));
}

gboolean
meta_rectangle_overlap (const MetaRectangle *rect1,
                        const MetaRectangle *rect2)
{
  g_return_val_if_fail (rect1 != NULL, FALSE);
  g_return_val_if_fail (rect2 != NULL, FALSE);

  return !((rect1->x + rect1->width  <= rect2->x) ||
           (rect2->x + rect2->width  <= rect1->x) ||
           (rect1->y + rect1->height <= rect2->y) ||
           (rect2->y + rect2->height <= rect1->y));
}

gboolean
meta_rectangle_vert_overlap (const MetaRectangle *rect1,
                             const MetaRectangle *rect2)
{
  return (rect1->y < rect2->y + rect2->height &&
          rect2->y < rect1->y + rect1->height);
}

gboolean
meta_rectangle_horiz_overlap (const MetaRectangle *rect1,
                              const MetaRectangle *rect2)
{
  return (rect1->x < rect2->x + rect2->width &&
          rect2->x < rect1->x + rect1->width);
}

gboolean
meta_rectangle_could_fit_rect (const MetaRectangle *outer_rect,
                               const MetaRectangle *inner_rect)
{
  return (outer_rect->width  >= inner_rect->width &&
          outer_rect->height >= inner_rect->height);
}

gboolean
meta_rectangle_contains_rect  (const MetaRectangle *outer_rect,
                               const MetaRectangle *inner_rect)
{
  return 
    inner_rect->x                      >= outer_rect->x &&
    inner_rect->y                      >= outer_rect->y &&
    inner_rect->x + inner_rect->width  <= outer_rect->x + outer_rect->width &&
    inner_rect->y + inner_rect->height <= outer_rect->y + outer_rect->height;
}

/* Not so simple helper function for get_minimal_spanning_set_for_region() */
static GList*
merge_spanning_rects_in_region (GList *region)
{
  /* NOTE FOR ANY OPTIMIZATION PEOPLE OUT THERE: Please see the
   * documentation of get_minimal_spanning_set_for_region() for performance
   * considerations that also apply to this function.
   */

  GList* compare;
#ifdef PRINT_DEBUG
  int num_contains, num_merged, num_part_contains, num_adjacent;
  num_contains = num_merged = num_part_contains = num_adjacent = 0;
#endif
  compare = region;
  g_assert (region);

#ifdef PRINT_DEBUG
  printf ("Merging stats:\n");
  printf ("  Length of initial list: %d\n", g_list_length (region));
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
#ifdef PRINT_DEBUG
              num_contains++;
              num_merged++;
#endif
            }
          /* If b contains a, just remove a */
          else if (meta_rectangle_contains_rect (a, b))
            {
              delete_me = compare;
#ifdef PRINT_DEBUG
              num_contains++;
              num_merged++;
#endif
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
#ifdef PRINT_DEBUG
                  num_part_contains++;
                  num_merged++;
#endif
                }
              /* If a and b are adjacent */
              else if (a->x + a->width == b->x || a->x == b->x + b->width)
                {
                  int new_x = MIN (a->x, b->x);
                  a->width = MAX (a->x + a->width, b->x + b->width) - new_x;
                  a->x = new_x;
                  delete_me = other;
#ifdef PRINT_DEBUG
                  num_adjacent++;
                  num_merged++;
#endif
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
#ifdef PRINT_DEBUG
                  num_part_contains++;
                  num_merged++;
#endif
                }
              /* If a and b are adjacent */
              else if (a->y + a->height == b->y || a->y == b->y + b->height)
                {
                  int new_y = MIN (a->y, b->y);
                  a->height = MAX (a->y + a->height, b->y + b->height) - new_y;
                  a->y = new_y;
                  delete_me = other;
#ifdef PRINT_DEBUG
                  num_adjacent++;
                  num_merged++;
#endif
                }
            }

          other = other->next;

          /* Delete any rectangle in the list that is no longer wanted */
          if (delete_me != NULL)
            {
#ifdef PRINT_DEBUG
              MetaRectangle *bla = delete_me->data;
              printf ("    Deleting rect %d,%d +%d,%d\n",
                      bla->x, bla->y, bla->width, bla->height);
#endif

              /* Deleting the rect we compare others to is a little tricker */
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

#ifdef PRINT_DEBUG
  /* Note that I believe it will be the case that num_part_contains and
   * num_adjacent will alwyas be 0 while num_contains will be equal to
   * num_merged.  If so, this might be useful information to use to come up
   * with some kind of optimization for this funcation, given that there
   * exists someone who really wants to do that.
   */
  printf ("  Num rectangles contained in others          : %d\n", 
          num_contains);
  printf ("  Num rectangles partially contained in others: %d\n", 
          num_part_contains);
  printf ("  Num rectangles adjacent to others           : %d\n", 
          num_adjacent);
  printf ("  Num rectangles merged with others           : %d\n",
          num_merged);
  printf ("  Final rectangles:\n");
  print_rect_list (region, "    ");
#endif

  return region;
}

/* Simple helper function for get_minimal_spanning_set_for_region()... */
static gint
compare_rect_areas (gconstpointer a, gconstpointer b)
{
  const MetaRectangle *a_rect = (gconstpointer) a;
  const MetaRectangle *b_rect = (gconstpointer) b;

  int a_area = meta_rectangle_area (a_rect);
  int b_area = meta_rectangle_area (b_rect);

  return b_area - a_area; /* positive ret value denotes b > a, ... */
}

/* This function is trying to find a "minimal spanning set (of rectangles)"
 * for a given region.
 *
 * The region is given by taking basic_rect, then removing the areas
 * covered by all the rectangles in the all_struts list, and then expanding
 * the resulting region by the given number of pixels in each direction.
 *
 * A "minimal spanning set (of rectangles)" is the best name I could come
 * up with for the concept I had in mind.  Basically, for a given region, I
 * want a set of rectangles with the property that a window is contained in
 * the region if and only if it is contained within at least one of the
 * rectangles.
 *
 * The GList* returned will be a list of (allocated) MetaRectangles.
 * The list will need to be freed by calling
 * meta_rectangle_free_spanning_set() on it (or by manually
 * implementing that function...)
 */
GList*
meta_rectangle_get_minimal_spanning_set_for_region (
  const MetaRectangle *basic_rect,
  const GSList  *all_struts,
  const int      left_expand,
  const int      right_expand,
  const int      top_expand,
  const int      bottom_expand)
{
  /* NOTE FOR OPTIMIZERS: This function *might* be somewhat slow,
   * especially due to the call to merge_spanning_rects_in_region() (which
   * is O(n^2) where n is the size of the list generated in this function).
   * This is made more onerous due to the fact that it involves a fair
   * number of memory allocation and deallocation calls.  However, n is 1
   * for default installations of Gnome (because partial struts aren't used
   * by default and only partial struts increase the size of the spanning
   * set generated).  With one partial strut, n will be 2 or 3.  With 2
   * partial struts, n will probably be 4 or 5.  So, n probably isn't large
   * enough to make this worth bothering.  If it ever does show up on
   * profiles (most likely because people start using large numbers of
   * partial struts), possible optimizations include:
   *
   * (1) rewrite merge_spanning_rects_in_region() to be O(n) or O(nlogn).
   *     I'm not totally sure it's possible, but with a couple copies of
   *     the list and sorting them appropriately, I believe it might be.
   * (2) only call merge_spanning_rects_in_region() with a subset of the
   *     full list of rectangles.  I believe from some of my preliminary
   *     debugging and thinking about it that it is possible to figure out
   *     apriori groups of rectangles which are only merge candidates with
   *     each other.  (See testboxes.c:get_screen_region() when which==2
   *     and track the steps of this function carefully to see what gave
   *     me the hint that this might work)
   * (3) figure out how to avoid merge_spanning_rects_in_region().  I think
   *     it might be possible to modify this function to make that
   *     possible, and I spent just a little while thinking about it, but n
   *     wasn't large enough to convince me to care yet.
   * (4) just don't call this function that much.  Currently, it's called
   *     from a few places in constraints.c, and thus is called multiple
   *     times for every meta_window_constrain() call, which itself is
   *     called an awful lot.  However, the answer we provide is always the
   *     same unless the screen size, number of xineramas, or list of
   *     struts has changed.  I'm not aware of any case where screen size
   *     or number of xineramas changes without logging out.  struts change
   *     very rarely.  So we should be able to just save the appropriate
   *     info in the MetaWorkspace (or maybe MetaScreen), update it when
   *     the struts change, and then just use those precomputed values
   *     instead of calling this function so much.
   *
   * In terms of work, 1-3 would be hard (and I'm not entirely certain that
   * they would work) and 4 would be relatively easy.  4 would also provide
   * the greatest benefit.  Therefore, do 4 first.  Don't even think about
   * 1-3 or other micro-optimizations until you've done that one.
   */

  GList         *ret;
  GList         *tmp_list;
  const GSList  *strut_iter;
  MetaRectangle *temp_rect;

  /* The algorithm is basically as follows:
   *   Ignore directional expansions until the end
   *   Initialize rectangle_set to basic_rect
   *   Foreach strut:
   *     Foreach rectangle in rectangle_set:
   *       - Split the rectangle into new rectangles that don't overlap the
   *         strut (but which are as big as possible otherwise)
   *   Now do directional expansion of all rectangles in rectangle_set
   */

  temp_rect = g_new (MetaRectangle, 1);
  *temp_rect = *basic_rect;
  ret = g_list_prepend (NULL, temp_rect);
#ifdef PRINT_DEBUG
  printf("Initialized spanning set with %s.\n", rect2String (basic_rect));
#endif

  strut_iter = all_struts;
  while (strut_iter)
    {
      GList *rect_iter; 
      MetaRectangle *strut = (MetaRectangle*) strut_iter->data;
#ifdef PRINT_DEBUG
      printf("Dealing with strut %s.\n", rect2String (strut));
#endif
      tmp_list = ret;
      ret = NULL;
      rect_iter = tmp_list;
      while (rect_iter)
        {
          MetaRectangle *rect = (MetaRectangle*) rect_iter->data;
#ifdef PRINT_DEBUG
          printf("  Looking if we need to chop up %s.\n", rect2String (rect));
#endif
          if (!meta_rectangle_overlap (rect, strut))
            {
            ret = g_list_prepend (ret, rect);
#ifdef PRINT_DEBUG
            printf("    No chopping of %s.\n", rect2String (rect));
#endif
            }
          else
            {
              /* If there is area in rect left of strut */
              if (rect->x < strut->x)
                {
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  temp_rect->width = strut->x - rect->x;
                  ret = g_list_prepend (ret, temp_rect);
#ifdef PRINT_DEBUG
                  printf("    Added %s.\n", rect2String (temp_rect));
#endif
                }
              /* If there is area in rect right of strut */
              if (rect->x + rect->width > strut->x + strut->width)
                {
                  int new_x;
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  new_x = strut->x + strut->width;
                  temp_rect->width = rect->x + rect->width - new_x;
                  temp_rect->x = new_x;
                  ret = g_list_prepend (ret, temp_rect);
#ifdef PRINT_DEBUG
                  printf("    Added %s.\n", rect2String (temp_rect));
#endif
                }
              /* If there is area in rect above strut */
              if (rect->y < strut->y)
                {
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  temp_rect->height = strut->y - rect->y;
                  ret = g_list_prepend (ret, temp_rect);
#ifdef PRINT_DEBUG
                  printf("    Added %s.\n", rect2String (temp_rect));
#endif
                }
              /* If there is area in rect below strut */
              if (rect->y + rect->height > strut->y + strut->height)
                {
                  int new_y;
                  temp_rect = g_new (MetaRectangle, 1);
                  *temp_rect = *rect;
                  new_y = strut->y + strut->height;
                  temp_rect->height = rect->y + rect->height - new_y;
                  temp_rect->y = new_y;
                  ret = g_list_prepend (ret, temp_rect);
#ifdef PRINT_DEBUG
                  printf("    Added %s.\n", rect2String (temp_rect));
#endif
                }
              g_free (rect);
            }
          rect_iter = rect_iter->next;
        }
      g_list_free (tmp_list);
      strut_iter = strut_iter->next;
    }

  /* Now it's time to do the directional expansion */
  tmp_list = ret;
  while (tmp_list)
    {
      MetaRectangle *rect = (MetaRectangle*) tmp_list->data;
      rect->x      -= left_expand;
      rect->width  += (left_expand + right_expand);
      rect->y      -= top_expand;
      rect->height += (top_expand + bottom_expand);
      tmp_list = tmp_list->next;
    }

  /* Sort by maximal area, just because I feel like it... */
  ret = g_list_sort (ret, compare_rect_areas);

  /* Merge rectangles if possible so that the list really is minimal */
  ret = merge_spanning_rects_in_region (ret);

  return ret;
}

void
meta_rectangle_free_spanning_set (GList *spanning_rects)
{
  g_list_foreach (spanning_rects, 
                  (void (*)(gpointer,gpointer))&g_free, /* ew, for ugly */
                  NULL);
  g_list_free (spanning_rects);
}

gboolean
meta_rectangle_could_fit_in_region (const GList         *spanning_rects,
                                    const MetaRectangle *rect)
{
  const GList *temp;
  gboolean     could_fit;

  temp = spanning_rects;
  could_fit = FALSE;
  while (!could_fit && temp != NULL)
    {
      could_fit = could_fit || meta_rectangle_could_fit_rect (temp->data, rect);
      temp = temp->next;
    }

  return could_fit;
}

gboolean
meta_rectangle_contained_in_region (const GList         *spanning_rects,
                                    const MetaRectangle *rect)
{
  const GList *temp;
  gboolean     contained;

  temp = spanning_rects;
  contained = FALSE;
  while (!contained && temp != NULL)
    {
      contained = contained || meta_rectangle_contains_rect (temp->data, rect);
      temp = temp->next;
    }

  return contained;
}

void
meta_rectangle_clamp_to_fit_into_region (const GList         *spanning_rects,
                                         FixedDirections      fixed_directions,
                                         MetaRectangle       *rect,
                                         const MetaRectangle *min_size)
{
  const GList *temp;
  const MetaRectangle *best_rect = NULL;
  int                  best_overlap = 0;

  /* First, find best rectangle from spanning_rects to which we can clamp
   * rect to fit into.
   */
  temp = spanning_rects;
  while (temp)
    {
      int factor = 1;
      MetaRectangle *compare_rect = temp->data;
      int            maximal_overlap_amount_for_compare;
      
      /* If x is fixed and the entire width of rect doesn't fit in compare, set
       * factor to 0.
       */
      if ((fixed_directions & FIXED_DIRECTION_X) &&
          (compare_rect->x > rect->x || 
           compare_rect->x + compare_rect->width < rect->x + rect->width))
        factor = 0;
        
      /* If y is fixed and the entire height of rect doesn't fit in compare, set
       * factor to 0.
       */
      if ((fixed_directions & FIXED_DIRECTION_Y) &&
          (compare_rect->y > rect->y || 
           compare_rect->y + compare_rect->height < rect->y + rect->height))
        factor = 0;

      /* If compare can't hold the min_size window, set factor to 0 */
      if (compare_rect->width  < min_size->width ||
          compare_rect->height < min_size->height)
        factor = 0;

      /* Determine maximal overlap amount */
      maximal_overlap_amount_for_compare =
        MIN (rect->width,  compare_rect->width) *
        MIN (rect->height, compare_rect->height);
      maximal_overlap_amount_for_compare *= factor;

      /* See if this is the best rect so far */
      if (maximal_overlap_amount_for_compare > best_overlap)
        {
          best_rect    = compare_rect;
          best_overlap = maximal_overlap_amount_for_compare;
        }

      temp = temp->next;
    }

  /* Clamp rect appropriately */
  if (best_rect == NULL)
    {
      meta_warning ("No rect whose size to clamp to found!\n");

      /* If it doesn't fit, at least make it no bigger than it has to be */
      if (!(fixed_directions & FIXED_DIRECTION_X))
        rect->width  = min_size->width;
      if (!(fixed_directions & FIXED_DIRECTION_Y))
        rect->height = min_size->height;
    }
  else
    {
      rect->width  = MIN (rect->width,  best_rect->width);
      rect->height = MIN (rect->height, best_rect->height);
    }
}

void
meta_rectangle_clip_to_region (const GList         *spanning_rects,
                               FixedDirections      fixed_directions,
                               MetaRectangle       *rect)
{
  const GList *temp;
  const MetaRectangle *best_rect = NULL;
  int                  best_overlap = 0;

  /* First, find best rectangle from spanning_rects to which we will clip
   * rect into.
   */
  temp = spanning_rects;
  while (temp)
    {
      int factor = 1;
      MetaRectangle *compare_rect = temp->data;
      MetaRectangle  overlap;
      int            maximal_overlap_amount_for_compare;
      
      /* If x is fixed and the entire width of rect doesn't fit in compare, set
       * factor to 0.
       */
      if ((fixed_directions & FIXED_DIRECTION_X) &&
          (compare_rect->x > rect->x || 
           compare_rect->x + compare_rect->width < rect->x + rect->width))
        factor = 0;
        
      /* If y is fixed and the entire height of rect doesn't fit in compare, set
       * factor to 0.
       */
      if ((fixed_directions & FIXED_DIRECTION_Y) &&
          (compare_rect->y > rect->y || 
           compare_rect->y + compare_rect->height < rect->y + rect->height))
        factor = 0;

      /* Determine maximal overlap amount */
      meta_rectangle_intersect (rect, compare_rect, &overlap);
      maximal_overlap_amount_for_compare = meta_rectangle_area (&overlap);
      maximal_overlap_amount_for_compare *= factor;

      /* See if this is the best rect so far */
      if (maximal_overlap_amount_for_compare > best_overlap)
        {
          best_rect    = compare_rect;
          best_overlap = maximal_overlap_amount_for_compare;
        }

      temp = temp->next;
    }

  /* Clip rect appropriately */
  if (best_rect == NULL)
    meta_warning ("No rect to clip to found!\n");
  else
    {
      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_X))
        {
          /* Find the new left and right */
          int new_x = MAX (rect->x, best_rect->x);
          rect->width = MIN ((rect->x + rect->width)           - new_x,
                             (best_rect->x + best_rect->width) - new_x);
          rect->x = new_x;
        }

      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_Y))
        {
          /* Clip the top, if needed */
          int new_y = MAX (rect->y, best_rect->y);
          rect->height = MIN ((rect->y + rect->height)           - new_y,
                              (best_rect->y + best_rect->height) - new_y);
          rect->y = new_y;
        }
    }
}

void
meta_rectangle_shove_into_region (const GList         *spanning_rects,
                                  FixedDirections      fixed_directions,
                                  MetaRectangle       *rect)
{
  const GList *temp;
  const MetaRectangle *best_rect = NULL;
  int                  best_overlap = 0;
  int                  shortest_distance = G_MAXINT;

  /* First, find best rectangle from spanning_rects to which we will shove
   * rect into.
   */
  temp = spanning_rects;
  while (temp)
    {
      int factor = 1;
      MetaRectangle *compare_rect = temp->data;
      int            maximal_overlap_amount_for_compare;
      int            dist_to_compare;
      
      /* If x is fixed and the entire width of rect doesn't fit in compare, set
       * factor to 0.
       */
      if ((fixed_directions & FIXED_DIRECTION_X) &&
          (compare_rect->x > rect->x || 
           compare_rect->x + compare_rect->width < rect->x + rect->width))
        factor = 0;
        
      /* If y is fixed and the entire height of rect doesn't fit in compare, set
       * factor to 0.
       */
      if ((fixed_directions & FIXED_DIRECTION_Y) &&
          (compare_rect->y > rect->y || 
           compare_rect->y + compare_rect->height < rect->y + rect->height))
        factor = 0;

      /* Determine maximal overlap amount between rect & compare_rect */
      maximal_overlap_amount_for_compare =
        MIN (rect->width,  compare_rect->width) *
        MIN (rect->height, compare_rect->height);

      /* Determine distance necessary to put rect into comapre_rect */
      dist_to_compare = 0;
      if (compare_rect->x > rect->x)
        dist_to_compare += compare_rect->x - rect->x;
      if (compare_rect->x + compare_rect->width < rect->x + rect->width)
        dist_to_compare += (rect->x + rect->width) -
                           (compare_rect->x + compare_rect->width);
      if (compare_rect->y > rect->y)
        dist_to_compare += compare_rect->y - rect->y;
      if (compare_rect->y + compare_rect->height < rect->y + rect->height)
        dist_to_compare += (rect->y + rect->height) -
                           (compare_rect->y + compare_rect->height);

      /* If we'd have to move in the wrong direction, disqualify compare_rect */
      if (factor == 0)
        {
          maximal_overlap_amount_for_compare = 0;
          dist_to_compare = G_MAXINT;
        }

      /* See if this is the best rect so far */
      if ((maximal_overlap_amount_for_compare > best_overlap) ||
          (maximal_overlap_amount_for_compare == best_overlap &&
           dist_to_compare                    <  shortest_distance))
        {
          best_rect         = compare_rect;
          best_overlap      = maximal_overlap_amount_for_compare;
          shortest_distance = dist_to_compare;
        }

      temp = temp->next;
    }

  /* Shove rect appropriately */
  if (best_rect == NULL)
    meta_warning ("No rect to shove into found!\n");
  else
    {
      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_X))
        {
          /* Shove to the right, if needed */
          if (best_rect->x > rect->x)
            rect->x = best_rect->x;

          /* Shove to the left, if needed */
          if (best_rect->x + best_rect->width < rect->x + rect->width)
            rect->x = (best_rect->x + best_rect->width) - rect->width;
        }

      /* Extra precaution with checking fixed direction shouldn't be needed
       * due to logic above, but it shouldn't hurt either.
       */
      if (!(fixed_directions & FIXED_DIRECTION_Y))
        {
          /* Shove down, if needed */
          if (best_rect->y > rect->y)
            rect->y = best_rect->y;

          /* Shove up, if needed */
          if (best_rect->y + best_rect->height < rect->y + rect->height)
            rect->y = (best_rect->y + best_rect->height) - rect->height;
        }
    }
}

#if 0
#if 0
gboolean
rectangles_intersect (const MetaRectangle *rect1, 
                      const MetaRectangle *rect2)
{
  g_return_val_if_fail (rect1 != NULL, FALSE);
  g_return_val_if_fail (rect2 != NULL, FALSE);

  return (!(rect1->x + rect1->width  < rect2.x) ||
           (rect2->x + rect2->width  < rect1.x) ||
           (rect1->y + rect1->height < rect2.y) ||
           (rect2->y + rect2->height < rect1.y));
}
#endif

gboolean
region_contains_rectangle (const GList         *region,
                           const MetaRectangle *rect)
{
  /* The whole trick to this function, is that since region contains
   * non-overlapping rectangles, we just need to add up the area of
   * intersections between rect and various rectangles of region; if it
   * adds up to the area of rect then region contains rect.
   */
  GList *tmp;
  int    area;
  int    compare_area;

  area = meta_rectangle_area (rect);
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *compare_rect = tmp->data;
      MetaRectangle overlap;

      meta_rectangle_intersect (rect, compare_rect, &overlap);
      compare_area += meta_rectangle_area (&overlap);

      tmp = tmp->next;
    }

  return compare_area == area;
}

gboolean
region_could_contain_rectangle (const GList         *region,
                                const MetaRectangle *rect)
{
  GList         *tmp;
  GList         *optimal_locations;
  int            area;
  MetaRectangle  temp;
  gboolean       retval;

  /* If rect fits in any one of the rectangles in region then it could fit;
   * if the total area of the rectangles in region is less than that of rect,
   * then it can't fit.  Try these easy two checks first.
   */
  area = 0;
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *compare_rect = tmp->data;
      if (compare_rect->height >= rect->height &&
          compare_rect->width  >= rect->width)
        return TRUE;
      area += meta_rectangle_area (compare_rect);

      tmp = tmp->next;
    }

  if (area < meta_rectangle_area (rect))
    return FALSE;

  /* Well, that didn't work, unfortunately.  That means that the only
   * way that rect could fit in region is if it spans multiple
   * rectangles from region...  
   */
  optimal_locations = get_optimal_locations (region);
  tmp = optimal_locations;
  retval = FALSE:
  while (tmp != NULL &&
         retval != TRUE)
    {
      MetaRectangle *current = optimal_locations->data;
      temp.x = current->x;
      temp.y = current->y;
      temp.width = rect->width;
      temp.height = rect->height;

      if (region_contains_rectangle (region, temp))
        retval = TRUE;

      tmp = tmp->next;
    }

  /* Free the data */
  free_the_list (optimal_locations);
  return retval;
}

gboolean
move_rectangle_into_region (MetaRectangle *rect,
                            const GList   *region)
{
  /* The basic idea here is similar to clip_rectangle_into_region, except
   * that instead of clipping rect away from individual rectangles R, it is
   * shifted away from them.  This function returns whether such a strategy
   * was successful in placing rect within region.  If it fails, an
   * approach similar to that found in region_could_contain_rectangle could
   * be used to try to place rect in region.
   */
  GList *tmp;
  GList *bad_region
  MetaRectangle copy;
  MetaRectangle *temp;
  gboolean retval;

  if (region_contains_rectangle (region, rect))
    return TRUE;

  temp = g_new (MetaRectangle, 1);
  *temp = *rect;
  bad_region = g_list_prepend(NULL, temp);
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *compare_rect = tmp->data;
      bad_region = remove_rectangle_from_region (compare_rect, bad_region)
      tmp = tmp->next;
    }

  copy = *rect;
  tmp = bad_region;
  while (tmp != NULL)
    {
      MetaRectange *bad_rect = tmp->data;
      move_rectangle_away_from_spot(rect, bad_rect, TRUE);
      tmp = tmp->next;
    }

  if (region_contains_rectangle (region, rect))
    {
      retval = TRUE;
      goto done;
    }

  /* Initial effort failed; this can happen, for example, with two
   * panels that don't span the width of the screen, both of which are
   * at the bottom of the screen and which have some open space
   * between them.  If a window almost fits between them but just
   * barely overlaps one of the panels, the algorithm above will shove
   * the window over the other panel.  So we repeat the algorithm
   * above but use a different flag to move_rectangle_away_from_spot()
   * to try to avoid this kind of problem.
   */
  *rect = copy;
  tmp = bad_region;
  while (tmp != NULL)
    {
      MetaRectange *bad_rect = tmp->data;
      move_rectangle_away_from_spot(rect, bad_rect, FALSE);
      tmp = tmp->next;
    }
  retval = region_contains_rectangle (region, rect);

done:
  free_the_list (bad_region);

  return retval;
}

void
clip_rectangle_into_region (MetaRectangle *rect,
                            const GList   *region)
{
  /* The basic idea here is to progressively remove each rectangle in
   * region from rect; then take the resulting bad region and for each
   * rectangle R that makes it (the bad region) up, clip rect away from R.
   */
  GList *tmp;
  GList *bad_region

  MetaRectangle *temp = g_new (MetaRectangle, 1);
  *temp = *rect;
  bad_region = g_list_prepend(NULL, temp);
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *compare_rect = tmp->data;
      bad_region = remove_rectangle_from_region (compare_rect, bad_region)
      tmp = tmp->next;
    }
  tmp = bad_region;
  while (tmp != NULL)
    {
      MetaRectange *bad_rect = tmp->data;
      clip_rectangle_away_from_spot(rect, bad_rect);
      tmp = tmp->next;
    }
  free_the_list (bad_region);
}

#if 0
/* Old, buggy implementation... */
void
clip_rectangle_into_region (MetaRectangle *rect,
                            const GList   *region)
{
  g_assert (0==1);
  GList *tmp;
  GList *bad_region

  MetaRectangle *temp = new MetaRectangle;
  *temp = *rect;
  bad_region = g_list_prepend(NULL, temp);
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *compare_rect = tmp->data;
      bad_region = remove_rectangle_from_region (compare_rect, bad_region)
      tmp = tmp->next;
    }
  tmp = bad_region;
  if (bad_region != NULL)
    {
      /* Get the minimum and maximum x and y coordinates that are bad */
      int minx, miny, maxx, maxy;
      minx = miny = INT_MAX;
      maxx = maxy = INT_MIN;

      tmp = region;
      while (tmp != NULL)
        {
          MetaRectangle *compare_rect = tmp->data;
          if (compare_rect.x < minx)
            minx = compare_rect.x;
          if (compare_rect.x + compare_rect.width > maxx)
            maxx = compare_rect.x + compare_rect.width;
          if (compare_rect.y < miny)
            miny = compare_rect.y;
          if (compare_rect.y + compare_rect.height > maxy)
            maxy = compare_rect.y + compare_rect.height;
          tmp = tmp->next;
        }

      /* See which direction(s) would be best to clip in (preserve as
       * much area as possible)
       */
      
    }

  g_list_free (bad_region);
}
#endif

void
region_expand (GList             *region,
               int                expand_amount,
               MetaRectDirection  directions)
{
  GList *tmp;
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *rect;
      rect = tmp->data;

      if (directions & META_RECTANGLE_LEFT)
        {
          tmp->x      -= expand_amount;
          tmp->width  += expand_amount;
        }
      if (directions & META_RECTANGLE_RIGHT)
        {
          tmp->width  += expand_amount;
        }
      if (directions & META_RECTANGLE_TOP)
        {
          tmp->y      -= expand_amount;
          tmp->height += expand_amount;
        }
      if (directions & META_RECTANGLE_BOTTOM)
        {
          tmp->height += expand_amount;
        }

      tmp = tmp->next;
    }
}

static void
free_the_list (GList *list)
{
  GList *tmp;
  tmp = list;
  while (tmp != NULL)
    {
      g_free (tmp->data);
      tmp = tmp->next;
    }

  g_list_free (list);
}

static gint
sort_by_leftmost (const MetaRectangle *a, const MetaRectangle *b)
{
  return a->x - b->x;
}

static gint
sort_by_topmost (const MetaRectangle *a, const MetaRectangle *b)
{
  return a->y - b->y;
}

static gint
sort_by_left_then_by_top (const MetaRectangle *a, const MetaRectangle *b)
{
  if (a->x == b->x)
    return a->y - b->y;
  return a->x - b->x;
}

static GList*
get_optimal_locations (GList *region)
{
  /* 
   * We'd like to be able to determine if a rectangle can fit within a
   * region, where a region is the union of N adjacent, nonoverlapping
   * rectangles.  Note that the rectangles are not necessarily aligned
   * nicely.  We'd like to do so with a minimal set of points where
   * the test for fitting in the region will be that it does so if and
   * only if it will fit in the region when its upper left corner is
   * placed at one of the given points.  This could be easy if we just
   * specified every point in the domain, but the trick is trying to
   * get a minimal set of points (typically O(N) of them though it can
   * be O(N^2) in pathological cases)
   *
   * To understand what this function tries to do, draw some
   * qualifying regions and ask yourself, "If I'm given any old
   * rectangle, what are the best locations to stick it so that it's
   * most likely to fit in this region?"  That's what this algorithm
   * is trying to find.
   *
   * The basic idea is that we want the upper left corner of each
   * rectangle.  Call these locations UL[i].  The difference is that
   * if there is open space to the left of or above any given UL[i] in
   * a different rectangle then we may want to also add (or replace
   * UL[i] with) a point that is horizontally or vertically aligned
   * with UL[i] at the edge of the adjacent rectangle.
   *
   * The sorted_left_to_right and sorted_top_to_bottom arrays should
   * help us with the following of a given UL[i] to the real edge of
   * the domain in the left and above directions.
   */
  GList *locations;
  GList *tmp;
  GArray *sorted_left_to_right, *sorted_top_to_bottom;
  locations = NULL;

  int len = g_list_length (region);
  sorted_left_to_right = g_array_sized_new (FALSE, FALSE, sizeof (GList*), len);
  sorted_top_to_bottom = g_array_sized_new (FALSE, FALSE, sizeof (GList*), len);
  tmp = region;
  int i = 0;
  do
    {
      g_array_index (sorted_left_to_right, i, GList*) =
        g_array_index (sorted_top_to_bottom, i, GList*) = tmp;

      tmp = tmp->next;
      i++;
    }
  while (tmp != NULL);

  g_array_sort (sorted_left_to_right, sort_by_leftmost);
  g_array_sort (sorted_top_to_bottom, sort_by_topmost);

  for (int i=0; i<len; i++)
    {
      MetaRectangle *orig_compare_rect;
      MetaRectangle *new_point;

      orig_compare_rect = g_array_index (sorted_left_to_right, i, GList*)->data;

      /************************************/
      /* FIRST: SEARCH POINTS TO THE LEFT */
      /************************************/
      new_point = g_new (MetaRectangle, 1);
      new_point->x = orig_compare_rect->x;
      new_point->y = orig_compare_rect->y;

      /* I need to know the width and height of the rectangle I'm
       * looking at because it affects whether there's more than one
       * "optimal point" for each rectangle when smaller rectangles
       * are adjacent.
       */
      new_point->width  = orig_compare_rect->width;
      new_point->height = orig_compare_rect->height;

      /* loop over rectangles that could be to the left of orig_compare_rect */
      for (int j=i-1; j>=0; j--)
        {
          MetaRectangle *compare_rect;
          compare_rect = g_array_index (sorted_left_to_right, j, GList*)->data;

          /* If this compare_rect's right touches the other's left... */
          if (compare_rect->x + compare_rect->width == new_rectangle->x)
            {
              /* Get the one pixel past the bottoms of both rectangles */
              int compare_end = compare_rect->y  + compare_rect->height;
              int new_end     = new_rectangle->y + new_rectangle->height;

              /* If compare_rect contains something to the left of the
               * specified point then we have more work to do
               */
              if (compare_rect->y <= new_rectangle->y &&
                  compare_end     >  new_rectangle->y)
                {
                  /* If compare_rect covers the full height of the relevant
                   * rectangle, then we can just use the corresponding left
                   * point of compare_rect; otherwise, we want both
                   * points.
                   */
                  if (compare_rect->y <= new_rectangle->y &&
                      compare_end     >= new_end)
                    {
                      new_rectangle->x = compare_rect->x;
                    }
                  else
                    {
                      MetaRectangle temp;

                      /* Record point stored in new_rectangle */
                      locations = g_list_prepend (locations, new_rectangle);

                      /* Create a new point, copied from the old */
                      temp = *new_rectangle;
                      new_rectangle = g_new (MetaRectangle, 1);
                      *new_rectangle = temp;

                      /* Initialize the values appropriately */
                      new_rectangle->x      = compare_rect->x;
                      new_rectangle->height = compare_end - new_rectangle->y;
                    }
                } // if compare_rect is vertically aligned okay too...
            } // if compare_rect ends to the left
        } // looping over rectangles that could be to the left

      /* Record the final point found and stored in new_rectangle */
      locations = g_list_prepend (locations, new_rectangle);

      /************************************/
      /* SECOND: SEARCH POINTS ABOVE      */
      /************************************/
      new_point = g_new (MetaRectangle, 1);
      new_point->x = orig_compare_rect->x;
      new_point->y = orig_compare_rect->y;

      /* I need to know the width and height of the rectangle I'm
       * looking at because it affects whether there's more than one
       * "optimal point" for each rectangle when smaller rectangles
       * are adjacent.
       */
      new_point->width  = orig_compare_rect->width;
      new_point->height = orig_compare_rect->height;

      /* loop over rectangles that could be to above orig_compare_rect */
      for (int j=i-1; j>=0; j--)
        {
          MetaRectangle *compare_rect;
          compare_rect = g_array_index (sorted_top_to_bottom, j, GList*)->data;

          /* If this compare_rect's bottom touches the other's top... */
          if (compare_rect->y + compare_rect->height == new_rectangle->y)
            {
              /* Get the one pixel past the rights of both rectangles */
              int compare_end = compare_rect->x  + compare_rect->width;
              int new_end     = new_rectangle->x + new_rectangle->width;

              /* If compare_rect contains something to above the
               * specified point then we have more work to do
               */
              if (compare_rect->x <= new_rectangle->x &&
                  compare_end     >  new_rectangle->x)
                {
                  /* If compare_rect covers the full width of the relevant
                   * rectangle, then we can just use the corresponding top
                   * point of compare_rect; otherwise, we want both
                   * points.
                   */
                  if (compare_rect->x <= new_rectangle->x &&
                      compare_end     >= new_end)
                    {
                      new_rectangle->y = compare_rect->y;
                    }
                  else
                    {
                      MetaRectangle temp;

                      /* Record point stored in new_rectangle */
                      locations = g_list_prepend (locations, new_rectangle);

                      /* Create a new point, copied from the old */
                      temp = *new_rectangle;
                      new_rectangle = g_new (MetaRectangle, 1);
                      *new_rectangle = temp;

                      /* Initialize the values appropriately */
                      new_rectangle->y     = compare_rect->y;
                      new_rectangle->width = compare_end - new_rectangle->x;
                    }
                } // if compare_rect is horizontally aligned okay too...
            } // if compare_rect ends to the top
        } // looping over rectangles that could be above

      /* Record the final point found and stored in new_rectangle */
      locations = g_list_prepend (locations, new_rectangle);
    }

  g_array_free (sorted_left_to_right, TRUE);
  g_array_free (sorted_top_to_bottom, TRUE);

  /* The above code will result in a lot of duplicates; e.g. the same point
   * can be added for both going left and going above--further, in some
   * cases such as when rectangles align nicely, the same point may be
   * added for a later rectangle that was added for a previous one.  So we
   * should remove duplicates from the locations list.
   */
  locations = g_list_sort (locations, sort_by_left_then_by_top);
  tmp = locations;
  while (tmp != NULL && tmp->next != NULL)
    {
      GList *next;
      next = tmp->next;
      if (sort_by_left_then_by_top (tmp->data, next->data) == 0)
        {
          g_free (tmp->data);
          locations = g_list_delete_link (locations, tmp);
        }
      tmp = next;
    }

  return locations;
}

/* Returns a new region resulting from removing rect from region.  NOTE:
 * region is freed (because it is effectively replaced by the return list).
 */
static GList*
remove_rectangle_from_region (const MetaRectangle *rect, GList *region)
{
  GList *tmp;
  GList *new;

  new = NULL;
  tmp = region;
  while (tmp != NULL)
    {
      MetaRectangle *compare_rect = tmp->data;
      MetaRectangle overlap;

      if (meta_rectangle_intersect (rect, compare_rect, &overlap))
        {
          MetaRectangle *new_rectangle;

          /* If compare_rect is totally overlaped, just don't add any of it to
           * the new GList.
           */
          if (overlap.width == compare_rect.width &&
              overlap.height == compare_rect.height)
            {
              continue;
            }

          /* Look for remaining rectangles above, below, to the left,
           * then to the right of the overlap area
           */
          if (overlap.y > compare_rect.y)
            {
              new_rectangle = g_new (MetaRectangle, 1);
              new_rectangle.x      = compare_rect.x;
              new_rectangle.width  = compare_rect.width;
              new_rectangle.y      = compare_rect.y;
              new_rectangle.height = overlap.y - compare_rect.y;
              new = g_list_prepend (new, new_rectangle);
            }
          if (overlap.y + overlap.height < compare_rect.y + compare_rect.height)
            {
              new_rectangle = g_new (MetaRectangle, 1);
              new_rectangle.x      = compare_rect.x;
              new_rectangle.width  = compare_rect.width;
              new_rectangle.y      = overlap.y + overlap.height;
              new_rectangle.height = compare_rect.y + compare_rect.height
                                     - new_rectangle.y;
              new = g_list_prepend (new, new_rectangle);
            }
          if (overlap.x > compare_rect.x)
            {
              new_rectangle = g_new (MetaRectangle, 1);
              new_rectangle.x      = compare_rect.x;
              new_rectangle.width  = overlap.x - compare_rect.x;
              new_rectangle.y      = overlap.y;
              new_rectangle.height = overlap.height;
              new = g_list_prepend (new, new_rectangle);
            }
          if (overlap.x + overlap.width < compare_rect.x + compare_rect.width)
            {
              new_rectangle = g_new (MetaRectangle, 1);
              new_rectangle.x      = overlap.x + overlap.width;
              new_rectangle.width  = compare_rect.x + compare_rect.width
                                     - new_rectangle.x;
              new_rectangle.y      = overlap.y;
              new_rectangle.height = overlap.height;
              new = g_list_prepend (new, new_rectangle);
            }
          g_free (tmp->data);
        }
      else
        {
          new = g_list_prepend (new, tmp->data);
        }
      tmp = tmp->next;
    }

  g_list_free (region);
  return new;
}

static void
clip_rectangle_away_from_spot(MetaRectangle       *rect,
                              const MetaRectangle *bad_rect)
{
  /* No need to do anything if these rectangles don't overlap */
  MetaRectangle overlap;
  if (!meta_rectangle_intersect (rect, bad_rect, &overlap))
    return;

  /* Determine direction to clip the rectangle in; try to minimize amount */
  gboolean clip_height;
  if (overlap->width == rect->width)
    clip_height = TRUE;
  else if (overlap->height == rect->height)
    clip_height = FALSE;
  else if (overlap->width > overlap->height)
    clip_height = TRUE;
  else
    clip_height = FALSE;

  if (clip_height)
    {
      /* If clipping from the bottom removes less area than clipping
         from the top */
      if (rect->y    + rect->height    - overlap->x <
          overlap->y + overlap->height - rect->x)
        {
          rect->height = overlap.y - rect->y;
        }
      else
        {
          int newy = overlap->y + overlap->height;
          rect->height -= (newy - rect->y);
          rect->y = newy;
        }
    }
  else
    {
      /* If clipping from the right removes less area than clipping
         from the left */
      if (rect->x    + rect->width    - overlap->x <
          overlap->x + overlap->width - rect->x)
        {
          rect->width = overlap.x - rect->x;
        }
      else
        {
          int newx = overlap->x + overlap->width;
          rect->width -= (newx - rect->x);
          rect->x = newx;
        }
    }
}

static void
move_rectangle_away_from_spot(MetaRectangle       *rect,
                              const MetaRectangle *bad_rect,
                              gboolean             shortest_path)
{
  /* No need to do anything if these rectangles don't overlap */
  MetaRectangle overlap;
  if (!meta_rectangle_intersect (rect, bad_rect, &overlap))
    return;

  /* Determine direction to move the rectangle in; try to minimize amount */
  gboolean move_height;
  if (overlap->width == rect->width)
    move_height = TRUE;
  else if (overlap->height == rect->height)
    move_height = FALSE;
  else if (overlap->width > overlap->height)
    move_height = TRUE;
  else
    move_height = FALSE;

  /* Er, actually we maximize the amount if shortest_path is false */
  if (!shortest_path)
    move_height = !move_height;

  if (move_height)
    {
      /* If moving away from the bottom removes less area than moving
       * away from the top
       */
      if (rect->y    + rect->height    - overlap->x <
          overlap->y + overlap->height - rect->x)
        {
          rect->y = overlap->y - rect->height;
        }
      else
        {
          rect->y = overlap->y + overlap->height;
        }
    }
  else
    {
      /* If moving away from the right removes less area than moving
       * away from the left
       */
      if (rect->x    + rect->width    - overlap->x <
          overlap->x + overlap->width - rect->x)
        {
          rect->x = overlap.x - rect->width;
        }
      else
        {
          rect->x = overlap->x + overlap->width;
        }
    }
}
#endif
