#!/usr/bin/env python3

from os import environ, path
from subprocess import call

prefix = environ.get('MESON_INSTALL_PREFIX', '/usr/local')
sharedir = path.join(prefix, 'share')
destdir = environ.get('DESTDIR', '')

# Package managers set this so we don't need to run
if not destdir:
    print('Updating icon cache...')
    call(['gtk4-update-icon-cache', '-qtf', path.join(sharedir, 'icons', 'hicolor')])

    print('Updating mime database...')
    call(['update-mime-database', path.join(sharedir, 'mime')])

    print('Updating desktop database...')
    call(['update-desktop-database', '-q', path.join(sharedir, 'applications')])

    print('Compiling GSettings schemas...')
    call(['glib-compile-schemas', path.join(sharedir, 'glib-2.0', 'schemas')])
