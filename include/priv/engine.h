/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2015-2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"

#include <stdarg.h>
#include <stdint.h>

Rewriter* allocRewriter(void);
void initRewriter(Rewriter* r);
void freeRewriter(Rewriter* r);

// Rewrite engine
void vEmulateAndCapture(Rewriter* r, va_list args);
void runOptsOnCaptured(Rewriter* r);
void generateBinaryFromCaptured(Rewriter* r);

#endif // ENGINE_H
