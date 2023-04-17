#
# Copyright (C) 2021 Collabora Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cpp = meson.get_compiler('cpp')
grpcpp_reflection_dep = cpp.find_library('grpc++_reflection')

applaunchd_dbus_deps = [
    dependency('gobject-2.0'),
    dependency('gio-unix-2.0'),
    dependency('libsystemd'),
]

applaunchd_deps = [
    dependency('gobject-2.0'),
    dependency('gio-unix-2.0'),
    dependency('libsystemd'),
    dependency('protobuf'),
    dependency('grpc'),
    dependency('grpc++'),
    grpcpp_reflection_dep,
]

protoc = find_program('protoc')
grpc_cpp = find_program('grpc_cpp_plugin')

protoc_gen = generator(protoc, \
                       output : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
                       arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../protos',
                         '--cpp_out=@BUILD_DIR@',
                         '@INPUT@'])
generated_protoc_sources = protoc_gen.process('../protos/applauncher.proto')

grpc_gen = generator(protoc, \
                     output : ['@BASENAME@.grpc.pb.cc', '@BASENAME@.grpc.pb.h'],
                     arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../protos',
                       '--grpc_out=@BUILD_DIR@',
                       '--plugin=protoc-gen-grpc=' + grpc_cpp.path(),
                       '@INPUT@'])
generated_grpc_sources = grpc_gen.process('../protos/applauncher.proto')

executable (
    'applaunchd-dbus',
    config_h,
    [
        generated_dbus_sources,
        'main.c',
        'app_info.c', 'app_info.h',
        'app_launcher.c', 'app_launcher.h',
        'systemd_manager.c', 'systemd_manager.h',
        'gdbus/systemd1_manager_interface.c',
        'gdbus/systemd1_unit_interface.c',
        'utils.c', 'utils.h',
    ],
    dependencies : applaunchd_dbus_deps,
    include_directories : include_directories('.', 'gdbus'),
    install : true
)

executable (
    'applaunchd',
    config_h,
    [
        generated_dbus_sources,
        generated_protoc_sources,
        generated_grpc_sources,
        'main-grpc.cc',
        'AppLauncherImpl.cc',
        'app_info.c', 'app_info.h',
        'systemd_manager.c', 'systemd_manager.h',
        'gdbus/systemd1_manager_interface.c',
        'gdbus/systemd1_unit_interface.c',
        'utils.c', 'utils.h',
    ],
    dependencies : applaunchd_deps,
    include_directories : include_directories('.', 'gdbus'),
    install : true
)
