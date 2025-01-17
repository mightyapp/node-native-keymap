/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include <node.h>
#include <map>

#include "keymapping.h"
#include "common.h"

namespace vscode_keyboard {

napi_status napi_set_named_property_string_utf8(napi_env env, napi_value object, const char *utf8Name, const char *value) {
  napi_value _value;
  NAPI_CALL_RETURN_STATUS(env, napi_create_string_utf8(env, value, NAPI_AUTO_LENGTH, &_value));
  NAPI_CALL_RETURN_STATUS(env, napi_set_named_property(env, object, utf8Name, _value));
  return napi_ok;
}

napi_status napi_set_named_property_int32(napi_env env, napi_value object, const char *utf8Name, int value) {
  napi_value _value;
  NAPI_CALL_RETURN_STATUS(env, napi_create_int32(env, value, &_value));
  NAPI_CALL_RETURN_STATUS(env, napi_set_named_property(env, object, utf8Name, _value));
  return napi_ok;
}

napi_value napi_fetch_null(napi_env env) {
  napi_value result;
  NAPI_CALL(env, napi_get_null(env, &result));
  return result;
}

napi_value napi_fetch_undefined(napi_env env) {
  napi_value result;
  NAPI_CALL(env, napi_get_undefined(env, &result));
  return result;
}

napi_value napi_fetch_boolean(napi_env env, bool value) {
  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, value, &result));
  return result;
}

void invokeNotificationCallback(NotificationCallbackData *data) {
  if (data->tsfn == NULL) {
    // This indicates we are in the shutdown phase and the thread safe function has been finalized
    return;
  }

  // No need to call napi_acquire_threadsafe_function because
  // the refcount is set to 1 in the main thread.
  napi_call_threadsafe_function(data->tsfn, NULL, napi_tsfn_blocking);
}

static void NotifyJS(napi_env env, napi_value func, void* context, void* data) {
  // env may be NULL if nodejs is shutting down
  if (env != NULL) {
    napi_value global;
    NAPI_CALL_RETURN_VOID(env, napi_get_global(env, &global));

    std::vector<napi_value> argv;
    NAPI_CALL_RETURN_VOID(env, napi_call_function(env, global, func, argv.size(), argv.data(), NULL));
  }
}

static void FinalizeThreadsafeFunction(napi_env env, void* raw_data, void* hint) {
  NotificationCallbackData *data;
  napi_get_instance_data(env, (void**)&data);
  data->tsfn = NULL;
}

napi_value _OnDidChangeKeyboardLayout(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NotificationCallbackData *data;
  NAPI_CALL(env, napi_get_instance_data(env, (void**)&data));
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 1, "Wrong number of arguments. Expects a single argument.");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));
  NAPI_ASSERT(env, valuetype0 == napi_function, "Wrong type of arguments. Expects a function as first argument.");

  napi_value func = args[0];

  napi_value resource_name;
  NAPI_CALL(env, napi_create_string_utf8(env, "onDidChangeKeyboardLayoutCallback", NAPI_AUTO_LENGTH, &resource_name));

  // Convert the callback retrieved from JavaScript into a thread-safe function
  NAPI_CALL(env, napi_create_threadsafe_function(env, func, NULL, resource_name, 0, 1, NULL,
                                                 FinalizeThreadsafeFunction, NULL, NotifyJS,
                                                 &data->tsfn));

  registerKeyboardLayoutChangeListener(data);

  return napi_fetch_undefined(env);
}

void DeleteInstanceData(napi_env env, void *raw_data, void *hint) {
  NotificationCallbackData *data = static_cast<NotificationCallbackData*>(raw_data);
  delete data;
}

napi_value Init(napi_env env, napi_value exports) {
  NotificationCallbackData *data = new NotificationCallbackData();
  NAPI_CALL(env, napi_set_instance_data(env, data, DeleteInstanceData, NULL));

  {
    napi_value getKeyMap;
    NAPI_CALL(env, napi_create_function(env, NULL, 0, _GetKeyMap, NULL, &getKeyMap));
    NAPI_CALL(env, napi_set_named_property(env, exports, "getKeyMap", getKeyMap));
  }
  {
    napi_value getCurrentKeyboardLayout;
    NAPI_CALL(env, napi_create_function(env, NULL, 0, _GetCurrentKeyboardLayout, NULL, &getCurrentKeyboardLayout));
    NAPI_CALL(env, napi_set_named_property(env, exports, "getCurrentKeyboardLayout", getCurrentKeyboardLayout));
  }
  {
    napi_value onDidChangeKeyboardLayout;
    NAPI_CALL(env, napi_create_function(env, NULL, 0, _OnDidChangeKeyboardLayout, NULL, &onDidChangeKeyboardLayout));
    NAPI_CALL(env, napi_set_named_property(env, exports, "onDidChangeKeyboardLayout", onDidChangeKeyboardLayout));
  }
  {
    napi_value isISOKeyboard;
    NAPI_CALL(env, napi_create_function(env, NULL, 0, _isISOKeyboard, NULL, &isISOKeyboard));
    NAPI_CALL(env, napi_set_named_property(env, exports, "isISOKeyboard", isISOKeyboard));
  }

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)

} // namespace vscode_keyboard
