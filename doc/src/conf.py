# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

# pylint: disable=invalid-name,missing-module-docstring

import os.path
import sys

# Basic project configuration
project_copyright = "2024, Rice University"

# Sphinx configuration options
language = "en"
nitpicky = True
master_doc = "index"

# Enable extensions. Custom extensions are under _ext/
sys.path.insert(0, os.path.abspath("./_ext"))
extensions = ["myst_parser", "sphinx_depfile"]
needs_extensions = {"myst_parser": "0.16"}

# Configuration for the (primary) HTML output.
html_theme = "alabaster"

# Configuration for MyST, the Markdown parser for Sphinx.
myst_enable_extensions = [
    "deflist",
    "replacements",
    "smartquotes",
]
