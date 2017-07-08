#include <algorithm>
#include <utility>
#include <vector>

#include <UnitTest++.h>
#include "model/screen.h"
#include "model/changes.h"
#include "model/client-model.h"

const Window a = 1,
      b = 2,
      c = 3;

const unsigned long long max_desktops = 5;

/*
 * Note that the implied screen configuration used is:
 *
 * +-----+-----+-----+
 * |     |     |     |
 * |     |     |     |
 * +-----+-----+-----+
 * |     |  x  |     |
 * |     |     |     |
 * +-----+-----+-----+
 * |     |     |     |
 * |     |     |     |
 * +-----+-----+-----+
 *
 * Note here that X marks the (default) spot for windows, and that each
 * screen is 100x100
 */

struct ClientModelFixture
{
    ClientModelFixture() :
        model(changes, manager, max_desktops)
    {
        reset_screen_graph();
    };

    ~ClientModelFixture()
    {};

    void reset_screen_graph()
    {
        std::vector<Box> screens;
        for (int x = 0; x < 300; x += 100)
            for (int y = 0; y < 300; y += 100)
                screens.push_back(Box(x, y, 100, 100));

        manager.rebuild_graph(screens);
    }

    CrtManager manager;
    ChangeStream changes;
    ClientModel model;
};

/// This makes it easier to construct screen shift test cases
struct ChangeScreenTest
{
    Direction direction;
    Dimension box_x, box_y;
    unsigned int box_width, box_height;
};

SUITE(ClientModelMemberSuite)
{
    TEST_FIXTURE(ClientModelFixture, test_default_members)
    {
        // Make sure that there are no clients by default
        CHECK_EQUAL(false, model.is_client(a));
        CHECK_EQUAL(false, model.is_client(b));

        // Ensure that the root screen is the top-left screen
        CHECK_EQUAL(model.get_root_screen(), Box(0, 0, 100, 100));

        // Add a new client, and ensure that it is present
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), true);

        // Make sure that a is now listed as a client
        CHECK_EQUAL(true, model.is_client(a));

        // Make sure that the client has no children by default
        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(0, children.size());

        // Make sure that a is now focused
        CHECK_EQUAL(a, model.get_focused());

        // Check the event stream for the most recent events
        const Change * change = changes.get_next();

        // First, the window appears on a desktop
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            Desktop *desktop(new UserDesktop(0));
            CHECK_EQUAL(ChangeClientDesktop(a, 0, desktop), *the_change);
        }
        delete change;

        // Secondly, it is stacked relative to other windows
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_layer_change());
        {
            const ChangeLayer *the_change = dynamic_cast<const ChangeLayer*>(change);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER), *the_change);
        }
        delete change;

        // Make sure it was focused
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change = dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        delete change;

        // Finally, this is the end of the event stream
        CHECK(!changes.has_more());

        // Then, remove the added client. Ensure that a 'ChangeFocus' event was
        // fired which includes the now-destroyed client.
        model.remove_client(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change = dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);

            // Since ChangeFocus was fired, ensure that the focus was updated correctly
            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        // Also ensure that a DestroyChange event was sent, to notify of the removed client
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_destroy_change());
        {
            const DestroyChange *the_change = dynamic_cast<const DestroyChange*>(change);
            Desktop *desktop(new UserDesktop(0));
            CHECK_EQUAL(DestroyChange(a, desktop, DEF_LAYER), *the_change);
        }

        CHECK(!changes.has_more());

        CHECK_EQUAL(false, model.is_client(a));
    }

    TEST_FIXTURE(ClientModelFixture, test_default_members_nofocus)
    {
        CHECK_EQUAL(false, model.is_client(a));
        CHECK_EQUAL(false, model.is_client(b));

        CHECK_EQUAL(model.get_root_screen(), Box(0, 0, 100, 100));

        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), false);

        CHECK_EQUAL(true, model.is_client(a));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(0, children.size());

        CHECK(model.get_focused() != a);

        const Change * change = changes.get_next();

        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            Desktop *desktop(new UserDesktop(0));
            CHECK_EQUAL(ChangeClientDesktop(a, 0, desktop), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_layer_change());
        {
            const ChangeLayer *the_change = dynamic_cast<const ChangeLayer*>(change);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        model.remove_client(a);

        // Also ensure that a DestroyChange event was sent, to notify of the removed client
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_destroy_change());
        {
            const DestroyChange *the_change = dynamic_cast<const DestroyChange*>(change);
            Desktop *desktop(new UserDesktop(0));
            CHECK_EQUAL(DestroyChange(a, desktop, DEF_LAYER), *the_change);
        }

        CHECK(!changes.has_more());

        CHECK_EQUAL(false, model.is_client(a));
    }

    TEST_FIXTURE(ClientModelFixture, test_visibility)
    {
        // Add a new client, and ensure that it is present
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), true);

        // The child goes along for the ride - we don't check anything about it,
        // we just want to make sure nothing explodes when we do all of the following
        // things
        model.add_child(a, b);

        // Make sure that the client is visible by default
        CHECK(model.is_visible(a));

        // Make sure moving clients are invisible
        model.start_moving(a);
        CHECK(!model.is_visible(a));
        model.stop_moving(a, Dimension2D(2, 2));
        CHECK(model.is_visible(a));

        // Make sure resizing clients are invisible
        model.start_resizing(a);
        CHECK(!model.is_visible(a));
        model.stop_resizing(a, Dimension2D(2, 2));
        CHECK(model.is_visible(a));

        // Make sure that iconified clients are invisible
        model.iconify(a);
        CHECK(!model.is_visible(a));
        model.deiconify(a);
        CHECK(model.is_visible(a));

        // Move a client to a different desktop and make sure it is invisible
        model.client_next_desktop(a);
        CHECK(!model.is_visible(a));
        model.client_prev_desktop(a);
        CHECK(model.is_visible(a));

        model.client_prev_desktop(a);
        CHECK(!model.is_visible(a));
        model.client_next_desktop(a);
        CHECK(model.is_visible(a));

        // View a different desktop and make sure the client is invisible
        model.next_desktop();
        CHECK(!model.is_visible(a));
        model.prev_desktop();
        CHECK(model.is_visible(a));

        model.prev_desktop();
        CHECK(!model.is_visible(a));
        model.next_desktop();
        CHECK(model.is_visible(a));

        // Stick a window, and then change desktops, making sure the stuck
        // window is still visible
        model.toggle_stick(a);

        model.next_desktop();
        CHECK(model.is_visible(a));
        model.prev_desktop();
        CHECK(model.is_visible(a));

        model.prev_desktop();
        CHECK(model.is_visible(a));
        model.next_desktop();
        CHECK(model.is_visible(a));

        // Remove the stickiness and then make sure that the tests display the
        // same results as last time
        model.toggle_stick(a);

        model.next_desktop();
        CHECK(!model.is_visible(a));
        model.prev_desktop();
        CHECK(model.is_visible(a));

        model.prev_desktop();
        CHECK(!model.is_visible(a));
        model.next_desktop();
        CHECK(model.is_visible(a));
    }

    TEST_FIXTURE(ClientModelFixture, test_desktop_visibility)
    {
        // First, make sure that the first desktop is visible, and
        // no other desktops are
        CHECK(model.is_visible_desktop(model.USER_DESKTOPS[0]));
        CHECK(model.is_visible_desktop(model.ALL_DESKTOPS));

        for (unsigned long long desktop = 0; desktop < max_desktops;
             desktop++)
        {
            if (desktop == 0)
                continue;
            CHECK(!model.is_visible_desktop(model.USER_DESKTOPS[desktop]));
        }

        CHECK(!model.is_visible_desktop(model.ICON_DESKTOP));
        CHECK(!model.is_visible_desktop(model.MOVING_DESKTOP));
        CHECK(!model.is_visible_desktop(model.RESIZING_DESKTOP));

        // Now, move to the next desktop and ensure that the same is true
        // of all the visibility states but that USER_DESKTOPS[1] is visible
        // and USER_DESKTOPS[0] is not
        model.next_desktop();

        CHECK(model.is_visible_desktop(model.USER_DESKTOPS[1]));
        CHECK(model.is_visible_desktop(model.ALL_DESKTOPS));

        for (unsigned long long desktop = 0; desktop < max_desktops;
             desktop++)
        {
            if (desktop == 1)
                continue;
            CHECK(!model.is_visible_desktop(model.USER_DESKTOPS[desktop]));
        }

        CHECK(!model.is_visible_desktop(model.ICON_DESKTOP));
        CHECK(!model.is_visible_desktop(model.MOVING_DESKTOP));
        CHECK(!model.is_visible_desktop(model.RESIZING_DESKTOP));
    }

    TEST_FIXTURE(ClientModelFixture, test_finder_functions)
    {
        // Make sure that the `find_*` functions return the correct results
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), true);

        const Desktop *desktop_of = model.find_desktop(a);
        CHECK(*desktop_of == UserDesktop(0));
        CHECK(model.find_layer(a) == DEF_LAYER);
    }

    TEST_FIXTURE(ClientModelFixture, test_getters)
    {
        // First, ensure that `get_clients_of` gets only clients on the given
        // desktop
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), true);

        std::vector<Window> result;
        model.get_clients_of(model.USER_DESKTOPS[0], result);
        CHECK_EQUAL(2, result.size());
        if (result[0] == a)
            CHECK_EQUAL(b, result[1]);
        else if (result[0] == b)
            CHECK_EQUAL(a, result[1]);
        else
            CHECK(false);

        // Also, ensure that all clients are marked as visible
        result.clear();
        model.get_visible_clients(result);
        CHECK_EQUAL(2, result.size());
        if (result[0] == a)
            CHECK_EQUAL(b, result[1]);
        else if (result[0] == b)
            CHECK_EQUAL(a, result[1]);
        else
            CHECK(false);

        // Move a client down, and ensure that it appears before the other in
        // stacking order
        model.down_layer(b);
        result.clear();
        model.get_visible_in_layer_order(result);
        CHECK_EQUAL(2, result.size());
        CHECK_EQUAL(result[0], b);
        CHECK_EQUAL(result[1], a);

        // Now, move the client up and ensure that the layer order is reversed
        model.up_layer(b);
        model.up_layer(b);
        result.clear();
        model.get_visible_in_layer_order(result);
        CHECK_EQUAL(2, result.size());
        CHECK_EQUAL(result[0], a);
        CHECK_EQUAL(result[1], b);

        // Move a client off this desktop, and ensure that it appears there
        // Also, ensure that the visible list no longer includes it
        model.client_next_desktop(b);

        result.clear();
        model.get_clients_of(model.USER_DESKTOPS[0], result);
        CHECK_EQUAL(1, result.size());
        CHECK_EQUAL(a, result[0]);

        result.clear();
        model.get_clients_of(model.USER_DESKTOPS[1], result);
        CHECK_EQUAL(1, result.size());
        CHECK_EQUAL(b, result[0]);

        // Ensure that the visible list includes only the client on this
        // desktop; the same goes for the visible clients in layer order
        result.clear();
        model.get_visible_clients(result);
        CHECK_EQUAL(1, result.size());
        CHECK_EQUAL(a, result[0]);

        result.clear();
        model.get_visible_in_layer_order(result);
        CHECK_EQUAL(1, result.size());
        CHECK_EQUAL(a, result[0]);

        // Go to the next desktop and make sure that the visible list is
        // fixed
        model.next_desktop();

        result.clear();
        model.get_visible_clients(result);
        CHECK_EQUAL(1, result.size());
        CHECK_EQUAL(b, result[0]);

        result.clear();
        model.get_visible_in_layer_order(result);
        CHECK_EQUAL(1, result.size());
        CHECK_EQUAL(b, result[0]);
    }

    TEST_FIXTURE(ClientModelFixture, test_layer_change)
    {
        // Move a client up, and then down - ensure that, both times, the
        // proper event is sent.
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // Up
        model.up_layer(a);
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_layer_change());
        {
            const ChangeLayer *the_change =
                dynamic_cast<const ChangeLayer*>(change);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER + 1), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Down
        model.down_layer(a);
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_layer_change());
        {
            const ChangeLayer *the_change =
                dynamic_cast<const ChangeLayer*>(change);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Set the layer
        model.set_layer(a, MIN_LAYER);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_layer_change());
        {
            const ChangeLayer *the_change =
                dynamic_cast<const ChangeLayer*>(change);
            CHECK_EQUAL(ChangeLayer(a, MIN_LAYER), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Set the layer to the same layer, and ensure that no change is
        // fired
        model.set_layer(a, MIN_LAYER);
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_layer_extremes)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);

        // First, put the client on the top
        model.set_layer(a, MIN_LAYER);
        changes.flush();

        // Then, try to move it up and ensure no changes occurred
        model.down_layer(a);
        CHECK(!changes.has_more());

        // Put the client on the bottom and run the same test, backwards
        model.set_layer(a, MAX_LAYER);
        changes.flush();

        model.up_layer(a);
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_client_desktop_change)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // First, move the client ahead and make sure it changes accordingly
        model.client_next_desktop(a);

        // The client should lose the focus, since it will not be visible soon
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0], model.USER_DESKTOPS[1]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the client behind and make sure it returns to its current position
        model.client_prev_desktop(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[1], model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the client back one more time and make sure that it wraps to
        // the last desktop
        model.client_prev_desktop(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0], model.USER_DESKTOPS[max_desktops - 1]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the client ahead and make sure that it wraps to the first desktop
        model.client_next_desktop(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[max_desktops - 1], model.USER_DESKTOPS[0]),
                *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the client, and then reset its desktop to the current one, and
        // ensure that we get the change
        model.client_next_desktop(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0], model.USER_DESKTOPS[1]),
                *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        model.client_reset_desktop(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[1], model.USER_DESKTOPS[0]),
                *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // With the client's desktop reset, we shouldn't get any changes from
        // resetting the desktop again
        model.client_reset_desktop(a);

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_client_desktop_change_child_loses_focus)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        model.add_child(a, b);

        CHECK_EQUAL(b, model.get_focused());
        changes.flush();

        // First, move the client ahead and make sure it changes accordingly
        model.client_next_desktop(a);

        // The child should lose the focus, since it will not be visible soon
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, None), *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0], model.USER_DESKTOPS[1]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_bad_client_desktop_change)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // First off, iconified clients cannot have their desktops changed
#define FLUSH_AFTER(expr) expr ; changes.flush()
        FLUSH_AFTER(model.iconify(a));
        model.client_next_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.deiconify(a));

        FLUSH_AFTER(model.iconify(a));
        model.client_prev_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.deiconify(a));

        FLUSH_AFTER(model.iconify(a));
        model.client_reset_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.deiconify(a));

        // Secondly, moving clients cannot be changed
        FLUSH_AFTER(model.start_moving(a));
        model.client_next_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_moving(a));
        model.client_prev_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_moving(a));
        model.client_reset_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        // Neither can resizing clients
        FLUSH_AFTER(model.start_resizing(a));
        model.client_next_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_resizing(a));
        model.client_prev_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_resizing(a));
        model.client_reset_desktop(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));

#undef FLUSH_AFTER
    }

    TEST_FIXTURE(ClientModelFixture, test_desktop_change)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        model.next_desktop();

        // The current should lose the focus, since it will not be visible soon
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change =
                dynamic_cast<const ChangeCurrentDesktop*>(change);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[0], model.USER_DESKTOPS[1]),
                *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the current behind and make sure it returns to its current position
        model.prev_desktop();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change =
                dynamic_cast<const ChangeCurrentDesktop*>(change);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[1],
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        // We should focus back onto a, since it was the last foucsed window
        // in the previous desktop
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change = dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the desktop back one more time and make sure that it wraps to
        // the last
        model.prev_desktop();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change = dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change =
                dynamic_cast<const ChangeCurrentDesktop*>(change);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[0],
                model.USER_DESKTOPS[max_desktops - 1]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Move the desktop ahead and make sure that it wraps to the first
        model.next_desktop();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change =
                dynamic_cast<const ChangeCurrentDesktop*>(change);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[max_desktops - 1],
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change = dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_desktop_change_child_loses_focus)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        model.add_child(a, b);

        CHECK_EQUAL(b, model.get_focused());
        changes.flush();

        model.next_desktop();

        // The child should lose the focus, since it will not be visible soon
        // The focus should also not revert to the parent, since the parent won't
        // be either
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, None), *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change =
                dynamic_cast<const ChangeCurrentDesktop*>(change);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[0], model.USER_DESKTOPS[1]),
                *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_bad_desktop_change)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

#define FLUSH_AFTER(expr) expr ; changes.flush()
        // The desktop can't be changed while a window is moving
        FLUSH_AFTER(model.start_moving(a));
        model.next_desktop();

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_moving(a));
        model.prev_desktop();

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        // The desktop can't be changed while a window is resizing
        FLUSH_AFTER(model.start_resizing(a));
        model.next_desktop();

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_resizing(a));
        model.prev_desktop();

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));
#undef FLUSH_AFTER
    }

    TEST_FIXTURE(ClientModelFixture, test_stick_does_not_lose_focus)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // Ensure that a window which is stuck does not lose its focus when
        // it is moved around
        model.toggle_stick(a);
        changes.flush();

        model.next_desktop();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change =
                dynamic_cast<const ChangeCurrentDesktop*>(change);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[0],
                model.USER_DESKTOPS[1]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Unstick it, and ensure that it was moved onto the current desktop.
        // This should not cause any focus changes.
        model.toggle_stick(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.ALL_DESKTOPS, model.USER_DESKTOPS[1]),
                *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_iconify)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // First, iconify the client. Ensure that it loses the focus, and that
        // we get the notification about the desktop change.
        model.iconify(a);

        Change const * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None),
                *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0],
                model.ICON_DESKTOP), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Then, deiconify it - ensure that it lands on the current desktop,
        // and that it regains the focus once it is on the current desktop.
        //
        // In order to ensure that the client lands on the currently visible
        // desktop, change the desktop to the next one.
        model.next_desktop();
        changes.flush();

        model.deiconify(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.ICON_DESKTOP,
                model.USER_DESKTOPS[1]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a),
                *the_change);

            CHECK_EQUAL(a, model.get_focused());
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_bad_iconify)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

#define FLUSH_AFTER(expr) expr ; changes.flush()
        // A window which is not iconified, cannot be deiconified
        model.deiconify(a);
        CHECK(!changes.has_more());

        // A window cannot be iconified while it is being moved
        FLUSH_AFTER(model.start_moving(a));
        model.iconify(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        // A window cannot be iconified while it is being resized
        FLUSH_AFTER(model.start_resizing(a));
        model.iconify(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));
#undef FLUSH_AFER
    }

    TEST_FIXTURE(ClientModelFixture, test_moving)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // Start moving the client - ensure that it is unfocused, and that it
        // its desktop changes
        model.start_moving(a);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0],
                model.MOVING_DESKTOP), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Stop moving the client, and ensure that we get a move event back after the client is restored.
        model.stop_moving(a, Dimension2D(42, 43));

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.MOVING_DESKTOP,
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 42, 43), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);

            CHECK_EQUAL(a, model.get_focused());
        }
        delete change;

        CHECK(!changes.has_more());
    }

    /*
     * Similar to test_moving, the only difference is that this ensures that
     * the window isn't refocused after it stops moving when the window is
     * created with nofocus.
     */
    TEST_FIXTURE(ClientModelFixture, test_moving_noflush)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), false);
        changes.flush();

        // Start moving the client - ensure that its desktop changes
        model.start_moving(a);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0],
                model.MOVING_DESKTOP), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Stop moving the client, and ensure that we get a move event back after the client is restored.
        model.stop_moving(a, Dimension2D(42, 43));

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.MOVING_DESKTOP,
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 42, 43), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_bad_moving)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

#define FLUSH_AFTER(expr) expr ; changes.flush()
        // A window which is not moving, cannot cease moving
        model.stop_moving(a, Dimension2D(1, 1));
        CHECK(!changes.has_more());

        // A window cannot be moved while it is iconified
        FLUSH_AFTER(model.iconify(a));
        model.start_moving(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.deiconify(a));

        // A window cannot be moved while it is being resized
        FLUSH_AFTER(model.start_resizing(a));
        model.start_moving(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));

        // A window cannot be moved while *any* other window is being resized
        // or moved.
        model.add_client(b, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        FLUSH_AFTER(model.start_moving(b));
        model.start_moving(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(b, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_resizing(b));
        model.start_moving(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(b, Dimension2D(1, 1)));

#undef FLUSH_AFER
    }

    TEST_FIXTURE(ClientModelFixture, test_resizing)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // Start resizing the client - ensure that it is unfocused, and that
        // it its desktop changes
        model.start_resizing(a);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);

            CHECK_EQUAL(None, model.get_focused());
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0],
                model.RESIZING_DESKTOP), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Stop moving the client, and ensure that we get a move event back
        // after the client is restored.
        model.stop_resizing(a, Dimension2D(42, 43));

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.RESIZING_DESKTOP,
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_size_change());
        {
            const ChangeSize *the_change =
                dynamic_cast<const ChangeSize*>(change);
            CHECK_EQUAL(ChangeSize(a, 42, 43), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);

            CHECK_EQUAL(a, model.get_focused());
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_resizing_nofocus)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), false);
        changes.flush();

        // Start resizing the client - ensure that it is unfocused, and that
        // it its desktop changes
        model.start_resizing(a);

        const Change * change = changes.get_next();
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0],
                model.RESIZING_DESKTOP), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Stop moving the client, and ensure that we get a move event back
        // after the client is restored.
        model.stop_resizing(a, Dimension2D(42, 43));

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.RESIZING_DESKTOP,
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_size_change());
        {
            const ChangeSize *the_change =
                dynamic_cast<const ChangeSize*>(change);
            CHECK_EQUAL(ChangeSize(a, 42, 43), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_bad_resizing)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

#define FLUSH_AFTER(expr) expr ; changes.flush()
        // A window which is not resizing, cannot cease resizing
        model.stop_resizing(a, Dimension2D(1, 1));
        CHECK(!changes.has_more());

        // A window cannot be resized while it is iconified
        FLUSH_AFTER(model.iconify(a));
        model.start_resizing(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.deiconify(a));

        // A window cannot be resized while it is being moved
        FLUSH_AFTER(model.start_moving(a));
        model.start_resizing(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        // A window cannot be moved while *any* other window is being resized
        // or moved.
        model.add_client(b, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        FLUSH_AFTER(model.start_moving(b));
        model.start_resizing(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_moving(b, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_resizing(b));
        model.start_resizing(a);

        CHECK(!changes.has_more());
        FLUSH_AFTER(model.stop_resizing(b, Dimension2D(1, 1)));

        // Unfocus whatever is currently focused, so that it doesn't taint
        // the ChangeFocus event in the next test
        FLUSH_AFTER(model.unfocus());

        // When resizing, giving an invalid size should restore the window's
        // desktop and focus, but should *not* trigger a ChangeSize event
        FLUSH_AFTER(model.start_resizing(a));

        model.stop_resizing(a, Dimension2D(0, 0));

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.RESIZING_DESKTOP,
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);

            CHECK_EQUAL(a, model.get_focused());
        }
        delete change;

        CHECK(!changes.has_more());
#undef FLUSH_AFER
    }

    TEST_FIXTURE(ClientModelFixture, test_toggle_stick)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // First, stick a client and ensure that we get the desktop change event
        model.toggle_stick(a);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0],
                model.ALL_DESKTOPS), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Then, unstick it and ensure that we get the change event again
        model.toggle_stick(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change =
                dynamic_cast<const ChangeClientDesktop*>(change);
            CHECK_EQUAL(ChangeClientDesktop(a, model.ALL_DESKTOPS,
                model.USER_DESKTOPS[0]), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_focus_unfocus)
    {
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // First, ensure that a is focused
        CHECK_EQUAL(a, model.get_focused());

        // Then, unfocus a and check for the event
        model.unfocus();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Refocus and ensure that another event is fired
        model.focus(a);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_location_size_changers)
    {
        // Avoid screen changes when positions change
        std::vector<Box> screens;
        screens.push_back(Box(0, 0, 1000, 1000));
        model.update_screens(screens);

        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        // First, move the window manually and ensure a change happens
        model.change_location(a, 100, 100);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 100, 100), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Then, cause a resize and check for the event
        model.change_size(a, 100, 100);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_size_change());
        {
            const ChangeSize *the_change =
                dynamic_cast<const ChangeSize*>(change);
            CHECK_EQUAL(ChangeSize(a, 100, 100), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Finally, try to use an invalid size, and ensure that no change is
        // propagated
        model.change_size(a, -1, -1);

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_stick_retention)
    {
        // Ensure that a stuck window, after being iconified and deiconified,
        // gets put back onto ALL_DESKTOPS
        model.add_client(a, IS_VISIBLE,
            Dimension2D(1, 1), Dimension2D(1, 1), true);
        model.toggle_stick(a);

        model.iconify(a);
        model.deiconify(a);
        changes.flush();
        // Go ahead and check the events, to ensure that it was
        // deiconified onto ALL_DESKTOPS
        CHECK_EQUAL(model.find_desktop(a), model.ALL_DESKTOPS);

        // Ensure that, after moving and resizing, a stuck window will end
        // up on ALL_DESKTOPS
        model.start_moving(a);
        model.stop_moving(a, Dimension2D(1, 1));
        changes.flush();

        CHECK_EQUAL(model.find_desktop(a), model.ALL_DESKTOPS);

        model.start_resizing(a);
        model.stop_resizing(a, Dimension2D(1, 1));
        changes.flush();

        CHECK_EQUAL(model.find_desktop(a), model.ALL_DESKTOPS);
    }

    TEST_FIXTURE(ClientModelFixture, test_mode_change)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1), true);
        changes.flush();

        CHECK_EQUAL(model.get_mode(a), CPS_FLOATING);

        // Start out by changing to floating. This should cause no events, since
        // all windows are floating by default
        model.change_mode(a, CPS_FLOATING);

        const Change * change;
        CHECK(!changes.has_more());

        // Go through all the other kinds of modes, and change to them.
        // Ensure that they produce the right kind of events
        ClientPosScale modes[] = {CPS_SPLIT_LEFT, CPS_SPLIT_RIGHT, CPS_SPLIT_TOP, CPS_SPLIT_BOTTOM, CPS_MAX};
        for (int idx = 0; idx < sizeof(modes) / sizeof(ClientPosScale); idx++)
        {
            ClientPosScale mode = modes[idx];
            model.change_mode(a, mode);

            const Change * change = changes.get_next();
            CHECK(change != 0);
            CHECK(change->is_mode_change());
            {
                const ChangeCPSMode *the_change =
                    dynamic_cast<const ChangeCPSMode*>(change);
                CHECK_EQUAL(the_change->window, a);
                CHECK_EQUAL(the_change->mode, mode);
            }
            delete change;

            CHECK(!changes.has_more());

            CHECK_EQUAL(model.get_mode(a), mode);
        }

        // Finally, check the floating mode, since we're changing over from CPS_MAX
        model.change_mode(a, CPS_FLOATING);

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_mode_change());
        {
            const ChangeCPSMode *the_change =
                dynamic_cast<const ChangeCPSMode*>(change);
            CHECK_EQUAL(the_change->window, a);
            CHECK_EQUAL(the_change->mode, CPS_FLOATING);
        }
        delete change;

        CHECK(!changes.has_more());

        CHECK_EQUAL(model.get_mode(a), CPS_FLOATING);
    }

    TEST_FIXTURE(ClientModelFixture, test_screen_shift)
    {
        // Each of these test cases moves the window to a given relative
        // screen, and checks to see that the bounding box of the screen it
        // ends up on is the same as the box given by the last four elements
        ChangeScreenTest tests[] = {
            {DIR_TOP, 100, 0, 100, 100},
            {DIR_BOTTOM, 100, 200, 100, 100},
            {DIR_LEFT, 0, 100, 100, 100},
            {DIR_RIGHT, 200, 100, 100, 100},
        };

        for (int test = 0; test < sizeof(tests) / sizeof(tests[0]); test++)
        {
            model.add_client(a, IS_VISIBLE, Dimension2D(100, 100), Dimension2D(1, 1), true);
            changes.flush();


            ChangeScreenTest &the_test = tests[test];
            model.to_relative_screen(a, the_test.direction);

            const Change *change = changes.get_next();
            CHECK(change != 0);
            CHECK(change->is_screen_change());
            {
                const ChangeScreen *the_change =
                    dynamic_cast<const ChangeScreen*>(change);

                Box dest_box(the_test.box_x, the_test.box_y,
                             the_test.box_width, the_test.box_height);
                CHECK_EQUAL(ChangeScreen(a, dest_box), *the_change);
            }
            delete change;

        CHECK(!changes.has_more());

            model.remove_client(a);
            changes.flush();
        }

        // Ensure that no change occurs if we move it to an invalid screen
        model.add_client(a, IS_VISIBLE, Dimension2D(0, 0), Dimension2D(1, 1), true);
        changes.flush();

        model.to_relative_screen(a, DIR_LEFT);

        CHECK(!changes.has_more());

        model.remove_client(a);
        changes.flush();

        // Ensure that it isn't moved anywhere if we start from an invalid
        // place
        model.add_client(a, IS_VISIBLE, Dimension2D(-1, -1), Dimension2D(1, 1), true);
        changes.flush();

        // This *would* be valid, if the location weren't off-screen
        model.to_relative_screen(a, DIR_RIGHT);

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_screen_box)
    {
        // Each of these test cases moves the client into the monitor given by
        // the box defined in the struct, and ensures that the client ends up
        // in the screen (which is defined by the same box)
        Box tests[] = {
            Box(0, 0, 100, 100),
            Box(100, 0, 100, 100),
            Box(200, 0, 100, 100),
            Box(0, 100, 100, 100),
            // Avoid the middle screen, since no change will be emitted
            Box(200, 100, 100, 100),
            Box(0, 200, 100, 100),
            Box(100, 200, 100, 100),
            Box(200, 200, 100, 100),
        };

        for (int test = 0; test < sizeof(tests) / sizeof(tests[0]); test++)
        {
            model.add_client(a, IS_VISIBLE, Dimension2D(100, 100), Dimension2D(1, 1), true);
            changes.flush();

            CHECK_EQUAL(model.get_screen(a), Box(100, 100, 100, 100));

            Box &the_test = tests[test];
            model.to_screen_box(a, the_test);

            const Change *change = changes.get_next();
            CHECK(change != 0);
            CHECK(change->is_screen_change());
            {
                const ChangeScreen *the_change =
                    dynamic_cast<const ChangeScreen*>(change);

                CHECK_EQUAL(ChangeScreen(a, the_test), *the_change);
            }
            delete change;

        CHECK(!changes.has_more());

            CHECK_EQUAL(model.get_screen(a), the_test);

            model.remove_client(a);
            changes.flush();
        }

        // Ensure that moving to a non-existent screen does nothing
        model.add_client(a, IS_VISIBLE, Dimension2D(100, 100), Dimension2D(1, 1), true);
        changes.flush();

        model.to_screen_box(a, Box(-1, -1, 100, 100));

        CHECK(!changes.has_more());

        CHECK_EQUAL(model.get_screen(a), Box(100, 100, 100, 100));

        model.remove_client(a);
        changes.flush();

        // Ensure that moving to the same screen does nothing
        model.add_client(a, IS_VISIBLE, Dimension2D(100, 100), Dimension2D(1, 1), true);
        changes.flush();

        model.to_screen_box(a, Box(100, 100, 100, 100));

        CHECK(!changes.has_more());

        CHECK_EQUAL(model.get_screen(a), Box(100, 100, 100, 100));

        model.remove_client(a);
        changes.flush();
    }

    TEST_FIXTURE(ClientModelFixture, test_screen_update)
    {
        Box tests_start_box[] = {
            Box(100, 100, 1, 1),
            Box(200, 200, 1, 1),
        };

        for (int test = 0; test < sizeof(tests_start_box) / sizeof(tests_start_box[0]); test++)
        {
            Box &start = tests_start_box[test];
            model.add_client(a, IS_VISIBLE, Dimension2D(start.x, start.y), Dimension2D(start.width, start.height), true);
            changes.flush();

            // Each client should start out on different screens, but they should all end up
            // in the same large box when we update the screen configuration
            std::vector<Box> screens;
            screens.push_back(Box(0, 0, 1000, 1000));

            model.update_screens(screens);

            const Change *change = changes.get_next();
            CHECK(change != 0);
            CHECK(change->is_screen_change());
            {
                const ChangeScreen *the_change =
                    dynamic_cast<const ChangeScreen*>(change);

                CHECK_EQUAL(ChangeScreen(a, screens[0]), *the_change);
            }
            delete change;

            CHECK(!changes.has_more());

            CHECK_EQUAL(model.get_screen(a), Box(0, 0, 1000, 1000));

            model.remove_client(a);
            reset_screen_graph();
            changes.flush();
        }

        // Now test a client that is not on any screen
        model.add_client(a, IS_VISIBLE, Dimension2D(-1, -1), Dimension2D(1, 1), true);
        changes.flush();

        CHECK_EQUAL(model.get_screen(a), Box(-1, -1, 0, 0));

        // A client which wasn't on  a screen to start shouldn't end up on a
        // screen later
        std::vector<Box> screens;
        screens.push_back(Box(0, 0, 1000, 1000));

        model.update_screens(screens);

        CHECK(!changes.has_more());

        model.remove_client(a);
        changes.flush();
    }

    /**
     * This ensures that windows which are unmapped emit an UnmapChange.
     */
    TEST_FIXTURE(ClientModelFixture, test_unmap)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(-1, -1), Dimension2D(1, 1), true);
        changes.flush();

        model.unmap_client(a);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_unmap_change());
        {
            const UnmapChange *the_change =
                dynamic_cast<const UnmapChange*>(change);
            CHECK_EQUAL(UnmapChange(a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    /**
     * This ensures that windows which are remapped should emit a LayerChange update
     */
    TEST_FIXTURE(ClientModelFixture, test_remap)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(-1, -1), Dimension2D(1, 1), true);

        model.unmap_client(a);
        changes.flush();

        model.remap_client(a);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_layer_change());
        {
            const ChangeLayer *the_change =
                dynamic_cast<const ChangeLayer*>(change);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    /**
     * This ensures that windows have their information assigned properly,
     * without doing any packing.
     */
    TEST_FIXTURE(ClientModelFixture, test_pack_basic_info)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.pack_client(a, PACK_NORTHWEST, 1);

        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(30, 10), true);

        // Ensure that all the basic information is there
        CHECK(model.is_packed_client(a));
        CHECK(!model.is_packed_client(b));
        CHECK_EQUAL(PACK_NORTHWEST, model.get_pack_corner(a));
    }

    /// Ensure that windows pack correctly into the northeast
    TEST_FIXTURE(ClientModelFixture, test_pack_northwest)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.pack_client(a, PACK_NORTHWEST, 1);

        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(30, 10), true);
        model.pack_client(b, PACK_NORTHWEST, 2);

        changes.flush();

        model.repack_corner(PACK_NORTHWEST);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 0, 0), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(b, 10, 0), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    /// Ensure that windows pack correctly into the northeast
    TEST_FIXTURE(ClientModelFixture, test_pack_northeast)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.pack_client(a, PACK_NORTHEAST, 1);

        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(30, 10), true);
        model.pack_client(b, PACK_NORTHEAST, 2);

        changes.flush();

        model.repack_corner(PACK_NORTHEAST);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 100 - 10, 0), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(b, 100 - (10 + 30), 0), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    /// Ensure that windows pack correctly into the southwest
    TEST_FIXTURE(ClientModelFixture, test_pack_southwest)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.pack_client(a, PACK_SOUTHWEST, 1);

        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(30, 10), true);
        model.pack_client(b, PACK_SOUTHWEST, 2);

        changes.flush();

        model.repack_corner(PACK_SOUTHWEST);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 0, 100 - 10), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(b, 10, 100 - 10), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    /// Ensure that windows pack correctly into the southeast
    TEST_FIXTURE(ClientModelFixture, test_pack_southeast)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.pack_client(a, PACK_SOUTHEAST, 1);

        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(30, 10), true);
        model.pack_client(b, PACK_SOUTHEAST, 2);

        changes.flush();

        model.repack_corner(PACK_SOUTHEAST);

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(a, 100 - 10, 100 - 10), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_location_change());
        {
            const ChangeLocation *the_change =
                dynamic_cast<const ChangeLocation*>(change);
            CHECK_EQUAL(ChangeLocation(b, 100 - (10 + 30), 100 - 10), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_child_add)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        changes.flush();

        model.add_child(a, b);

        // Make sure that the change propagates outside the model
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_child_add_change());
        {
            const ChildAddChange *the_change =
                dynamic_cast<const ChildAddChange*>(change);
            CHECK_EQUAL(ChildAddChange(a, b), *the_change);
        }
        delete change;

        // Children of autofocusable windows should themselves be focused
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Ensure that the model changes in the expected ways
        CHECK(model.is_child(b));
        CHECK_EQUAL(a, model.get_parent_of(b));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(1, children.size());
        CHECK_EQUAL(b, children[0]);

        // Children still aren't actual clients, though
        CHECK(!model.is_client(b));
    }

    TEST_FIXTURE(ClientModelFixture, test_child_add_no_autofocus)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), false);
        changes.flush();

        model.add_child(a, b);

        // Make sure that the change propagates outside the model
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_child_add_change());
        {
            const ChildAddChange *the_change =
                dynamic_cast<const ChildAddChange*>(change);
            CHECK_EQUAL(ChildAddChange(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Ensure that the model changes in the expected ways
        CHECK(model.is_child(b));
        CHECK_EQUAL(a, model.get_parent_of(b));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(1, children.size());
        CHECK_EQUAL(b, children[0]);

        // Children still aren't actual clients, though
        CHECK(!model.is_client(b));
    }

    TEST_FIXTURE(ClientModelFixture, test_remove_child_focus_parent)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.add_child(a, b);
        changes.flush();

        model.remove_child(b, true);

        // Deleting the child should restore focus to the parent
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_child_remove_change());
        {
            const ChildRemoveChange *the_change =
                dynamic_cast<const ChildRemoveChange*>(change);
            CHECK_EQUAL(ChildRemoveChange(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Ensure that the model changes in the expected ways
        CHECK(!model.is_child(b));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(0, children.size());
    }

    TEST_FIXTURE(ClientModelFixture, test_remove_child_unfocus)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.add_child(a, b);
        changes.flush();

        model.remove_child(b, false);

        // Deleting the child should unfocus it
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, None), *the_change);
        }
        delete change;

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_child_remove_change());
        {
            const ChildRemoveChange *the_change =
                dynamic_cast<const ChildRemoveChange*>(change);
            CHECK_EQUAL(ChildRemoveChange(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Ensure that the model changes in the expected ways
        CHECK(!model.is_child(b));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(0, children.size());
    }

    TEST_FIXTURE(ClientModelFixture, test_cannot_add_duplicate_children)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.add_child(a, b);
        changes.flush();
        model.add_child(a, b);

        // Duplicate children shouldn't trigger any changes
        CHECK(!changes.has_more());

        // All the standard child assumptions should still hold
        CHECK(model.is_child(b));
        CHECK_EQUAL(a, model.get_parent_of(b));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(1, children.size());
        CHECK_EQUAL(b, children[0]);

        CHECK(!model.is_client(b));
    }

    TEST_FIXTURE(ClientModelFixture, test_cannot_add_orphans)
    {
        model.add_child(a, b);
        changes.flush();

        // Trying to add a child for a non-existent parent should do nothing
        CHECK(!changes.has_more());
        CHECK(!model.is_child(b));
    }

    TEST_FIXTURE(ClientModelFixture, test_cannot_adopt)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.add_child(a, c);
        changes.flush();
        model.add_child(b, c);

        // Children can't be moved between parents, so this should do nothing
        CHECK(!changes.has_more());

        // All the standard child assumptions should still hold
        CHECK(model.is_child(c));
        CHECK_EQUAL(a, model.get_parent_of(c));

        std::vector<Window> children;
        model.get_children_of(a, children);
        CHECK_EQUAL(1, children.size());
        CHECK_EQUAL(c, children[0]);

        children.clear();
        model.get_children_of(b, children);
        CHECK_EQUAL(0, children.size());
    }

    TEST_FIXTURE(ClientModelFixture, test_removing_parent_removes_children)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.add_child(a, b);
        changes.flush();

        model.remove_client(a);

        // First, the child should unfocus itself when destroyed
        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, None), *the_change);
        }
        delete change;

        // Removing the child
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_child_remove_change());
        {
            const ChildRemoveChange *the_change =
                dynamic_cast<const ChildRemoveChange*>(change);
            CHECK_EQUAL(ChildRemoveChange(a, b), *the_change);
        }
        delete change;

        // Finally, the parent should remove itself
        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_destroy_change());
        {
            const DestroyChange *the_change = dynamic_cast<const DestroyChange*>(change);
            Desktop *desktop(new UserDesktop(0));
            CHECK_EQUAL(DestroyChange(a, desktop, DEF_LAYER), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // The both the child and the parent should be gone now
        CHECK(!model.is_child(b));
        CHECK(!model.is_client(a));
    }

    TEST_FIXTURE(ClientModelFixture, test_single_client_focus_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        changes.flush();

        // Trying to cycle the focus with only one client should do nothing
        model.cycle_focus_forward();
        CHECK(!changes.has_more());

        model.cycle_focus_backward();
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_two_client_focus_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        changes.flush();

        // b should be focused since it was set to autofocus on creation
        CHECK_EQUAL(model.get_focused(), b);

        // Going forward should focus a
        model.cycle_focus_forward();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Doing so again should wrap back to b
        model.cycle_focus_forward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Going back should wrap back to a
        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // We should get back to b after going back again
        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_two_client_with_child_focus_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        // c should be after a, even though we only knew about c after b
        model.add_child(a, c);
        changes.flush();

        // c should be focused since a was set to autofocus
        CHECK_EQUAL(model.get_focused(), c);

        // Going forward should focus b
        model.cycle_focus_forward();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(c, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Doing so again should wrap back to a
        model.cycle_focus_forward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Going back should wrap back to b
        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // We should get back to c after going back again
        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, c), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }
    TEST_FIXTURE(ClientModelFixture, test_all_desktop_clients)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        // The remainder should be the same as in the original case, since the
        // all desktop is slaved to each user desktop, and gets used when the
        // user desktop's focus is exhausted
        changes.flush();

        // b should be focused since it was set to autofocus on creation
        CHECK_EQUAL(model.get_focused(), b);

        // Going forward should focus a
        model.cycle_focus_forward();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Doing so again should wrap back to b
        model.cycle_focus_forward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // Going back should wrap back to a
        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // We should get back to b after going back again
        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_removed_not_in_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        changes.flush();

        model.remove_client(a);
        changes.flush();

        // With a gone, the focus should have nowhere to go
        model.cycle_focus_forward();
        CHECK(!changes.has_more());

        model.cycle_focus_backward();
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_removed_children_not_in_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_child(a, c);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        changes.flush();

        model.remove_client(a);
        changes.flush();

        // With a and c gone, the focus should have nowhere to go
        model.cycle_focus_forward();
        CHECK(!changes.has_more());

        model.cycle_focus_backward();
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_set_focus_updates_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(c, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        CHECK_EQUAL(model.get_focused(), c);

        // We should be able to set the focus on b and go forward to c
        model.focus(b);
        changes.flush();

        model.cycle_focus_forward();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, c), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        // We should be able to set the focus on b and go backward to a
        model.focus(b);
        changes.flush();

        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_unmapped_not_in_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.unmap_client(a);
        changes.flush();

        // With a gone, the focus should have nowhere to go
        model.cycle_focus_forward();
        CHECK(!changes.has_more());

        model.cycle_focus_backward();
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_unmapped_children_not_in_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_child(a, c);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.unmap_client(a);
        changes.flush();

        // With a and c gone, the focus should have nowhere to go
        model.cycle_focus_forward();
        CHECK(!changes.has_more());

        model.cycle_focus_backward();
        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_remapped_in_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.unmap_client(a);
        model.remap_client(a);
        changes.flush();

        // Remapping sets focus, so we should be able to cycle through both a 
        // and b
        model.cycle_focus_forward();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, b), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(b, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }

    TEST_FIXTURE(ClientModelFixture, test_remapped_children_in_cycle)
    {
        model.add_client(a, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);
        model.add_child(a, c);
        model.add_client(b, IS_VISIBLE, Dimension2D(20, 20), Dimension2D(10, 10), true);

        model.unmap_client(a);
        model.remap_client(a);
        changes.flush();

        // We should have focused a, which puts its child after
        model.cycle_focus_forward();

        const Change * change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(a, c), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());

        model.cycle_focus_backward();

        change = changes.get_next();
        CHECK(change != 0);
        CHECK(change->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(change);
            CHECK_EQUAL(ChangeFocus(c, a), *the_change);
        }
        delete change;

        CHECK(!changes.has_more());
    }
}

int main()
{
    return UnitTest::RunAllTests();
}
