# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#-------------------------------------------------------------------------------
# Use `iree-translate` to transform an MLIR file into an VM bytcode module.
#-------------------------------------------------------------------------------

# Resolve the executable binary path for iree-translate from the target name.
# set(_TRANSLATE_TOOL_EXECUTABLE $<TARGET_FILE:iree_tools_iree-translate>)

# # Define arguments passed to iree-translate
# set(_ARGS)
# list(APPEND _ARGS "-iree-mlir-to-vm-bytecode-module")
# list(APPEND _ARGS "-iree-hal-target-backends=vmla")
# # Uncomment the line below to use vulkan-spirv backend
# #list(APPEND _ARGS "-iree-hal-target-backends=vulkan-spirv")
# list(APPEND _ARGS "${CMAKE_CURRENT_SOURCE_DIR}/vmfb_embedding.mlir")
# list(APPEND _ARGS "-o")
# list(APPEND _ARGS "vmfb_embedding_bytecode_module.module")

# # Translate MLIR file to VM bytecode module
# add_custom_command(
#   OUTPUT "vmfb_embedding_bytecode_module.module"
#   COMMAND ${_TRANSLATE_TOOL_EXECUTABLE} ${_ARGS}
#   DEPENDS iree_tools_iree-translate
# )

#-------------------------------------------------------------------------------
# Embedd the VM bytcode module into a cc file via `generate_cc_embed_data`.
#-------------------------------------------------------------------------------

set(VMFB_FILE compiled_vmla.vmfb)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${VMFB_FILE}
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Define arguments passed to generate_cc_embed_data
set(_ARGS)
list(APPEND _ARGS "--output_header=vmfb_embedding_bytecode_module.h")
list(APPEND _ARGS "--output_impl=vmfb_embedding_bytecode_module.cc")
list(APPEND _ARGS "--identifier=vmfb_embedding_bytecode_module")
list(APPEND _ARGS "--cpp_namespace=iree::samples")
list(APPEND _ARGS "--flatten")
list(APPEND _ARGS ${VMFB_FILE})

# Embed VM bytecode module into cc source file
add_custom_command(
  OUTPUT
    "vmfb_embedding_bytecode_module.h"
    "vmfb_embedding_bytecode_module.cc"
  COMMAND generate_cc_embed_data ${_ARGS}
  DEPENDS generate_cc_embed_data ${VMFB_FILE}
)


#-------------------------------------------------------------------------------
# Create a library and thus a CMake target.
#-------------------------------------------------------------------------------

add_library(vmfb_embedding_bytecode_module_cc STATIC "")
target_sources(vmfb_embedding_bytecode_module_cc
  PRIVATE
    vmfb_embedding_bytecode_module.cc
    vmfb_embedding_bytecode_module.h
)


#-------------------------------------------------------------------------------
# Build a excutable for testing.
#-------------------------------------------------------------------------------

# iree_cc_binary(
#   NAME
#     iree_vmfb_embedding
#   OUT
#     iree_vmfb_embedding
#   SRCS
#     "iree_run_embedded_vmfb.cc"
#   DEPS
#     absl::flags
#     absl::strings
#     iree::base::internal::file_io
#     iree::base::internal::flags
#     iree::base::status
#     iree::base::tracing
#     iree::hal::drivers
#     iree::modules::hal
#     iree::tools::utils::vm_util
#     iree::vm
#     iree::vm::bytecode_module
#     vmfb_embedding_bytecode_module_cc
# )

add_executable(vmfb_embedding "")
target_sources(vmfb_embedding
  PRIVATE
    iree_run_embedded_vmfb.cc
)

set_target_properties(vmfb_embedding PROPERTIES OUTPUT_NAME vmfb_embedding)

target_include_directories(vmfb_embedding
  PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(vmfb_embedding
  absl::flags
  absl::flags_parse
  absl::flags_usage
  absl::strings
  benchmark
  iree::base::internal::file_io
  iree::base::internal::flags
  iree::base::status
  iree::base::tracing
  iree::hal::drivers
  iree::modules::hal
  iree::tools::utils::vm_util
  iree::vm
  iree::vm::bytecode_module
  vmfb_embedding_bytecode_module_cc
)
