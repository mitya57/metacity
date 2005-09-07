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
 *        app  resize:        clamp size, gravity adjust, shove-into-workarea
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
 * gboolean
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
  PRIORITY_CLAMP_TO_WORKAREA=1,
  PRIORITY_ENTIRELY_VISIBLE_ON_WORKAREA=1,
  PRIORITY_GRAVITY_ADJUST=1,
  PRIORITY_SIZE_HINTS_INCREMENTS=1,
  PRIORITY_MAXIMIZATION=2,
  PRIORITY_FULLSCREEN=2
  PRIORITY_SIZE_HINTS_LIMITS=3,
  PRIORITY_TITLEBAR_PARTIALLY_VISIBLE_ON_WORKAREA=4
  PRIORITY_PARTIALLY_VISIBLE_ON_WORKAREA=4,
  PRIORITY_MAXIMUM=4   # Dummy value used for loop end = max(all priorities)
} ConstraintPriority;

typedef struct
{
  MetaRectangle       orig;
  MetaRectangle       current;
  MetaFrameGeometry   fgeom;
  MetaMoveResizeFlags action_type;
  int                 resize_gravity;
  MetaRectangle       work_area_xinerama; /* current xinerama only */
  MetaRectangle       work_area_screen;   /* all xineramas */
  const MetaXineramaScreenInfo *xinerama_info;
} ConstraintInfo;

static gboolean constrain_gravity_adjust    (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_maximization      (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_fullscreen        (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_clamp_size        (MetaWindow         *window,
                                             ConstraintInfo     *info,
                                             ConstraintPriority  priority,
                                             gboolean            check_only);
static gboolean constrain_1d_size_hints     (MetaWindow         *window,
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
                                    MetaMoveResizeFlags  action_type,
                                    int                  resize_gravity,
                                    const MetaRectangle *orig,
                                    MetaRectangle       *new);
static void place_window_if_needed (MetaWindow     *window,
                                    ConstraintInfo  info);



void
meta_window_constrain (MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       MetaMoveResizeFlags  action_type,
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
                         action_type,
                         resize_gravity,
                         orig,
                         new);
  place_window_if_needed (window, info);

  ConstraintPriority priority = PRIORITY_MINIMUM;
  gboolean satisfied = false;
  while (!satisfied && priority <= PRIORITY_MAXIMUM) {
    gboolean check_only = FALSE;
    constrain_gravity_adjust    (window, &info, priority, check_only);
    constrain_maximization      (window, &info, priority, check_only);
    constrain_fullscreen        (window, &info, priority, check_only);
    constrain_clamp_size        (window, &info, priority, check_only);
    constrain_1d_size_hints     (window, &info, priority, check_only);
    constrain_aspect_ratio      (window, &info, priority, check_only);
    constrain_fully_onscreen    (window, &info, priority, check_only);
    constrain_titlebar_onscreen (window, &info, priority, check_only);

    check_only = TRUE;
    satisfied =
      constrain_gravity_adjust    (window, &info, priority, check_only) &&
      constrain_maximization      (window, &info, priority, check_only) &&
      constrain_fullscreen        (window, &info, priority, check_only) &&
      constrain_clamp_size        (window, &info, priority, check_only) &&
      constrain_1d_size_hints     (window, &info, priority, check_only) &&
      constrain_aspect_ratio      (window, &info, priority, check_only) &&
      constrain_fully_onscreen    (window, &info, priority, check_only) &&
      constrain_titlebar_onscreen (window, &info, priority, check_only);

    priority++;
  }
}

static void
setup_constraint_info (ConstraintInfo      *info,
                       MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       MetaMoveResizeFlags  action_type,
                       int                  resize_gravity,
                       const MetaRectangle *orig,
                       MetaRectangle       *new)
{
  info->orig    = *orig;
  info->current = *new;

  /* Create a fake frame geometry if none really exists */
  if (orig_fgeom && !window->fullscreen)
    info->fgeom = *orig_fgeom;
  else
    {
      info->fgeom.top_height = 0;
      info->fgeom.bottom_height = 0;
      info->fgeom.left_width = 0;
      info->fgeom.right_width = 0;
    }

  info->action_type = action_type;
  info->gravity = resize_gravity;

  meta_window_get_work_area_current_xinerama (window, &info->work_area_xinerama);
  meta_window_get_work_area_all_xineramas (window, &info->work_area_screen);

  info->xinerama_info =
    meta_screen_get_xinerama_for_window (window->screen, window);
}

static void
place_window_if_needed(MetaWindow     *window,
                       ConstraintInfo  info)
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
      MetaRectangle placed_rect = info.orig;

      meta_window_place (window, orig_fgeom, info.orig.x, info.orig.y,
                         &placed_rect.x, &placed_rect.y);
      did_placement = TRUE;

      /* placing the window may have changed the xinerama.  Find the
       * new xinerama and update the ConstraintInfo
       */
      info.xinerama = meta_screen_get_xinerama_for_rect (window->screen,
                                                         &placed_rect);
      meta_window_get_work_area_for_xinerama (window,
                                              info.xinerama->number,
                                              &info.work_area_xinerama);

      info.current.x = placed_rect.x;
      info.current.y = placed_rect.y;
    }

  if (window->maximize_after_placement &&
      (window->placed || did_placement))
    {
      window->maximize_after_placement = FALSE;

      if (info.current.width  >= info.work_area_xinerama.width &&
          info.current.height >= info.work_area_xinerama.height)
        {
          /* define a sane saved_rect so that the user can unmaximize
           * to something reasonable.
           */
          new->width = .75 * info.work_area_xinerama.width;
          new->height = .75 * info.work_area_xinerama.height;
          new->x = info.work_area_xinerama.x +
                   .125 * info.work_area_xinerama.width;
          new->y = info.work_area_xinerama.y +
                   .083 * info.work_area_xinerama.height;
        }

      meta_window_maximize_internal (window, new);

      /* maximization may have changed frame geometry */
      if (window->frame && !window->fullscreen)
        meta_frame_calc_geometry (window->frame,
                                  &info.fgeom);
    }
}

static gboolean
constrain_gravity_adjust (MetaWindow         *window,
                          ConstraintInfo     *info,
                          ConstraintPriority  priority,
                          gboolean            check_only)
{
  if (priority > PRIORITY_GRAVITY_ADJUST)
    return TRUE;

  /* Exit if constraint doesn't apply */
  if (!info->adjust_for_gravity)
    return TRUE;

  /* The only way you could have a situation that could be interpreted as
   * violating this constraint is if the window is near the screen edge
   * (but is fully onscreen) and adjusting according to gravity would send
   * it off--but the onscreen constraints shove it back on.  I guess I
   * could check old positions of the window versus new positions, but I
   * don't care that much--I'll just say it always is.
   */
  if (check_only)
    return TRUE;

  /* Do the gravity adjustment */

  return TRUE;
}

static gboolean
constrain_gravity_adjust (MetaWindow         *window,
                          ConstraintInfo     *info,
                          ConstraintPriority  priority,
                          gboolean            check_only)
    constrain_gravity_adjust    (window, &info, priority, check_only);
    constrain_maximization      (window, &info, priority, check_only);
    constrain_fullscreen        (window, &info, priority, check_only);
    constrain_clamp_size        (window, &info, priority, check_only);
    constrain_1d_size_hints     (window, &info, priority, check_only);
    constrain_aspect_ratio      (window, &info, priority, check_only);
    constrain_fully_onscreen    (window, &info, priority, check_only);
    constrain_titlebar_onscreen (window, &info, priority, check_only);

static gboolean
constrain_whatever (MetaWindow         *window,
                    ConstraintInfo     *info,
                    ConstraintPriority  priority,
                    gboolean            check_only)
{
  if (priority > PRIORITY_WHATEVER)
    return TRUE;

  /* Determine whether constraint applies; note that if the constraint
   * cannot possibly be satisfied, constraint_applies should be set to
   * false.  If we don't do this, all constraints with a lesser priority
   * will be dropped along with this one, and we'd rather apply as many as
   * possible.
   */
  if (!constraint_applies)
    return TRUE;

  /* Determine whether constraint is already satisfied; if we're only
   * checking the status of whether the constraint is satisfied, we end
   * here.
   */
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /* Enforce constraints */

  return TRUE;
}

