/* dummy-provider.h
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "gnome-todo.h"

G_BEGIN_DECLS

#define DUMMY_TYPE_PROVIDER (dummy_provider_get_type())

G_DECLARE_FINAL_TYPE (DummyProvider, dummy_provider, DUMMY, PROVIDER, GtdObject)

DummyProvider*       dummy_provider_new                          (void);

guint                dummy_provider_generate_task_lists          (DummyProvider      *self);

void                 dummy_provider_schedule_remove_task         (DummyProvider      *self);

guint                dummy_provider_randomly_remove_task         (DummyProvider      *self);

G_END_DECLS
