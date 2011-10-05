"""distutils.command

Package containing implementation of all the standard Distutils
commands."""

__revision__ = "$Id: __init__.py 74274 2009-08-01 09:34:39Z richard.tew $"

__all__ = ['build',
           'build_py',
           'build_ext',
           'build_clib',
           'build_scripts',
           'clean',
           'install',
           'install_lib',
           'install_headers',
           'install_scripts',
           'install_data',
           'sdist',
           'register',
           'bdist',
           'bdist_dumb',
           'bdist_rpm',
           'bdist_wininst',
           'check',
           'upload',
           # These two are reserved for future use:
           #'bdist_sdux',
           #'bdist_pkgtool',
           # Note:
           # bdist_packager is not included because it only provides
           # an abstract base class
          ]
