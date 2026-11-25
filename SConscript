from building import *
import os

cwd = GetCurrentDir()
objs = []

# Board support.
objs += SConscript(os.path.join(cwd, 'src/board/SConscript'),
                   variant_dir='board', duplicate=0)

# Reusable components: drivers, services, UI, applications.
objs += SConscript(os.path.join(cwd, 'src/components/drivers/SConscript'),
                   variant_dir='components/drivers', duplicate=0)
objs += SConscript(os.path.join(cwd, 'src/components/services/SConscript'),
                   variant_dir='components/services', duplicate=0)
objs += SConscript(os.path.join(cwd, 'src/components/apps/SConscript'),
                   variant_dir='components/apps', duplicate=0)

# Application entry and runtime wiring.
objs += SConscript(os.path.join(cwd, 'src/main/SConscript'),
                   variant_dir='main', duplicate=0)

# Resources and hardware self-tests.
objs += SConscript(os.path.join(cwd, 'src/resources/SConscript'),
                   variant_dir='resources', duplicate=0)
objs += SConscript(os.path.join(cwd, 'tests/SConscript'),
                   variant_dir='tests', duplicate=0)

Return('objs')
