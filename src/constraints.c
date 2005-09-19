/* Metacity size/position constraints */

/*
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003, 2004 Rob Adams
 * Copyright (C) 2005       Elijah Newren
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

#include "constraints.h"
#include "workspace.h"
#include "place.h"

/* Stupid disallowing of nested C comments makes a #if 0 mandatory... */
#if 0
/* There are a couple basic ideas behind how this code works:
 *
 *   1) Rely heavily on "workarea" (though we complicate it's meaning 
 *      with overloading, unfortunately)
 *   2) Clipping a window or shoving a window to a work_area are easy
 *      operations for people to understand and maintain
 *   3) A uni-dimensional view of constraints doesn't work
 *   4) Robustness can be added via constraint priorities
 *
 * In just a little more detail:
 *
 *   1) Relying on "workarea"
 *
 *      The previous code had two workareas, work_area_xinerama (remember,
 *      in a xinerama setup "monitor" == xinerama and "all monitors" ==
 *      screen), and work_area_screen.  These were mostly used for
 *      maximization constraints and determining "outermost position
 *      limits" (partially onscreen constraints).  In this code they are
 *      used more heavily.  They are an important part of the
 *      clip-to-workarea and shove-into-workarea methods that are part of
 *      (2).  Also, I want to use them to help force fully-onscreen
 *      constraints (clamp to screensize -- bug 143145, make appear
 *      onscreen -- bug 143145 & 156699, avoid struts when placing new
 *      windows -- bug 156699 & bug 122196, don't let app resize itself off
 *      the screen -- bug 136307; see also http://xrl.us/FullyOnscreenRants
 *      which had a number of d-d-l emails from Feb 2005 about this)
 *
 *      Two little wrinkles: (a) fully-onscreen should not be enforced in
 *      all cases (e.g. the user should be able to manually move a window
 *      offscreen, and once offscreen the fully-onscreen constraints should
 *      no longer apply until manually moved back onscreen.  Application
 *      specified placement may override as well (haven't decided, but it
 *      would mean setting window->onscreen to FALSE in place.c somewhere).
 *      Also, minimum size hints might be bigger than screen size).  Thus,
 *      we use a method of "growing" the workarea so that the extended
 *      region provides the constraint.  (b) docks can remove otherwise
 *      valid space from the workarea.  This doesn't pose much problem for
 *      docks that either span the width or the height of the screen.  It
 *      does cause problems when they only span part of the width or height
 *      ("partial struts"), because then the workarea (the area used by
 *      e.g. maximized windows) leaves out some available holes that
 *      smaller windows could use.  So we have an auxiliary workarea that
 *      takes these into account using a combination of
 *      get_outermost_onscreen_constraints() from the old code plus
 *      possible workarea expansion as noted in (a).  Things will probably
 *      be kind of hosed for docks that appear in some small rectangle in
 *      the middle of the screen; I don't know if I even want to worry
 *      about that special case, though.
 *
 *   2 & 3) Clip-to-workarea and shove-into-workarea are easier methods
 *
 *      The previous code tried to reform the constraints into terms of a
 *      single variable.  This made the code rather difficult to
 *      understand. ("This is a rather complicated fix for an obscure bug
 *      that happened when resizing a window and encountering a constraint
 *      such as the top edge of the screen.")  It also failed, even on the
 *      very example for which it used as justification for the complexity
 *      (bug 312104 -- when keyboard resizing the top of the window,
 *      Metacity extends the bottom once the titlebar hits the top panel),
 *      though the reason why it failed is somewhat mysterious as it should
 *      have worked.  Further, it didn't really reform the constraints in
 *      terms of a single variable -- there was both an x_move_delta and an
 *      x_resize_delta, and the existence of both caused bug 109553
 *      (gravity with simultaneous move and resize doesn't work)
 *
 *      We can reuse the example used to justify the old method in order to
 *      elucidate the problem that the old method attempted to fix, and in
 *      so doing motivate our new method: Windows are described by un upper
 *      left coordinate, a height, and a width.  If a user is resizing the
 *      window from the top, then they are both changing the position of
 *      the top of the window and the height of the window simultaneously.
 *      Now, if they move the window above the top of the screen and we
 *      have a titlebar-must-be-onscreen constraint, then the resize should
 *      be stopped at the screen edge.  However, the straightforward
 *      approach to trying to do this, setting upper position to 0, fails
 *      because without the window height also being decreased, the actual
 *      result is that the window grows from the bottom.  The old solution
 *      was to reformulate the resize in terms of a single variable,
 *      constrain that variable, and then do the adjustments to position
 *      and height after the constraint.  What would be much simpler,
 *      though, is just clipping the window to the appropriate workarea.
 *
 *      We can't always just clip, though.  If the user moves the window
 *      without resizing it so that it is offscreen, then clipping is
 *      nasty.  We instead need to shove the window onscreen.  The old
 *      method knew about this too and thus had separate constraints for
 *      moving and resizing, which we also obviously need.
 *
 *      There is a little wrinkle, though.  If an application does the
 *      resize instead of the user, the window should be shoved onscreen
 *      instead of clipped (see bug 136307 if you don't understand why).
 *      Also, we need to apply gravity constraints (user resizes specify
 *      both position and size implicitly, whereas app resizes only specify
 *      the size and the app can want the resize to be done relative to a
 *      different corner or side than the top left corner).  The old code
 *      did not account for differences between user and application
 *      actions like this, but we need to do so to fix outstanding bugs.
 *
 *      A small summary of how clipping-to-workarea and
 *      shoving-onto-workarea nicely solve the constrain-multiple-variables
 *      problem conceptually:
 *        user resize:        clip-to-workarea
 *        user move:          shove-into-(expanded)-workarea (see (1))
 *        user move & resize: who lied about what's happening or who's doing it?
 *        app  resize:        clamp size, shove-into-workarea
 *        app  move:          shove-into-workarea
 *        app  move & resize: clamp size, shove-into-workarea
 *
 *   4) Constraint priorities
 *
 *      In the old code, if each and every constraint could not be
 *      simultaneously satisfied, then it would result in some
 *      difficult-to-predict set of constraints being violated.  This was
 *      because constraints were applied in order, with the possibility for
 *      each making changes that violated previous constraints, with no
 *      checking done at the end.  There may be cases where it failed even
 *      when all constraints could be satisfied (because the constraints
 *      were highly segmented).
 *
 *      I suggest fixing that by adding priorities to all the constraints.
 *      Then, in a loop, apply all the constraints, check them all, if
 *      they're not all satisfied, increase the priority and repeat.  That
 *      way we make sure the most important constraints are satisfied.
 *      Also, note that if any one given constraint is impossible to apply
 *      (e.g. if minimum size hints specify a larger screen than the real
 *      workarea making fully-onscreen impossible) then we treat the
 *      constraint as being satisfied.  This sounds counter-intuitive, but
 *      the idea is that we want to satisfy as many constraints as possible
 *      and treating it as a violation means that all constraints with a
 *      lesser priority also get dropped along with the impossible one if
 *      we don't do this.
 *
 * Now that you understand the basic ideas behind this code, the crux of
 * adding/modifying/removing constraints is to look at the functions called
 * in the loop inside meta_window_constrain().  All these functions are of
 * the following form:
 *
 * /* constrain_whatever does the following:
 *  *   Quits (returning true) if priority is higher than PRIORITY_WHATEVER
 *  *   If check_only is TRUE
 *  *     Returns whether the constraint is satisfied or not
 *  *   otherwise
 *  *     Enforces the constraint
 *  * Note that the value of PRIORITY_WHATEVER is centralized with the
 *  * priorities of other constraints in the definition of ConstraintInfo
 *  * for easier maintenance and shuffling of priorities.
 *  */
 * static gboolean
 * constrain_whatever (MetaWindow         *window,
 *                     ConstraintInfo     *info,
 *                     ConstraintPriority  priority,
 *                     gboolean            check_only)
 * {
 *   if (priority > PRIORITY_WHATEVER)
 *     return TRUE;
 *
 *   /* Determine whether constraint applies; note that if the constraint
 *    * cannot possibly be satisfied, constraint_applies should be set to
 *    * false.  If we don't do this, all constraints with a lesser priority
 *    * will be dropped along with this one, and we'd rather apply as many as
 *    * possible.
 *    */
 *   if (!constraint_applies)
 *     return TRUE;
 *
 *   /* Determine whether constraint is already satisfied; if we're only
 *    * checking the status of whether the constraint is satisfied, we end
 *    * here.
 *    */
 *   if (check_only || constraint_already_satisfied)
 *     return constraint_already_satisfied;
 *
 *   /* Enforce constraints */
 *   return TRUE;  /* Note that we exited early if check_only is FALSE; also,
 *                  * we know we can return TRUE here because we exited early
 *                  * if the constraint could not be satisfied. */
 * }
 */
#endif

typedef enum   FIXME--there's more than this...
{
  PRIORITY_MINIMUM=0,  # Dummy value used for loop start = min(all priorities)
  PRIORITY_ASPECT_RATIO=0,
#if 0
  PRIORITY_CLAMP_SIZE=1,
#endif
  PRIORITY_ENTIRELY_VISIBLE_ON_WORKAREA=1,
  PRIORITY_SIZE_HINTS_INCREMENTS=1,
  PRIORITY_MAXIMIZATION=2,
  PRIORITY_FULLSCREEN=2
  PRIORITY_SIZE_HINTS_LIMITS=3,
  PRIORITY_TITLEBAR_PARTIALLY_VISIBLE_ON_WORKAREA=4
  PRIORITY_PARTIALLY_VISIBLE_ON_WORKAREA=4,
  PRIORITY_MAXIMUM=4   # Dummy value used for loop end = max(all priorities)
} ConstraintPriority;

typedef enum
{
  ACTION_MOVE,
  ACTION_RESIZE,
  ACTION_MOVE_AND_RESIZE
} ActionType;

typedef struct
{
  MetaRectangle        orig;
  MetaRectangle        current;
  MetaFrameGeometry   *fgeom;
  ActionType           action_type;
  gboolean             is_user_action;
  int                  resize_gravity;
  MetaRectangle        work_area_xinerama; /* current xinerama only */
  MetaRectangle        work_area_screen;   /* all xineramas */
  MetaRectangle        entire_xinerama;    /* current xinerama incl. struts */
} ConstraintInfo;

static gboolean constrain_maximization      (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_fullscreen        (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
#if 0
static gboolean constrain_clamp_size        (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
#endif
static gboolean constrain_size_increments   (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_size_limits       (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_aspect_ratio      (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_fully_onscreen    (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_titlebar_onscreen (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);

static void setup_constraint_info  (ConstraintInfo      *info,
                                    MetaWindow          *window,
                                    MetaFrameGeometry   *orig_fgeom,
                                    MetaMoveResizeFlags  flags,
                                    int                  resize_gravity,
                                    const MetaRectangle *orig,
                                    MetaRectangle       *new);
static void place_window_if_needed (MetaWindow     *window,
                                    ConstraintInfo *info);
static void extend_by_frame        (MetaRectangle           *rect,
                                    const MetaFrameGeometry *fgeom);
static void unextend_by_frame      (MetaRectangle           *rect,
                                    const MetaFrameGeometry *fgeom);
static void resize_with_gravity    (MetaRectangle     *rect,
                                    int                gravity,
                                    int                new_width,
                                    int                new_height);
inline void get_size_limits        (const MetaWindow        *window,
                                    const MetaFrameGeometry *fgeom,
                                    MetaRectangle *min_size,
                                    MetaRectangle *max_size);
static void get_outermost_onscreen_positions (const MetaWindow     *window,
                                              const ConstraintInfo *info,
                                              MetaRectangle        *positions)


void
meta_window_constrain (MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       MetaMoveResizeFlags  flags,
                       int                  resize_gravity,
                       const MetaRectangle *orig,
                       MetaRectangle       *new)
{
  ConstraintInfo info;
  MetaRectangle current;

  meta_topic (META_DEBUG_GEOMETRY,
              "Constraining %s in move from %d,%d %dx%d to %d,%d %dx%d\n",
              window->desc,
              orig->x, orig->y, orig->width, orig->height,
              new->x,  new->y,  new->width,  new->height);

  setup_constraint_info (&info,
                         window, 
                         orig_fgeom, 
                         flags,
                         resize_gravity,
                         orig,
                         new);
  place_window_if_needed (window, &info);

  /* Most constraints apply to the whole window, i.e. client area + frame;
   * but the data we were given only accounts for the client area, so we
   * adjust here.  (Alternatively, we could just make all the constraints
   * manually figure out the differences for the frame, but it's error
   * prone as it's too easy to forget to handle info.fgeom)
   */
  extend_by_frame (&info->orig,    info->fgeom);
  extend_by_frame (&info->current, info->fgeom);

  ConstraintPriority priority = PRIORITY_MINIMUM;
  gboolean satisfied = false;
  while (!satisfied && priority <= PRIORITY_MAXIMUM) {
    gboolean check_only = FALSE;
    constrain_maximization      (window, &info, priority, check_only);
    constrain_fullscreen        (window, &info, priority, check_only);
#if 0
    constrain_clamp_size        (window, &info, priority, check_only);
#endif
    constrain_size_increments   (window, &info, priority, check_only);
    constrain_size_limits       (window, &info, priority, check_only);
    constrain_aspect_ratio      (window, &info, priority, check_only);
    constrain_fully_onscreen    (window, &info, priority, check_only);
    constrain_titlebar_onscreen (window, &info, priority, check_only);

    check_only = TRUE;
    satisfied =
      constrain_maximization      (window, &info, priority, check_only) &&
      constrain_fullscreen        (window, &info, priority, check_only) &&
#if 0
      constrain_clamp_size        (window, &info, priority, check_only) &&
#endif
      constrain_size_increments   (window, &info, priority, check_only) &&
      constrain_size_limits       (window, &info, priority, check_only) &&
      constrain_aspect_ratio      (window, &info, priority, check_only) &&
      constrain_fully_onscreen    (window, &info, priority, check_only) &&
      constrain_titlebar_onscreen (window, &info, priority, check_only);

    priority++;
  }

  /* meta_window_move_resize_internal expects rectangles in terms of client
   * area only, so undo the adjustments for the frame.
   */
  unextend_by_frame (&info->orig,    info->fgeom);
  unextend_by_frame (&info->current, info->fgeom);

  /* Ew, what an ugly way to do things.  Destructors (in a real OOP language,
   * not gobject-style) or smart pointers would be so much nicer here.  *shrug*
   */
  if (!orig_fgeom)
    g_free (info.fgeom);
}

static void
setup_constraint_info (ConstraintInfo      *info,
                       MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       MetaMoveResizeFlags  flags,
                       int                  resize_gravity,
                       const MetaRectangle *orig,
                       MetaRectangle       *new)
{
  info->orig    = *orig;
  info->current = *new;

  /* Create a fake frame geometry if none really exists */
  if (orig_fgeom && !window->fullscreen)
    info->fgeom = orig_fgeom;
  else
    info->fgeom = g_new0 (MetaFrameGeometry, 1);

  if (flags & META_IS_MOVE_ACTION && flags & META_IS_RESIZE_ACTION)
    info->action_type = ACTION_MOVE_AND_RESIZE;
  else if (flags & META_IS_RESIZE_ACTION)
    info->action_type = ACTION_RESIZE;
  else if (flags & META_IS_MOVE_ACTION)
    info->action_type = ACTION_MOVE_AND_RESIZE;
  else
    g_error ("BAD, BAD developer!  No treat for you!  (Fix your calls to "
             "meta_window_move_resize_internal()).\n");

  info->is_user_action = (flags & META_USER_MOVE_RESIZE);

  info->resize_gravity = resize_gravity;
  /* We need to get a pseduo_gravity for user resize operations (so
   * that user resize followed by min size hints don't end up moving
   * window--see bug 312007).
   *
   * Note that only corner gravities are used here.  The reason is simply
   * because we don't allow complex user resizing.  West gravity would
   * correspond to resizing the right side, but since only one dimension is
   * being changed by the user, we can lump this in with either NorthWest
   * gravity or SouthWest.  Center gravity and Static gravity just don't
   * make sense in this context.
   */
  if (info->is_user_action && info->action_type = ACTION_RESIZE)
    {
      int pseudo_gravity = NorthWestGravity;
      if (orig->x == new->x)
        {
          if (orig->y == new->y)
            pseduo_gravity = NorthWestGravity;
          else if (orig->y + orig->height == new->y + new->height)
            pseduo_gravity = SouthWestGravity;
        }
      else if (orig->x + orig->width == new->x + new->width)
        {
          if (orig->y == new->y)
            pseduo_gravity = NorthEastGravity;
          else if (orig->y + orig->height == new->y + new->height)
            pseduo_gravity = SouthEastGravity;
        }
      info->resize_gravity = pseudo_gravity;      
    }

  meta_window_get_work_area_current_xinerama (window, &info->work_area_xinerama);
  meta_window_get_work_area_all_xineramas (window, &info->work_area_screen);

  const MetaXineramaScreenInfo *xinerama_info =
    meta_screen_get_xinerama_for_window (window->screen, window);
  info->entire_xinerama.x      = xinerama_info->x_origin;
  info->entire_xinerama.y      = xinerama_info->y_origin;
  info->entire_xinerama.width  = xinerama_info->width;
  info->entire_xinerama.height = xinerama_info->height;
}

static void
place_window_if_needed(MetaWindow     *window,
                       ConstraintInfo *info)
{
  gboolean did_placement;

  /* Do placement if any, so we go ahead and apply position
   * constraints in a move-only context. Don't place
   * maximized/fullscreen windows until they are unmaximized
   * and unfullscreened
   */
  did_placement = FALSE;
  if (!window->placed &&
      window->calc_placement &&
      !window->maximized &&
      !window->fullscreen)
    {
      MetaRectangle placed_rect = info->orig;

      meta_window_place (window, info->fgeom, info->orig.x, info->orig.y,
                         &placed_rect.x, &placed_rect.y);
      did_placement = TRUE;

      /* placing the window may have changed the xinerama.  Find the
       * new xinerama and update the ConstraintInfo
       */
      const MetaXineramaScreenInfo *xinerama_info =
        meta_screen_get_xinerama_for_rect (window->screen, &placed_rect);
      info->entire_xinerama.x      = xinerama_info->x_origin;
      info->entire_xinerama.y      = xinerama_info->y_origin;
      info->entire_xinerama.width  = xinerama_info->width;
      info->entire_xinerama.height = xinerama_info->height;
      meta_window_get_work_area_for_xinerama (window,
                                              xinerama_info->number,
                                              &info->work_area_xinerama);

      info->current.x = placed_rect.x;
      info->current.y = placed_rect.y;
    }

  if (window->maximize_after_placement &&
      (window->placed || did_placement))
    {
      window->maximize_after_placement = FALSE;

      if (meta_rectangle_could_fit_rect (info->current, 
                                         info->work_area_xinerama))
        {
          /* define a sane saved_rect so that the user can unmaximize
           * to something reasonable.
           */
          new->width = .75 * info->work_area_xinerama.width;
          new->height = .75 * info->work_area_xinerama.height;
          new->x = info->work_area_xinerama.x +
                   .125 * info->work_area_xinerama.width;
          new->y = info->work_area_xinerama.y +
                   .083 * info->work_area_xinerama.height;
        }

      meta_window_maximize_internal (window, new);

      /* maximization may have changed frame geometry */
      if (window->frame && !window->fullscreen)
        meta_frame_calc_geometry (window->frame, info->fgeom);
    }
}

static void
extend_by_frame (MetaRectangle           *rect,
                 const MetaFrameGeometry *fgeom)
{
  rect->x -= fgeom->left_width;
  rect->y -= fgeom->top_height;
  rect->width  += fgeom->left_width + fgeom->right_width;
  rect->height += fgeom->top_height + fgeom->bottom_height;
}

static void
unextend_by_frame (MetaRectangle           *rect,
                   const MetaFrameGeometry *fgeom)
{
  rect->x += fgeom->left_width;
  rect->y += fgeom->top_height;
  rect->width  -= fgeom->left_width + fgeom->right_width;
  rect->height -= fgeom->top_height + fgeom->bottom_height;
}

static void
resize_with_gravity (MetaRectangle     *rect,
                     int                gravity,
                     int                new_width,
                     int                new_height)
{
  /* Do the gravity adjustment */

  /* These formulas may look overly simplistic at first but you can work
   * everything out with a left_frame_with, right_frame_width,
   * border_width, and old and new client area widths (instead of old total
   * width and new total width) and you come up with the same formulas.
   *
   * Also, note that the reason we can treat NorthWestGravity and
   * StaticGravity the same is because we're not given a location at
   * which to place the window--the window was already placed
   * appropriately before.  So, NorthWestGravity for this function
   * means to just leave the upper left corner of the outer window
   * where it already is, and StaticGravity for this function means to
   * just leave the upper left corner of the inner window where it
   * already is.  But leaving either of those two corners where they
   * already are will ensure that the other corner is fixed as well
   * (since frame size doesn't change)--thus making the two
   * equivalent.
   */

  /* First, the x direction */
  int adjust = 0;
  switch (gravity)
    {
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      /* No need to modify rect->x */
      break;

    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      rect->x += (rect->width - new_width)/2;
      adjust = (rect->width - new_width) % 1;
      break;

    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      rect->x += (rect->width - new_width);
      break;

    case StaticGravity:
    default:
      /* No need to modify rect->x */
      break;
    }
  /* FIXME; the need for adjust sucks but not using it would cause North,
   * Center, and South gravity to break when resizing multiple times with
   * odd differences in sizes.  So we instead treat it like a
   * resize_increment kind of thing, though that's kinda weird.
   */
  rect->width = new_width - adjust;
  
  /* Next, the y direction */
  adjust = 0;
  switch (gravity)
    {
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      /* No need to modify rect->y */
      break;

    case WestGravity:
    case CenterGravity:
    case EastGravity:
      rect->y += (rect->height - new_height)/2;
      adjust = (rect->height - new_height) % 1;
      break;

    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      rect->y += (rect->height - new_height);
      break;

    case StaticGravity:
    default:
      /* No need to modify rect->y */
      break;
    }
  /* FIXME; this sucks; see previous FIXME in this function for details */
  rect->height = new_height - adjust;
}

/* We pack the results into MetaRectangle structs just for convienience; we
 * don't actually use the position of those rects.
 */
static inline void
get_size_limits (const MetaWindow        *window,
                 const MetaFrameGeometry *fgeom,
                 MetaRectangle *min_size,
                 MetaRectangle *max_size)
{
  int fw = info->fgeom->left_width + info->fgeom->right_width;
  int fh = info->fgeom->top_height + info->fgeom->bottom_height;

  *min_size->width  = window->size_hints->min_width  + fw;
  *min_size->height = window->size_hints->min_height + fh;
  *max_size->width  = window->size_hints->max_width  + fw;
  *max_size->height = window->size_hints->max_height + fh;
}

static void
get_outermost_onscreen_positions (const MetaWindow     *window,
                                  const ConstraintInfo *info,
                                  MetaRectangle        *positions)
{
  GList *workspaces;
  GList *tmp;
  GSList *stmp;

  /* to handle struts, we get the list of workspaces for the window
   * and traverse all the struts in each of the cached strut lists for
   * the workspaces.  Note that because the workarea has already been
   * computed, these strut lists should already be up to date. This function
   * should have good performance since we call it a lot.
   */

  workspaces = meta_window_get_workspaces (window);
  tmp = workspaces;

  position.x = positions.y = 0;
  position.width  = window->screen->width;
  position.height = window->screen->height;
  /* FIXME; should be position = info->entire_xinerama for single_xinerama... */

  /* FIXME: Why are we concerned with all workspaces instead of just
   * the active_workspace?
   */
  while (tmp)
    {
      stmp = ((MetaWorkspace*) tmp->data)->left_struts;
      while (stmp)
        {
          MetaRectangle *rect = (MetaRectangle*) stmp->data;
          if (meta_rectangle_vert_overlap (rect, position))
            meta_rectangle_clip_out_rect (position, rect, 
                                          META_RECTANGLE_LEFT);
          stmp = stmp->next;
        }
          
      stmp = ((MetaWorkspace*) tmp->data)->right_struts;
      while (stmp)
        {
          MetaRectangle *rect = (MetaRectangle*) stmp->data;
          if (meta_rectangle_vert_overlap (rect, position))
            meta_rectangle_clip_out_rect (position, rect, 
                                          META_RECTANGLE_RIGHT);
          stmp = stmp->next;
        }
          
      stmp = ((MetaWorkspace*) tmp->data)->top_struts;
      while (stmp)
        {
          MetaRectangle *rect = (MetaRectangle*) stmp->data;
          /* here the strut matters if the titlebar is overlapping
           * the window horizontally
           */
          if (meta_rectangle_horiz_overlap (rect, position))
            meta_rectangle_clip_out_rect (position, rect, 
                                          META_RECTANGLE_UP);
          stmp = stmp->next;
        }

      stmp = ((MetaWorkspace*) tmp->data)->bottom_struts;
      while (stmp)
        {
          MetaRectangle *rect = (MetaRectangle*) stmp->data;
          /* here the strut matters if the titlebar is overlapping
           * the window horizontally
           */
          if (meta_rectangle_horiz_overlap (rect, position))
            meta_rectangle_clip_out_rect (position, rect, 
                                          META_RECTANGLE_DOWN);
          stmp = stmp->next;
        }
    }
}

static gboolean
constrain_maximization (MetaWindow         *window,
                        ConstraintInfo     *info,
                        ConstraintPriority  priority,
                        gboolean            check_only);
{
  if (priority > PRIORITY_MAXIMIZATION)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't */
  if (!window->maximized)
    return TRUE;
  MetaRectangle min_size, max_size;
  MetaRectangle work_area = info->work_area_xinerama;
  get_size_limits (window, info->fgeom, &min_size, &max_size);
  gboolean too_big =   !meta_rectangle_could_fit_rect (work_area, min_size);
  gboolean too_small = !meta_rectangle_could_fit_rect (max_size, work_area);
  if (too_big || too_small)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean constraint_already_satisfied =
    meta_rectangle_equal (info->current, info->work_area_xinerama)
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  info->current = info->work_area_xinerama;
  return TRUE;
}

static gboolean
constrain_fullscreen (MetaWindow         *window,
                      ConstraintInfo     *info,
                      ConstraintPriority  priority,
                      gboolean            check_only);
{
  if (priority > PRIORITY_FULLSCREEN)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't */
  if (!window->fullscreen)
    return TRUE;
  MetaRectangle min_size, max_size;
  MetaRectangle xinerama = info->entire_xinerama;
  extend_by_frame (&xinerama, info->fgeom);
  get_size_limits (window, info->fgeom, &min_size, &max_size);
  gboolean too_big =   !meta_rectangle_could_fit_rect (xinerama, min_size);
  gboolean too_small = !meta_rectangle_could_fit_rect (max_size, xinerama);
  if (too_big || too_small)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean constraint_already_satisfied =
    meta_rectangle_equal (info->current, xinerama);
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  info->current = info->xinerama;
  return TRUE;
}

#if 0
static gboolean
constrain_clamp_size (MetaWindow         *window,
                      ConstraintInfo     *info,
                      ConstraintPriority  priority,
                      gboolean            check_only);
{
  if (priority > PRIORITY_CLAMP_SIZE)
    return TRUE;

  /* FIXME -- NOT YET WRITTEN PAST HERE */
  MetaRectangle positions;
  get_outermost_onscreen_positions (window, info, &positions)

  /* Determine whether constraint applies; exit if it doesn't */
  MetaRectangle min_size, max_size;
  MetaRectangle work_area = info->work_area_xinerama;
  get_size_limits (window, info->fgeom, &min_size, &max_size);
  gboolean too_big =   !meta_rectangle_could_fit_rect (work_area, min_size) ||
                       !meta_rectangle_could_fit_rect (positions, min_size);
  if (too_big)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean constraint_already_satisfied =
    meta_rectangle_contains_rect (work_area, info->current) || 
    meta_rectangle_contains_rect (positions, info->current);
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  MetaRectangle rect1, rect2;
  meta_rectangle_clip_into_rect (info->current, work_area, &rect1);
  meta_rectangle_clip_into_rect (info->current, positions, &rect2);
  if (meta_rectangle_area (rect1) > meta_rectangle_area (rect2))
    info->current = rect1;
  else
    info->current = rect2;

  return TRUE;
}
#endif

static gboolean
constrain_size_increments (MetaWindow         *window,
                           ConstraintInfo     *info,
                           ConstraintPriority  priority,
                           gboolean            check_only);
{
  if (priority > PRIORITY_SIZE_HINTS_INCREMENTS)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't */
  if (window->maximized || window->fullscreen || 
      info->action_type == ACTION_MOVE)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  int bh, hi, bw, wi, extra_height, extra_width;
  bh = window->size_hints.base_height;
  hi = window->size_hints.height_increment;
  bw = window->size_hints.base_width;
  wi = window->size_hints.width_increment;
  extra_height = (info->current.height - bh) % hi;
  extra_width  = (info->current.widht  - bw) % wi;
  gboolean constraint_already_satisfied = 
    (extra_height == 0 && extra_width == 0);

  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  /* Shrink to base + N * inc */
  resize_with_gravity (info->current, 
                       info->resize_gravity,
                       info->current.width - extra_width,
                       info->current.height - extra_height);
  return TRUE;
}

static gboolean
constrain_size_limits (MetaWindow         *window,
                       ConstraintInfo     *info,
                       ConstraintPriority  priority,
                       gboolean            check_only);
{
  if (priority > PRIORITY_SIZE_HINTS_LIMITS)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't.
   *
   * Note: The old code didn't apply this constraint for fullscreen or
   * maximized windows--but that seems odd to me.  *shrug*
   */
  MetaRectangle min_size, max_size;
  get_size_limits (window, info->fgeom, &min_size, &max_size);
  gboolean limits_are_inconsistent =
    min_size.width  > max_size.width  || min_size.height > max_size.height;
  if (limits_are_inconsistent || info->action_type == ACTION_MOVE)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean too_big =   !meta_rectangle_could_fit_rect (info->current, min_size);
  gboolean too_small = !meta_rectangle_could_fit_rect (max_size, info->current);
  gboolean constraint_already_satisfied = !too_big && !too_small;
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  int new_width  = MIN (max_size.width, 
                        MAX (min_size.width,  info->current.width));
  int new_height = MIN (max_size.height, 
                        MAX (min_size.height, info->current.height));
  resize_with_gravity (info->current, 
                       info->resize_gravity,
                       new_width,
                       new_height);
  return TRUE;
}

static gboolean
constrain_aspect_ratio (MetaWindow         *window,
                        ConstraintInfo     *info,
                        ConstraintPriority  priority,
                        gboolean            check_only)
{
  if (priority > PRIORITY_ASPECT_RATIO)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't.
  int min_ax, min_ay, max_ax, max_ay;
  min_ax = window->size_hints.min_aspect.x;
  min_ay = window->size_hints.min_aspect.y;
  max_ax = window->size_hints.max_aspect.x;
  max_ay = window->size_hints.max_aspect.y;
  gboolean constraints_are_inconsistent =
    min_ax / ((double)min_ay) > max_ax / ((double)max_ay);
  if (constraints_are_inconsistent || window->maximized || window->fullscreen || 
      info->action_type == ACTION_MOVE)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  int slop_allowed_for_min, slop_allowed_for_max;
  slop_allowed_for_min = (min_ax % min_ay == 0) ? 0 : 1;
  slop_allowed_for_max = (max_ax % max_ay == 0) ? 0 : 1;
  /*       min_ax     width    max_ax
   * Need: ------ <= ------ <= ------
   *       min_ay    height    max_ay
  gboolean constraint_already_satisfied = 
    info->current.width >=
      (info->current.height * min_ax / min_ay) - slop_allowed_for_min &&
    info->current.width <=
      (info->current.height * max_ax / max_ay) + slop_allowed_for_max;
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
#if 0
  if (info->current.width == info->orig.width)
    {
      /* User changed height; change width to match */
      int min_width, max_width, new_width;
      min_width = info->current.height * min_ax / min_ay;
      max_width = info->current.height * max_ax / max_ay;
      new_width = MIN (max_width, MAX (min_width, info->current.width));
      resize_with_gravity (info->current, 
                           info->resize_gravity,
                           new_width,
                           info->current.height);
    }
  else if (info->current.height == info->orig.height)
    {
      /* User changed width; change height to match */
      int min_height, max_height, new_height;
      min_height = info->current.width * min_ay / min_ax;
      max_height = info->current.width * max_ay / max_ax;
      new_height = MIN (max_height, MAX (min_height, info->current.height));
      resize_with_gravity (info->current, 
                           info->resize_gravity,
                           info->current.width,
                           new_height);
    }
#endif

  /* Pseduo-code:
   *
   * 1) Find new rect, A, based on keeping height fixed
   * 2) Find new rect, B, based on keeping width fixed
   * 3) If info->current is strictly larger than info->orig
   *    (i.e. could contain it), then discard either of A and B
   *    that represent an decrease of area relative to info->orig
   * 4) If info->current is strictly smaller than info->orig
   *    (i.e. could be contained in it), then discard either of A and
   *    B that represent an increase of area relative to info->orig
   * 5) If both A and B remain (possible in the cases of the user
   *    changing both width and height simultaneously), choose the one
   *    which is closer in area to info->orig.

   MetaRectangle A, B;
   int min_size, max_size, new_size;

   /* Find new rect A */
   A = B = info->current;
   min_size = info->current.height * min_ax / min_ay;
   max_size = info->current.height * max_ax / max_ay;
   new_size = MIN (max_size, MAX (min_size, info->current.width));
   A.width = new_size;

   /* Find new rect B */
   B = info->current;
   min_size = info->current.width * min_ay / min_ax;
   max_size = info->current.width * max_ay / max_ax;
   new_size = MIN (max_size, MAX (min_size, info->current.height));
   B.height = new_size;

   gboolean a_valid, b_valid;
   int ref_area, a_area, b_area;
   a_valid = b_valid = true;
   ref_area = meta_rectangle_area (info->orig);
   a_area = meta_rectangle_area (&A);
   b_area = meta_rectangle_area (&B);

   /* Discard too-small rects if we're increasing in size */
   if (meta_rectangle_could_fit_rect (info->current, info->orig))
     {
       if (a_area < ref_area)
         a_valid = FALSE;

       if (b_area < ref_area)
         b_valid = FALSE;
     }
  
   /* Discard too-large rects if we're decreasing in size */
   if (meta_rectangle_could_fit_rect (info->orig, info->current))
     {
       if (a_area > ref_area)
         a_valid = FALSE;

       if (b_area > ref_area)
         b_valid = FALSE;
     }

   /* If both are still valid, use the one closer in area to info->orig */
   if (a_valid && b_valid)
     {
       if (abs (a_area - ref_area) < abs (b_area - ref_area))
         b_valid = FALSE;
       else
         a_valid = FALSE;
     }

   if (!a_valid && !b_valid)
     /* This should only be possible for initial placement; we just
      * punt and pick one */
     resize_with_gravity (info->current, 
                          info->resize_gravity,
                          A.width,
                          A.height);
   else if (!b_valid)
     resize_with_gravity (info->current, 
                          info->resize_gravity,
                          A.width,
                          A.height);
   else if (!a_valid)     
     resize_with_gravity (info->current, 
                          info->resize_gravity,
                          A.width,
                          A.height);
   else
     g_error ("Was this programmed by monkeys?!?\n");

   return TRUE;
}
