// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iree/hal/drivers/init.h"
#include "iree/tools/utils/vm_util.h"
#include "iree/modules/hal/hal_module.h"

// Compiled module embedded here to avoid file IO:
#include "vmfb_embedding_bytecode_module.h"

// Removed:
// #include "absl/flags/flag.h"
// #include "absl/strings/string_view.h"
// #include "iree/base/internal/file_io.h"
// #include "iree/base/internal/flags.h"
// #include "iree/vm/api.h"
// #include "iree/vm/bytecode_module.h"
// #include "iree/base/status.h"
// #include "iree/base/tracing.h"

#define ENABLE_ERROR_CHECKING

#ifndef ENABLE_ERROR_CHECKING

#ifdef IREE_RETURN_IF_ERROR
#undef IREE_RETURN_IF_ERROR
#endif

#define IREE_RETURN_IF_ERROR_0() "FAILED"
#define IREE_RETURN_IF_ERROR_1(expr) (void) expr
#define IREE_RETURN_IF_ERROR_2(expr, B) (void) expr
#define IREE_RETURN_IF_ERROR_3(expr, B, C) (void) expr
#define IREE_RETURN_IF_ERROR_4(expr, B, C, D) (void) expr

#define IREE_RETURN_IF_ERROR_X(x, A, B, C, D, FUNC, ...) FUNC

#define IREE_RETURN_IF_ERROR(...)                                              \
  IREE_RETURN_IF_ERROR_X(, ##__VA_ARGS__, IREE_RETURN_IF_ERROR_4(__VA_ARGS__), \
                         IREE_RETURN_IF_ERROR_3(__VA_ARGS__),                  \
                         IREE_RETURN_IF_ERROR_2(__VA_ARGS__),                  \
                         IREE_RETURN_IF_ERROR_1(__VA_ARGS__),                  \
                         IREE_RETURN_IF_ERROR_0(__VA_ARGS__))

#ifdef IREE_TRACE_SCOPE0
#undef IREE_TRACE_SCOPE0
#endif

#define IREE_TRACE_SCOPE0(x)

#ifdef IREE_CHECK_OK
#undef IREE_CHECK_OK
#endif
#define IREE_CHECK_OK(x) {bool a = x;}
#endif
// ABSL_FLAG(std::string, entry_function, "",
//           "Name of a function contained in the module specified by
//           module_file " "to run.");

// ABSL_FLAG(std::string, driver, "vmla", "Backend driver to use.");

// ABSL_FLAG(std::vector<std::string>, function_inputs, {},
//           "A comma-separated list of of input buffers of the format:"
//           "[shape]xtype=[value]\n"
//           "2x2xi32=1 2 3 4\n"
//           "Optionally, brackets may be used to separate the element values. "
//           "They are ignored by the parser.\n"
//           "2x2xi32=[[1 2][3 4]]\n"
//           "Due to the absence of repeated flags in absl, commas should not be
//           " "used to separate elements. They are reserved for separating
//           input " "values:\n" "2x2xi32=[[1 2][3 4]], 1x2xf32=[[1 2]]");

// ABSL_FLAG(std::string, function_inputs_file, "",
//           "Provides a file for input shapes and optional values (see "
//           "ParseToVariantListFromFile in vm_util.h for details)");

namespace iree {
namespace {

Status GetModuleContents(std::string *out_contents) {
  IREE_TRACE_SCOPE0("GetModuleContentsFromFlags");
  const auto *vmfb_embedded = samples::vmfb_embedding_bytecode_module_create();
  *out_contents = std::string();
  out_contents->resize(vmfb_embedded->size);
  for (int i = 0; i < vmfb_embedded->size; ++i) {
    (*out_contents)[i] = vmfb_embedded->data[i];
  }
  return OkStatus();
}

Status Run() {
  std::string driver = "vmla";
  std::string function_name = "call";
  std::vector<std::string> function_inputs;
  function_inputs.push_back("1x320xf32");

  IREE_TRACE_SCOPE0("iree-run-embedded-vmfb");

  IREE_RETURN_IF_ERROR(iree_hal_module_register_types(),
                       "registering HAL types");
  iree_vm_instance_t *instance = nullptr;
  IREE_RETURN_IF_ERROR(
      iree_vm_instance_create(iree_allocator_system(), &instance),
      "creating instance");

  std::string module_data;
  IREE_RETURN_IF_ERROR(GetModuleContents(&module_data));
  iree_vm_module_t *input_module = nullptr;
  IREE_RETURN_IF_ERROR(LoadBytecodeModule(module_data, &input_module));

  iree_hal_device_t *device = nullptr;
  IREE_RETURN_IF_ERROR(CreateDevice(driver, &device));
  iree_vm_module_t *hal_module = nullptr;
  IREE_RETURN_IF_ERROR(CreateHalModule(device, &hal_module));

  iree_vm_context_t *context = nullptr;
  // Order matters. The input modulelikely be dependent on the hal module.
  std::array<iree_vm_module_t *, 2> modules = {hal_module, input_module};
  IREE_RETURN_IF_ERROR(iree_vm_context_create_with_modules(
                           instance, modules.data(), modules.size(),
                           iree_allocator_system(), &context),
                       "creating context");

  iree_vm_function_t function;
  IREE_RETURN_IF_ERROR(
      input_module->lookup_function(
          input_module->self, IREE_VM_FUNCTION_LINKAGE_EXPORT,
          iree_string_view_t{function_name.data(), function_name.size()},
          &function),
      "looking up function '%s'", function_name.c_str());

  IREE_RETURN_IF_ERROR(ValidateFunctionAbi(function));
  std::vector<RawSignatureParser::Description> input_descs;
  IREE_RETURN_IF_ERROR(ParseInputSignature(function, &input_descs));

  vm::ref<iree_vm_list_t> inputs;
  IREE_RETURN_IF_ERROR(
      ParseToVariantList(input_descs, iree_hal_device_allocator(device),
                         absl::MakeConstSpan(function_inputs), &inputs));

  std::vector<RawSignatureParser::Description> output_descs;
  IREE_RETURN_IF_ERROR(ParseOutputSignature(function, &output_descs));
  vm::ref<iree_vm_list_t> outputs;
  IREE_RETURN_IF_ERROR(iree_vm_list_create(/*element_type=*/nullptr,
                                           output_descs.size(),
                                           iree_allocator_system(), &outputs));

  IREE_RETURN_IF_ERROR(iree_vm_invoke(context, function, /*policy=*/nullptr,
                                      inputs.get(), outputs.get(),
                                      iree_allocator_system()),
                       "invoking function '%s'", function_name.c_str());

  IREE_RETURN_IF_ERROR(PrintVariantList(output_descs, outputs.get()),
                       "printing results");

  inputs.reset();
  outputs.reset();
  iree_vm_module_release(hal_module);
  iree_vm_module_release(input_module);
  iree_hal_device_release(device);
  iree_vm_context_release(context);
  iree_vm_instance_release(instance);
  std::cout << "DONE\n";
  return OkStatus();
}

} // namespace

extern "C" int main(int argc, char **argv) {
  // iree_flags_parse_checked(&argc, &argv);
  IREE_CHECK_OK(iree_hal_register_all_available_drivers(
      iree_hal_driver_registry_default()));
  IREE_CHECK_OK(Run());
  return 0;
}

} // namespace iree
