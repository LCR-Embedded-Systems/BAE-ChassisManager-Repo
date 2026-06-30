project('lcrfanctl', 'cpp',
  version: '1.0',
  default_options: ['cpp_std=c++20'],
)

conf = configuration_data()
conf.set_quoted('PROJECT_VERSION', meson.project_version())

configure_file(
  output: 'config.h',
  configuration: conf,
)

cli11_dep          = dependency('CLI11')
nlohmann_json_dep  = dependency('nlohmann_json')
phosphor_logging_dep = dependency('phosphor-logging')
sdbusplus_dep      = dependency('sdbusplus')
fmt_dep            = dependency('fmt')

lcrfanctl = executable(
  'lcrfanctl',
  'fanctl.cpp',
  dependencies: [
    cli11_dep,
    nlohmann_json_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    fmt_dep,
  ],
  include_directories: include_directories('.'),
  install: true,
)