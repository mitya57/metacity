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

#include <stdlib.h>

/* Stupid disallowing of nested C comments makes a #if 0 mandatory... */
#if 0
/* This is a huge comment with a brief overview of how to hack on it as
 * quickly as possible, followed by much more in depth details of how it
 * works, why it works that way, the ideas behind it, and comparisons to
 * the previous way of doing things.
 *
 * BRIEF OVERVIEW OF HOW TO HACK ON THIS FILE
 *
 * This can basically be explained by showing how to add a new
 * constraint, the steps of which are:
 *   1) Add a new entry in the ConstraintPriority enum; higher values
 *      have higher priority
 *   2) Write a new function following the format of the example below,
 *      "constrain_whatever".
 *   3) Add your function to the loop in meta_window_constrain() in both
 *      places.
 * 
 * An example constraint function, constrain_whatever:
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
 *
 * THE NASTY DETAILS (cue ominous music)
 *
 * There are a couple basic ideas behind how this code works and why
 * it works that way:
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
 *      would mean setting window->require_fully_onscreen to FALSE in
 *      place.c somewhere).  Also, minimum size hints might be bigger than
 *      screen size).  Thus, we use a method of "growing" the workarea so
 *      that the extended region provides the constraint.  (b) docks can
 *      remove otherwise valid space from the workarea.  This doesn't pose
 *      much problem for docks that either span the width or the height of
 *      the screen.  It does cause problems when they only span part of the
 *      width or height ("partial struts"), because then the workarea (the
 *      area used by e.g. maximized windows) leaves out some available
 *      holes that smaller windows could use.  So we have an auxiliary
 *      workarea that takes these into account using a combination of
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
 */
#endif

typedef enum
{
  PRIORITY_MINIMUM=0, // Dummy value used for loop start = min(all priorities)
  PRIORITY_ASPECT_RATIO=0,
  PRIORITY_ENTIRELY_VISIBLE_ON_SINGLE_XINERAMA=0,
  PRIORITY_ENTIRELY_VISIBLE_ON_WORKAREA=1,
  PRIORITY_SIZE_HINTS_INCREMENTS=1,
  PRIORITY_MAXIMIZATION=2,
  PRIORITY_FULLSCREEN=2,
  PRIORITY_SIZE_HINTS_LIMITS=3,
  PRIORITY_PARTIALLY_VISIBLE_ON_WORKAREA=4,
  PRIORITY_MAXIMUM=4  // Dummy value used for loop end = max(all priorities)
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

  /* See setup_constraint_info for explanation of the differences and
   * similarity between resize_gravity and fixed_directions
   */
  int                  resize_gravity;
  FixedDirections      fixed_directions;

  /* work_area_xinerama - current xinerama region minus struts
   * work_area_screen   - entire screen (all xineramas) minus struts
   * entire_xinerama    - current xienrama, including strut regions
   */
  MetaRectangle        work_area_xinerama;
  MetaRectangle        work_area_screen;
  MetaRectangle        entire_xinerama;
} ConstraintInfo;

static gboolean constrain_maximization       (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_fullscreen         (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_size_increments    (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_size_limits        (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_aspect_ratio       (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_to_single_xinerama (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_fully_onscreen     (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);
static gboolean constrain_partially_onscreen (MetaWindow         *window,
                                              ConstraintInfo     *info,
                                              ConstraintPriority  priority,
                                              gboolean            check_only);

static void setup_constraint_info        (ConstraintInfo      *info,
                                          MetaWindow          *window,
                                          MetaFrameGeometry   *orig_fgeom,
                                          MetaMoveResizeFlags  flags,
                                          int                  resize_gravity,
                                          const MetaRectangle *orig,
                                          MetaRectangle       *new);
static void place_window_if_needed       (MetaWindow     *window,
                                          ConstraintInfo *info);
static void update_onscreen_requirements (MetaWindow     *window,
                                          ConstraintInfo *info);
static void extend_by_frame              (MetaRectangle           *rect,
                                          const MetaFrameGeometry *fgeom);
static void unextend_by_frame            (MetaRectangle           *rect,
                                          const MetaFrameGeometry *fgeom);
static inline void get_size_limits       (const MetaWindow        *window,
                                          const MetaFrameGeometry *fgeom,
                                          gboolean include_frame,
                                          MetaRectangle *min_size,
                                          MetaRectangle *max_size);
static GList* get_screen_relative_spanning_rects (
                                          const MetaScreen *screen,
                                          const int         left_expand,
                                          const int         right_expand,
                                          const int         top_expand,
                                          const int         bottom_expand);


typedef gboolean (* ConstraintFunc) (MetaWindow         *window,
                                     ConstraintInfo     *info,
                                     ConstraintPriority  priority,
                                     gboolean            check_only);

static const ConstraintFunc all_constraints[] = {
  constrain_maximization,
  constrain_fullscreen,
  constrain_size_increments,
  constrain_size_limits,
  constrain_aspect_ratio,
  constrain_to_single_xinerama,
  constrain_fully_onscreen,
  constrain_partially_onscreen,
  NULL
};

static const char* all_constraint_names[] = {
  "constrain_maximization",
  "constrain_fullscreen",
  "constrain_size_increments",
  "constrain_size_limits",
  "constrain_aspect_ratio",
  "constrain_to_single_xinerama",
  "constrain_fully_onscreen",
  "constrain_partially_onscreen",
  NULL
};

static gboolean
do_all_constraints (MetaWindow         *window,
                    ConstraintInfo     *info,
                    ConstraintPriority  priority,
                    gboolean            check_only)
{
  const ConstraintFunc  *constraint;
  const char           **constraint_name;
  gboolean               satisfied;

  constraint = &all_constraints[0];
  constraint_name = &all_constraint_names[0];

  satisfied = TRUE;
  while (*constraint)
    {
      satisfied = satisfied &&
                  (*constraint) (window, info, priority, check_only);

      if (!check_only)
        {
          /* Log how the constraint modified the position */
          meta_topic (META_DEBUG_GEOMETRY,
                      "info->current is %d,%d +%d,%d after %s\n",
                      info->current.x, info->current.y, 
                      info->current.width, info->current.height,
                      *constraint_name);
        }
      else if (!satisfied)
        {
          /* Log which constraint was not satisfied */
          meta_topic (META_DEBUG_GEOMETRY,
                      "constraint %s not satisfied.\n",
                      *constraint_name);
          return FALSE;
        }
      ++constraint;
      ++constraint_name;
    }

  return TRUE;
}

void
meta_window_constrain (MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       MetaMoveResizeFlags  flags,
                       int                  resize_gravity,
                       const MetaRectangle *orig,
                       MetaRectangle       *new)
{
  ConstraintInfo info;

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

  ConstraintPriority priority = PRIORITY_MINIMUM;
  gboolean satisfied = FALSE;
  while (!satisfied && priority <= PRIORITY_MAXIMUM) {
    gboolean check_only = TRUE;

    /* Individually enforce all the high-enough priority constraints */
    do_all_constraints (window, &info, priority, !check_only);

    /* Check if all high-enough priority constraints are simultaneously 
     * satisfied
     */
    satisfied = do_all_constraints (window, &info, priority, check_only);

    /* Drop the least important constraints if we can't satisfy them all */
    priority++;
  }

  /* Make sure we use the constrained position */
  *new = info.current;

  /* We may need to update window->require_fully_onscreen,
   * window->require_on_single_xinerama, and perhaps other quantities
   * if this was a user move or user move-and-resize operation.
   */
  update_onscreen_requirements (window, &info);

  /* Ew, what an ugly way to do things.  Destructors (in a real OOP language,
   * not gobject-style--gobject would be more pain than it's worth) or
   * smart pointers would be so much nicer here.  *shrug*
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

  info->is_user_action = (flags & META_IS_USER_ACTION);

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
  if (info->is_user_action && info->action_type == ACTION_RESIZE)
    {
      int pseudo_gravity = NorthWestGravity;
      if (orig->x == new->x)
        {
          if (orig->y == new->y)
            pseudo_gravity = NorthWestGravity;
          else if (orig->y + orig->height == new->y + new->height)
            pseudo_gravity = SouthWestGravity;
        }
      else if (orig->x + orig->width == new->x + new->width)
        {
          if (orig->y == new->y)
            pseudo_gravity = NorthEastGravity;
          else if (orig->y + orig->height == new->y + new->height)
            pseudo_gravity = SouthEastGravity;
        }
      info->resize_gravity = pseudo_gravity;      
    }

  /* Note that although resize_gravity and fixed_directions look similar,
   * they are used for different purposes and I believe it helps code
   * clarity to keep them separate in those sections where each is used.
   *
   *   - resize_gravity is only for resize operations and is used for
   *     constraints unrelated to keeping a window within a certain region
   *   - fixed_directions is for both move and resize operations and is
   *     specifically for keeping a window within a specified region.
   *
   * Examples of where each are used:
   *
   *   - If a window is simultaneously moved and resized to the
   *     southeast corner with SouthEastGravity, but it turns out that
   *     the window was sized to something smaller than the minimum
   *     size hint, then the size_hints constraint should resize the
   *     window using the resize_gravity to ensure that the southeast
   *     corner doesn't move.
   *   - If an application resizes itself so that it grows downward only
   *     (which I note could be using any of three different gravities,
   *     most likely NorthWest), and happens to put the southeast part of
   *     the window under a partial strut, then the window needs to be
   *     forced back on screen.  (Yes, shoved onscreen and not clipped; see
   *     bug 136307).  It may be the case that moving the window to the
   *     left results in less movement of the window than moving the window
   *     up, which, in the absence of fixed directions would cause us to
   *     chose moving to the left.  But since the user knows that only the
   *     height of the window is changing, they would find moving to the
   *     left weird (especially if this were a dialog that had been
   *     centered on its parent).  It'd be better to shove the window
   *     upwards so we make sure to keep the left and right sides fixed in
   *     this case.  Note that moving the window upwards (or even if we
   *     were to go left) is totally against the gravity in this case; but
   *     that's okay because gravity typically assumes there's more than
   *     enough onscreen space for the resize and we only override the
   *     gravity when that assumption is wrong.
   *
   * Note that fixed directions might (though I haven't thought it
   * completely through) give an impossible to fulfill constraint; if they
   * do, then we could temporarily throw them out and retry the constraint
   * again.
   *
   * Now, some nasty details:
   *
   *   This should be fixed directions (as opposed to fixed sides), because
   *   I'm only interested in shoving/clipping in x versus y.  The nitty
   *   gritty of what gets fixed:
   *   User move:
   *     in x direction - y direction fixed
   *     in y direction - x direction fixed
   *     in both dirs.  - neither direction fixed
   *   User resize: (note that for clipping, only 1 side ever changed)
   *     in x direction - y direction fixed (technically other x side fixed too)
   *     in y direction - x direction fixed (technically other y side fixed too)
   *     in both dirs.  - neither direction fixed
   *   App move:
   *     in x direction - y direction fixed
   *     in y direction - x direction fixed
   *     in both dirs.  - neither direction fixed
   *   App resize
   *     in x direction - y direction fixed
   *     in y direction - x direction fixed
   *     in 2 parallel directions (center side gravity) - other dir. fixed
   *     in 2 orthogonal directions (corner gravity) - neither dir. fixed
   *     in 3 or 4 directions (a center-like gravity) - neither dir. fixed
   *   Move & resize
   *     Treat like resize case though this will usually mean all four sides
   *     change and result in neither direction being fixed
   */
  info->fixed_directions = 0;
  /* If x directions don't change but either y direction does */
  if ( orig->x == new->x && orig->x + orig->width  == new->x + new->width   &&
      (orig->y != new->y || orig->y + orig->height != new->y + new->height))
    {
      info->fixed_directions = FIXED_DIRECTION_X;
    }
  /* If y directions don't change but either x direction does */
  if ( orig->y == new->y && orig->y + orig->height == new->y + new->height  &&
      (orig->x != new->x || orig->x + orig->width  != new->x + new->width ))
    {
      info->fixed_directions = FIXED_DIRECTION_Y;
    }

  meta_window_get_work_area_current_xinerama (window, &info->work_area_xinerama);
  meta_window_get_work_area_all_xineramas (window, &info->work_area_screen);

  const MetaXineramaScreenInfo *xinerama_info =
    meta_screen_get_xinerama_for_window (window->screen, window);
  info->entire_xinerama.x      = xinerama_info->x_origin;
  info->entire_xinerama.y      = xinerama_info->y_origin;
  info->entire_xinerama.width  = xinerama_info->width;
  info->entire_xinerama.height = xinerama_info->height;

  /* Log all this information for debugging */
  meta_topic (META_DEBUG_GEOMETRY,
              "Setting up constraint info:\n"
              "  orig: %d,%d +%d,%d\n"
              "  new : %d,%d +%d,%d\n"
              "  fgeom: %d,%d,%d,%d\n"
              "  action_type     : %s\n"
              "  is_user_action  : %s\n"
              "  resize_gravity  : %s\n"
              "  fixed_directions: %s\n"
              "  work_area_xinerama: %d,%d +%d,%d\n"
              "  work_area_screen  : %d,%d +%d,%d\n"
              "  entire_xinerama   : %d,%d +%d,%d\n",
              info->orig.x, info->orig.y, info->orig.width, info->orig.height,
              info->current.x, info->current.y, 
                info->current.width, info->current.height,
              info->fgeom->left_width, info->fgeom->right_width,
                info->fgeom->top_height, info->fgeom->bottom_height,
              (info->action_type == ACTION_MOVE) ? "Move" :
                (info->action_type == ACTION_RESIZE) ? "Resize" :
                (info->action_type == ACTION_MOVE_AND_RESIZE) ? "Move&Resize" :
                "Freakin' Invalid Stupid",
              (info->is_user_action) ? "true" : "false",
              meta_gravity_to_string (info->resize_gravity),
              (info->fixed_directions == 0) ? "None" :
                (info->fixed_directions == FIXED_DIRECTION_X) ? "X fixed" :
                (info->fixed_directions == FIXED_DIRECTION_Y) ? "Y fixed" :
                "Freakin' Invalid Stupid",
              info->work_area_xinerama.x, info->work_area_xinerama.y,
                info->work_area_xinerama.width, 
                info->work_area_xinerama.height,
              info->work_area_screen.x, info->work_area_screen.y,
                info->work_area_screen.width, info->work_area_screen.height,
              info->entire_xinerama.x, info->entire_xinerama.y,
                info->entire_xinerama.width, info->entire_xinerama.height);
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

      /* Since we just barely placed the window, there's no reason to
       * consider any of the directions fixed.
       */
      info->fixed_directions = 0;
    }

  if (window->maximize_after_placement &&
      (window->placed || did_placement))
    {
      window->maximize_after_placement = FALSE;

      if (meta_rectangle_could_fit_rect (&info->current, 
                                         &info->work_area_xinerama))
        {
          /* define a sane saved_rect so that the user can unmaximize
           * to something reasonable.
           */
          info->current.width = .75 * info->work_area_xinerama.width;
          info->current.height = .75 * info->work_area_xinerama.height;
          info->current.x = info->work_area_xinerama.x +
                   .125 * info->work_area_xinerama.width;
          info->current.y = info->work_area_xinerama.y +
                   .083 * info->work_area_xinerama.height;
        }

      meta_window_maximize_internal (window, &info->current);

      /* maximization may have changed frame geometry */
      if (window->frame && !window->fullscreen)
        meta_frame_calc_geometry (window->frame, info->fgeom);
    }
}

static void
update_onscreen_requirements (MetaWindow     *window,
                              ConstraintInfo *info)
{
  /* FIXME: Naturally, I only want these flags to become false due to
   * user interactions (which is allowed since certain constraints are
   * ignored for user interactions regardless of the setting of these
   * flags).  However, do I want these flags to become true due to
   * just an application interaction?  It's possible that users may
   * find that strange since two application interactions that resize
   * in opposite ways don't end up cancelling--but it may also be
   * strange for the user to have an application resize the window so
   * that it's onscreen, the user forgets about it, and then later the
   * app is able to resize itself off the screen.  Anyway, for now,
   * I'm think the latter is the more problematic case but this may
   * need to be revisited.
   */

  /* Update whether we want future constraint runs to require the
   * window to be on fully onscreen.
   */
  GList *fully_onscreen_region = 
    get_screen_relative_spanning_rects (window->screen, 0, 0, 0, 0);
  window->require_fully_onscreen =
    meta_rectangle_contained_in_region (fully_onscreen_region,
                                        &info->current);
  meta_rectangle_free_spanning_set (fully_onscreen_region);

  /* Update whether we want future constraint runs to require the
   * window to be on a single xinerama.
   */
  window->require_on_single_xinerama =
    meta_rectangle_contains_rect (&info->entire_xinerama,
                                  &info->current);
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

/* We pack the results into MetaRectangle structs just for convienience; we
 * don't actually use the position of those rects.
 */
static inline void
get_size_limits (const MetaWindow        *window,
                 const MetaFrameGeometry *fgeom,
                 gboolean                 include_frame,
                 MetaRectangle *min_size,
                 MetaRectangle *max_size)
{
  min_size->width  = window->size_hints.min_width;
  min_size->height = window->size_hints.min_height;
  max_size->width  = window->size_hints.max_width;
  max_size->height = window->size_hints.max_height;

  if (include_frame)
    {
      int fw = fgeom->left_width + fgeom->right_width;
      int fh = fgeom->top_height + fgeom->bottom_height;

      min_size->width  += fw;
      min_size->height += fh;
      max_size->width  += fw;
      max_size->height += fh;
    }
}

static gboolean
constrain_maximization (MetaWindow         *window,
                        ConstraintInfo     *info,
                        ConstraintPriority  priority,
                        gboolean            check_only)
{
  if (priority > PRIORITY_MAXIMIZATION)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't */
  if (!window->maximized)
    return TRUE;
  MetaRectangle min_size, max_size;
  MetaRectangle work_area = info->work_area_xinerama;
  unextend_by_frame (&work_area, info->fgeom);
  get_size_limits (window, info->fgeom, FALSE, &min_size, &max_size);
  gboolean too_big =   !meta_rectangle_could_fit_rect (&work_area, &min_size);
  gboolean too_small = !meta_rectangle_could_fit_rect (&max_size, &work_area);
  if (too_big || too_small)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean constraint_already_satisfied =
    meta_rectangle_equal (&info->current, &work_area);
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  info->current = work_area;
  return TRUE;
}

static gboolean
constrain_fullscreen (MetaWindow         *window,
                      ConstraintInfo     *info,
                      ConstraintPriority  priority,
                      gboolean            check_only)
{
  if (priority > PRIORITY_FULLSCREEN)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't */
  if (!window->fullscreen)
    return TRUE;
  MetaRectangle min_size, max_size;
  MetaRectangle xinerama = info->entire_xinerama;
  get_size_limits (window, info->fgeom, FALSE, &min_size, &max_size);
  gboolean too_big =   !meta_rectangle_could_fit_rect (&xinerama, &min_size);
  gboolean too_small = !meta_rectangle_could_fit_rect (&max_size, &xinerama);
  if (too_big || too_small)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean constraint_already_satisfied =
    meta_rectangle_equal (&info->current, &xinerama);
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  info->current = xinerama;
  return TRUE;
}

static gboolean
constrain_size_increments (MetaWindow         *window,
                           ConstraintInfo     *info,
                           ConstraintPriority  priority,
                           gboolean            check_only)
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
  hi = window->size_hints.height_inc;
  bw = window->size_hints.base_width;
  wi = window->size_hints.width_inc;
  extra_height = (info->current.height - bh) % hi;
  extra_width  = (info->current.width  - bw) % wi;
  gboolean constraint_already_satisfied = 
    (extra_height == 0 && extra_width == 0);

  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  /* Shrink to base + N * inc */
  meta_rectangle_resize_with_gravity (&info->orig,
                                      &info->current, 
                                      info->resize_gravity,
                                      info->current.width - extra_width,
                                      info->current.height - extra_height);
  return TRUE;
}

static gboolean
constrain_size_limits (MetaWindow         *window,
                       ConstraintInfo     *info,
                       ConstraintPriority  priority,
                       gboolean            check_only)
{
  if (priority > PRIORITY_SIZE_HINTS_LIMITS)
    return TRUE;

  /* Determine whether constraint applies; exit if it doesn't.
   *
   * Note: The old code didn't apply this constraint for fullscreen or
   * maximized windows--but that seems odd to me.  *shrug*
   */
  if (info->action_type == ACTION_MOVE)
    return TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  MetaRectangle min_size, max_size;
  get_size_limits (window, info->fgeom, FALSE, &min_size, &max_size);
  gboolean too_big =   
    !meta_rectangle_could_fit_rect (&info->current, &min_size);
  gboolean too_small = 
    !meta_rectangle_could_fit_rect (&max_size, &info->current);
  gboolean constraint_already_satisfied = !too_big && !too_small;
  if (check_only || constraint_already_satisfied)
    return constraint_already_satisfied;

  /*** Enforce constraint ***/
  int new_width  = MIN (max_size.width, 
                        MAX (min_size.width,  info->current.width));
  int new_height = MIN (max_size.height, 
                        MAX (min_size.height, info->current.height));
  meta_rectangle_resize_with_gravity (&info->orig,
                                      &info->current, 
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

  /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
  /* This is absolutely totally busted -- disable it for now */
  return TRUE;
  /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME */

  /* Determine whether constraint applies; exit if it doesn't. */
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
   */
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

  /* Pseudocode:
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
   */
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
   a_valid = b_valid = TRUE;
   ref_area = meta_rectangle_area (&info->orig);
   a_area = meta_rectangle_area (&A);
   b_area = meta_rectangle_area (&B);

   /* Discard too-small rects if we're increasing in size */
   if (meta_rectangle_could_fit_rect (&info->current, &info->orig))
     {
       if (a_area < ref_area)
         a_valid = FALSE;

       if (b_area < ref_area)
         b_valid = FALSE;
     }
  
   /* Discard too-large rects if we're decreasing in size */
   if (meta_rectangle_could_fit_rect (&info->orig, &info->current))
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
     meta_rectangle_resize_with_gravity (&info->orig,
                                         &info->current, 
                                         info->resize_gravity,
                                         A.width,
                                         A.height);
   else if (!b_valid)
     meta_rectangle_resize_with_gravity (&info->orig,
                                         &info->current, 
                                         info->resize_gravity,
                                         A.width,
                                         A.height);
   else if (!a_valid)     
     meta_rectangle_resize_with_gravity (&info->orig,
                                         &info->current, 
                                         info->resize_gravity,
                                         A.width,
                                         A.height);
   else
     g_error ("Was this programmed by monkeys?!?\n");

   return TRUE;
}

static gboolean
do_screen_and_xinerama_relative_constraints (
  MetaWindow     *window,
  GList          *region_spanning_rectangles,
  ConstraintInfo *info,
  gboolean        check_only)
{
  gboolean exit_early = FALSE;

  /* First, log some debugging information */
  char spanning_region[1 + 28 * g_list_length (region_spanning_rectangles)];
  meta_topic (META_DEBUG_GEOMETRY,
              "screen/xinerama constraint; region_spanning_rectangles: %s\n"
              meta_rectangle_region_to_string (region_spanning_rectangles, ", ", 
                                               spanning_region));

  /* Determine whether constraint applies; exit if it doesn't */
  MetaRectangle how_far_it_can_be_smushed, min_size, max_size;
  how_far_it_can_be_smushed = info->current;
  get_size_limits (window, info->fgeom, TRUE, &min_size, &max_size);
  extend_by_frame (&info->current, info->fgeom);

  if (info->action_type != ACTION_MOVE)
    {
      if (!(info->fixed_directions & FIXED_DIRECTION_X))
        how_far_it_can_be_smushed.width = min_size.width;

      if (!(info->fixed_directions & FIXED_DIRECTION_Y))
        how_far_it_can_be_smushed.height = min_size.height;
    }
  if (!meta_rectangle_could_fit_in_region (region_spanning_rectangles,
                                           &how_far_it_can_be_smushed))
    exit_early = TRUE;

  /* Determine whether constraint is already satisfied; exit if it is */
  gboolean constraint_satisfied = 
    meta_rectangle_contained_in_region (region_spanning_rectangles,
                                        &info->current);
  if (exit_early || constraint_satisfied || check_only)
    {
      unextend_by_frame (&info->current, info->fgeom);
      return constraint_satisfied;
    }

  /* Enforce constraint */

  /* Clamp rectangle size for user move+resize, app move+resize, and
   * app resize; FIXME FIXME FIXME: Is this really right?!?  Why not
   * clamp for user resize if clamping for user move+resize?!?  Just
   * because it'll be clipped below anyway?  Also, why doesn't the
   * comment match the code?!??
   *
   * QUIT WRITING SUCH STINKING BUGGY CODE AND COMMENTS!!!!!
   */
  if (info->action_type == ACTION_MOVE_AND_RESIZE ||
      (info->is_user_action && info->action_type == ACTION_RESIZE))
    {
      meta_rectangle_clamp_to_fit_into_region (region_spanning_rectangles,
                                               info->fixed_directions,
                                               &info->current,
                                               &min_size);
    }

  if (info->is_user_action && info->action_type == ACTION_RESIZE)
    /* For user resize, clip to the relevant region */
    meta_rectangle_clip_to_region (region_spanning_rectangles,
                                   info->fixed_directions,
                                   &info->current);
  else
    /* For everything else, shove the rectangle into the relevant region */
    meta_rectangle_shove_into_region (region_spanning_rectangles,
                                      info->fixed_directions,
                                      &info->current);

  unextend_by_frame (&info->current, info->fgeom);
  return TRUE;
}

static GSList*
get_all_workspace_struts (const MetaWorkspace *workspace)
{
  GSList *all_struts;
  all_struts = g_slist_concat (g_slist_copy (workspace->left_struts),
                               NULL);
  all_struts = g_slist_concat (g_slist_copy (workspace->right_struts),
                               all_struts);
  all_struts = g_slist_concat (g_slist_copy (workspace->top_struts),
                               all_struts);
  all_struts = g_slist_concat (g_slist_copy (workspace->bottom_struts),
                               all_struts);
  return all_struts;
}

static void
get_screen_rect_size (const MetaScreen *screen, MetaRectangle *rect)
{
  rect->x = rect->y = 0;
  rect->width  = screen->width;
  rect->height = screen->height;
}

static GList*
get_screen_relative_spanning_rects (const MetaScreen *screen,
                                    const int         left_expand,
                                    const int         right_expand,
                                    const int         top_expand,
                                    const int         bottom_expand)
{
  MetaRectangle  whole_screen;
  GSList         *all_struts;
  GList          *fully_onscreen_region;

  get_screen_rect_size (screen, &whole_screen);
  all_struts = get_all_workspace_struts (screen->active_workspace);
  fully_onscreen_region =
    meta_rectangle_get_minimal_spanning_set_for_region (&whole_screen,
                                                        all_struts,
                                                        left_expand,
                                                        right_expand,
                                                        top_expand,
                                                        bottom_expand);
  g_slist_free (all_struts);

  return fully_onscreen_region;
}

static gboolean
constrain_to_single_xinerama (MetaWindow         *window,
                              ConstraintInfo     *info,
                              ConstraintPriority  priority,
                              gboolean            check_only)
{
  if (priority > PRIORITY_ENTIRELY_VISIBLE_ON_SINGLE_XINERAMA)
    return TRUE;

  /* Exit early if we know the constraint won't apply */
  if (!window->require_on_single_xinerama || info->is_user_action)
    return TRUE;

  /* Have a helper function handle the constraint for us */
  GSList         *all_struts;
  GList          *single_xinerama_region;
  all_struts = get_all_workspace_struts (window->screen->active_workspace);
  single_xinerama_region =
    meta_rectangle_get_minimal_spanning_set_for_region (&info->entire_xinerama,
                                                        all_struts,
                                                        0, 0, 0, 0);
  gboolean retval =
    do_screen_and_xinerama_relative_constraints (window, 
                                                 single_xinerama_region,
                                                 info,
                                                 check_only);

  /* Free up the data we allocated */
  meta_rectangle_free_spanning_set (single_xinerama_region);
  g_slist_free (all_struts);
  return retval;
}

static gboolean
constrain_fully_onscreen (MetaWindow         *window,
                          ConstraintInfo     *info,
                          ConstraintPriority  priority,
                          gboolean            check_only)
{
  if (priority > PRIORITY_ENTIRELY_VISIBLE_ON_WORKAREA)
    return TRUE;

  /* Exit early if we know the constraint won't apply */
  if (!window->require_fully_onscreen || 
      (info->is_user_action && info->action_type != ACTION_RESIZE))
    return TRUE;

  /* Have a helper function handle the constraint for us */
  GList *fully_onscreen_region = 
    get_screen_relative_spanning_rects (window->screen, 0, 0, 0, 0);
  gboolean retval =
    do_screen_and_xinerama_relative_constraints (window, 
                                                 fully_onscreen_region,
                                                 info,
                                                 check_only);

  /* Free up the data we allocated */
  meta_rectangle_free_spanning_set (fully_onscreen_region);
  return retval;
}

static gboolean
constrain_partially_onscreen (MetaWindow         *window,
                              ConstraintInfo     *info,
                              ConstraintPriority  priority,
                              gboolean            check_only)
{
  if (priority > PRIORITY_PARTIALLY_VISIBLE_ON_WORKAREA)
    return TRUE;

  /* No special reasons, other than possibly priority, for this not applying */

  /* Determine how much offscreen things are allowed.  We use 25% window
   * width/height but clamp to the range of (10,75) pixels.  This is
   * somewhat of a seat of my pants random guess at what might look good.
   */
  int horiz_amount = info->current.width  / 4;
  int vert_amount  = info->current.height / 4;
  horiz_amount = MAX (10, MIN (75, horiz_amount));
  vert_amount  = MAX (10, MIN (75, vert_amount));

  /* FIXME!!!! I need to change amounts for user resize so that user
   * doesn't move the window further offscreen than it already is.
   */

  /* Have a helper function handle the constraint for us */
  GList *partially_onscreen_region;
  partially_onscreen_region = 
    get_screen_relative_spanning_rects (window->screen,
                                        horiz_amount,
                                        horiz_amount, 
                                        vert_amount,
                                        vert_amount);
  gboolean retval =
    do_screen_and_xinerama_relative_constraints (window, 
                                                 partially_onscreen_region,
                                                 info,
                                                 check_only);

  /* Free up the data we allocated */
  meta_rectangle_free_spanning_set (partially_onscreen_region);
  return retval;
}
