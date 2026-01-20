project('mpvpaper-stop', 'c',
    version: run_command('cat', join_paths(meson.project_source_root(), 'VERSION'), check: true).stdout().strip(),
    default_options : ['c_std=gnu11', 'buildtype=debug']
)

inc = include_directories('include')
cjson_dep = dependency('libcjson', required: true)

add_global_link_arguments(language:'c')

executable('mpv-stop-dbg','src/sockets.c','src/utils.c','src/context.c', 'src/logging.c', 'src/multi_monitor.c' ,'src/main.c', 
    dependencies: [cjson_dep],
    include_directories: inc,
    install: true
)
