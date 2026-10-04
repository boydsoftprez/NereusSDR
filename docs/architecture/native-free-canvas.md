# Native Free Canvas

Container Settings offers **Canvas** for individual placement and resizing,
**Vertical stack** for automatic rows, and **Saved legacy canvas** for existing
imported normalized compositions. The saved numeric modes remain legacy=0 and
stack=1; editable Canvas adds mode2.

The first explicit switch from Stack to Canvas starts from the actual separated
preview rows when no free positions have been saved. Switching away and back
reuses saved positions. **Start from stacked layout** explicitly replaces free
positions for supported visible objects using their current preferred row heights.
**Use saved legacy positions** explicitly converts the retained normalized
rectangles against the displayed preview extent. Both actions affect the draft;
Apply commits them, and Cancel restores the last applied document. Hidden,
unsupported and opaque records keep their stored data. Imported legacy loads and
ordinary Apply never infer that full-surface or overlapping positions are disposable.

Each object has an external top-left grip and a bottom-right resize handle, shown
on hover, focus or selection. Arrow keys move or resize the focused handle by one
logical pixel; Shift changes the step to ten. Escape cancels the current gesture.
The contents list still selects objects and offers arrangement/recovery actions.
Numeric X/Y/Width/Height fields edit only the changed double axis, and Layer is an
explicit paint order. Selecting an object does not change its paint order. Lock
blocks placement, size, layer and reseeding while retaining the unlock control.

Free positions are independent device-independent pixel doubles in the per-entry
`freeCanvasRect` extension (`[x,y,width,height]`). The container's
`freeCanvasExtent` extension retains the initial logical scene extent. Neither
rewrites `canvasRect`, raw `legacyRecord`, properties/overrides, remembered homes
or opaque extension values. QWidget integer projection is presentation only;
untouched doubles never round-trip through the widgets. Negative, overlapping,
zero-size and high-precision saved rectangles are retained. Native face/applet
minimum sizes constrain newly edited sizes rather than sanitizing saved imports.

Objects retain their viewport sizes when the container shrinks. The viewport
scrolls horizontally and vertically to expose the scene. Free AutoHeight uses the
scene extent and caps floating/overlay viewport growth to the available screen;
Panel sizing stays under the shell's existing rules. Canvas object size changes
reach the actual native meter/applet viewport. The separate all-family proportional
font work is not part of this implementation.

Free Canvas hosts one native QWidget leaf per meter entry with its own effective
source context, and borrows each live singleton applet without creating another
model or subscription. Preview applets remain inert tiles/snapshots; preview meter
controls remain inert and consume cached readings without registering new poll
work. Geometry changes update leaves in place and preserve histories, polling
targets and singleton pointers. Live gesture completion uses the revisioned
workspace transaction; storage or concurrent placement failure restores committed
geometry. Legacy Canvas retains its original one-surface primitive compositing.

The initial native composition and interaction evidence uses offline Cocoa/Metal
fixtures with real Nereus faces. Physical pointer/monitor and other-platform
acceptance are separate from Qt-delivered native event tests.
