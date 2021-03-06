// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef OPENGLUI_EMULATOR_EMULATOR_GRAPHICS_HANDLER_H_
#define OPENGLUI_EMULATOR_EMULATOR_GRAPHICS_HANDLER_H_

#include "openglui/common/graphics_handler.h"

class EmulatorGraphicsHandler : public GraphicsHandler {
  public:
    EmulatorGraphicsHandler(int argc, char** argv);

    int32_t Start();
    void Stop();
};

#endif  // OPENGLUI_EMULATOR_EMULATOR_GRAPHICS_HANDLER_H_

