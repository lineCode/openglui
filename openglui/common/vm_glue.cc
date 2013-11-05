// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bin/builtin.h"
#include "bin/dartutils.h"
#include "bin/directory.h"
#include "bin/dbg_connection.h"
#include "bin/eventhandler.h"
#include "bin/vmservice_impl.h"

#include "openglui/common/extension.h"
#include "openglui/common/log.h"
#include "openglui/common/vm_glue.h"
#include "openglui/common/resources.h"

#include "include/dart_api.h"
#include "include/dart_debugger_api.h"

// Let's reuse dart's builtin stuff
namespace dart {
  namespace bin {
    // snapshot_buffer points to a snapshot if we link in a snapshot otherwise
    // it is initialized to NULL.
    extern const uint8_t* snapshot_buffer;

    bool trace_debug_protocol = false;
  }
}

// This is a snapshot of gl.dart
extern const uint8_t* gl_snapshot_buffer;

bool VMGlue::initialized_vm_ = false;

//static const char* package_root = NULL;

VMGlue::VMGlue(ISized* surface,
               const char* script_path,
               const char* main_script,
               int setup_flag)
    : surface_(surface),
      isolate_(NULL),
      initialized_script_(false),\
      x_(0.0),
      y_(0.0),
      z_(0.0),
      accelerometer_changed_(false),
      setup_flag_(setup_flag),
      debugger_start(false),
      debugger_port(-1),
      debugger_ip(NULL),
      vm_service_start(false),
      vm_service_server_port(-1) {
  LOGI("Creating VMGlue");
  if (main_script == NULL) {
    main_script = "main.dart";
  }
  size_t len = strlen(script_path) + strlen(main_script) + 2;
  main_script_ = new char[len];
  snprintf(main_script_, len, "%s/%s", script_path, main_script);
}

Dart_Handle VMGlue::CheckError(Dart_Handle handle) {
  if (Dart_IsError(handle)) {
    LOGE("Unexpected Error Handle: %s", Dart_GetError(handle));
    Dart_PropagateError(handle);
  }
  return handle;
}

#define CHECK_RESULT(result)                   \
  if (Dart_IsError(result)) {                  \
    *error = strdup(Dart_GetError(result));    \
    LOGE("%s", *error);                        \
    Dart_ExitScope();                          \
    Dart_ShutdownIsolate();                    \
    return false;                              \
  }

Dart_Handle VMGlue::LibraryTagHandler(Dart_LibraryTag tag,
                                      Dart_Handle library,
                                      Dart_Handle urlHandle) {

  const char* url;
  Dart_StringToCString(urlHandle, &url);

  if (tag != Dart_kCanonicalizeUrl) {

    //LOGE("Looking for %s", url);

    // All builtin libraries should be handled here (or moved into a snapshot).
    if (strcmp(url, "dart:html") == 0) {
      // Let's load gl.dart from resources
      const char* gl_source = NULL;
      openglui::Resources::ResourceLookup("/gl.dart", &gl_source);
      Dart_Handle source = Dart_NewStringFromCString(gl_source);
      Dart_Handle library = CheckError(Dart_LoadLibrary(urlHandle, source));
      CheckError(Dart_SetNativeResolver(library, ResolveName));
      return library;
    }
    // TODO(nfgs): Create web_gl.dart
    if (strcmp(url, "dart:web_gl") == 0) {
      return Dart_Null();
    }
    // TODO(nfgs): Create web_audio.dart
    if (strcmp(url, "dart:web_audio") == 0) {
      return Dart_Null();
    }

  }

  // Handle 'import' or 'part' requests for all other URIs.
  return dart::bin::DartUtils::LibraryTagHandler(tag, library, urlHandle);
}

// Returns true on success, false on failure.
Dart_Isolate VMGlue::CreateIsolateAndSetupHelper(const char* script_uri,
                                         const char* main,
                                         void* data,
                                         char** error) {
  LOGI("Creating isolate %s, %s", script_uri, main);
  Dart_Isolate isolate =
      Dart_CreateIsolate(script_uri, main, dart::bin::snapshot_buffer, data, error);
  if (isolate == NULL) {
    LOGE("Couldn't create isolate: %s", *error);
    return NULL;
  }

  LOGI("Entering scope");
  Dart_EnterScope();

  if (dart::bin::snapshot_buffer != NULL) {
    // Setup the native resolver as the snapshot does not carry it.
    dart::bin::Builtin::SetNativeResolver(dart::bin::Builtin::kBuiltinLibrary);
    dart::bin::Builtin::SetNativeResolver(dart::bin::Builtin::kIOLibrary);
  }

  // Set up the library tag handler for this isolate.
  LOGI("Setting up library tag handler");
  Dart_Handle result = CheckError(Dart_SetLibraryTagHandler(LibraryTagHandler));
  CHECK_RESULT(result);

  // Load the specified application script into the newly created isolate.

  // Prepare builtin and its dependent libraries for use to resolve URIs.
  // The builtin library is part of the core snapshot and would already be
  // available here in the case of script snapshot loading.
  Dart_Handle builtin_lib =
      dart::bin::Builtin::LoadAndCheckLibrary(dart::bin::Builtin::kBuiltinLibrary);
  CHECK_RESULT(builtin_lib);

  // Prepare for script loading by setting up the 'print' and 'timer'
  // closures and setting up 'package root' for URI resolution.
  //CheckError(dart::bin::DartUtils::PrepareForScriptLoading(package_root, builtin_lib));
  
  // Setup the internal library's 'internalPrint' function.
  Dart_Handle internal_lib =
      Dart_LookupLibrary(Dart_NewStringFromCString("dart:_collection-dev"));
  CHECK_RESULT(internal_lib);
  Dart_Handle print = Dart_Invoke(
      builtin_lib, Dart_NewStringFromCString("_getPrintClosure"), 0, NULL);
  result = Dart_SetField(internal_lib,
                                     Dart_NewStringFromCString("_printClosure"),
                                     print);
  CHECK_RESULT(result);

  // Setup the 'timer' factory.
  Dart_Handle url = Dart_NewStringFromCString(dart::bin::DartUtils::kAsyncLibURL);
  CHECK_RESULT(url);
  Dart_Handle async_lib = Dart_LookupLibrary(url);
  CHECK_RESULT(async_lib);
  Dart_Handle io_lib = dart::bin::Builtin::LoadAndCheckLibrary(dart::bin::Builtin::kIOLibrary);
  Dart_Handle timer_closure =
      Dart_Invoke(io_lib, Dart_NewStringFromCString("_getTimerFactoryClosure"), 0, NULL);
  Dart_Handle args[1];
  args[0] = timer_closure;
  CHECK_RESULT(Dart_Invoke(
      async_lib, Dart_NewStringFromCString("_setTimerFactoryClosure"), 1, args));

  // Begin DartUtils::PrepareForScriptLoading
  // Setup the corelib 'Uri.base' getter.
  Dart_Handle corelib = Dart_LookupLibrary(Dart_NewStringFromCString("dart:core"));
  CHECK_RESULT(corelib);
  Dart_Handle uri_base = Dart_Invoke(
      builtin_lib, Dart_NewStringFromCString("_getUriBaseClosure"), 0, NULL);
  CHECK_RESULT(uri_base);
  result = Dart_SetField(corelib,
                         Dart_NewStringFromCString("_uriBaseClosure"),
                         uri_base);
  CHECK_RESULT(result);

  // Set working directory
  char * dir = dart::bin::Directory::Current();
  LOGE("Setting working directory to %s", dir);
  Dart_Handle directory = Dart_NewStringFromCString(dir);
  Dart_Handle dart_args[1];
  dart_args[0] = directory;
  Dart_Invoke(builtin_lib, Dart_NewStringFromCString("_setWorkingDirectory"), 1, dart_args);

  // End DartUtils::PrepareForScriptLoading

  // This will set _entryPointScript which is needed for imports to work
  dart::bin::DartUtils::ResolveScriptUri(Dart_NewStringFromCString(script_uri), builtin_lib);

  Dart_ExitScope();
  return isolate;
}

Dart_Isolate VMGlue::CreateIsolateAndSetup(const char* script_uri,
  const char* main,
  void* data, char** error) {
  return CreateIsolateAndSetupHelper(script_uri,
                                     main,
                                     data,
                                     error);
}

const char* VM_FLAGS[] = {
  "--enable_type_checks",  // TODO(gram): This should be an option!
  // "--trace_isolates",
  // "--trace_natives",
  // "--trace_compiler",
};

static void* openFileCallback(const char* name, bool write) {
  return fopen(name, write ? "w" : "r");
}

static void readFileCallback(const uint8_t** data, intptr_t* fileLength,
    void* stream) {
  if (!stream) {
    *data = 0;
    *fileLength = 0;
  } else {
    FILE* file = reinterpret_cast<FILE*>(stream);

    // Get the file size.
    fseek(file, 0, SEEK_END);
    *fileLength = ftell(file);
    rewind(file);

    // Allocate data buffer.
    *data = new uint8_t[*fileLength];
    *fileLength = fread(const_cast<uint8_t*>(*data), 1, *fileLength, file);
  }
}

static void writeFileCallback(const void* data, intptr_t length, void* file) {
  fwrite(data, 1, length, reinterpret_cast<FILE*>(file));
}

static void closeFileCallback(void* file) {
  fclose(reinterpret_cast<FILE*>(file));
}

int VMGlue::InitializeVM() {
  // We need the next call to get Dart_Initialize not to bail early.
  LOGI("Setting VM Options");
  Dart_SetVMFlags(sizeof(VM_FLAGS) / sizeof(VM_FLAGS[0]), VM_FLAGS);

  // Initialize the Dart VM, providing the callbacks to use for
  // creating and shutting down isolates.
  LOGI("Initializing Dart");
  if (!Dart_Initialize(CreateIsolateAndSetup,
                       0,
                       0,
                       0,
                       openFileCallback,
                       readFileCallback,
                       writeFileCallback,
                       closeFileCallback)) {
    LOGE("VM initialization failed\n");
    return -1;
  }
  initialized_vm_ = true;

  return 0;
}

Dart_Handle VMGlue::LoadSourceFromFile(const char* url) {
  FILE* file = fopen(url, "r");
  if (file == NULL) {
    LOGE("Main script not found at: %s\n", url);
    return NULL;
  }

  struct stat sb;
  int fd = fileno(file);
  fstat(fd, &sb);
  int length = sb.st_size;
  LOGI("Entry file %s is %d bytes.\n", url, length);

  char* buffer = new char[length+1];
  if (read(fd, buffer, length) < 0) {
    LOGE("Could not read script %s.\n", url);
    return NULL;
  }
  buffer[length] = 0;
  fclose(file);

  Dart_Handle contents = CheckError(Dart_NewStringFromCString(buffer));
  delete[] buffer;
  return contents;
}

void VMGlue::EnableDebugger(const char * ip, int port) {
  debugger_start = true;
  if (ip != NULL) debugger_ip = ip;
  if (port != -1) debugger_port = port;
}

void VMGlue::EnableVMService(int port) {
  vm_service_start = true;
  if (port != -1) vm_service_server_port = port;
}

int VMGlue::StartMainIsolate() {
  if (!initialized_vm_) {
    int rtn = InitializeVM();
    if (rtn != 0) return rtn;
  }
  
  // Start the debugger wire protocol handler if necessary.
  if (debugger_start) {
    ASSERT(debugger_port >= 0);
    debugger_port = dart::bin::DebuggerConnectionHandler::StartHandler(debugger_ip, debugger_port);
    LOGE("Debugger listening on port %d\n", debugger_port);
  }

  // Start event handler.
  dart::bin::EventHandler::Start();

  if (vm_service_start) {
    LOGE("Starting VM Service isolate at %d", vm_service_server_port);
    ASSERT(vm_service_server_port >= 0);
    bool r = dart::bin::VmService::Start(vm_service_server_port);
    if (!r) {
      LOGE("Could not start VM Service isolate %s\n",
                    dart::bin::VmService::GetErrorMessage());
    }
  }

  // Create an isolate and loads up the application script.
  char* error = NULL;
  // Do not run "main" yet, wait for CallSetup
  if (!CreateIsolateAndSetup(main_script_, NULL, NULL, &error)) {
    LOGE("CreateIsolateAndSetup: %s\n", error);
    free(error);
    return -1;
  }
  LOGI("Created isolate");
  isolate_ = Dart_CurrentIsolate();
  Dart_EnterScope();

  Dart_Handle url = CheckError(Dart_NewStringFromCString(main_script_));
  Dart_Handle source = LoadSourceFromFile(main_script_);
  CheckError(Dart_LoadScript(url, source, 0, 0));

  //Platform::SetPackageRoot(package_root);
  Dart_Handle io_lib_url = Dart_NewStringFromCString("dart:io");
  Dart_Handle io_lib = Dart_LookupLibrary(io_lib_url);
  CheckError(io_lib);
  Dart_Handle platform_class_name = Dart_NewStringFromCString("Platform");
  Dart_Handle platform_type =
      Dart_GetType(io_lib, platform_class_name, 0, NULL);
  CheckError(platform_type);
  Dart_Handle script_name_name = Dart_NewStringFromCString("_nativeScript");
  Dart_Handle dart_script = Dart_NewStringFromCString(main_script_);
  Dart_Handle set_script_name =
      Dart_SetField(platform_type, script_name_name, dart_script);
  CheckError(set_script_name);

  dart::bin::VmService::SendIsolateStartupMessage();

  // Make the isolate runnable so that it is ready to handle messages.
  
  Dart_ExitScope();
  Dart_ExitIsolate();
  bool retval = Dart_IsolateMakeRunnable(isolate_);
  if (!retval) {
    error = strdup("Invalid isolate state - Unable to make it runnable");
    Dart_EnterIsolate(isolate_);
    Dart_ShutdownIsolate();
    return NULL;
  }

  return 0;
}

int VMGlue::CallSetup(bool force) {
  // TODO(gram): See if we actually need this flag guard here, or if
  // we can eliminate it along with the need for the force parameter.
  if (!initialized_script_ || force) {
    initialized_script_ = true;
    LOGI("Invoking setup(null, %d,%d,%d)",
        surface_->width(), surface_->height(), setup_flag_);
    Dart_EnterIsolate(isolate_);
    Dart_EnterScope();

    Dart_Handle library = Dart_RootLibrary();
    Dart_Handle window = Dart_GetField(library, Dart_NewStringFromCString("window"));

    // set window.innerWidth
    CheckError(Dart_SetField(window,
                         Dart_NewStringFromCString("innerWidth"),
                         Dart_NewInteger(surface_->width())));

    // set window.innerHeight
    CheckError(Dart_SetField(window,
                         Dart_NewStringFromCString("innerHeight"),
                         Dart_NewInteger(surface_->height())));

    Dart_Handle args[1];
    args[0] = Dart_NewList(0);

    // First try with 1 argument.
    int rtn = Invoke("main", 1, args);

    if (rtn == -1) {
      // Finally try with 0 arguments.
      rtn = Invoke("main", 0, NULL);
    }

    // Processes any incoming messages for the current isolate.
    Dart_RunLoop();

    Dart_ExitScope();

    Dart_ExitIsolate();
    LOGI("Done setup");
    return rtn;
  }
  return 0;
}

int VMGlue::CallUpdate() {
  if (initialized_script_) {
    // If the accelerometer has changed, first do that
    // event.
    Dart_EnterIsolate(isolate_);
    if (accelerometer_changed_) {
      Dart_Handle args[3];
      LOGI("Invoking onAccelerometer(%f,%f,%f)", x_, y_, z_);
      Dart_EnterScope();
      args[0] = CheckError(Dart_NewDouble(x_));
      args[1] = CheckError(Dart_NewDouble(y_));
      args[2] = CheckError(Dart_NewDouble(z_));
      Invoke("onAccelerometer", 3, args, false);
      Dart_ExitScope();
      accelerometer_changed_ = false;
    }
    Dart_EnterScope();
    int rtn = Invoke("update_", 0, 0);

    // Process any incoming messages for the current isolate.
    Dart_RunLoop();

    Dart_ExitScope();

    Dart_ExitIsolate();
    LOGI("Invoke update_ returns %d", rtn);

    return rtn;
  }
  return -1;
}

int VMGlue::CallShutdown() {
  if (initialized_script_) {
    Dart_EnterIsolate(isolate_);
    Dart_EnterScope();
    int rtn = Invoke("shutdown", 0, 0);
    Dart_ExitScope();
    Dart_ExitIsolate();
    return rtn;
  }
  return -1;
}

int VMGlue::OnMotionEvent(const char* pFunction, int64_t pWhen,
  float pMoveX, float pMoveY) {
  if (initialized_script_) {
    LOGI("Invoking %s", pFunction);
    Dart_EnterIsolate(isolate_);
    Dart_EnterScope();
    Dart_Handle args[3];
    args[0] = CheckError(Dart_NewInteger(pWhen));
    args[1] = CheckError(Dart_NewDouble(pMoveX));
    args[2] = CheckError(Dart_NewDouble(pMoveY));
    int rtn = Invoke(pFunction, 3, args, false);
    Dart_ExitScope();
    Dart_ExitIsolate();
    LOGI("Done %s", pFunction);
    return rtn;
  }
  return -1;
}

int VMGlue::OnKeyEvent(const char* function, int64_t when, int32_t key_code,
                       bool isAltKeyDown, bool isCtrlKeyDown,
                       bool isShiftKeyDown, int32_t repeat) {
  if (initialized_script_) {
    LOGI("Invoking %s(_,%d,...)", function, key_code);
    Dart_EnterIsolate(isolate_);
    Dart_EnterScope();
    Dart_Handle args[6];
    args[0] = CheckError(Dart_NewInteger(when));
    args[1] = CheckError(Dart_NewInteger(key_code));
    args[2] = CheckError(Dart_NewBoolean(isAltKeyDown));
    args[3] = CheckError(Dart_NewBoolean(isCtrlKeyDown));
    args[4] = CheckError(Dart_NewBoolean(isShiftKeyDown));
    args[5] = CheckError(Dart_NewInteger(repeat));
    int rtn = Invoke(function, 6, args, false);
    Dart_ExitScope();
    Dart_ExitIsolate();
    LOGI("Done %s", function);
    return rtn;
  }
  return -1;
}

int VMGlue::Invoke(const char* function,
                   int argc,
                   Dart_Handle* args,
                   bool failIfNotDefined) {
  // Lookup the library of the root script.
  Dart_Handle library = Dart_RootLibrary();
  if (Dart_IsNull(library)) {
     LOGE("Unable to find root library\n");
     return -1;
  }

  Dart_Handle nameHandle = Dart_NewStringFromCString(function);

  Dart_Handle result = Dart_Invoke(library, nameHandle, argc, args);

  if (Dart_IsError(result)) {
    const char* error = Dart_GetError(result);
    LOGE("Invoke %s failed: %s", function, error);
    if (failIfNotDefined) {
      return -1;
    }
  }

  return 0;
}

void VMGlue::FinishMainIsolate() {
  LOGI("Finish main isolate");
  Dart_EnterIsolate(isolate_);
  // Shutdown the isolate.
  Dart_ShutdownIsolate();
  isolate_ = NULL;
  initialized_script_ = false;
}

