/** @file */
#ifndef __SMALLWM_XDATA__
#define __SMALLWM_XDATA__

#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "common.hpp"
#include "logging/logging.hpp"

/**
 * An X graphics context which is used to draw on windows.
 */
class XGC
{
public:
XGC(Display *dpy, Window window) :
    m_display(dpy), m_window(window) {
    m_gc = XCreateGC(dpy, window, 0, NULL);
};

~XGC() {
    XFree(m_gc);
};

void clear();
void draw_string(Dimension, Dimension, const std::string&);
Dimension2D copy_pixmap(Drawable, Dimension, Dimension);

private:
/** The raw X display - this is necessary to have since XData doesn't
 * expose it. */
Display *m_display;


/// The window this graphics context belongs to
Window m_window;

/// The X graphics context this sits above
GC m_gc;
};

/**
 * Identifies the colors which can be used for window borders and the like.
 */
enum MonoColor {
    X_BLACK,
    X_WHITE,
};

/**
 * This forms a layer above raw Xlib, which stores the X display, root
 * window, etc. and provides the most common operations which use these data.
 */
class XData
{
public:
XData(Log &logger, Display *dpy, Window root, int screen) :
    m_display(dpy), m_logger(logger), m_confined(None),
    m_old_root_mask(NoEventMask), m_substructure_depth(0) {
    m_root = DefaultRootWindow(dpy);
    m_screen = DefaultScreen(dpy);

    init_xrandr();
    load_modifier_flags();
};

void init_xrandr();
void load_modifier_flags();

XGC * create_gc(Window);
Window create_window(bool);

void change_property(Window, const std::string&, Atom,
                     const unsigned char *, size_t);

void next_event(XEvent&);
void get_latest_event(XEvent&, int);

void add_hotkey(KeySym, bool);
void add_hotkey_mouse(unsigned int);

void confine_pointer(Window);
void stop_confining_pointer();
void grab_mouse(Window);
void ungrab_mouse(Window);

void select_input(Window, long);

void get_windows(std::vector<Window>&);
void get_pointer_location(Dimension&, Dimension&);

Window get_input_focus();
bool set_input_focus(Window);

void map_win(Window);
void unmap_win(Window);
void request_close(Window);
void destroy_win(Window);

void get_attributes(Window, XWindowAttributes&);
void set_attributes(Window, XSetWindowAttributes&,
                    unsigned long);
bool is_mapped(Window);

#ifdef WITH_BORDERS
void set_border_color(Window, MonoColor);
void set_border_width(Window, Dimension);
#endif

void move_window(Window, Dimension, Dimension);
void resize_window(Window, Dimension, Dimension);
void raise(Window);
void restack(const std::vector<Window>&);

bool get_wm_hints(Window, XWMHints&);
void get_size_hints(Window, XSizeHints&);
Window get_transient_hint(Window);
void get_icon_name(Window, std::string&);
void get_class(Window, std::string&);

void get_screen_boxes(std::vector<Box>&);

KeySym get_keysym(int);
void keysym_to_string(KeySym, std::string&);

void forward_configure_request(XEvent&, unsigned int);
void forward_circulate_request(XEvent&);

/// The event code X adds to each XRandR event (used by XEvents)
int randr_event_offset;

unsigned int primary_mod_flag;
unsigned int secondary_mod_flag;

unsigned int num_mod_flag;
unsigned int caps_mod_flag;
unsigned int scroll_mod_flag;

private:
Atom intern_if_needed(const std::string&);
unsigned long decode_monocolor(MonoColor);

void enable_substructure_events();
void disable_substructure_events();

/**  We save this to ensure that we can re-enable substructure events if they
 * were enabled before a call to disable_substructure_events.
 */
long m_old_root_mask;

/// How deep we are inside of a nested group of enable/disable substruture events
int m_substructure_depth;

/// The logging interface
Log &m_logger;

/// The connection to the X server
Display *m_display;

/// The root window on the display
Window m_root;

/// The default X11 screen
int m_screen;

/// The pre-defined atoms, which are accessible via a string
std::map<std::string, Atom> m_atoms;

/// The window the pointer is confined to, or None
Window m_confined;
};

#endif // ifndef __SMALLWM_XDATA__
