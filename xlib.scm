
(define-module (xlib xlib))

(dynamic-call "init_xlib_core" (dynamic-link "libguilexlib"))

(export x-open-display!
	x-close-display!
	x-no-op!
	x-flush!
	x-connection-number
	x-screen-count
	x-default-screen
	x-q-length
	x-server-vendor
	x-protocol-version
	x-protocol-revision
	x-vendor-release
	x-display-string
	x-bitmap-unit
	x-bitmap-bit-order
	x-bitmap-pad
	x-image-byte-order
	x-next-request
	x-last-known-request-processed
	x-display-of
	x-all-planes
	x-root-window
	x-black-pixel
	x-white-pixel
	x-display-width
	x-display-height
	x-display-width-mm
	x-display-height-mm
	x-display-planes
	x-display-cells
	x-screen-of-display
	x-screen-number-of-screen
	x-min-colormaps
	x-max-colormaps
	x-create-window!
	x-map-window!
	x-unmap-window!
	x-destroy-window!
	x-clear-window!
	x-clear-area!
	x-create-pixmap!
	x-copy-area!
	x-default-gc
	x-free-gc!
	x-create-gc!
	x-change-gc!
	x-set-dashes!
	x-set-clip-rectangles!
	x-copy-gc!
	x-draw-arcs!
	x-draw-lines!
	x-draw-points!
	x-draw-segments!
	x-draw-rectangles!
	x-check-mask-event!
	x-check-typed-event!
	x-check-typed-window-event!
	x-check-window-event!
	x-events-queued!
	x-pending!
	x-mask-event!
	x-next-event!
	x-peek-event!
	x-select-input!
	x-window-event!)

;;; {General}

;;; The universal null resource or null atom.

(define-public None                            0)

;;; Bit and byte ordering values.

(define-public LSBFirst                        0)
(define-public MSBFirst                        1)

;;; Two names for the same thing, even in C Xlib.

(define-public x-default-depth                 x-display-cells)

;;; guile-xlib implements a class of display- and screen-related
;;; procedures to accept a mixture of display, screen number and
;;; screen object arguments, such that the following procedures are
;;; just aliases for each other.

(define-public x-width-of-screen               x-display-width)
(define-public x-height-of-screen              x-display-height)
(define-public x-width-mm-of-screen            x-display-width-mm)
(define-public x-height-mm-of-screen           x-display-height-mm)
(define-public x-planes-of-screen              x-display-planes)
(define-public x-cells-of-screen               x-display-cells)
(define-public x-min-cmaps-of-screen           x-min-colormaps)
(define-public x-max-cmaps-of-screen           x-max-colormaps)


;;; {Events}

;;; Values for the optional mode argument to x-pending.

(define-public QueuedAlready                   0)
(define-public QueuedAfterReading              1)
(define-public QueuedAfterFlush                2)

;;; Event mask values for x-select-input! and related procedures.

(define-public NoEventMask			0)
(define-public KeyPressMask			(expt 2 0))
(define-public KeyReleaseMask			(expt 2 1))
(define-public ButtonPressMask			(expt 2 2))
(define-public ButtonReleaseMask		(expt 2 3))
(define-public EnterWindowMask			(expt 2 4))
(define-public LeaveWindowMask			(expt 2 5))
(define-public PointerMotionMask		(expt 2 6))
(define-public PointerMotionHintMask		(expt 2 7))
(define-public Button1MotionMask		(expt 2 8))
(define-public Button2MotionMask		(expt 2 9))
(define-public Button3MotionMask		(expt 2 10))
(define-public Button4MotionMask		(expt 2 11))
(define-public Button5MotionMask		(expt 2 12))
(define-public ButtonMotionMask	        	(expt 2 13))
(define-public KeymapStateMask			(expt 2 14))
(define-public ExposureMask			(expt 2 15))
(define-public VisibilityChangeMask		(expt 2 16))
(define-public StructureNotifyMask		(expt 2 17))
(define-public ResizeRedirectMask		(expt 2 18))
(define-public SubstructureNotifyMask		(expt 2 19))
(define-public SubstructureRedirectMask	        (expt 2 20))
(define-public FocusChangeMask			(expt 2 21))
(define-public PropertyChangeMask		(expt 2 22))
(define-public ColormapChangeMask		(expt 2 23))
(define-public OwnerGrabButtonMask		(expt 2 24))

;;; Event type values.

(define-public KeyPress		                2)
(define-public KeyRelease		        3)
(define-public ButtonPress		        4)
(define-public ButtonRelease		        5)
(define-public MotionNotify		        6)
(define-public EnterNotify		        7)
(define-public LeaveNotify		        8)
(define-public FocusIn			        9)
(define-public FocusOut		                10)
(define-public KeymapNotify		        11)
(define-public Expose			        12)
(define-public GraphicsExpose		        13)
(define-public NoExpose		                14)
(define-public VisibilityNotify	                15)
(define-public CreateNotify		        16)
(define-public DestroyNotify		        17)
(define-public UnmapNotify		        18)
(define-public MapNotify		        19)
(define-public MapRequest		        20)
(define-public ReparentNotify		        21)
(define-public ConfigureNotify		        22)
(define-public ConfigureRequest	                23)
(define-public GravityNotify		        24)
(define-public ResizeRequest		        25)
(define-public CirculateNotify		        26)
(define-public CirculateRequest	                27)
(define-public PropertyNotify		        28)
(define-public SelectionClear		        29)
(define-public SelectionRequest	                30)
(define-public SelectionNotify		        31)
(define-public ColormapNotify		        32)
(define-public ClientMessage		        33)
(define-public MappingNotify		        34)

(define-public LASTEvent		        35)	; must be bigger than any event #

;;; Named accessors for the slots in guile-xlib's vector
;;; representation of an X event.

(define (x-event:slot-ref n)
  (lambda (event)
    (vector-ref event n)))

(define-public x-event:type                    (x-event:slot-ref 0))
(define-public x-event:serial                  (x-event:slot-ref 1))
(define-public x-event:send-event              (x-event:slot-ref 2))
(define-public x-event:display                 (x-event:slot-ref 3))
(define-public x-event:window                  (x-event:slot-ref 4))
(define-public x-event:root                    (x-event:slot-ref 5))
(define-public x-event:subwindow               (x-event:slot-ref 6))
(define-public x-event:time                    (x-event:slot-ref 7))
(define-public x-event:x                       (x-event:slot-ref 8))
(define-public x-event:y                       (x-event:slot-ref 9))
(define-public x-event:x-root                  (x-event:slot-ref 10))
(define-public x-event:y-root                  (x-event:slot-ref 11))
(define-public x-event:state                   (x-event:slot-ref 12))
(define-public x-event:keycode                 (x-event:slot-ref 13))
(define-public x-event:same-screen             (x-event:slot-ref 14))
(define-public x-event:detail                  (x-event:slot-ref 15))
(define-public x-event:focus                   (x-event:slot-ref 16))

(define-public x-event:button                  x-event:keycode)
(define-public x-event:is-hint                 x-event:keycode)
(define-public x-event:mode                    x-event:keycode)
(define-public x-event:key-vector              x-event:keycode)
(define-public x-event:width                   x-event:x-root)
(define-public x-event:height                  x-event:y-root)
(define-public x-event:count                   x-event:keycode)
(define-public x-event:major-code              x-event:detail)
(define-public x-event:minor-code              x-event:focus)
(define-public x-event:drawable                x-event:window)
(define-public x-event:parent                  x-event:root)
(define-public x-event:border-width            x-event:focus)
(define-public x-event:override-redirect       x-event:detail)
(define-public x-event:event                   x-event:time)
(define-public x-event:from-configure          x-event:keycode)
(define-public x-event:above                   x-event:subwindow)
(define-public x-event:value-mask              x-event:same-screen)
(define-public x-event:place                   x-event:keycode)
(define-public x-event:atom                    x-event:keycode)
(define-public x-event:selection               x-event:keycode)
(define-public x-event:owner                   x-event:window)
(define-public x-event:requestor               x-event:root)
(define-public x-event:target                  x-event:subwindow)
(define-public x-event:property                x-event:same-screen)
(define-public x-event:colormap                x-event:keycode)
(define-public x-event:new                     x-event:detail)
(define-public x-event:message-type            x-event:detail)
(define-public x-event:data                    x-event:keycode)
(define-public x-event:format                  x-event:focus)
(define-public x-event:request                 x-event:detail)
(define-public x-event:first-keycode           x-event:focus)
(define-public x-event:resourceid              x-event:window)
(define-public x-event:error-code              x-event:x)
(define-public x-event:request-code            x-event:y)


;;; {Graphics Contexts}

;;; GC field numbers.  Note that the following constants differ from C
;;; Xlib: they are bit numbers rather than bit masks.  The
;;; corresponding bit mask is calculated by x-create-gc!, x-change-gc!
;;; and x-copy-gc!.

(define-public GCFunction                       0)
(define-public GCPlaneMask                      1)
(define-public GCForeground                     2)
(define-public GCBackground                     3)
(define-public GCLineWidth                      4)
(define-public GCLineStyle                      5)
(define-public GCCapStyle                       6)
(define-public GCJoinStyle		        7)
(define-public GCFillStyle		        8)
(define-public GCFillRule		        9)
(define-public GCTile			        10)
(define-public GCStipple		        11)
(define-public GCTileStipXOrigin	        12)
(define-public GCTileStipYOrigin	        13)
(define-public GCFont 			        14)
(define-public GCSubwindowMode		        15)
(define-public GCGraphicsExposures              16)
(define-public GCClipXOrigin		        17)
(define-public GCClipYOrigin		        18)
(define-public GCClipMask		        19)
(define-public GCDashOffset		        20)
(define-public GCDashList		        21)
(define-public GCArcMode		        22)

;;; GC function values.

(define-public GXclear		                0)
(define-public GXand		                1)
(define-public GXandReverse	                2)
(define-public GXcopy		                3)
(define-public GXandInverted	                4)
(define-public GXnoop		                5)
(define-public GXxor		                6)
(define-public GXor		                7)
(define-public GXnor		                8)
(define-public GXequiv		                9)
(define-public GXinvert	                        10)
(define-public GXorReverse	                11)
(define-public GXcopyInverted	                12)
(define-public GXorInverted	                13)
(define-public GXnand		                14)
(define-public GXset		                15)

;;; GC line style values.

(define-public LineSolid	                0)
(define-public LineOnOffDash	                1)
(define-public LineDoubleDash	                2)

;;; GC cap style values.

(define-public CapNotLast	                0)
(define-public CapButt		                1)
(define-public CapRound	                        2)
(define-public CapProjecting	                3)

;;; GC join style values.

(define-public JoinMiter	                0)
(define-public JoinRound	                1)
(define-public JoinBevel	                2)

;;; GC fill style values.

(define-public FillSolid	                0)
(define-public FillTiled	                1)
(define-public FillStippled	                2)
(define-public FillOpaqueStippled	        3)

;;; GC fill rule values.

(define-public EvenOddRule	                0)
(define-public WindingRule	                1)

;;; GC subwindow mode values.

(define-public ClipByChildren	                0)
(define-public IncludeInferiors                 1)

;;; GC arc mode values.

(define-public ArcChord                         0)
(define-public ArcPieSlice                      1)

;;; GC set clip rectangles ordering values.

(define-public Unsorted	                        0)
(define-public YSorted		                1)
(define-public YXSorted	                        2)
(define-public YXBanded	                        3)

;;; Convenience procedures to set a single GC field.

(define (x-gc-setter field)
  (lambda (gc value)
    (x-change-gc! gc field value)))

(define-public x-set-function!                 (x-gc-setter GCFunction))
(define-public x-set-plane-mask!               (x-gc-setter GCPlaneMask))
(define-public x-set-foreground!               (x-gc-setter GCForeground))
(define-public x-set-background!               (x-gc-setter GCBackground))
(define-public x-set-line-width!               (x-gc-setter GCLineWidth))
(define-public x-set-line-style!               (x-gc-setter GCLineStyle))
(define-public x-set-cap-style!                (x-gc-setter GCCapStyle))
(define-public x-set-join-style!               (x-gc-setter GCJoinStyle))
(define-public x-set-fill-style!               (x-gc-setter GCFillStyle))
(define-public x-set-fill-rule!                (x-gc-setter GCFillRule))
(define-public x-set-tile!                     (x-gc-setter GCTile))
(define-public x-set-stipple!                  (x-gc-setter GCStipple))
(define-public x-set-ts-x-origin!              (x-gc-setter GCTileStipXOrigin))
(define-public x-set-ts-y-origin!              (x-gc-setter GCTileStipYOrigin))
(define-public x-set-font!                     (x-gc-setter GCFont))
(define-public x-set-subwindow-mode!           (x-gc-setter GCSubwindowMode))
(define-public x-set-graphics-exposures!       (x-gc-setter GCGraphicsExposures))
(define-public x-set-clip-x-origin!            (x-gc-setter GCClipXOrigin))
(define-public x-set-clip-y-origin!            (x-gc-setter GCClipYOrigin))
(define-public x-set-clip-mask!                (x-gc-setter GCClipMask))
(define-public x-set-dash-offset!              (x-gc-setter GCDashOffset))
(define-public x-set-dash-list!                (x-gc-setter GCDashList))
(define-public x-set-arc-mode!                 (x-gc-setter GCArcMode))

;;; Convenience procedures to set useful combinations of GC fields.

(define-public (x-set-clip-origin! gc x y)
  (x-change-gc! gc
                GCClipXOrigin x
                GCClipYOrigin y))

(define-public (x-set-line-attributes! gc width style cap-style join-style)
  (x-change-gc! gc
                GCLineWidth width
                GCLineStyle style
                GCCapStyle cap-style
                GCJoinStyle join-style))

(define-public (x-set-ts-origin! gc x y)
  (x-change-gc! gc
                GCTileStipXOrigin x
                GCTileStipYOrigin y))

(define-public (x-set-state! gc foreground background function plane-mask)
  (x-change-gc! gc
                GCForeground foreground
                GCBackground background
                GCFunction function
                GCPlaneMask plane-mask))


;;; {Drawing}

;;; guile-xlib has primitives for the multiple arc/line/etc. versions
;;; of the following.  Here we define-public the single
;;; arc/line/etc. procedures in terms of those primitives.

(define-public (x-draw-arc! window gc x y width height angle1 angle2)
  (let ((arcs (make-typed-array 's16 0 6)))
    (array-set! arcs x      0 0)
    (array-set! arcs y      0 1)
    (array-set! arcs width  0 2)
    (array-set! arcs height 0 3)
    (array-set! arcs angle1 0 4)
    (array-set! arcs angle2 0 5)
    (x-draw-arcs! window gc arcs)))

(define-public (x-draw-line! window gc x1 y1 x2 y2)
  (let ((pts (make-typed-array 's16 0 2 2)))
    (array-set! pts x1 0 0)
    (array-set! pts y1 0 1)
    (array-set! pts x2 1 0)
    (array-set! pts y2 1 1)
    (x-draw-lines! window gc pts)))

(define-public (x-draw-point! window gc x y)
  (let ((pts (make-typed-array 's16 0 1 2)))
    (array-set! pts x 0 0)
    (array-set! pts y 0 1)
    (x-draw-points! window gc pts)))

(define-public x-draw-segment! x-draw-line!)

(define-public (x-draw-rectangle! window gc x y width height)
  (let ((rectangles (make-typed-array 's16 0 1 4)))
    (array-set! rectangles x      0 0)
    (array-set! rectangles y      0 1)
    (array-set! rectangles width  0 2)
    (array-set! rectangles height 0 3)
    (x-draw-rectangles! window gc rectangles)))


;;; {Event Loop}

;;; A few definitions to set up and test a basic X event loop.

;; On @var{display}, run @var{hook} w/ event @var{event}, looping until
;; `x-event-loop-quit' is caught, at which point return the thrown args.
;; See also `x-event-loop-quit!'.
;;
(define-public (x-event-loop! display hook . event)
  (catch 'x-event-loop-quit
         (lambda ()
           (let loop ((e (apply x-next-event! display event)))
             (run-hook hook e)
             (loop (x-next-event! display e))))
         (lambda (key . values)
           values)))

;; Throw `x-event-loop-quit' with zero or more @var{values}.
;; See also `x-event-loop!'.
;;
(define-public (x-event-loop-quit! . values)
  (apply throw 'x-event-loop-quit values))

(define-public (x-print-event! event)
  (display event)
  (newline))

(define-public (x-button-quit! event)
  (if (= (x-event:type event) ButtonPress)
      (begin
        (x-print-event! event)
        (x-event-loop-quit! (x-event:button event)))))

;; Test the guile-xlib event loop on display @var{d}.
;; This procedure will probably go away at some point.
;;
(define-public (test-event-loop d)
  (let ((w (x-create-window! d))
        (h (make-hook 1)))
    (add-hook! h x-print-event!)
    (add-hook! h x-button-quit!)
    (dynamic-wind
        (lambda ()
          (x-select-input! w (logior ButtonPressMask ExposureMask KeyPressMask))
          (x-map-window! w))
        (lambda ()
          (x-event-loop! w h))
        (lambda ()
          (x-destroy-window! w)))))
