#include "clientmodel-events.hpp"
#include "xdata.hpp"

/**
 * Maps all the windows in the given window list.
 */
void ClientModelEvents::map_all(const std::vector<Window> &windows) {
    for (std::vector<Window>::const_iterator win = windows.cbegin();
         win != windows.cend();
         win++) {
        m_xmodel.set_effect(*win, EXPECT_MAP);
        m_xdata.map_win(*win);
    }
}

/**
 * Unmaps all the windows in the given window list, and unfocuses any that might
 * be focused.
 */
void ClientModelEvents::unmap_unfocus_all(const std::vector<Window> &windows) {
    for (std::vector<Window>::const_iterator win = windows.cbegin();
         win != windows.cend();
         win++) {
        m_xmodel.set_effect(*win, EXPECT_UNMAP);
        m_clients.unfocus_if_focused(*win);
        m_xdata.unmap_win(*win);
    }
}

/**
 * Raises a client first, and then puts all its children above it.
 */
void ClientModelEvents::raise_family(Window client) {
    std::vector<Window> children;

    m_clients.get_children_of(client, children);

    m_xdata.raise(client);

    for (std::vector<Window>::iterator win = children.begin();
         win != children.end();
         win++) {
        m_xdata.raise(*win);
    }
}

/**
 * Handles all the currently queued change events, returning when the
 * ClientModel change list is exhausted.
 */
void ClientModelEvents::handle_queued_changes() {
    m_should_relayer = false;
    m_should_reposition_icons = false;

    while ((m_change = m_changes.get_next()) != 0) {
        if (m_change->is_layer_change()) handle_layer_change();
        else if (m_change->is_focus_change()) handle_focus_change();
        else if (m_change->is_client_desktop_change()) handle_client_desktop_change();
        else if (m_change->is_current_desktop_change()) handle_current_desktop_change();
        else if (m_change->is_screen_change()) handle_screen_change();
        else if (m_change->is_mode_change()) handle_mode_change();
        else if (m_change->is_location_change()) handle_location_change();
        else if (m_change->is_size_change()) handle_size_change();
        else if (m_change->is_destroy_change()) handle_destroy_change();
        else if (m_change->is_unmap_change()) handle_unmap_change();

        delete m_change;
    }

    if (m_should_relayer) do_relayer();

    if (m_should_reposition_icons) reposition_icons();
}

/**
 * Sets a flag so that relayering occurs later - this avoid relayering on
 * every ChangeLayer event.
 */
void ClientModelEvents::handle_layer_change() {
    m_should_relayer = true;
}

/**
 * Changes the focus from one window to another.
 *
 * The focus model used by SmallWM is click-to-focus, so clicks must be
 * captured on unfocused clients, while focused clients should not be captured.
 */
void ClientModelEvents::handle_focus_change() {
    const ChangeFocus *change_event = dynamic_cast<const ChangeFocus *>(m_change);

    // First, unfocus whatever the model says is foucsed. Note that the
    // client which is being unfocused may not exist anymore.
    Window unfocused_client = change_event->prev_focus;

    if (m_clients.is_client(unfocused_client) ||
        m_clients.is_child(unfocused_client)) {
        // Since this window will possibly be focused later, capture the clicks
        // going to it so we know when it needs to be focused again
        #ifdef WITH_BORDERS
        m_xdata.set_border_color(unfocused_client, X_WHITE);
        #endif
        m_xdata.grab_mouse(unfocused_client);
    }

    Window focused_client = change_event->next_focus;

    if (focused_client != None) {
        // Since this is now focused, let the client process events in by
        // ungrabbing the mouse and setting the keyboard focus
        if (focused_client != None && m_xdata.set_input_focus(focused_client)) {
            #ifdef WITH_BORDERS
            m_xdata.set_border_color(focused_client, X_BLACK);
            #endif

            m_xdata.ungrab_mouse(focused_client);
        } else {
            m_clients.cycle_focus_forward();

            #ifdef WITH_BORDERS
            m_xdata.set_border_color(focused_client, X_WHITE);
            #endif

            // Also, make sure to apply the grab to the window
            m_xdata.grab_mouse(focused_client);
        }
    } else m_xdata.set_input_focus(None);

    // Since the focus probably changed, go ahead and shuffle windows around to
    // ensure that the focused window is on top
    m_should_relayer = true;
}

/**
 * This changes the desktop of a client whose desktop should be changed.
 *
 * This method consists mostly of dispatches to other methods which handle
 * each case. These cases are:
 *
 *   UserDesktop -> UserDesktop
 *   UserDesktop -> AllDesktops
 *   UserDesktop -> IconDesktop
 *   UserDesktop -> MovingDesktop
 *   UserDesktop -> ResizingDesktop
 *
 *   AllDesktops -> UserDesktop
 *   AllDesktops -> IconDesktop
 *   AllDesktops -> MovingDesktop
 *   AllDesktops -> ResizingDesktop
 *
 *   IconDesktop -> UserDesktop
 *
 *   MovingDesktop -> UserDesktop
 *
 *   ResizingDesktop -> UserDesktop
 */
void ClientModelEvents::handle_client_desktop_change() {
    const ChangeClientDesktop *change = dynamic_cast<const ChangeClientDesktop *>(m_change);

    Desktop *old_desktop = change->prev_desktop;
    Desktop *new_desktop = change->next_desktop;
    Window client = change->window;

    // The previous desktop can be NULL if this client has been freshly mapped
    if (!old_desktop) handle_new_client_desktop_change(new_desktop, client);
    else if (old_desktop->is_user_desktop()) handle_client_change_from_user_desktop(old_desktop, new_desktop,
                                                                                    client);
    else if (old_desktop->is_all_desktop()) handle_client_change_from_all_desktop(old_desktop, new_desktop,
                                                                                  client);
    else if (old_desktop->is_icon_desktop()) handle_client_change_from_icon_desktop(old_desktop, new_desktop,
                                                                                    client);
    else if (old_desktop->is_moving_desktop()) handle_client_change_from_moving_desktop(old_desktop, new_desktop,
                                                                                        client);
    else if (old_desktop->is_resizing_desktop()) handle_client_change_from_resizing_desktop(old_desktop, new_desktop,
                                                                                            client);
    else
        m_logger.log(LOG_WARNING) <<
            "Unanticipated switch by " << client << " from " <<
            old_desktop  << " to " << new_desktop << Log::endl;
}

/**
 * Sets the desktop of a newly created client.
 *
 * In this state, the only possibilities are either a UserDesktop or an
 * IconDesktop if the window starts out minimized.
 */
void ClientModelEvents::handle_new_client_desktop_change(Desktop *new_desktop, Window client) {
    if (new_desktop->is_user_desktop()) {
        bool will_be_visible = m_clients.is_visible_desktop(new_desktop);

        if (will_be_visible) m_should_relayer = true;
    } else if (new_desktop->is_icon_desktop()) register_new_icon(client, true);
    else {
        m_logger.log(LOG_WARNING) <<
            "New client " << client << " asked to start on desktop " <<
            new_desktop << "- making an icon instead"
                                  << Log::endl;

        // Since the API doesn't allow us to get the current desktop, and
        // we can't reset it since `ClientModel::client_reset_desktop` requires
        // that the client have a previous desktop, just make it an icon.
        handle_new_client_desktop_change(m_clients.ICON_DESKTOP, client);
    }
}

/**
 * Changes the desktop of a client from a user desktop to some other kind of
 * desktop.
 *
 * Windows can generally move from user desktops to any other kind of desktop,
 * since user desktops are the starting point for every window.
 */
void ClientModelEvents::handle_client_change_from_user_desktop(
    Desktop *old_desktop,
    Desktop *new_desktop,
    Window  client) {
    std::vector<Window> children;

    m_clients.get_children_of(client, children);

    if (new_desktop->is_user_desktop()) {
        bool is_currently_visible = m_clients.is_visible_desktop(old_desktop);
        bool will_be_visible = m_clients.is_visible_desktop(new_desktop);

        if (is_currently_visible && !will_be_visible) {
            m_xmodel.set_effect(client, EXPECT_UNMAP);
            m_clients.unfocus_if_focused(client);
            m_xdata.unmap_win(client);

            unmap_unfocus_all(children);
        } else if (!is_currently_visible && will_be_visible) {
            m_xmodel.set_effect(client, EXPECT_MAP);
            m_xdata.map_win(client);
            m_clients.focus(client);

            map_all(children);
            m_should_relayer = true;
        } else if (!is_currently_visible && !will_be_visible) {
            // Do nothing here - the client will still be invisible and
            // thus will not alter the focus
        } else
            m_logger.log(LOG_WARNING) <<
                "If client is switched from a " << old_desktop << " to "
                                      << new_desktop << " then it cannot be visible in both places."
                                      << Log::endl;

        /*
         * This is because there is only ever one visible desktop - to
         * have a window be visible on more than one desktop would
         * somehow break that invariant.
         */
    } else if (new_desktop->is_all_desktop()) {
        bool is_visible = m_clients.is_visible_desktop(old_desktop);

        if (!is_visible) {
            m_xmodel.set_effect(client, EXPECT_MAP);
            m_xdata.map_win(client);
            m_clients.focus(client);

            map_all(children);
            m_should_relayer = true;
        }
    } else if (new_desktop->is_icon_desktop()) {
        bool is_visible = m_clients.is_visible_desktop(old_desktop);

        if (is_visible) unmap_unfocus_all(children);

        register_new_icon(client, is_visible);
    } else if (new_desktop->is_moving_desktop()) {
        unmap_unfocus_all(children);
        start_moving(client);
    } else if (new_desktop->is_resizing_desktop()) {
        unmap_unfocus_all(children);
        start_resizing(client);
    }
}

/**
 * Changes the desktop of a client from the 'all' desktop to some other kind
 * of desktop.
 *
 * Most of this code is the same as in the user desktop changes, since
 * windows from the 'all' desktop can generally move to any other kind of
 * desktop as well.
 */
void ClientModelEvents::handle_client_change_from_all_desktop(
    Desktop *old_desktop,
    Desktop *new_desktop,
    Window  client) {
    std::vector<Window> children;

    m_clients.get_children_of(client, children);

    if (new_desktop->is_user_desktop()) {
        bool will_be_visible = m_clients.is_visible_desktop(new_desktop);

        if (!will_be_visible) {
            m_xmodel.set_effect(client, EXPECT_UNMAP);
            m_clients.unfocus_if_focused(client);
            m_xdata.unmap_win(client);

            unmap_unfocus_all(children);
            m_should_relayer = true;
        }
    } else if (new_desktop->is_icon_desktop()) {
        unmap_unfocus_all(children);
        register_new_icon(client, true);
    } else if (new_desktop->is_moving_desktop()) {
        unmap_unfocus_all(children);
        start_moving(client);
    } else if (new_desktop->is_resizing_desktop()) {
        unmap_unfocus_all(children);
        start_resizing(client);
    }
}

/**
 * Changes the desktop of a client from the icon desktop to some other kind of
 * desktop. In fact, the only target desktop that this method will do accept
 * is a user desktop.
 */
void ClientModelEvents::handle_client_change_from_icon_desktop(
    Desktop *old_desktop,
    Desktop *new_desktop,
    Window  client) {
    std::vector<Window> children;

    m_clients.get_children_of(client, children);

    if (new_desktop->is_user_desktop() || new_desktop->is_all_desktop()) {
        // Get the relevant icon information, and destroy it
        Icon *icon = m_xmodel.find_icon_from_client(client);

        if (!icon)
            m_logger.log(LOG_ERR) <<
                "Tried to de-iconify a client (" << client << ") "
                "that is not currently iconified." << Log::endl;
        else {
            m_xdata.destroy_win(icon->icon);
            m_xmodel.unregister_icon(icon);

            delete icon->gc;
            delete icon;

            bool will_be_visible = m_clients.is_visible_desktop(new_desktop);

            if (will_be_visible) {
                m_xmodel.set_effect(client, EXPECT_MAP);
                m_xdata.map_win(client);
                m_clients.focus(client);

                map_all(children);
            }

            m_should_reposition_icons = true;
        }
    }
}

/**
 * Changes the desktop of a client from the moving desktop to some other kind
 * of desktop. The only supported target desktop is a user desktop.
 */
void ClientModelEvents::handle_client_change_from_moving_desktop(
    Desktop *old_desktop,
    Desktop *new_desktop,
    Window  client) {
    std::vector<Window> children;

    m_clients.get_children_of(client, children);

    if (new_desktop->is_user_desktop() || new_desktop->is_all_desktop()) {
        Window placeholder = m_xmodel.get_move_resize_placeholder();

        if (placeholder == None)
            m_logger.log(LOG_ERR) <<
                "Tried to stop moving a client (" << client << ") "
                "that is not currently moving." << Log::endl;
        else {
            XWindowAttributes placeholder_attr;
            m_xdata.get_attributes(placeholder, placeholder_attr);
            m_xdata.move_window(client, placeholder_attr.x,
                                placeholder_attr.y);

            m_xdata.stop_confining_pointer();
            m_xdata.destroy_win(placeholder);
            m_xmodel.exit_move_resize();

            bool will_be_visible = m_clients.is_visible_desktop(new_desktop);

            if (will_be_visible) {
                m_xmodel.set_effect(client, EXPECT_MAP);
                m_xdata.map_win(client);
                m_clients.focus(client);

                map_all(children);
                m_should_relayer = true;
            }
        }
    }
}

/**
 * Changes the desktop of a client from the resizing desktop to some other
 * kind of desktop (as with `handle_client_change_from_moving_desktop`, only
 * user desktops are supported targets).
 */
void ClientModelEvents::handle_client_change_from_resizing_desktop(
    Desktop *old_desktop,
    Desktop *new_desktop,
    Window  client) {
    std::vector<Window> children;

    m_clients.get_children_of(client, children);

    if (new_desktop->is_user_desktop() || new_desktop->is_all_desktop()) {
        Window placeholder = m_xmodel.get_move_resize_placeholder();

        if (placeholder == None)
            m_logger.log(LOG_ERR) <<
                "Tried to stop resizing a client (" << client << ") "
                "that is not currently resizing." << Log::endl;
        else {
            XWindowAttributes placeholder_attr;
            m_xdata.get_attributes(placeholder, placeholder_attr);
            m_xdata.resize_window(client, placeholder_attr.width,
                                  placeholder_attr.height);

            m_xdata.stop_confining_pointer();
            m_xdata.destroy_win(placeholder);
            m_xmodel.exit_move_resize();

            bool will_be_visible = m_clients.is_visible_desktop(new_desktop);

            if (will_be_visible) {
                m_xmodel.set_effect(client, EXPECT_MAP);
                m_xdata.map_win(client);
                m_clients.focus(client);

                map_all(children);
                m_should_relayer = true;
            }
        }
    }
}

/**
 * This changes the currently visible desktop, which involves figuring out
 * which windows are visible on the current desktop, which are not, and then
 * showing those that are visible and hiding those that are not.
 */
void ClientModelEvents::handle_current_desktop_change() {
    const ChangeCurrentDesktop *change = dynamic_cast<const ChangeCurrentDesktop *>(m_change);

    std::vector<Window> old_desktop_list;
    std::vector<Window> new_desktop_list;

    m_clients.get_clients_of(change->prev_desktop, old_desktop_list);
    m_clients.get_clients_of(change->next_desktop, new_desktop_list);

    // The output size will have, at most, the size of its largest input
    size_t max_output_size = std::max(old_desktop_list.size(),
                                      new_desktop_list.size());

    // std::set_difference requires sorted inputs
    std::sort(old_desktop_list.begin(), old_desktop_list.end());
    std::sort(new_desktop_list.begin(), new_desktop_list.end());

    std::vector<Window> to_make_invisible;
    std::vector<Window> to_make_visible;

    // Fill out all of the output vectors to the length of the input iterators,
    // so that std::set_difference has somewhere to put the data
    to_make_invisible.resize(max_output_size, None);
    to_make_visible.resize(max_output_size, None);

    // The old -> new set difference will produce the list of windows
    // which are on the old desktop, but not this one - these need to be
    // hidden
    std::set_difference(
        old_desktop_list.begin(), old_desktop_list.end(),
        new_desktop_list.begin(), new_desktop_list.end(),
        to_make_invisible.begin());

    for (std::vector<Window>::iterator to_hide = to_make_invisible.begin();
         /* The extra check that the iterator is not None is necessary,
          * since the vector was padded with None when it was resized.
          */
         to_hide != to_make_invisible.end() && *to_hide != None;
         to_hide++) {
        m_xmodel.set_effect(*to_hide, EXPECT_UNMAP);
        m_xdata.unmap_win(*to_hide);

        std::vector<Window> children;
        m_clients.get_children_of(*to_hide, children);
        unmap_unfocus_all(children);
    }

    // The new -> old set difference will produce the list of windows which
    // are on the new desktop, but not on the old - these need to be made
    // visible
    std::set_difference(
        new_desktop_list.begin(), new_desktop_list.end(),
        old_desktop_list.begin(), old_desktop_list.end(),
        to_make_visible.begin());

    for (std::vector<Window>::iterator to_show = to_make_visible.begin();
         to_show != to_make_visible.end() && *to_show != None;
         to_show++) {
        m_xmodel.set_effect(*to_show, EXPECT_MAP);
        m_xdata.map_win(*to_show);

        std::vector<Window> children;
        m_clients.get_children_of(*to_show, children);
        map_all(children);
    }

    // Since we've made some windows visible and some others invisible, we've
    // invalidated the previous stacking order, so restack everything according
    // to what is now visible
    m_should_relayer = true;
}

/**
 * Handles the screen of a client changing.
 */
void ClientModelEvents::handle_screen_change() {
    const ChangeScreen *change = dynamic_cast<const ChangeScreen *>(m_change);

    Window client = change->window;
    const Box &box = change->bounds;

    // If the window went to an invalid screen, then there's nothing we can do
    if (box == Box(-1, -1, 0, 0)) return;

    XWindowAttributes attrib;

    m_xdata.get_attributes(client, attrib);

    ClientPosScale cps_mode = m_clients.get_mode(change->window);

    Dimension new_x = attrib.x,
              new_y = attrib.y,
              new_width = attrib.width,
              new_height = attrib.height;

    switch (cps_mode) {
        case CPS_FLOATING:

            // For floating windows, just make sure that they don't extend
            // beyond the window they're supposed to inhabit
            if (attrib.x + attrib.width > box.x + box.width) new_width = (box.x + box.width) - attrib.x;

            if (attrib.y + attrib.height > box.y + box.height) new_height = (box.y + box.height) - attrib.y;

            m_clients.change_size(client, new_width, new_height);

            // Ensure that the window actually is located in the screen it
            // claims to be in
            if (attrib.x < box.x || attrib.x >= box.x + box.width) new_x = box.x;

            if (attrib.y < box.y || attrib.y >= box.y + box.height) new_y = box.y;

            m_clients.change_location(client, new_x, new_y);
            break;

        default:
            // If we're doing the managing for this window, then correct for
            // the screen change
            update_location_size_for_cps(client, cps_mode);
    }
}

/**
 * Handles a change in the mode of a client.
 */
void ClientModelEvents::handle_mode_change() {
    const ChangeCPSMode *change = dynamic_cast<const ChangeCPSMode *>(m_change);

    // Floating doesn't impose any position or size requirements on the window
    if (change->mode == CPS_FLOATING) return;

    update_location_size_for_cps(change->window, change->mode);
}

/**
 * Handles a change in location for a particular window.
 */
void ClientModelEvents::handle_location_change() {
    const ChangeLocation *change = dynamic_cast<const ChangeLocation *>(m_change);

    m_xdata.move_window(change->window, change->x, change->y);
}

/**
 * Handles a change in size for a particular window.
 */
void ClientModelEvents::handle_size_change() {
    const ChangeSize *change = dynamic_cast<const ChangeSize *>(m_change);

    m_xdata.resize_window(change->window, change->w, change->h);
}

/**
 * Handles a window which is being destroyed, depending upon what its current
 * desktop is:
 *
 *  (1) A client which is iconified needs to have its icon unregistered.
 *  (2) A client which is being moved/resized needs to stop moving/resizing.
 */
void ClientModelEvents::handle_destroy_change() {
    const DestroyChange *change = dynamic_cast<const DestroyChange *>(m_change);
    Window destroyed_window = change->window;
    Desktop *old_desktop = change->desktop;

    if (old_desktop->is_icon_desktop() || old_desktop->is_moving_desktop() ||
        old_desktop->is_resizing_desktop()) {
        // Note that we don't apply any changes in the client model, since the
        // desktop of the client (and its layer, etc.) are not stored any more.
        //
        // All we have to do is clean up the state left over in m_xmodel.
        if (old_desktop->is_icon_desktop()) {
            Icon *old_icon = m_xmodel.find_icon_from_client(destroyed_window);
            m_xmodel.unregister_icon(old_icon);

            m_xdata.destroy_win(old_icon->icon);
            delete old_icon->gc;
            delete old_icon;

            // Since we won't be changing the ClientModel, and thus issuing a
            // ClientDesktopChange, we have to the work that it does
            m_should_reposition_icons = true;
        } else if (old_desktop->is_moving_desktop() ||
                   old_desktop->is_resizing_desktop()) {
            Window placeholder = m_xmodel.get_move_resize_placeholder();

            m_xdata.stop_confining_pointer();
            m_xdata.destroy_win(placeholder);
            m_xmodel.exit_move_resize();
        }
    }
}

/**
 * Iconifies a client window, creating and registering a new icon while
 * hiding the client.
 *
 * @param client The client to create the icon for.
 * @param do_unmap `true` to unmap the client, `false` to not unmap it.
 *                 Useful if the client is already unmapped for some reason.
 */
void ClientModelEvents::register_new_icon(Window client, bool do_unmap) {
    Window icon_window = m_xdata.create_window(true);

    m_xdata.select_input(icon_window,
                         ButtonPressMask | ButtonReleaseMask | ExposureMask);

    m_xdata.resize_window(icon_window, m_config.icon_width,
                          m_config.icon_height);
    m_xdata.map_win(icon_window);

    XGC *gc = m_xdata.create_gc(icon_window);
    Icon *the_icon = new Icon(client, icon_window, gc);

    m_clients.unfocus_if_focused(client);

    if (do_unmap) {
        m_xmodel.set_effect(client, EXPECT_UNMAP);
        m_xdata.unmap_win(client);
    }

    m_xmodel.register_icon(the_icon);

    m_should_reposition_icons = true;
}

/**
 * Creates and configures a placeholder window, used for moving/resizing a
 * client.
 *
 * @param client The client to create the placeholder for.
 * @return The placeholder window.
 */
Window ClientModelEvents::create_placeholder(Window client) {
    XWindowAttributes client_attrs;

    m_xdata.get_attributes(client, client_attrs);

    // The placeholder should be ignored (create_window(true)) because it
    // is not an actual client, but an internal window that doesn't need
    // to be managed
    Window placeholder = m_xdata.create_window(true);

    m_xdata.move_window(placeholder, client_attrs.x, client_attrs.y);
    m_xdata.resize_window(placeholder, client_attrs.width,
                          client_attrs.height);

    // With the window in place, show it and make sure that the cursor is
    // glued to it, to make sure that all of the movements are captured
    m_xdata.map_win(placeholder);
    m_xdata.confine_pointer(placeholder);

    // Since we need the placeholder to move up, go ahead and schedule a
    // relayering
    m_should_relayer = true;

    return placeholder;
}

/**
 * Handles the necessary work to start moving a client.
 *
 * @param client The client window to start moving.
 */
void ClientModelEvents::start_moving(Window client) {
    Window placeholder = create_placeholder(client);

    // The placeholder needed the client's position and size - now that the
    // placeholder is open, we can hide the client
    m_xmodel.set_effect(client, EXPECT_UNMAP);
    m_clients.unfocus_if_focused(client);
    m_xdata.unmap_win(client);

    Dimension pointer_x, pointer_y;

    m_xdata.get_pointer_location(pointer_x, pointer_y);

    m_xmodel.enter_move(client, placeholder, Dimension2D(pointer_x,
                                                         pointer_y));
}

/**
 * Handles the necessary work to start resizing a client.
 *
 * @param client The client window to start resizing.
 */
void ClientModelEvents::start_resizing(Window client) {
    Window placeholder = create_placeholder(client);

    // The placeholder needed the client's position and size - now that the
    // placeholder is open, we can hide the client
    m_xmodel.set_effect(client, EXPECT_UNMAP);
    m_clients.unfocus_if_focused(client);
    m_xdata.unmap_win(client);

    Dimension pointer_x, pointer_y;

    m_xdata.get_pointer_location(pointer_x, pointer_y);

    m_xmodel.enter_resize(client, placeholder, Dimension2D(pointer_x,
                                                           pointer_y));
}

/**
 * Actually does the relayering.
 *
 * This involves sorting the clients, and then sticking the icons and
 * move/resize placeholder on the top.
 */
void ClientModelEvents::do_relayer() {
    std::vector<Window> ordered_windows;

    m_clients.get_visible_in_layer_order(ordered_windows);

    // Figure out the currently focused client, and where it's at. We'll need
    // this information in order to place it above its peers.
    Window focused_window = m_clients.get_focused();

    // Children aren't raised in any particular order, but we need to make
    // sure that the parent is raised if any child is
    if (focused_window != None && m_clients.is_child(focused_window)) focused_window = m_clients.get_parent_of(focused_window);

    Layer focused_layer;

    if (focused_window != None) focused_layer = m_clients.find_layer(focused_window);

    for (std::vector<Window>::iterator client_iter = ordered_windows.begin();
         client_iter != ordered_windows.end();
         client_iter++) {
        Window current_client = *client_iter;
        std::vector<Window> children;
        m_clients.get_children_of(*client_iter, children);

        Layer current_layer = m_clients.find_layer(current_client);

        // We have to check if we're at the point where we can put up the
        // focused window - this happens when we've passed the layer that the
        // focused window is on. We want to put the focused window above all of
        // its peers, so before putting up the first client on the next layer,
        // put up the focused window
        if (focused_window != None &&
            current_layer > focused_layer) {
            raise_family(focused_window);

            // Make sure to erase the focused client, so that we don't raise
            // it more than once
            focused_window = None;
        }

        if (current_client != focused_window) raise_family(current_client);
    }

    // If we haven't cleared the focused window, then we need to raise it before
    // moving on
    if (focused_window != None) raise_family(focused_window);

    // Now, raise all the icons since they should always be above all other
    // windows so they aren't obscured
    std::vector<Icon *> icon_list;

    m_xmodel.get_icons(icon_list);

    for (std::vector<Icon *>::iterator icon = icon_list.begin();
         icon != icon_list.end();
         icon++) {
        Window icon_win = (*icon)->icon;
        m_xdata.raise(icon_win);
    }

    // Don't obscure the placeholder, since the user is actively working with it
    Window placeholder_win = m_xmodel.get_move_resize_placeholder();

    if (placeholder_win != None) m_xdata.raise(placeholder_win);
}

/**
 * Repositions icon windows after one has been added or removed.
 *
 * Icon windows are arranged in rows, starting from the top left and going
 * toward the bottom right.
 */
void ClientModelEvents::reposition_icons() {
    Dimension x = 0, y = 0;

    const Dimension icon_width = m_config.icon_width,
                    icon_height = m_config.icon_height;
    const Box &screen = m_clients.get_root_screen();

    std::vector<Icon *> icon_list;

    m_xmodel.get_icons(icon_list);

    for (std::vector<Icon *>::iterator icon_iter = icon_list.begin();
         icon_iter != icon_list.end(); icon_iter++) {
        Icon *the_icon = *icon_iter;

        if (x + icon_width > screen.width) {
            x = 0;
            y += icon_height;
        }

        m_xdata.move_window(the_icon->icon, x, y);
        x += icon_width;
    }
}

/**
 * Updates the location and size of a window based upon its current CPS mode.
 */
void ClientModelEvents::update_location_size_for_cps(Window client, ClientPosScale mode) {
    const Box &screen = m_clients.get_screen(client);

    int left_x = screen.x;
    int right_x = left_x + screen.width;
    int middle_x = left_x + screen.width / 2;

    int top_y = screen.y;
    int bottom_y = top_y + screen.height;
    int middle_y = top_y + screen.height / 2;

    // If the client is on the root screen, then the icon row has to be taken
    // into account
    if (screen.x == 0 && screen.y == 0) {
        top_y = screen.y + m_config.icon_height;

        int working_height = screen.height - m_config.icon_height;
        middle_y = top_y + working_height / 2;
    } else {
        top_y = screen.y;
        middle_y = top_y + screen.height / 2;
    }

    #ifdef WITH_BORDERS
    Dimension border = m_config.border_width * 2;

    switch (mode) {
        case CPS_SPLIT_LEFT:
            m_clients.change_location(client, left_x, top_y);
            m_clients.change_size(client, middle_x - left_x - border, bottom_y - top_y - border);
            break;

        case CPS_SPLIT_RIGHT:
            m_clients.change_location(client, middle_x, top_y);
            m_clients.change_size(client, right_x - middle_x - border, bottom_y - top_y - border);
            break;

        case CPS_SPLIT_TOP:
            m_clients.change_location(client, left_x, top_y);
            m_clients.change_size(client, right_x - left_x - border, middle_y - top_y - border);
            break;

        case CPS_SPLIT_BOTTOM:
            m_clients.change_location(client, left_x, middle_y);
            m_clients.change_size(client, right_x - left_x - border, bottom_y - middle_y - border);
            break;

        case CPS_MAX:
            m_clients.change_location(client, left_x, top_y);
            m_clients.change_size(client, right_x - left_x - border, bottom_y - top_y - border);
            break;
    }
    #endif // ifdef WITH_BORDERS
}

/*
 * Unmapped windows have to be unfocused and removed from various lists
 * in order to prevent them from being confused with regular, usable
 * windows.
 */
void ClientModelEvents::handle_unmap_change() {
    const UnmapChange *change_event = dynamic_cast<const UnmapChange *>(m_change);

    std::vector<Window> children;

    m_clients.get_children_of(change_event->window, children);
    unmap_unfocus_all(children);
}
