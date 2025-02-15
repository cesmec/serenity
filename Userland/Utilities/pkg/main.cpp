/*
 * Copyright (c) 2023, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "AvailablePort.h"
#include "InstalledPort.h"
#include <LibCore/ArgsParser.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <unistd.h>

static void print_port_details(InstalledPort const& port)
{
    outln("{}, installed as {}, version {}", port.name(), port.type_as_string_view(), port.version());
}

static void print_port_details_without_version(InstalledPort const& port)
{
    outln("{}, installed as {}", port.name(), port.type_as_string_view());
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio recvfd thread unix rpath cpath wpath"));

    TRY(Core::System::unveil("/tmp/session/%sid/portal/request", "rw"));
    TRY(Core::System::unveil("/usr/Ports/installed.db"sv, "rwc"sv));
    TRY(Core::System::unveil("/usr/Ports/AvailablePorts.md"sv, "rwc"sv));
    TRY(Core::System::unveil("/res"sv, "r"sv));
    TRY(Core::System::unveil("/usr/lib"sv, "r"sv));
    TRY(Core::System::unveil(nullptr, nullptr));

    bool verbose = false;
    bool show_all_installed_ports = false;
    bool show_all_dependency_ports = false;
    bool update_packages_db = false;
    StringView query_package {};

    Core::ArgsParser args_parser;
    args_parser.add_option(show_all_installed_ports, "Show all manually-installed ports", "list-manual-ports", 'l');
    args_parser.add_option(show_all_dependency_ports, "Show all dependencies' ports", "list-dependency-ports", 'd');
    args_parser.add_option(update_packages_db, "Sync/Update ports database", "update-ports-database", 'u');
    args_parser.add_option(query_package, "Query ports database for package name", "query-package", 'q', "Package name to query");
    args_parser.add_option(verbose, "Verbose", "verbose", 'v');
    args_parser.parse(arguments);

    if (!update_packages_db && !show_all_installed_ports && !show_all_dependency_ports && query_package.is_null()) {
        outln("pkg: No action to be performed was specified.");
        return 0;
    }

    HashMap<String, InstalledPort> installed_ports;
    HashMap<String, AvailablePort> available_ports;
    if (show_all_installed_ports || show_all_dependency_ports || !query_package.is_null()) {
        installed_ports = TRY(InstalledPort::read_ports_database());
    }

    int return_value = 0;
    if (update_packages_db) {
        if (getuid() != 0) {
            outln("pkg: Requires root to update packages database.");
            return 1;
        }
        return_value = TRY(AvailablePort::update_available_ports_list_file());
    }

    if (!query_package.is_null()) {
        if (Core::System::access("/usr/Ports/AvailablePorts.md"sv, R_OK).is_error()) {
            outln("pkg: Please run this program with -u first!");
            return 0;
        }
        available_ports = TRY(AvailablePort::read_available_ports_list());
    }

    if (show_all_installed_ports) {
        outln("Manually-installed ports:");
        TRY(InstalledPort::for_each_by_type(installed_ports, InstalledPort::Type::Manual, [](auto& port) -> ErrorOr<void> {
            print_port_details(port);
            return {};
        }));
    }

    if (show_all_dependency_ports) {
        outln("Dependencies-installed ports:");
        TRY(InstalledPort::for_each_by_type(installed_ports, InstalledPort::Type::Dependency, [](auto& port) -> ErrorOr<void> {
            // NOTE: Dependency entries don't specify versions, so we don't
            // try to print it.
            print_port_details_without_version(port);
            return {};
        }));
    }

    if (!query_package.is_null()) {
        if (query_package.is_empty()) {
            outln("pkg: Queried package name is empty.");
            return 0;
        }
        return_value = TRY(AvailablePort::query_details_for_package(available_ports, installed_ports, query_package, verbose));
    }

    return return_value;
}
