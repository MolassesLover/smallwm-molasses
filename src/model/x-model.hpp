/** @file */
#ifndef __SMALLWM_X_MODEL__
#define __SMALLWM_X_MODEL__
#include <map>
#include <vector>

#include "../common.hpp"
#include "../xdata.hpp"

/**
 * Stores the data necessary to handle an icon.
 */
struct Icon {
    Icon(Window _client, Window _icon, XGC * _gc) :
        client(_client), icon(_icon), gc(_gc) {
    };

    /// The window that the icon "stands for"
    Window client;

    /// The icon window itself
    Window icon;

    /// The graphical context used to draw the icon
    XGC *gc;
};

/**
 * The state of the client which is currently being moved or resized.
 */
enum MoveResizeState {
    MR_INVALID = 0,
    MR_MOVE,
    MR_RESIZE,
};

/**
 * Stores the data necessary to move or resize a window.
 */
struct MoveResize {
    MoveResize(Window _client, Window _placeholder, MoveResizeState _state) :
        client(_client), placeholder(_placeholder), state(_state) {
    };

    /// If this data is for a mover or a resizer
    MoveResizeState state;

    /// The placeholder window
    Window placeholder;

    /// The moved/resized client itself
    Window client;
};

/**
 * A ClientEffect communicates to the X event handler that certain types of
 * events are expected, and that their usual processing can be ignored.
 */
enum ClientEffect {
    EXPECT_MAP   = 1 << 0,
    EXPECT_UNMAP = 1 << 1
};


/**
 * A data store for information about the UI of the window manager (rather than
 * information about the windows which are being managed).
 */
class XModel
{
public:
XModel() : m_moveresize(0) {
};

void register_icon(Icon *);
void unregister_icon(Icon *);

Icon * find_icon_from_client(Window) const;
Icon * find_icon_from_icon_window(Window) const;
void get_icons(std::vector<Icon *>&);

void enter_move(Window, Window, Dimension2D);
void enter_resize(Window, Window, Dimension2D);

Dimension2D update_pointer(Dimension, Dimension);

Window get_move_resize_placeholder() const;
Window get_move_resize_client() const;
MoveResizeState get_move_resize_state() const;

void exit_move_resize();

bool has_effect(Window, ClientEffect);
void set_effect(Window, ClientEffect);
void clear_effect(Window, ClientEffect);
void remove_all_effects(Window);

private:
/// A mapping between clients and their icons
std::map<Window, Icon *> m_clients_to_icons;

/// A mapping between icon windows and the icon structures
std::map<Window, Icon *> m_icon_windows_to_icons;

/// The effects present on each window
std::map<Window, ClientEffect> m_effects;

/// The current data about moving or resizing
MoveResize *m_moveresize;

/// The current pointer location
Dimension2D m_pointer;
};

#endif // ifndef __SMALLWM_X_MODEL__
