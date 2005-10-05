/* Simple box operations */

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

#ifndef META_BOXES_H
#define META_BOXES_H

typedef struct _MetaRectangle MetaRectangle;

struct _MetaRectangle
{
  int x;
  int y;
  int width;
  int height;
};

typedef enum
{
  META_RECTANGLE_LEFT      = 1 << 0,
  META_RECTANGLE_RIGHT     = 1 << 1,
  META_RECTANGLE_TOP       = 1 << 2,
  META_RECTANGLE_BOTTOM    = 1 << 3
} MetaRectDirection;

int      meta_rectangle_area            (const MetaRectangle *rect);
gboolean meta_rectangle_intersect       (const MetaRectangle *src1,
                                         const MetaRectangle *src2,
                                         MetaRectangle       *dest);
gboolean meta_rectangle_equal           (const MetaRectangle *src1,
                                         const MetaRectangle *src2);
gboolean meta_rectangle_overlap         (const MetaRectangle *rect1,
                                         const MetaRectangle *rect2);
gboolean meta_rectangle_vert_overlap    (const MetaRectangle *rect1,
                                         const MetaRectangle *rect2);
gboolean meta_rectangle_horiz_overlap   (const MetaRectangle *rect1,
                                         const MetaRectangle *rect2);
gboolean meta_rectangle_could_fit_rect  (const MetaRectangle *outer_rect,
                                         const MetaRectangle *inner_rect);
gboolean meta_rectangle_contains_rect   (const MetaRectangle *outer_rect,
                                         const MetaRectangle *inner_rect);

GList*   meta_rectangle_get_minimal_spanning_set_for_region (
                                         const MetaRectangle *basic_rect,
                                         const GSList        *all_struts,
                                         const int            left_expand,
                                         const int            right_expand,
                                         const int            top_expand,
                                         const int            bottom_expand);
gboolean meta_rectangle_could_be_contained_in_region (
                                         const GList         *spanning_rects,
                                         const MetaRectangle *rect);
gboolean meta_rectangle_contained_in_region (
                                         const GList         *spanning_rects,
                                         const MetaRectangle *rect);
void     meta_rectangle_clamp_to_fit_into_region (
                                         const GList         *spanning_rects,
                                         FixedDirections      fixed_directions,
                                         MetaRectangle       *rect,
                                         const MetaRectangle *min_size);
gboolean meta_rectangle_clip_to_region  (const GList         *spanning_rects,
                                         FixedDirections      fixed_directions,
                                         MetaRectangle       *rect);
gboolean meta_rectangle_shove_into_region(
                                         const GList         *spanning_rects,
                                         FixedDirections      fixed_directions,
                                         MetaRectangle       *rect);

#if 0
  May not be needed--depends on if constrain_clamp_size remains...
gboolean meta_rectangle_clip_into_rect  (const MetaRectangle *orig_area,
                                         const MetaRectangle *allowed_area,
                                         MetaRectangle       *dest);
#endif

#if 0
#if 0
gboolean rectangles_intersect           (const MetaRectangle *rect1, 
                                         const MetaRectangle *rect2);
#endif
gboolean region_contains_rectangle      (const GList         *region,
                                         const MetaRectangle *rect);
gboolean region_could_contain_rectangle (const GList         *region,
                                         const MetaRectangle *rect)
gboolean move_rectangle_into_region     (MetaRectangle       *rect,
                                         const GList         *region);
void     clip_rectangle_into_region     (MetaRectangle       *rect,
                                         const GList         *region);
void     region_expand                  (GList               *region,
                                         int                  expand_amount,
                                         MetaRectDirection    directions);
#endif

#endif /* META_BOXES_H */
