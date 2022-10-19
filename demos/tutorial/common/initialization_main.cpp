/*!
 * \file initialization_main.cpp
 * \brief initialization_main.cpp
 *
 * Copyright 2019 InvisionApp.
 *
 * Contact: kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include "initialization.hpp"

int
main(int argc, char **argv)
{
  DemoRunner demo_runner;
  return demo_runner.main<Initialization>(argc, argv);
}
