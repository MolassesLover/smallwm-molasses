#include <csignal>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>

#include "actions.hpp"
#include "clientmodel-events.hpp"
#include "configparse.hpp"
#include "common.hpp"
#include "logging/logging.hpp"
#include "logging/file.hpp"
#include "logging/syslog.hpp"
#include "model/changes.hpp"
#include "model/client-model.hpp"
#include "model/screen.hpp"
#include "model/x-model.hpp"
#include "xdata.hpp"
#include "x-events.hpp"

bool should_execute_dump = false;

/**
 * Triggers a model state dump after the current batch of events has been processed.
 */
void enable_dump(int signal) {
    should_execute_dump = true;
}

/**
 * Prints out X errors to enable diagnosis, but doesn't kill us.
 * @param display The display the error occurred on
 * @param event The error event that happened
 */
int x_error_handler(Display *display, XErrorEvent *event) {
    char err_desc[500];

    XGetErrorText(display, event->error_code, err_desc, 500);

    std::cerr << "X Error: \n"
        "\tdisplay = " << XDisplayName(NULL) << "'\n"
        "\tserial = " << event->serial << "'\n"
        "\terror = '" << err_desc << "\n"
        "\trequest = " << static_cast<int>(event->request_code) << "\n"
        "\tminor = " << static_cast<int>(event->minor_code) << "\n";

    return 0;
}

int main() {
    // Make sure that child processes don't generate zombies. This is an
    // alternative to the wait() reaping loop under POSIX 2001
    signal(SIGCHLD, SIG_IGN);

    XSetErrorHandler(x_error_handler);
    signal(SIGUSR1, enable_dump);

    WMConfig config;
    config.load();

    Log *logger;

    if (config.log_file == std::string("syslog")) {
        SysLog *sys_logger = new SysLog();
        sys_logger->set_identity("SmallWM");
        sys_logger->set_facility(LOG_USER);
        sys_logger->set_log_mask(LOG_UPTO(config.log_mask));
        sys_logger->start();
        logger = sys_logger;
    } else {
        logger = new FileLog(config.log_file, config.log_mask);
    }

    Display *display = XOpenDisplay(NULL);

    if (!display) {
        logger->log(LOG_ERR) <<
            "Could not open X display - terminating" << Log::endl;
        logger->stop();
        delete logger;

        std::exit(2);
    }

    Window default_root = DefaultRootWindow(display);
    XData xdata(*logger, display, default_root, DefaultScreen(display));
    xdata.select_input(default_root,
                       PointerMotionMask |
                       StructureNotifyMask |
                       SubstructureNotifyMask |
                       SubstructureRedirectMask);

    CrtManager crt_manager;
    std::vector<Box> screens;
    xdata.get_screen_boxes(screens);
    crt_manager.rebuild_graph(screens);

    ChangeStream changes;
    #ifdef WITH_BORDERS
    ClientModel clients(changes, crt_manager, config.num_desktops, config.border_width);
    #else
    ClientModel clients(changes, crt_manager, config.num_desktops);
    #endif

    std::vector<Window> existing_windows;
    xdata.get_windows(existing_windows);

    XModel xmodel;
    XEvents x_events(config, xdata, clients, xmodel);

    for (std::vector<Window>::iterator win_iter = existing_windows.begin();
         win_iter != existing_windows.end();
         win_iter++) {
        if (*win_iter != default_root) x_events.add_window(*win_iter);
    }

    ClientModelEvents client_events(config, *logger, changes,
                                    xdata, clients, xmodel);

    // Make sure to process all the changes produced by the class actions for
    // the first set of windows
    client_events.handle_queued_changes();

    while (x_events.step()) {
        if (should_execute_dump) {
            should_execute_dump = false;

            logger->log(LOG_NOTICE) <<
                "Executing dump to target file '" << config.dump_file <<
                "'" << Log::endl;

            std::fstream dump_file(config.dump_file.c_str(),
                                   std::fstream::out | std::fstream::app);

            if (dump_file) {
                dump_file << "#BEGIN DUMP\n";
                crt_manager.dump(dump_file);
                clients.dump(dump_file);
                dump_file << "#END DUMP\n";
                dump_file.close();
            } else {
                logger->log(LOG_ERR) <<
                    "Could not open dump file '" << config.dump_file <<
                    "' for writing" << Log::endl;
            }
        }

        client_events.handle_queued_changes();
    }

    logger->stop();
    delete logger;

    return 0;
}
