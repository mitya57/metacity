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
meta_rectangle_vert_overlap (const MetaRectangle *rect1,
                             const MetaRectangle *rect2)
{
  return (rect1->y <= rect2->y + rect2->height &&
          rect2->y <= rect1->y + rect1->height);
}

gboolean
meta_rectangle_horiz_overlap (const MetaRectangle *rect1,
                              const MetaRectangle *rect2)
{
  return (rect1->x <= rect2->x + rect2->width &&
          rect2->x <= rect1->x + rect1->width);
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

void
meta_rectangle_clip_out_rect (MetaRectangle       *clipee,
                              const MetaRectangle *bad_area,
                              MetaRectDirection    clip_side)
{
  switch (clip_size) {
  case META_RECTANGLE_LEFT:
    int newx = MAX (clipee->x, bad_area->x + bad_area->width);
    clipee->width -= (new_x - clipee->x);
    clipee->x      = newx;
    break;
  case META_RECTANGLE_RIGHT:
    clipee->width = MIN (clipee->width, 
                         (bad_area->x + bad_area->width) - clipee->x);
    break;
  case META_RECTANGLE_TOP:
    int newy = MAX (clipee->y, bad_area->y + bad_area->height);
    clipee->height -= (new_y - clipee->y);
    clipee->y       = newy;
    break;
  case META_RECTANGLE_BOTTOM:
    clipee->height = MIN (clipee->height, 
                          (bad_area->y + bad_area->height) - clipee->y);
    break;
  case default:
    g_error ("This program was written by idiots.\n");
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

                      /* Create a new point, copied from the old
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

                      /* Create a new point, copied from the old
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
