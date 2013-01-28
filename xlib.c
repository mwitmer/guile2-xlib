#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <libguile.h>

/* Compatibility for old Guiles. */
#ifndef SCM_VECTOR_LENGTH
# define SCM_VECTOR_LENGTH(x) (SCM_LENGTH(x))
#endif
#ifndef SCM_STRING_CHARS
# define SCM_STRING_CHARS(x) (SCM_ROCHARS(x))
#endif

/* Note on differences between this interface and raw Xlib in C.

   Some differences are inevitable.  When the power of Xlib is made
   available in an general purpose interpreted environment like Guile,
   we need to make sure that the interface cannot be called in a way
   that would cause the environment as a whole to crash or hang.  For
   example, in C you can call XCloseDisplay and then, say,
   XDestroyWindow for the display that you have just closed; and your
   program will probably hang or crash as a result.  An interface like
   the one implemented here must protect the environment against such
   problems by detecting and rejecting invalid usage.

   In practice, this means that the interface needs to track the
   current state of X resources like displays and windows.  So the
   Guile Xlib interface differs from C Xlib at least in that it stores
   certain additional state information and uses this information to
   detect and disallow invalid usage.

   Given that some difference is inevitable, one piece of Schemely
   behaviour is sufficiently useful - and sufficiently easier to
   implement in this interface than in a Scheme layer above it - that
   it merits a further small departure from C Xlib.  This is the
   automatic freeing of X resources when the interface-level objects
   associated with them are garbage collected.  It applies to displays
   (using XCloseDisplay), windows (XDestroyWindow) and non-default gcs
   (XFreeGC).  Note that it is still possible to free these resources
   explicitly, using the x-close-display!, x-destroy-window! and
   x-free-gc! primitives respectively.

   Many further differences (between this interface and C Xlib) are
   possible, but none are compelling.  The X people presumably thought
   quite carefully about the structure and completeness of the Xlib
   interface, and that's worth benefitting from.  Layers presenting a
   graphical X interface with a different structure, or from a
   different angle, can easily be implemented in Scheme on top of
   this one. */


/* Note on garbage collection and freeing of X resources.

   The one wrinkle in implementing automatic freeing of X resources is
   that almost all X resources depend on a valid display, so we have
   to be careful that the display resource is always freed (using
   XCloseDisplay) last of all.

   In most cases this is handled by having resource smobs include a
   reference to the display smob.  But there is still a problem when
   all remaining X resource references are dropped between one GC
   cycle and the next: when this happens, the next GC sweep could free
   the display smob before it gets to some of the other resource
   smobs.

   Fortunately, resource smobs can check, in their free functions,
   whether this has happened, by looking at the SCM_TYP16 of their
   reference to the display smob.  If the display smob is still valid,
   this will be scm_tc16_xdisplay, and the relevant X resource should
   be freed as normal.  If the display smob has been freed earlier in
   this sweep, GC will have set its SCM_TYP16 to scm_tc_free_cell;
   this indicates that XCloseDisplay has already been called, and so
   the relevant X resource no longer needs to be freed. */


/* SMOB TYPES */

typedef struct xdisplay_t
{
  /* The underlying Xlib display pointer. */
  Display *dsp;

  /* State - open/closed. */
  int state;

#define XDISPLAY_STATE_OPEN         1
#define XDISPLAY_STATE_CLOSED       2
#define XDISPLAY_STATE_ANY          ( XDISPLAY_STATE_OPEN | XDISPLAY_STATE_CLOSED )

  /* Cached default gc smob for this display. */
  SCM gc;

} xdisplay_t;

typedef struct xscreen_t
{
  /* The display that this screen is on. */
  SCM dsp;

  /* The underlying Xlib screen structure. */
  Screen *scr;

} xscreen_t;

typedef struct xwindow_t
{
  /* The display that this window is on. */
  SCM dsp;

  /* The underlying Xlib window ID. */
  Window win;

  /* State - mapped/unmapped/destroyed. */
  int state;

#define XWINDOW_STATE_UNMAPPED      1
#define XWINDOW_STATE_MAPPED        2
#define XWINDOW_STATE_DESTROYED     4
#define XWINDOW_STATE_THIRD_PARTY   8
#define XWINDOW_STATE_PIXMAP        16

} xwindow_t;

typedef struct xgc_t
{
  /* The display that this GC belongs to. */
  SCM dsp;

  /* The underlying Xlib GC ID. */
  GC gc;

  /* State - default/created/freed. */
  int state;

#define XGC_STATE_DEFAULT           1
#define XGC_STATE_CREATED           2
#define XGC_STATE_FREED             4

} xgc_t;


/* DECLARATIONS */

int scm_tc16_xdisplay = 0;
int scm_tc16_xscreen = 0;
int scm_tc16_xwindow = 0;
int scm_tc16_xgc = 0;

SCM resource_id_hash;

#define XDISPLAY(display) ((xdisplay_t *) SCM_SMOB_DATA (display))
#define XSCREEN(screen)   ((xscreen_t *) SCM_SMOB_DATA (screen))

#define XDATA_ARCS            0
#define XDATA_LINES           1
#define XDATA_POINTS          2
#define XDATA_SEGMENTS        3
#define XDATA_RECTANGLES      4

static int xdisplay_print (SCM display, SCM port, scm_print_state *pstate);
static size_t xdisplay_free (SCM display);
static SCM xdisplay_mark (SCM display);
static SCM valid_dsp (SCM arg, int pos, int expected, const char *func);

static SCM xscreen_mark (SCM screen);
static int valid_scr (SCM display, SCM screen, int pos, xdisplay_t *dsp, const char *func);

SCM scm_x_open_display_x (SCM host);
SCM scm_x_close_display_x (SCM display);
SCM scm_x_no_op_x (SCM display);
SCM scm_x_flush_x (SCM display);
SCM scm_x_connection_number (SCM display);
SCM scm_x_screen_count (SCM display);
SCM scm_x_default_screen (SCM display);
SCM scm_x_q_length (SCM display);
SCM scm_x_server_vendor (SCM display);
SCM scm_x_protocol_version (SCM display);
SCM scm_x_protocol_revision (SCM display);
SCM scm_x_vendor_release (SCM display);
SCM scm_x_display_string (SCM display);
SCM scm_x_bitmap_unit (SCM display);
SCM scm_x_bitmap_bit_order (SCM display);
SCM scm_x_bitmap_pad (SCM display);
SCM scm_x_image_byte_order (SCM display);
SCM scm_x_next_request (SCM display);
SCM scm_x_last_known_request_processed (SCM display);
SCM scm_x_display_of (SCM whatever);
SCM scm_x_all_planes (void);
SCM scm_x_root_window (SCM display, SCM screen);
SCM scm_x_black_pixel (SCM display, SCM screen);
SCM scm_x_white_pixel (SCM display, SCM screen);
SCM scm_x_display_width (SCM display, SCM screen);
SCM scm_x_display_height (SCM display, SCM screen);
SCM scm_x_display_width_mm (SCM display, SCM screen);
SCM scm_x_display_height_mm (SCM display, SCM screen);
SCM scm_x_display_planes (SCM display, SCM screen);
SCM scm_x_display_cells (SCM display, SCM screen);
SCM scm_x_screen_of_display (SCM display, SCM screen);
SCM scm_x_screen_number_of_screen (SCM screen);
SCM scm_x_min_colormaps (SCM display, SCM screen);
SCM scm_x_max_colormaps (SCM display, SCM screen);

static int xwindow_print (SCM window, SCM port, scm_print_state *pstate);
static size_t xwindow_free (SCM window);
static SCM xwindow_mark (SCM window);
static xwindow_t * valid_win (SCM arg, int pos, int expected, const char *func);

SCM scm_x_create_window_x (SCM display);          /* @@@ simplified */
SCM scm_x_map_window_x (SCM window);
SCM scm_x_unmap_window_x (SCM window);
SCM scm_x_destroy_window_x (SCM window);
SCM scm_x_clear_window_x (SCM window);
SCM scm_x_clear_area_x (SCM window, SCM x, SCM y, SCM width, SCM height, SCM exposures);

SCM scm_x_create_pixmap_x (SCM display, SCM screen, SCM width, SCM height, SCM depth);
SCM scm_x_copy_area_x (SCM source, SCM destination, SCM gc, SCM src_x, SCM src_y, SCM width, SCM height, SCM dst_x, SCM dst_y);

static int xgc_print (SCM window, SCM port, scm_print_state *pstate);
static size_t xgc_free (SCM gc);
static SCM xgc_mark (SCM gc);
static xgc_t * valid_gc (SCM arg, int pos, int expected, const char *func);

SCM scm_x_default_gc (SCM display, SCM screen);
SCM scm_x_free_gc_x (SCM gc);
SCM scm_x_create_gc_x (SCM gc, SCM changes);
SCM scm_x_change_gc_x (SCM gc, SCM changes);
SCM scm_x_set_dashes_x (SCM gc, SCM offset, SCM dashes);
SCM scm_x_set_clip_rectangles_x (SCM gc, SCM x, SCM y, SCM rectangles, SCM ordering);
SCM scm_x_copy_gc_x (SCM src, SCM dst, SCM fields);

static void * valid_data (SCM arg, int pos, int type, int *allocatedp, int *count, const char *func);
static SCM draw (SCM window, SCM gc, SCM data, int type, const char *func);

SCM scm_x_draw_arcs_x (SCM window, SCM gc, SCM arcs);
SCM scm_x_draw_lines_x (SCM window, SCM gc, SCM points);
SCM scm_x_draw_points_x (SCM window, SCM gc, SCM points);
SCM scm_x_draw_segments_x (SCM window, SCM gc, SCM segments);
SCM scm_x_draw_rectangles_x (SCM window, SCM gc, SCM rectangles);

static SCM copy_event_fields (SCM display, XEvent *e, SCM event, const char *func);
static SCM lookup_window (SCM display, XID id, const char *func);

SCM scm_x_check_mask_event_x (SCM display, SCM mask, SCM event);
SCM scm_x_check_typed_event_x (SCM display, SCM type, SCM event);
SCM scm_x_check_typed_window_event_x (SCM window, SCM type, SCM event);
SCM scm_x_check_window_event_x (SCM window, SCM mask, SCM event);
SCM scm_x_events_queued_x (SCM display, SCM mode);
SCM scm_x_pending_x (SCM display);
SCM scm_x_mask_event_x (SCM display, SCM mask, SCM event);
SCM scm_x_next_event_x (SCM display, SCM event);
SCM scm_x_peek_event_x (SCM display, SCM event);
SCM scm_x_select_input_x (SCM window, SCM mask);
SCM scm_x_window_event_x (SCM window, SCM mask, SCM event);

void init_xlib_core (void);


/* DISPLAYS */

/* Smob print hook for displays. */
int xdisplay_print (SCM display, SCM port, scm_print_state *pstate)
{
  scm_puts ("#<x-display ", port);
  scm_intprint (SCM_UNPACK (SCM_CDR (display)), 16, port);
  scm_putc (' ', port);
  switch (((xdisplay_t *) SCM_SMOB_DATA (display))->state)
    {
    case XDISPLAY_STATE_OPEN:
      scm_puts ("open", port);
      break;
    case XDISPLAY_STATE_CLOSED:
      scm_puts ("closed", port);
      break;
    default:
      scm_puts ("corrupt", port);
      break;
    }
  scm_putc ('>', port);
  return 1;
}

/* Smob free hook for displays: close the display first. */
static size_t xdisplay_free (SCM display)
{
  xdisplay_t *dsp = (xdisplay_t *) SCM_SMOB_DATA (display);

  if (dsp->state == XDISPLAY_STATE_OPEN)
    scm_x_close_display_x (display);

  return 0;
}

/* Smob mark hook for displays: mark the default GC. */
static SCM xdisplay_mark (SCM display)
{
  xdisplay_t *dsp = (xdisplay_t *) SCM_SMOB_DATA (display);

  return dsp->gc;
}

static SCM valid_dsp (SCM arg, int pos, int expected, const char *func)
{
  SCM arg1 = arg;
  xdisplay_t *dsp = NULL;

  SCM_ASSERT (SCM_NIMP (arg1), arg1, pos, func);

  /* Allow obtention of a valid display value from other types that
     contain a display. */
  if (SCM_TYP16 (arg1) == scm_tc16_xscreen)
    arg1 = ((xscreen_t *) SCM_SMOB_DATA (arg1))->dsp;
  else if (SCM_TYP16 (arg1) == scm_tc16_xwindow)
    arg1 = ((xwindow_t *) SCM_SMOB_DATA (arg1))->dsp;
  else if (SCM_TYP16 (arg1) == scm_tc16_xgc)
    arg1 = ((xgc_t *) SCM_SMOB_DATA (arg1))->dsp;

  if (SCM_TYP16 (arg1) == scm_tc16_xdisplay)
    dsp = XDISPLAY (arg1);
  else
    scm_wrong_type_arg (func, pos, arg1);

  /* Check for valid display state. */
  if ((dsp->state & expected) == 0)
    {
      /* Invalid - work out an appropriate error message, depending on
         what we were expecting. */
      switch (expected)
        {
        case XDISPLAY_STATE_OPEN:
          scm_misc_error (func, "Display ~S has been closed", scm_list_1 (arg));

        default:
          scm_misc_error (func,
                          "Corrupt display state (~S)",
                          scm_list_1 (scm_from_int (expected)));
        }
    }

  return arg1;
}

/* Smob mark hook for screens: need to mark the display as well. */
SCM xscreen_mark (SCM screen)
{
  xscreen_t *scr = (xscreen_t *) SCM_SMOB_DATA (screen);

  return scr->dsp;
}

static int valid_scr (SCM display, SCM screen, int pos, xdisplay_t *dsp, const char *func)
#define FUNC_NAME func
{
  int scr;

  if (SCM_TYP16 (display) == scm_tc16_xscreen)
    scr = XScreenNumberOfScreen (((xscreen_t *) SCM_SMOB_DATA (display))->scr);
  else if (!SCM_UNBNDP (screen))
    {
      SCM_ASSERT (scm_to_int (screen), screen, pos, func);
      scr = scm_to_int (screen);
      SCM_ASSERT_RANGE (SCM_ARG2,
                        screen,
                        (scr >= 0) && (scr < ScreenCount (dsp->dsp)));
    }
  else
    scr = DefaultScreen (dsp->dsp);

  return scr;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_open_display_x, "x-open-display!", 0, 1, 0,
            (SCM host),
            "Opens a display connection to the X server on @var{host}\n"
            "and returns a value that can be used to refer to this\n"
            "connection in future X procedure calls.  If @var{host} is\n"
            "omitted, @code{x-open-display!} uses the value of the\n"
            "DISPLAY environment variable.")
#define FUNC_NAME s_scm_x_open_display_x
{
  char *dsparg = NULL;
  xdisplay_t *dsp;

  if (!SCM_UNBNDP (host))
    {
      SCM_ASSERT (scm_string_p (host), host, SCM_ARG1, FUNC_NAME);
      dsparg = scm_to_utf8_stringn (host, NULL);
    }

  dsp = scm_gc_malloc (sizeof (xdisplay_t), FUNC_NAME);

  dsp->state = XDISPLAY_STATE_OPEN;
  dsp->gc    = SCM_BOOL_F;
  dsp->dsp   = XOpenDisplay (dsparg);

  if (dsp->dsp == NULL)
    {
      scm_gc_free (dsp, sizeof(xdisplay_t), FUNC_NAME);
      scm_misc_error (FUNC_NAME,
                      "Failed to open X display on ~S",
                      scm_list_1 (host));
    }

  SCM_RETURN_NEWSMOB (scm_tc16_xdisplay, dsp);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_close_display_x, "x-close-display!", 1, 0, 0,
            (SCM display),
            "Closes the X server connection @var{display}.")
#define FUNC_NAME s_scm_x_close_display_x
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  dsp->state = XDISPLAY_STATE_CLOSED;
  XCloseDisplay (dsp->dsp);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_no_op_x, "x-no-op!", 1, 0, 0,
            (SCM display),
            "See XNoOp.")
#define FUNC_NAME s_scm_x_no_op_x
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  XNoOp (dsp->dsp);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_flush_x, "x-flush!", 1, 0, 0,
            (SCM display),
            "Flushes pending events for the X server connection @var{display}.")
#define FUNC_NAME s_scm_x_flush_x
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  XFlush (dsp->dsp);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_connection_number, "x-connection-number", 1, 0, 0,
            (SCM display),
            "Return the file descriptor for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_connection_number
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (ConnectionNumber (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_screen_count, "x-screen-count", 1, 0, 0,
            (SCM display),
            "Return the number of screens for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_screen_count
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (ScreenCount (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_default_screen, "x-default-screen", 1, 0, 0,
            (SCM display),
            "Return the default screen number of the specified DISPLAY.")
#define FUNC_NAME s_scm_x_default_screen
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (DefaultScreen (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_q_length, "x-q-length", 1, 0, 0,
            (SCM display),
            "Return the number of events queued for the specified DISPLAY")
#define FUNC_NAME s_scm_x_q_length
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (QLength (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_server_vendor, "x-server-vendor", 1, 0, 0,
            (SCM display),
            "Return the server vendor string for the specified DISPLAY")
#define FUNC_NAME s_scm_x_server_vendor
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  char* server_vendor = ServerVendor (dsp->dsp);
  if(server_vendor == NULL) 
  {
    return scm_from_locale_string("");
  }

  return scm_from_locale_string (server_vendor);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_protocol_version, "x-protocol-version", 1, 0, 0,
            (SCM display),
            "Return the protocol version number for the specified DISPLAY")
#define FUNC_NAME s_scm_x_protocol_version
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (ProtocolVersion (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_protocol_revision, "x-protocol-revision", 1, 0, 0,
            (SCM display),
            "Return the protocol revision number for the specified DISPLAY")
#define FUNC_NAME s_scm_x_protocol_revision
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (ProtocolRevision (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_vendor_release, "x-vendor-release", 1, 0, 0,
            (SCM display),
            "Return the vendor release number for the specified DISPLAY")
#define FUNC_NAME s_scm_x_vendor_release
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (VendorRelease (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_string, "x-display-string", 1, 0, 0,
            (SCM display),
            "Return the string that was passed to x-open-display! when\n"
            "the specified DISPLAY was opened.")
#define FUNC_NAME s_scm_x_display_string
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  char* display_string = ServerVendor (dsp->dsp);
  if(display_string == NULL) 
  {
    return scm_from_locale_string("");
  }

  return scm_from_locale_string (display_string);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_bitmap_unit, "x-bitmap-unit", 1, 0, 0,
            (SCM display),
            "Return the size of a bitmap's scanline unit in bits,\n"
            "for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_bitmap_unit
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (BitmapUnit (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_bitmap_bit_order, "x-bitmap-bit-order", 1, 0, 0,
            (SCM display),
            "Return either LSBFirst or MSBFirst to indicate whether\n"
            "the leftmost bit in the bitmap as displayed on the screen\n"
            "is the least or most significant bit in the unit,\n"
            "for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_bitmap_bit_order
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (BitmapBitOrder (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_bitmap_pad, "x-bitmap-pad", 1, 0, 0,
            (SCM display),
            "Return the number of bits that each scanline must be padded,\n"
            "for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_bitmap_pad
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (BitmapPad (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_image_byte_order, "x-image-byte-order", 1, 0, 0,
            (SCM display),
            "Return the required byte order for images for each\n"
            "scanline unit in XY format (bitmap) or for each pixel\n"
            "value in Z format, for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_image_byte_order
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (ImageByteOrder (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_next_request, "x-next-request", 1, 0, 0,
            (SCM display),
            "Return the full serial number that is to be used for\n"
            "the next request, for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_next_request
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (NextRequest (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_last_known_request_processed, "x-last-known-request-processed", 1, 0, 0,
            (SCM display),
            "Return the full serial number of the last request known\n"
            "by Xlib to have been processed by the X server,\n"
            "for the specified DISPLAY.")
#define FUNC_NAME s_scm_x_last_known_request_processed
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (LastKnownRequestProcessed (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_of, "x-display-of", 1, 0, 0,
            (SCM whatever),
            "Extract a display from the supplied argument.")
#define FUNC_NAME s_scm_x_display_of
{
  return valid_dsp (whatever, SCM_ARG1, XDISPLAY_STATE_ANY, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_all_planes, "x-all-planes", 0, 0, 0,
            (),
            "Return a value with all valid plane bits set.")
#define FUNC_NAME s_scm_x_all_planes
{
  return scm_from_ulong (AllPlanes);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_root_window, "x-root-window", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the root window of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_root_window
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return lookup_window (display,
                        RootWindow (dsp->dsp, scr),
                        FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_black_pixel, "x-black-pixel", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the black pixel value of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_black_pixel
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_uint (BlackPixel (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_white_pixel, "x-white-pixel", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the white pixel value of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_white_pixel
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_uint (WhitePixel (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_width, "x-display-width", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the width in pixels of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_display_width
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_int (DisplayWidth (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_height, "x-display-height", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the height in pixels of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_display_height
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_int (DisplayHeight (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_width_mm, "x-display-width-mm", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the width in mm of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_display_width_mm
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_int (DisplayWidthMM (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_height_mm, "x-display-height-mm", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the height in mm of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_display_height_mm
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_int (DisplayHeightMM (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_planes, "x-display-planes", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the depth of the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_display_planes
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_int (DisplayPlanes (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_display_cells, "x-display-cells", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the number of entries in the default colormap depth of\n"
            "the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_display_cells
{
  xdisplay_t *dsp;
  int scr;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  return scm_from_int (DisplayCells (dsp->dsp, scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_screen_of_display, "x-screen-of-display", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return a screen object for the specified DISPLAY and SCREEN number.\n"
            "If SCREEN is omitted, return a screen object for the default screen.")
#define FUNC_NAME s_scm_x_screen_of_display
{
  SCM display1;
  xdisplay_t *dsp;
  int scr;
  xscreen_t *scr1;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  scr1 = scm_gc_malloc (sizeof (xscreen_t), FUNC_NAME);

  scr1->scr = XScreenOfDisplay (dsp->dsp, scr);
  scr1->dsp = display1;

  SCM_RETURN_NEWSMOB (scm_tc16_xscreen, scr1);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_screen_number_of_screen, "x-screen-number-of-screen", 1, 0, 0,
            (SCM screen),
            "Return the screen number of the specified screen object.")
#define FUNC_NAME s_scm_x_screen_number_of_screen
{
  xdisplay_t *dsp;

  /* Check that the associated display is still open. */
  dsp = XDISPLAY (valid_dsp (screen, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (valid_scr (screen, screen, SCM_ARG1, dsp, FUNC_NAME));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_min_colormaps, "x-min-colormaps", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the minimum number of colormaps for\n"
            "the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_min_colormaps
{
  SCM scr;

  scr = scm_x_screen_of_display (display, screen);

  return scm_from_int (XMinCmapsOfScreen (XSCREEN (scr)->scr));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_max_colormaps, "x-max-colormaps", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Return the maximum number of colormaps for\n"
            "the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_max_colormaps
{
  SCM scr;

  scr = scm_x_screen_of_display (display, screen);

  return scm_from_int (XMaxCmapsOfScreen (XSCREEN (scr)->scr));
}
#undef FUNC_NAME


/* WINDOWS */

/* Smob print hook for windows. */
int xwindow_print (SCM window, SCM port, scm_print_state *pstate)
{
  xwindow_t *win = (xwindow_t *) SCM_SMOB_DATA (window);

  scm_puts ("#<x-window ", port);
  scm_intprint (SCM_UNPACK (SCM_CDR (window)), 16, port);
  scm_puts (" XID ", port);
  scm_intprint (win->win, 16, port);
  scm_putc (' ', port);
  switch (win->state)
    {
    case XWINDOW_STATE_UNMAPPED:
      scm_puts ("unmapped", port);
      break;
    case XWINDOW_STATE_MAPPED:
      scm_puts ("mapped", port);
      break;
    case XWINDOW_STATE_DESTROYED:
      scm_puts ("destroyed", port);
      break;
    case XWINDOW_STATE_THIRD_PARTY:
      scm_puts ("third party", port);
      break;
    case XWINDOW_STATE_PIXMAP:
      scm_puts ("pixmap", port);
      break;
    default:
      scm_puts ("corrupt", port);
      break;
    }
  scm_putc ('>', port);
  return 1;
}

/* Smob free hook for windows: destroy the window first. */
size_t xwindow_free (SCM window)
{
  xwindow_t *win = (xwindow_t *) SCM_SMOB_DATA (window);

  /* Only destroy this window if the display is still valid. */
  if ((SCM_TYP16 (win->dsp) == scm_tc16_xdisplay) &&
      (win->state != XWINDOW_STATE_DESTROYED) &&
      (win->state != XWINDOW_STATE_THIRD_PARTY))
    scm_x_destroy_window_x (window);

  return 0;
}

/* Smob mark hook for windows: need to mark the display as well. */
SCM xwindow_mark (SCM window)
{
  xwindow_t *win = (xwindow_t *) SCM_SMOB_DATA (window);

  return win->dsp;
}

static xwindow_t * valid_win (SCM arg, int pos, int expected, const char *func)
{
  SCM arg1 = arg;
  xwindow_t *win = NULL;

  SCM_ASSERT (SCM_NIMP (arg1), arg1, pos, func);

  if (SCM_TYP16 (arg1) == scm_tc16_xwindow)
    win = (xwindow_t *) SCM_SMOB_DATA (arg1);
  else
    scm_wrong_type_arg (func, pos, arg1);

  /* Check for valid display state. */
  if ((win->state & expected) == 0)
    {
      /* Invalid - work out an appropriate error message, depending on
         the actual window state. */
      switch (win->state)
        {
        case XWINDOW_STATE_UNMAPPED:
          scm_misc_error (func, "Window ~S is already unmapped", scm_list_1 (arg));

        case XWINDOW_STATE_MAPPED:
          scm_misc_error (func, "Window ~S is already mapped", scm_list_1 (arg));

        case XWINDOW_STATE_DESTROYED:
          scm_misc_error (func, "Window ~S has been destroyed", scm_list_1 (arg));

        case XWINDOW_STATE_THIRD_PARTY:
          scm_misc_error (func, "Window ~S belongs to a third party", scm_list_1 (arg));

        case XWINDOW_STATE_PIXMAP:
          scm_misc_error (func, "Window ~S is a pixmap", scm_list_1 (arg));

        default:
          scm_misc_error (func,
                          "Corrupt window state (~S)",
                          scm_list_1 (scm_from_int (win->state)));
        }
    }

  return win;
}

SCM_DEFINE (scm_x_create_window_x, "x-create-window!", 1, 0, 0,
            (SCM display),
            "Creates a new X window on the specified @var{display}\n"
            "and returns a value that can be used to refer to the\n"
            "created window in X drawing procedures.")
#define FUNC_NAME s_scm_x_create_window_x
{
  SCM display1;
  xdisplay_t *dsp;
  xwindow_t *win;
  XSizeHints hints;
  int screen;
  XSetWindowAttributes xswa;
  SCM window;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);

  /* Set up params required for XCreateWindow call. */
  hints.width  = 600;
  hints.height = 400;
  hints.flags  = PSize;

  screen = XDefaultScreen(dsp->dsp);

  xswa.background_pixel = XWhitePixel (dsp->dsp, screen);

  /* Now create the window. */
  win = scm_gc_malloc (sizeof (xwindow_t), FUNC_NAME);

  win->state = XWINDOW_STATE_UNMAPPED;
  win->dsp = display1;
  win->win = XCreateWindow (dsp->dsp,
                            DefaultRootWindow (dsp->dsp),
                            0,
                            0,
                            hints.width,
                            hints.height,
                            0,                    /* border width */
                            DefaultDepth (dsp->dsp, screen),
                            InputOutput,
                            DefaultVisual (dsp->dsp, screen),
                            CWBackPixel,
                            &xswa);

  if (win->win == 0)
    {
      scm_gc_free (win, sizeof(xwindow_t), FUNC_NAME);
      scm_misc_error (FUNC_NAME, "Failed to create X window on ~S", scm_list_1 (display));
    }

  /* Provide window manager hints. */
  XSetNormalHints (dsp->dsp, win->win, &hints);
  XStoreName (dsp->dsp, win->win, "Guile/X");

  SCM_NEWSMOB (window, scm_tc16_xwindow, win);

  /* Add this resource and smob to the resource ID hash. */
  scm_hashq_set_x (resource_id_hash, scm_from_int (win->win), window);

  return window;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_map_window_x, "x-map-window!", 1, 0, 0,
            (SCM window),
            "Maps the X window @var{window}.")
#define FUNC_NAME s_scm_x_map_window_x
{
  xdisplay_t *dsp;
  xwindow_t *win;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  win = valid_win (window, SCM_ARG1, XWINDOW_STATE_UNMAPPED, FUNC_NAME);

  win->state = XWINDOW_STATE_MAPPED;
  XMapWindow (dsp->dsp, win->win);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_unmap_window_x, "x-unmap-window!", 1, 0, 0,
            (SCM window),
            "Unmaps the X window @var{window}.")
#define FUNC_NAME s_scm_x_unmap_window_x
{
  xdisplay_t *dsp;
  xwindow_t *win;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  win = valid_win (window, SCM_ARG1, XWINDOW_STATE_MAPPED, FUNC_NAME);

  win->state = XWINDOW_STATE_UNMAPPED;
  XUnmapWindow (dsp->dsp, win->win);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_destroy_window_x, "x-destroy-window!", 1, 0, 0,
            (SCM window),
            "Destroys the X window @var{window}.")
#define FUNC_NAME s_scm_x_destroy_window_x
{
  xdisplay_t *dsp;
  xwindow_t *win;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  win = valid_win (window, SCM_ARG1, ~(XWINDOW_STATE_DESTROYED |
				       XWINDOW_STATE_THIRD_PARTY |
				       XWINDOW_STATE_PIXMAP), FUNC_NAME);

  win->state = XWINDOW_STATE_DESTROYED;
  XDestroyWindow (dsp->dsp, win->win);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_clear_window_x, "x-clear-window!", 1, 0, 0,
            (SCM window),
            "Clears the X window @var{window}.")
#define FUNC_NAME s_scm_x_clear_window_x
{
  xdisplay_t *dsp;
  xwindow_t *win;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  win = valid_win (window, SCM_ARG1, ~(XWINDOW_STATE_DESTROYED |
				       XWINDOW_STATE_PIXMAP), FUNC_NAME);
  XClearWindow (dsp->dsp, win->win);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_clear_area_x, "x-clear-area!", 5, 1, 0,
            (SCM window,
	     SCM x,
	     SCM y,
	     SCM width,
	     SCM height,
	     SCM exposures),
            "Clears an area of the X window @var{window}.")
#define FUNC_NAME s_scm_x_clear_area_x
{
  xdisplay_t *dsp;
  xwindow_t *win;
  int x1, y1;
  unsigned int w1, h1;
  Bool exp1 = False;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  win = valid_win (window, SCM_ARG1, ~(XWINDOW_STATE_DESTROYED), FUNC_NAME);
  SCM_VALIDATE_INT_COPY (SCM_ARG2, x, x1);
  SCM_VALIDATE_INT_COPY (SCM_ARG3, y, y1);
  SCM_VALIDATE_UINT_COPY (SCM_ARG4, width, w1);
  SCM_VALIDATE_UINT_COPY (SCM_ARG5, height, h1);
  if (!SCM_UNBNDP (exposures))
    SCM_VALIDATE_BOOL_COPY (SCM_ARG6, exposures, exp1);

  XClearArea (dsp->dsp, win->win, x1, y1, w1, h1, exp1);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


/* PIXMAPS */

/* As pixmaps and windows together form "drawables", the SMOB for a
   pixmap currently wraps an xwindow_t struct. */

SCM_DEFINE (scm_x_create_pixmap_x, "x-create-pixmap!", 5, 0, 0,
            (SCM display,
             SCM screen,
	     SCM width,
	     SCM height,
	     SCM depth),
            "Create and return a new pixmap on the specified DISPLAY and SCREEN.\n"
            "If SCREEN is omitted, the display's default screen is assumed.")
#define FUNC_NAME s_scm_x_create_pixmap_x
{
  SCM display1;
  xdisplay_t *dsp;
  int scr;
  int width1;
  int height1;
  int depth1;
  xwindow_t *pix;
  SCM pixmap;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);
  SCM_VALIDATE_INT_COPY (SCM_ARG3, width, width1);
  SCM_VALIDATE_INT_COPY (SCM_ARG4, height, height1);
  SCM_VALIDATE_INT_COPY (SCM_ARG5, depth, depth1);

  /* Allocate space for SMOB data. */
  pix = scm_gc_malloc (sizeof (xwindow_t), FUNC_NAME);

  pix->state = XWINDOW_STATE_PIXMAP;
  pix->dsp = display1;
  pix->win = XCreatePixmap (dsp->dsp,
			    RootWindow (dsp->dsp, scr),
			    width1,
			    height1,
			    depth1);

  if (pix->win == 0)
    {
      scm_gc_free (pix, sizeof (xwindow_t), FUNC_NAME);
      scm_misc_error (FUNC_NAME, "Failed to create X pixmap on ~S", scm_list_1 (display));
    }

  SCM_NEWSMOB (pixmap, scm_tc16_xwindow, pix);

  /* Add this resource and smob to the resource ID hash. */
  scm_hashq_set_x (resource_id_hash, scm_from_int (pix->win), pixmap);

  return pixmap;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_copy_area_x, "x-copy-area!", 9, 0, 0,
            (SCM source,
	     SCM destination,
	     SCM gc,
	     SCM src_x, SCM src_y,
	     SCM width, SCM height,
	     SCM dst_x, SCM dst_y),
            "Copy specified area from one drawable to another.")
#define FUNC_NAME s_scm_x_copy_area_x
{
  xwindow_t *src;
  xwindow_t *dst;
  xgc_t *gc1;
  int src_x1;
  int src_y1;
  unsigned int width1;
  unsigned int height1;
  int dst_x1;
  int dst_y1;

  src = valid_win (source, SCM_ARG1, (XWINDOW_STATE_MAPPED |
				      XWINDOW_STATE_PIXMAP |
				      XWINDOW_STATE_THIRD_PARTY), FUNC_NAME);
  dst = valid_win (destination, SCM_ARG2, (XWINDOW_STATE_MAPPED |
					   XWINDOW_STATE_PIXMAP |
					   XWINDOW_STATE_THIRD_PARTY), FUNC_NAME);
  gc1 = valid_gc (gc, SCM_ARG3, (XGC_STATE_CREATED | XGC_STATE_DEFAULT), FUNC_NAME);
  SCM_VALIDATE_INT_COPY (SCM_ARG4, src_x, src_x1);
  SCM_VALIDATE_INT_COPY (SCM_ARG5, src_y, src_y1);
  SCM_VALIDATE_UINT_COPY (SCM_ARG6, width, width1);
  SCM_VALIDATE_UINT_COPY (SCM_ARG7, height, height1);
  SCM_VALIDATE_INT_COPY (8, dst_x, dst_x1);
  SCM_VALIDATE_INT_COPY (9, dst_y, dst_y1);

  XCopyArea (XDISPLAY (src->dsp)->dsp, src->win, dst->win, gc1->gc,
	     src_x1, src_y1,
	     width1, height1,
	     dst_x1, dst_y1);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


/* GCS */

/* Smob print hook for gcs. */
int xgc_print (SCM gc, SCM port, scm_print_state *pstate)
{
  scm_puts ("#<x-gc ", port);
  scm_intprint (SCM_UNPACK (SCM_CDR (gc)), 16, port);
  scm_putc (' ', port);
  switch (((xgc_t *) SCM_SMOB_DATA (gc))->state)
    {
    case XGC_STATE_DEFAULT:
      scm_puts ("default", port);
      break;
    case XGC_STATE_CREATED:
      scm_puts ("created", port);
      break;
    case XGC_STATE_FREED:
      scm_puts ("freed", port);
      break;
    default:
      scm_puts ("corrupt", port);
      break;
    }
  scm_putc ('>', port);
  return 1;
}

/* Smob free hook for gcs: free the gc first. */
size_t xgc_free (SCM gc)
{
  xgc_t *gc1 = (xgc_t *) SCM_SMOB_DATA (gc);

  /* Only free this gc if the display is still valid. */
  if ((SCM_TYP16 (gc1->dsp) == scm_tc16_xdisplay) &&
      (gc1->state == XGC_STATE_CREATED))
    scm_x_free_gc_x (gc);

  return 0;
}

/* Smob mark hook for gcs: need to mark the display as well. */
SCM xgc_mark (SCM gc)
{
  xgc_t *gc1 = (xgc_t *) SCM_SMOB_DATA (gc);

  return gc1->dsp;
}

static xgc_t * valid_gc (SCM arg, int pos, int expected, const char *func)
{
  SCM arg1 = arg;
  xgc_t *gc1 = NULL;

  SCM_ASSERT (SCM_NIMP (arg1), arg1, pos, func);

  if (SCM_TYP16 (arg1) == scm_tc16_xgc)
    gc1 = (xgc_t *) SCM_SMOB_DATA (arg1);
  else
    scm_wrong_type_arg (func, pos, arg1);

  /* Check for valid display state. */
  if ((gc1->state & expected) == 0)
    {
      /* Invalid - work out an appropriate error message, depending on
         the actual gc state. */
      switch (gc1->state)
        {
        case XGC_STATE_DEFAULT:
          scm_misc_error (func, "GC ~S is the default GC", scm_list_1 (arg));

        case XGC_STATE_CREATED:
          scm_misc_error (func, "GC ~S is a created GC", scm_list_1 (arg));

        case XGC_STATE_FREED:
          scm_misc_error (func, "GC ~S has been freed", scm_list_1 (arg));

        default:
          scm_misc_error (func,
                          "Corrupt GC state (~S)",
                          scm_list_1 (scm_from_int (gc1->state)));
        }
    }

  return gc1;
}

SCM_DEFINE (scm_x_default_gc, "x-default-gc", 1, 1, 0,
            (SCM display,
             SCM screen),
            "Returns the default GC for the specified @var{display} and @var{screen}.")
#define FUNC_NAME s_scm_x_default_gc
{
  SCM display1;
  xdisplay_t *dsp;
  int scr;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  scr = valid_scr (display, screen, SCM_ARG2, dsp, FUNC_NAME);

  if (dsp->gc == SCM_BOOL_F)
    {
      /* Create and cache default GC smob. */
      xgc_t *gc1 = scm_gc_malloc (sizeof (xgc_t), FUNC_NAME);

      gc1->gc = DefaultGC (dsp->dsp, scr);
      gc1->dsp = display1;
      gc1->state = XGC_STATE_DEFAULT;

      SCM_NEWSMOB (dsp->gc, scm_tc16_xgc, gc1);
    }

  return dsp->gc;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_free_gc_x, "x-free-gc!", 1, 0, 0,
            (SCM gc),
            "Frees the specified @var{gc}.")
#define FUNC_NAME s_scm_x_free_gc_x
{
  xdisplay_t *dsp;
  xgc_t *gc1;

  dsp = XDISPLAY (valid_dsp (gc, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  gc1 = valid_gc (gc, SCM_ARG1, XGC_STATE_CREATED, FUNC_NAME);

  XFreeGC (dsp->dsp, gc1->gc);
  gc1->state = XGC_STATE_FREED;

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

typedef struct xgc_field_t
{
  /* Function to handle setting a field of an XGCValues struct. */
  void (*handler) (XGCValues *gcv, int offset, SCM value);

  /* Offset of field position within XGCValues. */
  int offset;

} xgc_field_t;

void gc_set_numeric_field (XGCValues *gcv, int offset, SCM value);
void gc_set_pixmap_field (XGCValues *gcv, int offset, SCM value);
void gc_set_font_field (XGCValues *gcv, int offset, SCM value);
void gc_set_boolean_field (XGCValues *gcv, int offset, SCM value);
void gc_set_char_field (XGCValues *gcv, int offset, SCM value);

xgc_field_t gc_fields[23] = {
  { gc_set_numeric_field, offsetof (XGCValues, function)           },
  { gc_set_numeric_field, offsetof (XGCValues, plane_mask)         },
  { gc_set_numeric_field, offsetof (XGCValues, foreground)         },
  { gc_set_numeric_field, offsetof (XGCValues, background)         },
  { gc_set_numeric_field, offsetof (XGCValues, line_width)         },
  { gc_set_numeric_field, offsetof (XGCValues, line_style)         },
  { gc_set_numeric_field, offsetof (XGCValues, cap_style)          },
  { gc_set_numeric_field, offsetof (XGCValues, join_style)         },
  { gc_set_numeric_field, offsetof (XGCValues, fill_style)         },
  { gc_set_numeric_field, offsetof (XGCValues, fill_rule)          },
  { gc_set_numeric_field, offsetof (XGCValues, arc_mode)           },
  { gc_set_pixmap_field,  offsetof (XGCValues, tile)               },
  { gc_set_pixmap_field,  offsetof (XGCValues, stipple)            },
  { gc_set_numeric_field, offsetof (XGCValues, ts_x_origin)        },
  { gc_set_numeric_field, offsetof (XGCValues, ts_y_origin)        },
  { gc_set_font_field,    offsetof (XGCValues, font)               },
  { gc_set_numeric_field, offsetof (XGCValues, subwindow_mode)     },
  { gc_set_boolean_field, offsetof (XGCValues, graphics_exposures) },
  { gc_set_numeric_field, offsetof (XGCValues, clip_x_origin)      },
  { gc_set_numeric_field, offsetof (XGCValues, clip_y_origin)      },
  { gc_set_pixmap_field,  offsetof (XGCValues, clip_mask)          },
  { gc_set_numeric_field, offsetof (XGCValues, dash_offset)        },
  { gc_set_char_field,    offsetof (XGCValues, dashes)             }
};

SCM_DEFINE (scm_x_create_gc_x, "x-create-gc!", 1, 0, 1,
            (SCM drawable,
             SCM changes),
            "Creates the specified @var{gc}.")
#define FUNC_NAME s_scm_x_create_gc_x
{
  SCM display1;
  xdisplay_t *dsp;
  xwindow_t *win;
  unsigned long mask = 0;
  XGCValues gcv;
  xgc_t *gc1;

  display1 = valid_dsp (drawable, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  win = valid_win (drawable, SCM_ARG1, ~XWINDOW_STATE_DESTROYED, FUNC_NAME);

  SCM_ASSERT ((scm_ilength (changes) & 1) == 0, changes, SCM_ARGn, FUNC_NAME);

  for (; !SCM_NULLP (changes); changes = SCM_CDDR (changes))
    {
      SCM field = SCM_CAR (changes);
      int fld;

      SCM_ASSERT (scm_integer_p (field), field, SCM_ARGn, FUNC_NAME);
      fld = scm_to_int (field);
      SCM_ASSERT_RANGE (SCM_ARG2, field, (fld >= 0) && (fld <= 22));

      mask = mask | (1L << fld);
      (*gc_fields[fld].handler) (&gcv, gc_fields[fld].offset, SCM_CADR (changes));
    }

  gc1 = scm_gc_malloc (sizeof (xgc_t), FUNC_NAME);

  gc1->gc = XCreateGC (dsp->dsp, win->win, mask, &gcv);
  gc1->dsp = display1;
  gc1->state = XGC_STATE_CREATED;

  SCM_RETURN_NEWSMOB (scm_tc16_xgc, gc1);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_change_gc_x, "x-change-gc!", 1, 0, 1,
            (SCM gc,
             SCM changes),
            "Changes the specified @var{gc}.")
#define FUNC_NAME s_scm_x_change_gc_x
{
  xdisplay_t *dsp;
  xgc_t *gc1;
  unsigned long mask = 0;
  XGCValues gcv;

  dsp = XDISPLAY (valid_dsp (gc, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  gc1 = valid_gc (gc, SCM_ARG1, (XGC_STATE_CREATED | XGC_STATE_DEFAULT), FUNC_NAME);

  SCM_ASSERT ((scm_ilength (changes) & 1) == 0, changes, SCM_ARGn, FUNC_NAME);

  for (; !SCM_NULLP (changes); changes = SCM_CDDR (changes))
    {
      SCM field = SCM_CAR (changes);
      int fld;

      SCM_ASSERT (scm_integer_p (field), field, SCM_ARGn, FUNC_NAME);
      fld = scm_to_int (field);
      SCM_ASSERT_RANGE (SCM_ARG2, field, (fld >= 0) && (fld <= 22));

      mask = mask | (1L << fld);
      (*gc_fields[fld].handler) (&gcv, gc_fields[fld].offset, SCM_CADR (changes));
    }

  XChangeGC (dsp->dsp, gc1->gc, mask, &gcv);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

#define FUNC_NAME s_scm_x_change_gc_x
void gc_set_numeric_field (XGCValues *gcv, int offset, SCM value)
{
  SCM_ASSERT (SCM_NUMBERP (value), value, SCM_ARGn, FUNC_NAME);
  *((int *) (((char *) gcv) + offset)) = SCM_NUM2ULONG (SCM_ARGn, value);
}

void gc_set_pixmap_field (XGCValues *gcv, int offset, SCM value)
{
  SCM_ASSERT (0, value, SCM_ARGn, FUNC_NAME);
}

void gc_set_font_field (XGCValues *gcv, int offset, SCM value)
{
  SCM_ASSERT (0, value, SCM_ARGn, FUNC_NAME);
}

void gc_set_boolean_field (XGCValues *gcv, int offset, SCM value)
{
  SCM_ASSERT (SCM_BOOLP (value), value, SCM_ARGn, FUNC_NAME);
  *((Bool *) (((char *) gcv) + offset)) = (Bool) SCM_NFALSEP (value);
}

void gc_set_char_field (XGCValues *gcv, int offset, SCM value)
{
  SCM_ASSERT (scm_integer_p (value), value, SCM_ARGn, FUNC_NAME);
  *(((char *) gcv) + offset) = (char) scm_to_int (value);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_set_dashes_x, "x-set-dashes!", 3, 0, 0,
            (SCM gc,
             SCM offset,
             SCM dashes),
            "See XSetDashes.")
#define FUNC_NAME s_scm_x_set_dashes_x
{
  xdisplay_t *dsp;
  xgc_t *gc1;
  int n;
  char *dash_list;

  dsp = XDISPLAY (valid_dsp (gc, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  gc1 = valid_gc (gc, SCM_ARG1, (XGC_STATE_CREATED | XGC_STATE_DEFAULT), FUNC_NAME);
  SCM_ASSERT (scm_integer_p (offset), offset, SCM_ARG2, FUNC_NAME);
  SCM_VALIDATE_LIST_COPYLEN (SCM_ARG3, dashes, n);

  /* Allocate a character array to pass dash list to Xlib. */
  dash_list = (char *) scm_gc_malloc (n * sizeof (char), FUNC_NAME);

  /* Copy dash lengths into this array. */
  for (n = 0; !SCM_NULLP (dashes); n++, dashes = SCM_CDR (dashes))
    {
      SCM len = SCM_CAR (dashes);

      if (!scm_integer_p (len))
        scm_gc_free (dash_list, n * sizeof (char), FUNC_NAME);

      SCM_ASSERT (scm_integer_p (len), len, SCM_ARG3, FUNC_NAME);
      dash_list[n] = scm_to_int (len);
    }

  XSetDashes (dsp->dsp, gc1->gc, scm_to_int (offset), dash_list, n);

  scm_gc_free (dash_list, n * sizeof(char), FUNC_NAME);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_set_clip_rectangles_x, "x-set-clip-rectangles!", 4, 1, 0,
            (SCM gc,
             SCM x,
             SCM y,
             SCM rectangles,
             SCM ordering),
            "See XSetClipRectangles.")
#define FUNC_NAME s_scm_x_set_clip_rectangles_x
{
  xdisplay_t *dsp;
  xgc_t *gc1;
  int order;
  int allocatedp;
  int num_rectangles;
  XRectangle *dat;

  dsp = XDISPLAY (valid_dsp (gc, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  gc1 = valid_gc (gc, SCM_ARG1, (XGC_STATE_CREATED | XGC_STATE_DEFAULT), FUNC_NAME);
  SCM_ASSERT (scm_integer_p (x), x, SCM_ARG2, FUNC_NAME);
  SCM_ASSERT (scm_integer_p (y), y, SCM_ARG3, FUNC_NAME);

  if (!SCM_UNBNDP (ordering))
    {
      SCM_ASSERT (scm_integer_p (ordering), ordering, SCM_ARG5, FUNC_NAME);
      order = scm_to_int (ordering);
      SCM_ASSERT_RANGE (SCM_ARG5,
                        ordering,
                        (order >= Unsorted) && (order <= YXBanded));
    }
  else
    order = Unsorted;

  dat = (XRectangle *) valid_data (rectangles,
                                   SCM_ARG4,
                                   XDATA_RECTANGLES,
                                   &allocatedp,
                                   &num_rectangles,
                                   FUNC_NAME);

  XSetClipRectangles (dsp->dsp,
                      gc1->gc,
                      scm_to_int (x),
                      scm_to_int (y),
                      dat,
                      num_rectangles,
                      order);

  if (allocatedp)
    scm_gc_free (dat, sizeof(XRectangle), FUNC_NAME);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME
SCM_DEFINE (scm_x_copy_gc_x, "x-copy-gc!", 2, 0, 1,
            (SCM src,
             SCM dst,
             SCM fields),
            "See XCopyGC.")
#define FUNC_NAME s_scm_x_copy_gc_x
{
  xdisplay_t *dsp;
  xgc_t *gc1, *gc2;
  unsigned long mask = 0;

  dsp = XDISPLAY (valid_dsp (src, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  gc1 = valid_gc (src, SCM_ARG1, (XGC_STATE_CREATED | XGC_STATE_DEFAULT), FUNC_NAME);
  gc2 = valid_gc (dst, SCM_ARG2, ~XGC_STATE_FREED, FUNC_NAME);

  SCM_VALIDATE_LIST (SCM_ARG3, fields);

  for (; !SCM_NULLP (fields); fields = SCM_CDDR (fields))
    {
      SCM field = SCM_CAR (fields);
      int fld;

      SCM_ASSERT (scm_integer_p (field), field, SCM_ARGn, FUNC_NAME);
      fld = scm_to_int (field);
      SCM_ASSERT_RANGE (SCM_ARG3, field, (fld >= 0) && (fld <= 22));

      mask = mask | (1L << fld);
    }

  XCopyGC (dsp->dsp, gc1->gc, mask, gc2->gc);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


/* VISUALS */

/* DefaultVisual */


/* COLORMAPS */

/* DefaultColormap */


/* DRAWING (NON-TEXT) */

static shorts_per_datum[5] = { 6, 2, 2, 4, 4 };

#define XDATACONV_UNKNOWN     0
#define XDATACONV_REQUIRED    1
#define XDATACONV_UNNECESSARY 2

static data_conversion[5] = {
  XDATACONV_UNKNOWN,
  XDATACONV_UNKNOWN,
  XDATACONV_UNKNOWN,
  XDATACONV_UNKNOWN,
  XDATACONV_UNKNOWN
};

static datum_size[5] = {
  sizeof (XArc),
  sizeof (XPoint),
  sizeof (XSegment),
  sizeof (XRectangle)
};

static void * valid_data (SCM arg,
                          int pos,
                          int type,
                          int *allocatedp,
                          int *count,
                          const char *func)
{
  scm_t_array_dim *dims;
  scm_t_array_handle handle;
  int num_shorts_per_datum;
  int num_data;
  short *vdat;
  void *data;
  int i;

  /* SCM_VALIDATE_ARRAY. */
  SCM_ASSERT (SCM_NIMP (arg) && (SCM_BOOL_F != scm_array_p (arg, SCM_UNDEFINED)),
              arg, pos, func);

  /* The underlying vector must be a uniform vector of shorts. */
  SCM_ASSERT (scm_typed_array_p(arg, scm_from_utf8_symbol("s8")), arg, pos, func);

  scm_array_get_handle(arg, &handle);

  /* Array must have two dimensions. */
  SCM_ASSERT (scm_array_handle_rank (&handle) == 2, arg, pos, func);

  /* Read dimensions. */
  dims = scm_array_handle_dims (&handle);
  num_data             = dims[0].ubnd - dims[0].lbnd + 1;
  num_shorts_per_datum = dims[1].ubnd - dims[1].lbnd + 1;
  if (num_shorts_per_datum != shorts_per_datum[type])
    scm_misc_error (func,
                    "Data has incorrect dimensions (~S, expected ~S)",
                    scm_list_2 (scm_from_int (num_shorts_per_datum),
                               scm_from_int (shorts_per_datum[type])));

  vdat = (short *) scm_array_handle_uniform_elements(&handle);

  /* Do we need to make a copy of the data? */
  if (data_conversion[type] != XDATACONV_UNNECESSARY)
    {
      /* Yes. */
      data = scm_gc_malloc (num_data * datum_size[type], func);
      switch (type)
        {
        case XDATA_ARCS:
          for (i = 0; i < num_data; i++)
            {
              ((XArc *) data)[i].x      = *vdat++;
              ((XArc *) data)[i].y      = *vdat++;
              ((XArc *) data)[i].width  = *vdat++;
              ((XArc *) data)[i].height = *vdat++;
              ((XArc *) data)[i].angle1 = *vdat++;
              ((XArc *) data)[i].angle2 = *vdat++;
            }
          break;

        case XDATA_LINES:
        case XDATA_POINTS:
          for (i = 0; i < num_data; i++)
            {
              ((XPoint *) data)[i].x = *vdat++;
              ((XPoint *) data)[i].y = *vdat++;
            }
          break;

        case XDATA_SEGMENTS:
          for (i = 0; i < num_data; i++)
            {
              ((XSegment *) data)[i].x1 = *vdat++;
              ((XSegment *) data)[i].y1 = *vdat++;
              ((XSegment *) data)[i].x2 = *vdat++;
              ((XSegment *) data)[i].y2 = *vdat++;
            }
          break;

        case XDATA_RECTANGLES:
          for (i = 0; i < num_data; i++)
            {
              ((XRectangle *) data)[i].x      = *vdat++;
              ((XRectangle *) data)[i].y      = *vdat++;
              ((XRectangle *) data)[i].width  = *vdat++;
              ((XRectangle *) data)[i].height = *vdat++;
            }
          break;

        default:
          scm_misc_error (func,
                          "Internal X data type error (~S)",
                          scm_list_1 (scm_from_int (type)));
        }

      /* Now, if we haven't done so before, compare the copied data
         with the original.  If they are the same, we don't need to do
         this conversion again in the future. */
      if ((data_conversion[type] == XDATACONV_UNKNOWN) && (num_data >= 6))
        {
          vdat = (short *) scm_array_handle_uniform_elements(&handle);

          if (memcmp(vdat, data, num_data * datum_size[type]) == 0)
            {
              data_conversion[type] = XDATACONV_UNNECESSARY;
            }
          else
            {
              data_conversion[type] = XDATACONV_REQUIRED;
            }
        }
      
      /* Set up other return data. */
      *allocatedp = 1;
      *count = num_data;
    }
  else
    {
      /* No need to convert, just return data as is. */
      data = vdat;
      *allocatedp = 0;
      *count = num_data;
    }

  scm_array_handle_release(&handle);
  return data;
}

static SCM draw (SCM window, SCM gc, SCM data, int type, const char *func)
{
  xdisplay_t *dsp;
  xwindow_t *win;
  xgc_t *gc1;
  void *dat;
  int allocatedp;
  int num_data = 0;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, func));
  win = valid_win (window, SCM_ARG1, ~XWINDOW_STATE_DESTROYED, func);
  gc1 = valid_gc (gc, SCM_ARG2, ~XGC_STATE_FREED, func);
  dat = valid_data (data, SCM_ARG3, type, &allocatedp, &num_data, func);

  switch (type)
    {
    case XDATA_ARCS:
      XDrawArcs (dsp->dsp,
                 win->win,
                 gc1->gc,
                 (XArc *) dat,
                 num_data);
      break;

    case XDATA_LINES:
      XDrawLines (dsp->dsp,
                  win->win,
                  gc1->gc,
                  (XPoint *) dat,
                  num_data,
                  CoordModeOrigin);
      break;

    case XDATA_POINTS:
      XDrawPoints (dsp->dsp,
                   win->win,
                   gc1->gc,
                   (XPoint *) dat,
                   num_data,
                   CoordModeOrigin);
      break;

    case XDATA_SEGMENTS:
      XDrawSegments (dsp->dsp,
                     win->win,
                     gc1->gc,
                     (XSegment *) dat,
                     num_data);
      break;

    case XDATA_RECTANGLES:
      XDrawRectangles (dsp->dsp,
                       win->win,
                       gc1->gc,
                       (XRectangle *) dat,
                       num_data);
      break;

    default:
      scm_misc_error (func,
                      "Internal X data type error (~S)",
                      scm_list_1 (scm_from_int (type)));
    }

  /* Free the copy of the point data, if one was made. */
  if (allocatedp)
    scm_gc_free (dat, num_data * datum_size[type], func);


  return SCM_UNSPECIFIED;
}

SCM_DEFINE (scm_x_draw_arcs_x, "x-draw-arcs!", 3, 0, 0,
            (SCM window,
             SCM gc,
             SCM arcs),
            "Draws a set of arcs on the specified @var{window}\n"
            "using the specified graphics context @var{gc}.\n"
            "@var{arcs} should be a uniform array of shorts\n"
            "with dimensions N x 6, where N is the number of arcs.\n"
            "The 6 elements that specify each arc are, in order,\n"
            "X, Y, WIDTH, HEIGHT, ANGLE1 and ANGLE2.")
#define FUNC_NAME s_scm_x_draw_arcs_x
{
  return draw (window, gc, arcs, XDATA_ARCS, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_draw_lines_x, "x-draw-lines!", 3, 0, 0,
            (SCM window,
             SCM gc,
             SCM points),
            "Draws a set of lines on the specified @var{window}\n"
            "using the specified graphics context @var{gc}.\n"
            "@var{points} should be a uniform array of shorts\n"
            "with dimensions N x 2, where N is the number of points.")
#define FUNC_NAME s_scm_x_draw_lines_x
{
  return draw (window, gc, points, XDATA_LINES, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_draw_points_x, "x-draw-points!", 3, 0, 0,
            (SCM window,
             SCM gc,
             SCM points),
            "Draws a set of points on the specified @var{window}\n"
            "using the specified graphics context @var{gc}.\n"
            "@var{points} should be a uniform array of shorts\n"
            "with dimensions N x 2, where N is the number of points.")
#define FUNC_NAME s_scm_x_draw_points_x
{
  return draw (window, gc, points, XDATA_POINTS, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_draw_segments_x, "x-draw-segments!", 3, 0, 0,
            (SCM window,
             SCM gc,
             SCM segments),
            "Draws a set of line segments on the specified @var{window}\n"
            "using the specified graphics context @var{gc}.\n"
            "@var{segments} should be a uniform array of shorts\n"
            "with dimensions N x 4, where N is the number of segments.\n"
            "The 4 elements that specify each line segment are, in order,\n"
            "X1, Y1, X2, Y2.")
#define FUNC_NAME s_scm_x_draw_segments_x
{
  return draw (window, gc, segments, XDATA_SEGMENTS, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_draw_rectangles_x, "x-draw-rectangles!", 3, 0, 0,
            (SCM window,
             SCM gc,
             SCM rectangles),
            "Draws a set of rectangles on the specified @var{window}\n"
            "using the specified graphics context @var{gc}.\n"
            "@var{rectangles} should be a uniform array of shorts\n"
            "with dimensions N x 4, where N is the number of rectangles.\n"
            "The 4 elements that specify each rectangle are, in order,\n"
            "X1, Y1, WIDTH, HEIGHT.")
#define FUNC_NAME s_scm_x_draw_rectangles_x
{
  return draw (window, gc, rectangles, XDATA_RECTANGLES, FUNC_NAME);
}
#undef FUNC_NAME


/* EVENTS */

/* An X events is represented as a vector.  The vector always has the
   same length, regardless of event type, and event data with the same
   conceptual information has the same slot index for all event types
   to which it is relevant.  Slots that are irrelevant to the current
   event type are set to #f. */

/* XAnyEvent */
#define XEVENT_SLOT_TYPE            0
#define XEVENT_SLOT_SERIAL          1
#define XEVENT_SLOT_SEND_EVENT      2
#define XEVENT_SLOT_DISPLAY         3

/* XKeyEvent */
#define XEVENT_SLOT_WINDOW          4
#define XEVENT_SLOT_ROOT            5
#define XEVENT_SLOT_SUBWINDOW       6
#define XEVENT_SLOT_TIME            7
#define XEVENT_SLOT_X               8
#define XEVENT_SLOT_Y               9
#define XEVENT_SLOT_X_ROOT          10
#define XEVENT_SLOT_Y_ROOT          11
#define XEVENT_SLOT_STATE           12
#define XEVENT_SLOT_KEYCODE         13
#define XEVENT_SLOT_SAME_SCREEN     14

/* XButtonEvent */
#define XEVENT_SLOT_BUTTON          XEVENT_SLOT_KEYCODE

/* XMotionEvent */
#define XEVENT_SLOT_IS_HINT         XEVENT_SLOT_KEYCODE

/* XCrossingEvent */
#define XEVENT_SLOT_MODE            XEVENT_SLOT_KEYCODE
#define XEVENT_SLOT_DETAIL          15
#define XEVENT_SLOT_FOCUS           16

/* XKeymapEvent */
#define XEVENT_SLOT_KEY_VECTOR      XEVENT_SLOT_KEYCODE

/* XExposeEvent */
#define XEVENT_SLOT_WIDTH           XEVENT_SLOT_X_ROOT
#define XEVENT_SLOT_HEIGHT          XEVENT_SLOT_Y_ROOT
#define XEVENT_SLOT_COUNT           XEVENT_SLOT_KEYCODE

/* XGraphicsExposeEvent */
#define XEVENT_SLOT_MAJOR_CODE      XEVENT_SLOT_DETAIL
#define XEVENT_SLOT_MINOR_CODE      XEVENT_SLOT_FOCUS

/* XNoExposeEvent */
#define XEVENT_SLOT_DRAWABLE        XEVENT_SLOT_WINDOW

/* XCreateWindowEvent */
#define XEVENT_SLOT_PARENT          XEVENT_SLOT_ROOT
#define XEVENT_SLOT_BORDER_WIDTH    XEVENT_SLOT_FOCUS
#define XEVENT_SLOT_OVERRIDE_REDIRECT XEVENT_SLOT_DETAIL

/* XDestroyWindowEvent */
#define XEVENT_SLOT_EVENT           XEVENT_SLOT_TIME

/* XUnmapEvent */
#define XEVENT_SLOT_FROM_CONFIGURE  XEVENT_SLOT_KEYCODE

/* XConfigureEvent */
#define XEVENT_SLOT_ABOVE           XEVENT_SLOT_SUBWINDOW

/* XConfigureRequestEvent */
#define XEVENT_SLOT_VALUE_MASK      XEVENT_SLOT_SAME_SCREEN

/* XCirculateEvent */
#define XEVENT_SLOT_PLACE           XEVENT_SLOT_KEYCODE

/* XPropertyEvent */
#define XEVENT_SLOT_ATOM            XEVENT_SLOT_KEYCODE

/* XSelectionClearEvent */
#define XEVENT_SLOT_SELECTION       XEVENT_SLOT_KEYCODE

/* XSelectionRequestEvent */
#define XEVENT_SLOT_OWNER           XEVENT_SLOT_WINDOW
#define XEVENT_SLOT_REQUESTOR       XEVENT_SLOT_ROOT
#define XEVENT_SLOT_TARGET          XEVENT_SLOT_SUBWINDOW
#define XEVENT_SLOT_PROPERTY        XEVENT_SLOT_SAME_SCREEN

/* XColormapEvent */
#define XEVENT_SLOT_COLORMAP        XEVENT_SLOT_KEYCODE
#define XEVENT_SLOT_NEW             XEVENT_SLOT_DETAIL

/* XClientMessageEvent */
#define XEVENT_SLOT_MESSAGE_TYPE    XEVENT_SLOT_DETAIL
#define XEVENT_SLOT_DATA            XEVENT_SLOT_KEYCODE
#define XEVENT_SLOT_FORMAT          XEVENT_SLOT_FOCUS

/* XMappingEvent */
#define XEVENT_SLOT_REQUEST         XEVENT_SLOT_DETAIL
#define XEVENT_SLOT_FIRST_KEYCODE   XEVENT_SLOT_FOCUS

/* XErrorEvent */
#define XEVENT_SLOT_RESOURCEID      XEVENT_SLOT_WINDOW
#define XEVENT_SLOT_ERROR_CODE      XEVENT_SLOT_X
#define XEVENT_SLOT_REQUEST_CODE    XEVENT_SLOT_Y

/* Total number of slots. */
#define XEVENT_NUM_SLOTS            17

static SCM lookup_window (SCM display, XID id, const char *func)
{
  SCM window;

  if (id == None)
    return SCM_BOOL_F;

  window = scm_hashq_ref (resource_id_hash, scm_from_int (id), SCM_BOOL_F);

  /* If no existing entry, or if an old entry has been garbage
     collected... */
  if ((window == SCM_BOOL_F) || (SCM_TYP16 (window) != scm_tc16_xwindow))
    {
      xwindow_t *win = scm_gc_malloc (sizeof (xwindow_t), func);

      win->state = XWINDOW_STATE_THIRD_PARTY;
      win->dsp   = display;
      win->win   = id;

      SCM_NEWSMOB (window, scm_tc16_xwindow, win);

      scm_hashq_set_x (resource_id_hash, scm_from_int (id), window);
    }

  return window;
}

static SCM copy_event_fields (SCM display, XEvent *e, SCM event, const char *func)
{
  int i;

  /* Make a new vector if we need to. */
  if (SCM_UNBNDP (event))
    event = scm_make_vector (scm_from_int (XEVENT_NUM_SLOTS), SCM_UNSPECIFIED);

  /* Initial all slots to unspecified. */
  for (i = 0; i < XEVENT_NUM_SLOTS; i++)
    scm_c_vector_set_x(event, i, SCM_UNSPECIFIED);

  /* Fill in the slots that are relevant to the current X event. */
  switch (e->type)
    {
#define E e->xkey
    case KeyPress:
    case KeyRelease:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_ROOT,         lookup_window (display, E.root, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SUBWINDOW,    lookup_window (display, E.subwindow, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_X_ROOT,       scm_from_int (E.x_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y_ROOT,       scm_from_int (E.y_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      scm_c_vector_set_x(event, XEVENT_SLOT_KEYCODE,      scm_from_int (E.keycode));
      scm_c_vector_set_x(event, XEVENT_SLOT_SAME_SCREEN,  SCM_BOOL (E.same_screen));
      break;
#undef E

#define E e->xbutton
    case ButtonPress:
    case ButtonRelease:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_ROOT,         lookup_window (display, E.root, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SUBWINDOW,    lookup_window (display, E.subwindow, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_X_ROOT,       scm_from_int (E.x_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y_ROOT,       scm_from_int (E.y_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      scm_c_vector_set_x(event, XEVENT_SLOT_BUTTON,       scm_from_int (E.button));
      scm_c_vector_set_x(event, XEVENT_SLOT_SAME_SCREEN,  SCM_BOOL (E.same_screen));
      break;
#undef E

#define E e->xmotion
    case MotionNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_ROOT,         lookup_window (display, E.root, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SUBWINDOW,    lookup_window (display, E.subwindow, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_X_ROOT,       scm_from_int (E.x_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y_ROOT,       scm_from_int (E.y_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      scm_c_vector_set_x(event, XEVENT_SLOT_IS_HINT,      scm_from_int (E.is_hint));
      scm_c_vector_set_x(event, XEVENT_SLOT_SAME_SCREEN,  SCM_BOOL (E.same_screen));
      break;
#undef E

#define E e->xcrossing
    case EnterNotify:
    case LeaveNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_ROOT,         lookup_window (display, E.root, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SUBWINDOW,    lookup_window (display, E.subwindow, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_X_ROOT,       scm_from_int (E.x_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y_ROOT,       scm_from_int (E.y_root));
      scm_c_vector_set_x(event, XEVENT_SLOT_MODE,         scm_from_int (E.mode));
      scm_c_vector_set_x(event, XEVENT_SLOT_DETAIL,       scm_from_int (E.detail));
      scm_c_vector_set_x(event, XEVENT_SLOT_SAME_SCREEN,  SCM_BOOL (E.same_screen));
      scm_c_vector_set_x(event, XEVENT_SLOT_FOCUS,        SCM_BOOL (E.focus));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      break;
#undef E

#define E e->xfocus
    case FocusIn:
    case FocusOut:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_MODE,         scm_from_int (E.mode));
      scm_c_vector_set_x(event, XEVENT_SLOT_DETAIL,       scm_from_int (E.detail));
      break;
#undef E

#define E e->xkeymap
    case KeymapNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_KEY_VECTOR,   SCM_BOOL_F);
      break;
#undef E

#define E e->xexpose
    case Expose:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_WIDTH,        scm_from_int (E.width));
      scm_c_vector_set_x(event, XEVENT_SLOT_HEIGHT,       scm_from_int (E.height));
      scm_c_vector_set_x(event, XEVENT_SLOT_COUNT,        scm_from_int (E.count));
      break;
#undef E

#define E e->xgraphicsexpose
    case GraphicsExpose:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_DRAWABLE,     SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_WIDTH,        scm_from_int (E.width));
      scm_c_vector_set_x(event, XEVENT_SLOT_HEIGHT,       scm_from_int (E.height));
      scm_c_vector_set_x(event, XEVENT_SLOT_COUNT,        scm_from_int (E.count));
      scm_c_vector_set_x(event, XEVENT_SLOT_MAJOR_CODE,   scm_from_int (E.major_code));
      scm_c_vector_set_x(event, XEVENT_SLOT_MINOR_CODE,   scm_from_int (E.minor_code));
      break;
#undef E

#define E e->xnoexpose
    case NoExpose:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_DRAWABLE,     SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_MAJOR_CODE,   scm_from_int (E.major_code));
      scm_c_vector_set_x(event, XEVENT_SLOT_MINOR_CODE,   scm_from_int (E.minor_code));
      break;
#undef E

#define E e->xvisibility
    case VisibilityNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      break;
#undef E

#define E e->xcreatewindow
    case CreateNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_PARENT,       lookup_window (display, E.parent, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_WIDTH,        scm_from_int (E.width));
      scm_c_vector_set_x(event, XEVENT_SLOT_HEIGHT,       scm_from_int (E.height));
      scm_c_vector_set_x(event, XEVENT_SLOT_BORDER_WIDTH, scm_from_int (E.border_width));
      scm_c_vector_set_x(event, XEVENT_SLOT_OVERRIDE_REDIRECT, SCM_BOOL (E.override_redirect));
      break;
#undef E

#define E e->xdestroywindow
    case DestroyNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      break;
#undef E

#define E e->xunmap
    case UnmapNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_FROM_CONFIGURE, SCM_BOOL (E.from_configure));
      break;
#undef E

#define E e->xmap
    case MapNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_OVERRIDE_REDIRECT, SCM_BOOL (E.override_redirect));
      break;
#undef E

#define E e->xmaprequest
    case MapRequest:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_PARENT,       lookup_window (display, E.parent, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      break;
#undef E

#define E e->xreparent
    case ReparentNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_PARENT,       lookup_window (display, E.parent, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_OVERRIDE_REDIRECT, SCM_BOOL (E.override_redirect));
      break;
#undef E

#define E e->xconfigure
    case ConfigureNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_WIDTH,        scm_from_int (E.width));
      scm_c_vector_set_x(event, XEVENT_SLOT_HEIGHT,       scm_from_int (E.height));
      scm_c_vector_set_x(event, XEVENT_SLOT_BORDER_WIDTH, scm_from_int (E.border_width));
      scm_c_vector_set_x(event, XEVENT_SLOT_ABOVE,        lookup_window (display, E.above, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_OVERRIDE_REDIRECT, SCM_BOOL (E.override_redirect));
      break;
#undef E

#define E e->xconfigurerequest
    case ConfigureRequest:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_PARENT,       lookup_window (display, E.parent, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      scm_c_vector_set_x(event, XEVENT_SLOT_WIDTH,        scm_from_int (E.width));
      scm_c_vector_set_x(event, XEVENT_SLOT_HEIGHT,       scm_from_int (E.height));
      scm_c_vector_set_x(event, XEVENT_SLOT_BORDER_WIDTH, scm_from_int (E.border_width));
      scm_c_vector_set_x(event, XEVENT_SLOT_ABOVE,        lookup_window (display, E.above, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_DETAIL,       scm_from_int (E.detail));
      scm_c_vector_set_x(event, XEVENT_SLOT_VALUE_MASK,   scm_from_int (E.value_mask));
      break;
#undef E

#define E e->xgravity
    case GravityNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_X,            scm_from_int (E.x));
      scm_c_vector_set_x(event, XEVENT_SLOT_Y,            scm_from_int (E.y));
      break;
#undef E

#define E e->xresizerequest
    case ResizeRequest:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WIDTH,        scm_from_int (E.width));
      scm_c_vector_set_x(event, XEVENT_SLOT_HEIGHT,       scm_from_int (E.height));
      break;
#undef E

#define E e->xcirculate
    case CirculateNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_EVENT,        lookup_window (display, E.event, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_PLACE,        scm_from_int (E.place));
      break;
#undef E

#define E e->xcirculaterequest
    case CirculateRequest:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_PARENT,       lookup_window (display, E.parent, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_PLACE,        scm_from_int (E.place));
      break;
#undef E

#define E e->xproperty
    case PropertyNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_ATOM,         SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      break;
#undef E

#define E e->xselectionclear
    case SelectionClear:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SELECTION,    SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      break;
#undef E

#define E e->xselectionrequest
    case SelectionRequest:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_OWNER,        lookup_window (display, E.owner, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_REQUESTOR,    lookup_window (display, E.requestor, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SELECTION,    SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_TARGET,       SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_PROPERTY,     SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      break;
#undef E

#define E e->xselection
    case SelectionNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_REQUESTOR,    lookup_window (display, E.requestor, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_SELECTION,    SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_TARGET,       SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_PROPERTY,     SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_TIME,         scm_from_int (E.time));
      break;
#undef E

#define E e->xcolormap
    case ColormapNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_COLORMAP,     SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_NEW,          SCM_BOOL (E.new));
      scm_c_vector_set_x(event, XEVENT_SLOT_STATE,        scm_from_int (E.state));
      break;
#undef E

#define E e->xclient
    case ClientMessage:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_MESSAGE_TYPE, SCM_BOOL_F);
      scm_c_vector_set_x(event, XEVENT_SLOT_FORMAT,       scm_from_int (E.format));
      scm_c_vector_set_x(event, XEVENT_SLOT_DATA,         SCM_BOOL_F);
      break;
#undef E

#define E e->xmapping
    case MappingNotify:
      scm_c_vector_set_x(event, XEVENT_SLOT_TYPE,         scm_from_int (E.type));
      scm_c_vector_set_x(event, XEVENT_SLOT_SERIAL,       scm_from_int (E.serial));
      scm_c_vector_set_x(event, XEVENT_SLOT_SEND_EVENT,   SCM_BOOL (E.send_event));
      scm_c_vector_set_x(event, XEVENT_SLOT_DISPLAY,      display);
      scm_c_vector_set_x(event, XEVENT_SLOT_WINDOW,       lookup_window (display, E.window, func));
      scm_c_vector_set_x(event, XEVENT_SLOT_REQUEST,      scm_from_int (E.request));
      scm_c_vector_set_x(event, XEVENT_SLOT_FIRST_KEYCODE, scm_from_int (E.first_keycode));
      scm_c_vector_set_x(event, XEVENT_SLOT_COUNT,        scm_from_int (E.count));
      break;
#undef E
    }

  return event;
}

static void validate_event_arg (SCM event, int pos, const char *func)
{
  if (!SCM_UNBNDP (event))
    {
      SCM_ASSERT (scm_is_vector (event), event, pos, func);
      SCM_ASSERT (scm_c_vector_length (event) == XEVENT_NUM_SLOTS, event, pos, func);
    }
}

SCM_DEFINE (scm_x_check_mask_event_x, "x-check-mask-event!", 2, 1, 0,
            (SCM display,
             SCM mask,
             SCM event),
            "See XCheckMaskEvent.")
#define FUNC_NAME s_scm_x_check_mask_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  XEvent e;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  SCM_ASSERT (scm_integer_p (mask), mask, SCM_ARG2, FUNC_NAME);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  if (XCheckMaskEvent (dsp->dsp, scm_to_int (mask), &e))
    event = copy_event_fields (display1, &e, event, FUNC_NAME);
  else
    event = SCM_BOOL_F;

  return event;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_check_typed_event_x, "x-check-typed-event!", 2, 1, 0,
            (SCM display,
             SCM type,
             SCM event),
            "See XCheckTypedEvent.")
#define FUNC_NAME s_scm_x_check_typed_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  XEvent e;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  SCM_ASSERT (scm_integer_p (type), type, SCM_ARG2, FUNC_NAME);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  if (XCheckTypedEvent (dsp->dsp, scm_to_int (type), &e))
    event = copy_event_fields (display1, &e, event, FUNC_NAME);
  else
    event = SCM_BOOL_F;

  return event;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_check_typed_window_event_x, "x-check-typed-window-event!", 2, 1, 0,
            (SCM window,
             SCM type,
             SCM event),
            "See XCheckTypedWindowEvent.")
#define FUNC_NAME s_scm_x_check_typed_window_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  xwindow_t *win;
  XEvent e;

  display1 = valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  win = valid_win (window, SCM_ARG1, ~XWINDOW_STATE_DESTROYED, FUNC_NAME);
  SCM_ASSERT (scm_integer_p (type), type, SCM_ARG2, FUNC_NAME);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  if (XCheckTypedWindowEvent (dsp->dsp, win->win, scm_to_int (type), &e))
    event = copy_event_fields (display1, &e, event, FUNC_NAME);
  else
    event = SCM_BOOL_F;

  return event;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_check_window_event_x, "x-check-window-event!", 2, 1, 0,
            (SCM window,
             SCM mask,
             SCM event),
            "See XCheckWindowEvent.")
#define FUNC_NAME s_scm_x_check_window_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  xwindow_t *win;
  XEvent e;

  display1 = valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  win = valid_win (window, SCM_ARG1, ~XWINDOW_STATE_DESTROYED, FUNC_NAME);
  SCM_ASSERT (scm_integer_p (mask), mask, SCM_ARG2, FUNC_NAME);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  if (XCheckWindowEvent (dsp->dsp, win->win, scm_to_int (mask), &e))
    event = copy_event_fields (display1, &e, event, FUNC_NAME);
  else
    event = SCM_BOOL_F;

  return event;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_events_queued_x, "x-events-queued!", 1, 1, 0,
            (SCM display,
             SCM mode),
            "See XEventsQueued.")
#define FUNC_NAME s_scm_x_events_queued_x
{
  xdisplay_t *dsp;
  int cmode;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  if (!SCM_UNBNDP(mode))
    {
      SCM_ASSERT (scm_integer_p (mode), mode, SCM_ARG2, FUNC_NAME);
      cmode = scm_to_int (mode);
      SCM_ASSERT_RANGE (SCM_ARG2,
                        mode,
                        (cmode == QueuedAlready) ||
                        (cmode == QueuedAfterReading) ||
                        (cmode == QueuedAfterFlush));
    }
  else
    cmode = QueuedAlready;

  return scm_from_int (XEventsQueued (dsp->dsp, cmode));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_pending_x, "x-pending!", 1, 0, 0,
            (SCM display),
            "See XPending.")
#define FUNC_NAME s_scm_x_pending_x
{
  xdisplay_t *dsp;

  dsp = XDISPLAY (valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));

  return scm_from_int (XPending (dsp->dsp));
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_mask_event_x, "x-mask-event!", 2, 1, 0,
            (SCM display,
             SCM mask,
             SCM event),
            "See XMaskEvent.")
#define FUNC_NAME s_scm_x_mask_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  XEvent e;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  SCM_ASSERT (scm_integer_p (mask), mask, SCM_ARG2, FUNC_NAME);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  XMaskEvent (dsp->dsp, scm_to_int (mask), &e);

  return copy_event_fields (display1, &e, event, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_next_event_x, "x-next-event!", 1, 1, 0,
            (SCM display,
             SCM event),
            "See XNextEvent.")
#define FUNC_NAME s_scm_x_next_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  XEvent e;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  XNextEvent (dsp->dsp, &e);

  return copy_event_fields (display1, &e, event, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_peek_event_x, "x-peek-event!", 1, 1, 0,
            (SCM display,
             SCM event),
            "See XPeekEvent.")
#define FUNC_NAME s_scm_x_peek_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  XEvent e;

  display1 = valid_dsp (display, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  XPeekEvent (dsp->dsp, &e);

  return copy_event_fields (display1, &e, event, FUNC_NAME);
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_select_input_x, "x-select-input!", 1, 1, 0,
            (SCM window,
             SCM mask),
            "See XSelectInput.")
#define FUNC_NAME s_scm_x_select_input_x
{
  xdisplay_t *dsp;
  xwindow_t *win;

  dsp = XDISPLAY (valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME));
  win = valid_win (window, SCM_ARG1, ~XWINDOW_STATE_DESTROYED, FUNC_NAME);
  SCM_VALIDATE_NUMBER (SCM_ARG2, mask);

  XSelectInput (dsp->dsp, win->win, scm_to_int (mask));

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_x_window_event_x, "x-window-event!", 2, 1, 0,
            (SCM window,
             SCM mask,
             SCM event),
            "See XWindowEvent.")
#define FUNC_NAME s_scm_x_window_event_x
{
  SCM display1;
  xdisplay_t *dsp;
  xwindow_t *win;
  XEvent e;

  display1 = valid_dsp (window, SCM_ARG1, XDISPLAY_STATE_OPEN, FUNC_NAME);
  dsp = XDISPLAY (display1);
  win = valid_win (window, SCM_ARG1, ~XWINDOW_STATE_DESTROYED, FUNC_NAME);
  SCM_VALIDATE_NUMBER (SCM_ARG2, mask);
  validate_event_arg (event, SCM_ARG3, FUNC_NAME);

  XWindowEvent (dsp->dsp, win->win, scm_to_int (mask), &e);

  return copy_event_fields (display1, &e, event, FUNC_NAME);
}
#undef FUNC_NAME


/* INITIALIZATION */

void
init_xlib_core ()
{
  scm_tc16_xdisplay = scm_make_smob_type ("x-display", sizeof (xdisplay_t));
  scm_set_smob_free (scm_tc16_xdisplay, xdisplay_free);
  scm_set_smob_mark (scm_tc16_xdisplay, xdisplay_mark);
  scm_set_smob_print (scm_tc16_xdisplay, xdisplay_print);

  scm_tc16_xscreen = scm_make_smob_type ("x-screen", sizeof (xscreen_t));
  scm_set_smob_mark (scm_tc16_xscreen, xscreen_mark);

  scm_tc16_xwindow = scm_make_smob_type ("x-window", sizeof (xwindow_t));
  scm_set_smob_free (scm_tc16_xwindow, xwindow_free);
  scm_set_smob_mark (scm_tc16_xwindow, xwindow_mark);
  scm_set_smob_print (scm_tc16_xwindow, xwindow_print);

  scm_tc16_xgc = scm_make_smob_type ("x-gc", sizeof (xgc_t));
  scm_set_smob_free (scm_tc16_xgc, xgc_free);
  scm_set_smob_mark (scm_tc16_xgc, xgc_mark);
  scm_set_smob_print (scm_tc16_xgc, xgc_print);

  /* A weak value hash table mapping known X resource IDs to
     corresponding smob instances.  This allows us to present the
     resource IDs in, e.g., X event data in a form that is useful on
     the Scheme level, in two senses: firstly, any two Scheme level
     values that refer to the same X resource will be eq?; secondly,
     any `third-party' resource IDs (e.g. that of the root window) are
     presented as values that can be passed to other procedures, to
     the extent that the interface permits the use of such third-party
     resources. */
  resource_id_hash =
    scm_gc_protect_object (scm_make_weak_value_hash_table (scm_from_int (19)));

#include "xlib.x"
}

/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
