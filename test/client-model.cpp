#include <algorithm>
#include <utility>

#include <UnitTest++.h>
#include "client-model.h"

const Window a = 1,
      b = 2,
      c = 3;

const unsigned long long max_desktops = 5;

struct ClientModelFixture
{
    ClientModelFixture() :
        model(max_desktops)
    {};

    ~ClientModelFixture()
    {};

    ClientModel model;
};

SUITE(ClientModelMemberSuite)
{
    TEST_FIXTURE(ClientModelFixture, test_default_members)
    {
        // Make sure that there are no clients by default
        CHECK_EQUAL(false, model.is_client(a));
        CHECK_EQUAL(false, model.is_client(b));

        // Add a new client, and ensure that it is present
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1));

        // Make sure that a is now listed as a client
        CHECK_EQUAL(true, model.is_client(a));

        // Check the event stream for the most recent events
        ClientModel::change_iter iterator = model.changes_begin();

        // First, the window appears on a desktop
        CHECK((*iterator)->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change = 
                dynamic_cast<const ChangeClientDesktop*>(*iterator);
            const Desktop *desktop(new UserDesktop(0));
            CHECK_EQUAL(ChangeClientDesktop(a, desktop), *the_change);
        }
        iterator++;

        // Secondly, it is stacked relative to other windows
        {
            const ChangeLayer *the_change = dynamic_cast<const ChangeLayer*>(*iterator);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER), *the_change);
        }
        iterator++;

        // Make sure it was focused
        {
            const ChangeFocus *the_change = dynamic_cast<const ChangeFocus*>(*iterator);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        iterator++;

        // Finally, this is the end of the event stream
        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Then, remove the added client
        model.remove_client(a);
        CHECK_EQUAL(false, model.is_client(a));
    }

    TEST_FIXTURE(ClientModelFixture, test_visibility)
    {
        // Add a new client and ensure that it is present
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1));

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

    TEST_FIXTURE(ClientModelFixture, test_finder_functions)
    {
        // Make sure that the `find_*` functions return the correct results
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1));

        const Desktop *desktop_of = model.find_desktop(a);
        CHECK(*desktop_of == UserDesktop(0));
        CHECK(model.find_layer(a) == DEF_LAYER);
    }

    TEST_FIXTURE(ClientModelFixture, test_getters)
    {
        // First, ensure that `get_clients_of` gets only clients on the given
        // desktop
        model.add_client(a, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1));
        model.add_client(b, IS_VISIBLE, Dimension2D(1, 1), Dimension2D(1, 1));

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
            Dimension2D(1, 1), Dimension2D(1, 1));
        model.flush_changes();

        // Up
        model.up_layer(a);
        ClientModel::change_iter iterator = model.changes_begin();

        CHECK((*iterator)->is_layer_change());
        {
            const ChangeLayer *the_change = 
                dynamic_cast<const ChangeLayer*>(*iterator);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER + 1), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Down
        model.down_layer(a);
        iterator = model.changes_begin();

        CHECK((*iterator)->is_layer_change());
        {
            const ChangeLayer *the_change = 
                dynamic_cast<const ChangeLayer*>(*iterator);
            CHECK_EQUAL(ChangeLayer(a, DEF_LAYER), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Set the layer
        model.set_layer(a, MIN_LAYER);
        iterator = model.changes_begin();

        CHECK((*iterator)->is_layer_change());
        {
            const ChangeLayer *the_change = 
                dynamic_cast<const ChangeLayer*>(*iterator);
            CHECK_EQUAL(ChangeLayer(a, MIN_LAYER), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Set the layer to the same layer, and ensure that no change is
        // fired
        model.set_layer(a, MIN_LAYER);
        iterator = model.changes_begin();

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();
    }

    TEST_FIXTURE(ClientModelFixture, test_layer_extremes)
    {
        model.add_client(a, IS_VISIBLE, 
            Dimension2D(1, 1), Dimension2D(1, 1));

        // First, put the client on the top
        model.set_layer(a, MIN_LAYER);
        model.flush_changes();

        // Then, try to move it up and ensure no changes occurred
        model.down_layer(a);
        CHECK_EQUAL(model.changes_begin(), model.changes_end());

        // Put the client on the bottom and run the same test, backwards
        model.set_layer(a, MAX_LAYER);
        model.flush_changes();

        model.up_layer(a);
        CHECK_EQUAL(model.changes_begin(), model.changes_end());
    }

    TEST_FIXTURE(ClientModelFixture, test_client_desktop_change)
    {
        model.add_client(a, IS_VISIBLE, 
            Dimension2D(1, 1), Dimension2D(1, 1));
        model.flush_changes();

        // First, move the client ahead and make sure it changes accordingly
        model.client_next_desktop(a);
        ClientModel::change_iter iterator = model.changes_begin();

        // The client should lose the focus, since it will not be visible soon
        CHECK((*iterator)->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(*iterator);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);
        }

        iterator++;
        CHECK((*iterator)->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change = 
                dynamic_cast<const ChangeClientDesktop*>(*iterator);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[1]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Move the client behind and make sure it returns to its current position
        model.client_prev_desktop(a);
        iterator = model.changes_begin();

        CHECK((*iterator)->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change = 
                dynamic_cast<const ChangeClientDesktop*>(*iterator);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Move the client back one more time and make sure that it wraps to
        // the last desktop
        model.client_prev_desktop(a);
        iterator = model.changes_begin();

        CHECK((*iterator)->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change = 
                dynamic_cast<const ChangeClientDesktop*>(*iterator);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[max_desktops - 1]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Move the client ahead and make sure that it wraps to the first desktop
        model.client_next_desktop(a);
        iterator = model.changes_begin();

        CHECK((*iterator)->is_client_desktop_change());
        {
            const ChangeClientDesktop *the_change = 
                dynamic_cast<const ChangeClientDesktop*>(*iterator);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[0]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();
    }

    TEST_FIXTURE(ClientModelFixture, test_bad_client_desktop_change)
    {
        model.add_client(a, IS_VISIBLE, 
            Dimension2D(1, 1), Dimension2D(1, 1));
        model.flush_changes();

        // First off, iconified clients cannot have their desktops changed
#define FLUSH_AFTER(expr) expr ; model.flush_changes()
        FLUSH_AFTER(model.iconify(a));
        model.client_next_desktop(a);

        CHECK_EQUAL(model.changes_begin(), model.changes_end());
        FLUSH_AFTER(model.deiconify(a));

        FLUSH_AFTER(model.iconify(a));
        model.client_prev_desktop(a);

        CHECK_EQUAL(model.changes_begin(), model.changes_end());
        FLUSH_AFTER(model.deiconify(a));

        // Secondly, moving clients cannot be changed
        FLUSH_AFTER(model.start_moving(a));
        model.client_next_desktop(a);

        CHECK_EQUAL(model.changes_begin(), model.changes_end());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_moving(a));
        model.client_prev_desktop(a);

        CHECK_EQUAL(model.changes_begin(), model.changes_end());
        FLUSH_AFTER(model.stop_moving(a, Dimension2D(1, 1)));

        // Neither can resizing clients
        FLUSH_AFTER(model.start_resizing(a));
        model.client_next_desktop(a);

        CHECK_EQUAL(model.changes_begin(), model.changes_end());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));

        FLUSH_AFTER(model.start_resizing(a));
        model.client_prev_desktop(a);

        CHECK_EQUAL(model.changes_begin(), model.changes_end());
        FLUSH_AFTER(model.stop_resizing(a, Dimension2D(1, 1)));
#undef FLUSH_AFTER
    }

    TEST_FIXTURE(ClientModelFixture, test_desktop_change)
    {
        model.add_client(a, IS_VISIBLE, 
            Dimension2D(1, 1), Dimension2D(1, 1));
        model.flush_changes();

        // Move to the next desktop
        model.next_desktop();
        ClientModel::change_iter iterator = model.changes_begin();

        // The current should lose the focus, since it will not be visible soon
        CHECK((*iterator)->is_focus_change());
        {
            const ChangeFocus *the_change =
                dynamic_cast<const ChangeFocus*>(*iterator);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);
        }

        iterator++;
        CHECK((*iterator)->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change = 
                dynamic_cast<const ChangeCurrentDesktop*>(*iterator);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[1]), 
                *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Move the current behind and make sure it returns to its current position
        model.prev_desktop();
        iterator = model.changes_begin();

        CHECK((*iterator)->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change = 
                dynamic_cast<const ChangeCurrentDesktop*>(*iterator);
            CHECK_EQUAL(ChangeCurrentDesktop(model.
                USER_DESKTOPS[0]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Move the desktop back one more time and make sure that it wraps to
        // the last
        model.prev_desktop();
        iterator = model.changes_begin();

        CHECK((*iterator)->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change = 
                dynamic_cast<const ChangeCurrentDesktop*>(*iterator);
            CHECK_EQUAL(ChangeCurrentDesktop(model.
                USER_DESKTOPS[max_desktops - 1]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Move the desktop ahead and make sure that it wraps to the first
        model.next_desktop();
        iterator = model.changes_begin();

        CHECK((*iterator)->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change = 
                dynamic_cast<const ChangeCurrentDesktop*>(*iterator);
            CHECK_EQUAL(ChangeCurrentDesktop(model.
                USER_DESKTOPS[0]), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();
    }

    TEST_FIXTURE(ClientModelFixture, test_stick_does_not_lose_focus)
    {
        model.add_client(a, IS_VISIBLE, 
            Dimension2D(1, 1), Dimension2D(1, 1));
        model.flush_changes();

        // Ensure that a window which is stuck does not lose its focus when
        // it is moved around
        model.toggle_stick(a);
        model.next_desktop();

        ClientModel::change_iter iterator = model.changes_begin();

        CHECK((*iterator)->is_current_desktop_change());
        {
            const ChangeCurrentDesktop *the_change = 
                dynamic_cast<const ChangeCurrentDesktop*>(*iterator);
            CHECK_EQUAL(ChangeCurrentDesktop(model.USER_DESKTOPS[1]), 
                *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Unstick it, and ensure that it is unfocused
        model.toggle_stick(a);

        ClientModel::change_iter iterator = model.changes_begin();

        CHECK((*iterator)->is_focus_change())
        {
            const ChangeFocus *the_change = 
                dynamic_cast<const ChangeFocus*>(*iterator);
            CHECK_EQUAL(ChangeFocus(a, None), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Go back, so that we can test how changing the desktop of the client
        // works
        model.prev_desktop();
        model.flush_changes();

        // The window should be visible, now that we're on the same desktop as
        // it
        CHECK(model.is_visible(a));

        // Focus it, and ensure that we get the message
        model.focus(a);

        ClientModel::change_iter iterator = model.changes_begin();

        CHECK((*iterator)->is_focus_change())
        {
            const ChangeFocus *the_change = 
                dynamic_cast<const ChangeFocus*>(*iterator);
            CHECK_EQUAL(ChangeFocus(None, a), *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();

        // Stick the window, and then move it and ensure that it doesn't lose
        // focus
        model.client_next_desktop(a);

        ClientModel::change_iter iterator = model.changes_begin();

        CHECK((*iterator)->is_client_desktop_change())
        {
            const ChangeClientDesktop *the_change = 
                dynamic_cast<const ChangeClientDesktop*>(*iterator);
            CHECK_EQUAL(ChangeClientDesktop(a, model.USER_DESKTOPS[1], 
                *the_change);
        }
        iterator++;

        CHECK_EQUAL(model.changes_end(), iterator);
        model.flush_changes();
    }
};

int main()
{
    return UnitTest::RunAllTests();
}
