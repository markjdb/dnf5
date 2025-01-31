/*
Copyright Contributors to the libdnf project.

This file is part of libdnf: https://github.com/rpm-software-management/libdnf/

Libdnf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Libdnf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libdnf.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "command.hpp"

#include "context.hpp"
#include "dnf5daemon-server/dbus.hpp"
#include "dnf5daemon-server/utils.hpp"
#include "utils.hpp"
#include "wrappers/dbus_goal_wrapper.hpp"
#include "wrappers/dbus_package_wrapper.hpp"

#include <dnf5daemon-server/dbus.hpp>
#include <libdnf5-cli/exception.hpp>
#include <libdnf5-cli/output/transaction_table.hpp>
#include <libdnf5-cli/utils/userconfirm.hpp>
#include <libdnf5/base/goal.hpp>

#include <iostream>
#include <vector>

namespace dnfdaemon::client {

void TransactionCommand::run_transaction() {
    auto & ctx = get_context();
    dnfdaemon::KeyValueMap options = {};

    // resolve the transaction
    options["allow_erasing"] = ctx.allow_erasing.get_value();
    std::vector<dnfdaemon::DbusTransactionItem> transaction;
    unsigned int result_int;
    ctx.session_proxy->callMethod("resolve")
        .onInterface(dnfdaemon::INTERFACE_GOAL)
        .withTimeout(static_cast<uint64_t>(-1))
        .withArguments(options)
        .storeResultsTo(transaction, result_int);
    dnfdaemon::ResolveResult result = static_cast<dnfdaemon::ResolveResult>(result_int);
    DbusGoalWrapper dbus_goal_wrapper(transaction);

    if (result != dnfdaemon::ResolveResult::NO_PROBLEM) {
        // retrieve and print resolving error messages
        std::vector<std::string> problems;
        ctx.session_proxy->callMethod("get_transaction_problems_string")
            .onInterface(dnfdaemon::INTERFACE_GOAL)
            .withTimeout(static_cast<uint64_t>(-1))
            .storeResultsTo(problems);
        if (result == dnfdaemon::ResolveResult::ERROR) {
            throw libdnf5::cli::GoalResolveError(problems);
        }
        dbus_goal_wrapper.set_resolve_logs(std::move(problems));
    }

    ctx.reset_download_cb();

    // print the transaction to the user and ask for confirmation
    if (!libdnf5::cli::output::print_transaction_table(dbus_goal_wrapper)) {
        return;
    }

    if (!libdnf5::cli::utils::userconfirm::userconfirm(ctx)) {
        throw libdnf5::cli::AbortedByUserError();
    }

    // do the transaction
    options.clear();
    ctx.session_proxy->callMethod("do_transaction")
        .onInterface(dnfdaemon::INTERFACE_GOAL)
        .withTimeout(static_cast<uint64_t>(-1))
        .withArguments(options);
}

}  // namespace dnfdaemon::client
